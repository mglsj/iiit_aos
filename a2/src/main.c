#include "autocomplete.h"
#include "background.h"
#include "executable.h"
#include "helpers.h"
#include "history.h"
#include "processor.h"

#include <readline/readline.h>
#include <signal.h>

ImmutableString progname;

static volatile sig_atomic_t interrupted = 0;

// Record Ctrl-C arrived
static void handle_sigint(int signo) {
  (void)signo;
  interrupted = 1;
}

// prompt fresh line on interrupt
static int discard_line_on_interrupt(void) {
  if (interrupted) {
    interrupted = 0;
    last_exit_status = 130; // 128 + SIGINT

    rl_replace_line("", 0);
    rl_crlf();
    rl_on_new_line();
    rl_redisplay();
  }
  return 0;
}

void init() {
  // set ~
  init_shell_home();

  // Readline config
  rl_bind_key('\t', rl_complete);
  rl_attempted_completion_function = custom_completion;
  rl_variable_bind("show-all-if-ambiguous", "on");

  // Load history
  init_history();

  // Handle SIGINT
  rl_catch_signals = 0;
  rl_signal_event_hook = discard_line_on_interrupt;

  struct sigaction sa = {};
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);

  // Setup Background jobs table
  background_jobs = List_BackgroundJob_new();

  // Own the terminal (ignores interrupts)
  init_job_control();
}

void cleanup() {
  // free background jobs table
  List_BackgroundJob_delete(background_jobs);
}

int main(int argc, ImmutableString argv[static argc]) {
  progname = argv[0];

  // Flush after every printf
  setbuf(stdout, nullptr);

  init();

  while (true) {
    interrupted = 0;

    AUTOFREE String prompt = get_prompt();
    AUTOFREE String line = readline(prompt);

    if (line == nullptr) {
      if (interrupted) {
        // Ctrl-C
        interrupted = 0;
        printf("\n");
        continue;
      }
      break; // Ctrl-D
    }

    AUTOFREE String args = strip_whitespace(line);

    if (args != nullptr && args[0] != '\0') {
      record_command(args); // add to history
      process_command(args);
    }
  }

  cleanup();

  return last_exit_status;
}
