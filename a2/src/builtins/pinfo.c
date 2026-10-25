#include "commands.h"

#include "../executable.h"
#include "../helpers.h"

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

// False if the process could not be inspected.
static bool print_pinfo(const pid_t pid) {
  AUTOFREE String stat_path = nullptr;
  AUTOFREE String exe_path = nullptr;

  if (asprintf(&stat_path, "/proc/%d/stat", pid) < 0 ||
      asprintf(&exe_path, "/proc/%d/exe", pid) < 0) {
    fprintf(stderr, "%s: pinfo: out of memory\n", progname);
    return false;
  }

  AUTOCLOSE FILE *fp = fopen(stat_path, "r");

  if (fp == nullptr) {
    fprintf(stderr, "%s: pinfo: %d: %s\n", progname, pid, strerror(errno));
    return false;
  }

  char state;
  int pgrp, tpgid;
  unsigned long vsize;

  if (fscanf(fp,
             "%*d (%*[^)]) %c %*d %d %*d %*d %d %*u %*u %*u %*u %*u %*u %*u "
             "%*d %*d %*d %*d %*d %*d %*u %lu",
             &state, &pgrp, &tpgid, &vsize) != 4) {
    fprintf(stderr, "%s: pinfo: %d: cannot parse /proc/%d/stat\n", progname,
            pid, pid);
    return false;
  }

  // mark foreground process
  char status[4];
  snprintf(status, sizeof(status), (pgrp == tpgid) ? "%c+" : "%c", state);

  char executable[PATH_MAX];
  const ssize_t len = readlink(exe_path, executable, sizeof(executable) - 1);

  if (len < 0) {
    strcpy(executable, "Unavailable");
  } else {
    executable[len] = '\0';

    AUTOFREE String collapsed = collapse_user_directory(executable);
    if (collapsed != nullptr) {
      snprintf(executable, sizeof(executable), "%s", collapsed);
    }
  }

  printf("pid             -- %d\n", pid);
  printf("Process Status  -- %s\n", status);
  printf("Memory          -- %lu {Virtual Memory}\n", vsize);
  printf("Executable Path -- %s\n", executable);
  return true;
}

void pinfo(const ImmutableString *argv) {
  pid_t pid;
  int i;

  // Any PID that could not be reported fails the command as a whole
  bool reported = true;

  for (i = 1; argv[i] != nullptr; i++) {
    if (i > 1)
      putchar('\n');

    if (is_number(argv[i])) {
      pid = atoi(argv[i]);
      reported &= print_pinfo(pid);
    } else {
      fprintf(stderr, "%s: pinfo: invalid PID '%s'\n", progname, argv[i]);
      reported = false;
    }
  }

  if (i == 1) {
    // current process info
    pid = getpid();
    reported = print_pinfo(pid);
  }

  if (!reported) {
    last_exit_status = 1;
  }
}
