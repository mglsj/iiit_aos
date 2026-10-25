/*
 * Source:
 * https://tiswww.case.edu/php/chet/readline/readline.html#A-Short-Completion-Example-1
 */

#include "autocomplete.h"

#include "builtins.h"
#include "executable.h"

#include <dirent.h>
#include <sys/stat.h>

#include <readline/history.h>
#include <readline/readline.h>

static String builtin_generator(ImmutableString text, const int state) {
  static size_t len;
  static int list_index;

  if (!state) {
    len = strlen(text);
    list_index = 0;
  }

  ImmutableString name;
  while ((name = builtins[list_index++].name) != nullptr) {
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return nullptr;
}

static String path_generator(ImmutableString text, const int state) {
  static size_t len;

  static String paths;
  static String current_path;
  static DIR *dir;
  static char current_dir_path[1024];

  if (!state) {
    len = strlen(text);

    if (dir) {
      closedir(dir);
      dir = nullptr;
    }
    free(paths);

    ImmutableString env_path = getenv("PATH");
    paths = env_path ? strdup(env_path) : nullptr;
    current_path = paths ? strtok(paths, ":") : nullptr;
  }

  while (current_path != nullptr || dir != nullptr) {
    if (!dir) {
      if (!current_path)
        break;

      strncpy(current_dir_path, current_path, sizeof(current_dir_path));
      current_dir_path[sizeof(current_dir_path) - 1] = '\0';

      dir = opendir(current_dir_path);
      current_path = strtok(nullptr, ":"); // fetch next
      if (!dir)
        continue;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] == '.')
        continue;

      if (strncmp(entry->d_name, text, len) != 0)
        continue;

      String fullpath;
      if (asprintf(&fullpath, "%s/%s", current_dir_path, entry->d_name) < 0)
        continue;

      bool executable = is_executable(fullpath);
      free(fullpath);

      if (executable) {
        return strdup(entry->d_name);
      }
    }
    closedir(dir);
    dir = nullptr;
  }

  free(paths);
  paths = nullptr;

  return nullptr;
}

static String relative_path_generator(ImmutableString text, const int state) {
  static size_t base_len;
  static String dir_part;
  static String base_part;
  static DIR *dir;

  if (!state) {
    if (dir) {
      closedir(dir);
      dir = nullptr;
    }
    free(dir_part);
    free(base_part);

    ImmutableString slash = strrchr(text, '/');
    size_t dir_len = (size_t)(slash - text) + 1;

    dir_part = strndup(text, dir_len);
    base_part = strdup(slash + 1);
    base_len = strlen(base_part);

    dir = opendir(dir_part);
  }

  if (dir) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] == '.')
        continue;

      if (strncmp(entry->d_name, base_part, base_len) != 0)
        continue;

      String fullpath;
      if (asprintf(&fullpath, "%s%s", dir_part, entry->d_name) < 0)
        continue;

      struct stat st;
      if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
        String match;
        if (asprintf(&match, "%s/", fullpath) < 0) {
          free(fullpath);
          continue;
        }
        free(fullpath);
        return match;
      }

      if (is_executable(fullpath)) {
        return fullpath;
      }

      free(fullpath);
    }

    closedir(dir);
    dir = nullptr;
  }

  free(dir_part);
  free(base_part);
  dir_part = nullptr;
  base_part = nullptr;

  return nullptr;
}

static bool already_suggested(Vector_String *seen, ImmutableString name) {
  for (size_t i = 0; i < seen->size; i++) {
    if (STR_EQL(seen->data[i], name)) {
      return true;
    }
  }
  return false;
}

static String generator(ImmutableString text, const int state) {
  static bool searching_path;
  static bool is_relative;
  static Vector_String *seen;

  if (!state) {
    is_relative = strchr(text, '/') != nullptr;
    searching_path = false;
  }

  if (is_relative) {
    return relative_path_generator(text, state);
  }

  if (!state) {
    if (seen) {
      Vector_String_delete(seen);
    }
    seen = Vector_String_new();
  }

  while (true) {
    String match;

    if (!searching_path) {
      match = builtin_generator(text, state);
      if (match == nullptr) {
        searching_path = true;
        match = path_generator(text, 0);
      }
    } else {
      match = path_generator(text, 1);
    }

    if (match == nullptr) {
      Vector_String_delete(seen);
      seen = nullptr;
      return nullptr;
    }

    if (already_suggested(seen, match)) {
      free(match);
      continue;
    }

    String record = strdup(match);
    if (record == nullptr || !Vector_String_push_back(seen, record))
      free(record);

    return match;
  }
}

String *custom_completion(ImmutableString text, int start,
                          [[maybe_unused]] int end) {
  String *matches = nullptr;

  /* Only autocomplete custom commands if this is the first word on the line */
  if (start == 0) {
    matches = rl_completion_matches(text, generator);
  }

  return matches;
}
