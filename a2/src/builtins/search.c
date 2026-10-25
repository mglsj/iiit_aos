#define _GNU_SOURCE

#include "commands.h"

#include "../executable.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define CURR_DIR "."
#define PREV_DIR ".."

static inline void auto_close_dir(DIR **dir_p) {
  if (dir_p != nullptr && *dir_p != nullptr) {
    closedir(*dir_p);
    *dir_p = nullptr;
  }
}

#define AUTOCLOSEDIR __attribute__((cleanup(auto_close_dir)))

static bool is_dot_entry(const ImmutableString name) {
  return STR_EQL(name, CURR_DIR) || STR_EQL(name, PREV_DIR);
}

// Depth First Search
static bool find(const ImmutableString filename,
                 const ImmutableString dirname) {
  AUTOCLOSEDIR DIR *dir_p = opendir(dirname);

  if (dir_p == nullptr) {
    return false; // unreadable directory
  }

  const int dir_fd = dirfd(dir_p);
  struct dirent *entry;

  while ((entry = readdir(dir_p)) != nullptr) {
    if (is_dot_entry(entry->d_name)) {
      continue;
    }

    if (STR_EQL(entry->d_name, filename)) {
      return true;
    }

    struct stat entry_stat;

    if (fstatat(dir_fd, entry->d_name, &entry_stat, AT_SYMLINK_NOFOLLOW) < 0) {
      continue;
    }

    if (!S_ISDIR(entry_stat.st_mode)) {
      continue;
    }

    AUTOFREE String sub_dir = nullptr;
    if (asprintf(&sub_dir, "%s/%s", dirname, entry->d_name) < 0) {
      continue;
    }

    if (find(filename, sub_dir)) {
      return true;
    }
  }

  return false;
}

void search(const ImmutableString *argv) {
  if (argv[1] == nullptr) {
    fprintf(stderr, "%s: search: missing operand\n", progname);
    last_exit_status = 1;
    return;
  }

  for (int i = 1; argv[i] != nullptr; i++) {
    ImmutableString filename = argv[i];

    // get filename only
    ImmutableString slash = strrchr(filename, '/');
    if (slash != nullptr) {
      filename = slash + 1;
    }

    const bool tty = isatty(STDOUT_FILENO) == 1;
    const bool found = find(filename, CURR_DIR);

    printf("%s%s%s\n", tty ? (found ? GREEN : RED) : "",
           found ? "True" : "False", tty ? COLOR_OFF : "");
  }
}
