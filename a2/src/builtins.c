#define _GNU_SOURCE

#include "builtins.h"

#include "builtins/commands.h"
#include "executable.h"
#include "fcntl.h"
#include "ls/ls.h"

#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

void shell_exit(const ImmutableString *argv) {
  int status = last_exit_status;

  if (argv[1] != nullptr)
    status = (int)strtol(argv[1], nullptr, 10) & 0xFF;

  exit(status);
}

Builtin builtins[] = {
    {"cd", cd},
    {"complete", complete},
    {"echo", echo},
    {"exit", shell_exit},
    {"history", history},
    {"jobs", jobs},
    {"ls", ls},
    {"pwd", pwd},
    {"search", search},
    {"type", type},
    {"pinfo", pinfo},
    {},
};

bool is_builtin(const ImmutableString command_name) {
  for (int i = 0; builtins[i].name != nullptr; i++) {
    if (STR_EQL(builtins[i].name, command_name)) {
      return true;
    }
  }

  return false;
}

typedef struct {
  int fd[3];
} SavedFds;

#define NO_SAVED_FDS ((SavedFds){.fd = {-1, -1, -1}})

static void restore_std_fds(SavedFds *saved) {
  for (int i = 0; i < 3; i++) {
    if (saved->fd[i] != -1) {
      dup2(saved->fd[i], i);
      close(saved->fd[i]);
      saved->fd[i] = -1;
    }
  }
}

static int open_redirect(const Redirect redirect) {
  switch (redirect.mode) {
  case READ:
    return open(redirect.filename, O_RDONLY);
  case WRITE:
    return open(redirect.filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  case APPEND:
    return open(redirect.filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
  default:
    unreachable();
  }
}

static bool apply_redirects(Command *const restrict command_p,
                            SavedFds *saved) {
  if (command_p == nullptr) {
    return true;
  }

  for (size_t i = 0; i < command_p->options.redirects.size; i++) {
    const Redirect redirect = command_p->options.redirects.data[i];

    const int fd = open_redirect(redirect);
    if (fd < 0) {
      perror(redirect.filename);
      if (saved != nullptr) {
        restore_std_fds(saved);
      }
      return false;
    }

    const bool is_std_fd = redirect.fd >= 0 && redirect.fd < 3;
    if (saved != nullptr && is_std_fd && saved->fd[redirect.fd] == -1) {
      saved->fd[redirect.fd] = dup(redirect.fd);
    }

    dup2(fd, redirect.fd);
    close(fd);
  }

  return true;
}

bool apply_redirects_child(Command *const restrict command_p) {
  return apply_redirects(command_p, nullptr);
}

void apply_redirects_only(Command *const restrict command_p) {
  SavedFds saved = NO_SAVED_FDS;

  if (!apply_redirects(command_p, &saved)) {
    last_exit_status = 1;
    return;
  }

  restore_std_fds(&saved);
  last_exit_status = 0;
}

void run_builtin_here(Command *const restrict command_p) {
  ImmutableString command_name = command_p->argv.data[0];

  const ImmutableString *const argv =
      (const ImmutableString *const)command_p->argv.data;

  for (int i = 0; builtins[i].name != nullptr; i++) {
    if (STR_EQL(builtins[i].name, command_name)) {
      builtins[i].operation(argv);
      return;
    }
  }
}

void execute_builtin(Command *const restrict command_p) {
  SavedFds saved = NO_SAVED_FDS;

  // io redirection
  if (!apply_redirects(command_p, &saved)) {
    last_exit_status = 1;
    return;
  }

  last_exit_status = 0;

  run_builtin_here(command_p);

  fflush(stdout);
  fflush(stderr);

  restore_std_fds(&saved);
}
