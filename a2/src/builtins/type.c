#include "commands.h"

#include "../builtins.h"
#include "../executable.h"

void type(const ImmutableString *argv) {
  if (*(argv++) == nullptr) {
    return;
  }

  for (ImmutableString command = *(argv++); command != nullptr;
       command = *(argv++)) {
    if (is_builtin(command)) {
      printf("%s is a shell builtin\n", command);
      continue;
    }

    AUTOFREE String executable = nullptr;

    if ((executable = find_executable(command)) != nullptr) {
      printf("%s is %s\n", command, executable);
      continue;
    }

    fprintf(stderr, "%s: not found\n", command);
    last_exit_status = 1;
  }
}
