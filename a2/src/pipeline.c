#include "pipeline.h"

#include "background.h"
#include "builtins.h"
#include "executable.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int Pipe[2];

static void close_pipes(size_t count, Pipe pipes[count]) {
  for (size_t k = 0; k < count; k++) {
    close(pipes[k][0]);
    close(pipes[k][1]);
  }
}

// Open count pipes
static bool open_pipes(size_t count, Pipe pipes[count]) {
  for (size_t made = 0; made < count; made++) {
    if (pipe(pipes[made]) < 0) {
      perror("pipe");
      close_pipes(made, pipes);
      return false;
    }
  }
  return true;
}

// Replace this process with command
[[noreturn]] static void become_command(Command *command) {
  if (command->argv.size == 0) {
    // empty command (only redirects)
    _exit(EXIT_SUCCESS);
  }

  ImmutableString name = command->argv.data[0];

  if (is_builtin(name)) {
    last_exit_status = 0;
    run_builtin_here(command);
    fflush(nullptr);
    _exit(last_exit_status);
  }

  String executable = resolve_command(name);
  if (executable == nullptr) {
    // command not found
    _exit(last_exit_status);
  }

  execv(executable, command->argv.data);

  perror(executable);
  _exit(errno == ENOENT ? 127 : 126);
}

// child half os stage
[[noreturn]] static void become_stage(size_t index, size_t n,
                                      Command commands[n], Pipe *pipes,
                                      pid_t pgid, bool background) {
  // join process group
  const pid_t group = (pgid == 0) ? getpid() : pgid;
  setpgid(0, group);

  // Claim the terminal
  if (!background) {
    give_terminal_to(group);
  }

  // respond to interrupts
  signal(SIGINT, background ? SIG_IGN : SIG_DFL);
  signal(SIGQUIT, background ? SIG_IGN : SIG_DFL);
  signal(SIGTSTP, background ? SIG_IGN : SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);

  // copy pipe to stdio
  if (index > 0) {
    // first process skip input pipe
    dup2(pipes[index - 1][0], STDIN_FILENO);
  }
  if (index + 1 < n) {
    // last process skip output pipe
    dup2(pipes[index][1], STDOUT_FILENO);
  }
  close_pipes(n - 1, pipes);

  if (!apply_redirects_child(&commands[index])) {
    _exit(EXIT_FAILURE);
  }

  become_command(&commands[index]);
}

// wait for entire pipeline to finish
static bool wait_for_pipeline(pid_t *pids, size_t count) {
  void (*istat)(int) = signal(SIGINT, SIG_IGN);
  void (*qstat)(int) = signal(SIGQUIT, SIG_IGN);

  bool stopped = false;

  for (size_t i = 0; i < count && !stopped; i++) {
    int w, status;
    do {
      w = waitpid(pids[i], &status, WUNTRACED);
    } while (w == -1 && errno == EINTR);

    if (w > 0 && WIFSTOPPED(status)) {
      last_exit_status = 128 + WSTOPSIG(status);
      stopped = true;
      break;
    }

    if (i + 1 < count) {
      continue;
    }

    if (w == -1) {
      last_exit_status = -1;
    } else if (WIFEXITED(status)) {
      last_exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      last_exit_status = 128 + WTERMSIG(status);
    }
  }

  signal(SIGINT, istat);
  signal(SIGQUIT, qstat);

  return stopped;
}

// how `jobs` labels a backgrounded pipeline
[[nodiscard]] static String describe_pipeline(size_t n, Command commands[n]) {
  Vector_char label;
  if (!Vector_char_init(&label)) {
    return nullptr;
  }

  for (size_t i = 0; i < n; i++) {
    for (size_t a = 0; a < commands[i].argv.size; a++) {
      if (a > 0 && !Vector_char_push_back(&label, ' ')) {
        goto out_of_memory;
      }

      for (ImmutableString c = commands[i].argv.data[a]; *c != '\0'; c++) {
        if (!Vector_char_push_back(&label, *c)) {
          goto out_of_memory;
        }
      }
    }

    if (i + 1 < n) {
      for (ImmutableString sep = " | "; *sep != '\0'; sep++) {
        if (!Vector_char_push_back(&label, *sep)) {
          goto out_of_memory;
        }
      }
    }
  }

  String description = strdup(label.data);
  Vector_char_free(&label);
  return description;

out_of_memory:
  Vector_char_free(&label);
  return nullptr;
}

void execute_pipeline(size_t n, Command commands[n]) {
  if (n == 0) {
    return;
  }

  const bool background = commands[n - 1].options.operation == BACKGROUND;
  const size_t pipe_count = n - 1;

  Pipe *pipes = calloc(pipe_count + 1, sizeof(*pipes)); // +1: never calloc(0)
  pid_t *pids = calloc(n, sizeof(*pids));

  if (pipes == nullptr || pids == nullptr) {
    fprintf(stderr, "%s: out of memory\n", progname);
    goto failed;
  }

  if (!open_pipes(pipe_count, pipes)) {
    goto failed;
  }

  // clear buffers before fork
  fflush(nullptr);

  size_t started = 0;
  pid_t pgid = 0; // the first stage to start becomes the group leader

  for (; started < n; started++) {
    pid_t pid = fork();

    if (pid < 0) {
      perror("fork");
      break;
    }
    if (pid == 0) {
      become_stage(started, n, commands, pipes, pgid, background);
    }

    if (pgid == 0) {
      pgid = pid;
    }
    setpgid(pid, pgid);

    pids[started] = pid;
  }

  // drop every pipe as parent
  close_pipes(pipe_count, pipes);
  free(pipes);

  if (started == 0) {
    pipes = nullptr;
    goto failed;
  }

  if (background) {
    // only track final job
    register_background_job(pids[started - 1], describe_pipeline(n, commands),
                            RUNNING);
    last_exit_status = 0;
  } else {
    give_terminal_to(pgid);
    const bool stopped = wait_for_pipeline(pids, started);

    // take back terminal from pipeline
    give_terminal_to(shell_pgid);

    if (stopped) {
      // Ctrl-Z
      register_background_job(pids[started - 1], describe_pipeline(n, commands),
                              STOPPED);
    } else if (started != n) {
      // fork failed partway
      last_exit_status = 1;
    }
  }

  free(pids);
  return;

failed:
  free(pipes);
  free(pids);
  last_exit_status = 1;
}
