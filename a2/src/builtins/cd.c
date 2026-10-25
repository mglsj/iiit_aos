#define _GNU_SOURCE

#include "commands.h"

#include "../executable.h"
#include "../helpers.h"

#include <errno.h>
#include <unistd.h>

static String previous_directory = nullptr;

static void remember_previous_directory(String directory) {
  free(previous_directory);
  previous_directory = directory;
}

[[nodiscard]] static String target_directory(const ImmutableString operand) {
  if (operand[0] == '~') {
    return expand_user_directory(operand);
  }

  if (STR_EQL(operand, "-")) {
    if (previous_directory == nullptr) {
      fprintf(stderr, "%s: cd: OLDPWD not set\n", progname);
      return nullptr;
    }

    String path = strdup(previous_directory);
    if (path != nullptr) {
      printf("%s\n", path);
    }
    return path;
  }

  return strdup(operand);
}

void cd(const ImmutableString *argv) {
  if (argv[1] != nullptr && argv[2] != nullptr) {
    fprintf(stderr, "%s: cd: too many arguments\n", progname);
    last_exit_status = 1;
    return;
  }

  const ImmutableString operand = (argv[1] != nullptr) ? argv[1] : "~";

  AUTOFREE String path = target_directory(operand);
  if (path == nullptr) {
    last_exit_status = 1;
    return;
  }

  String leaving = get_current_dir_name();

  if (chdir(path) == -1) {
    fprintf(stderr, "%s: cd: %s: %s\n", progname, path, strerror(errno));
    free(leaving);
    last_exit_status = 1;
    return;
  }

  remember_previous_directory(leaving);
  last_exit_status = 0;
}
