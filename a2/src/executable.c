#include "executable.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int last_exit_status;

bool is_executable(const ImmutableString path) {
  struct stat stbuf;

  if (stat(path, &stbuf) == -1) {
    return false;
  }

  return S_ISREG(stbuf.st_mode) && access(path, X_OK) == 0;
}

String find_executable(const ImmutableString command) {
  if (command == nullptr || *command == '\0') {
    return nullptr;
  }

  // Handle ./ ../ /bin/ sub/
  if (strchr(command, '/') != nullptr) {
    return is_executable(command) ? strdup(command) : nullptr;
  }

  const ImmutableString env_path = getenv("PATH");
  if (env_path == nullptr) {
    return nullptr;
  }

  // Walk PATH
  for (ImmutableString start = env_path;; start++) {
    ImmutableString end = strchr(start, ':');
    size_t len = (end != nullptr) ? (size_t)(end - start) : strlen(start);

    String full_command;
    const int written =
        (len == 0)
            // an empty PATH entry means the current directory
            ? asprintf(&full_command, "./%s", command)
            : asprintf(&full_command, "%.*s/%s", (int)len, start, command);

    if (written < 0) {
      return nullptr;
    }

    if (is_executable(full_command)) {
      return full_command; // hand over the buffer rather than copying it
    }
    free(full_command); // this candidate did not match; try the next entry

    if (end == nullptr) {
      break;
    }
    start = end;
  }

  return nullptr;
}

String resolve_command(const ImmutableString command) {
  if (strchr(command, '/') == nullptr) {
    String executable = find_executable(command);

    if (executable == nullptr) {
      fprintf(stderr, "%s: command not found\n", command);
      last_exit_status = 127;
    }

    return executable;
  }

  struct stat stbuf;

  if (stat(command, &stbuf) == -1) {
    fprintf(stderr, "%s: %s\n", command, strerror(errno));
    last_exit_status = 127;
    return nullptr;
  }

  if (S_ISDIR(stbuf.st_mode)) {
    fprintf(stderr, "%s: Is a directory\n", command);
    last_exit_status = 126;
    return nullptr;
  }

  if (!S_ISREG(stbuf.st_mode) || access(command, X_OK) != 0) {
    fprintf(stderr, "%s: Permission denied\n", command);
    last_exit_status = 126;
    return nullptr;
  }

  return strdup(command);
}
