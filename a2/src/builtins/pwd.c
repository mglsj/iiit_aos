#define _GNU_SOURCE

#include "commands.h"

#include "../executable.h"

#include <unistd.h>

void pwd(const ImmutableString *argv) {
  (void)argv;

  AUTOFREE String path = get_current_dir_name();

  if (path == nullptr) {
    perror("pwd");
    last_exit_status = 1;
  } else {
    printf("%s\n", path);
  }
}
