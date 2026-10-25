#define _GNU_SOURCE

#include "helpers.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

static String shell_home;

void init_shell_home(void) { shell_home = get_current_dir_name(); }

ImmutableString get_shell_home(void) {
  return (shell_home != nullptr) ? shell_home : getenv("HOME");
}

bool is_number(ImmutableString s) {
  if (*s == '\0') {
    return false;
  }

  while (*s != '\0') {
    if (!isdigit((unsigned char)*s)) {
      return false;
    }
    s++;
  }
  return true;
}

// Trims leading and trailing whitespace only
String strip_whitespace(ImmutableString s) {
  ImmutableString start = s;
  while (*start != '\0' && isspace((unsigned char)*start)) {
    start++;
  }

  ImmutableString end = start + strlen(start);
  while (end > start && isspace((unsigned char)end[-1])) {
    end--;
  }

  return strndup(start, (size_t)(end - start));
}

ImmutableString get_current_username(void) {
  struct passwd *pw = getpwuid(geteuid());

  if (pw != nullptr && pw->pw_name != nullptr && *pw->pw_name != '\0') {
    return pw->pw_name;
  }
  return nullptr;
}

static ImmutableString home_of_named_user(const ImmutableString dir,
                                          ImmutableString *rest) {
  *rest = strchr(dir + 1, '/');
  const size_t name_len =
      (*rest != nullptr) ? (size_t)(*rest - (dir + 1)) : strlen(dir + 1);

  AUTOFREE String username = strndup(dir + 1, name_len);
  if (username == nullptr) {
    fprintf(stderr, "%s: out of memory\n", progname);
    return nullptr;
  }

  /* Report here rather than at the call sites: every caller that expands a
     `~user` needs the same message, and only this scope knows the name that
     failed to resolve. */
  struct passwd *pw = getpwnam(username);
  if (pw == nullptr) {
    fprintf(stderr, "%s: ~%s: no such user\n", progname, username);
    return nullptr;
  }

  if (*rest == nullptr) {
    *rest = "";
  }
  return pw->pw_dir;
}

String expand_user_directory(const ImmutableString dir) {
  ImmutableString home;
  ImmutableString rest;

  if (dir[1] == '\0' || dir[1] == '/') {
    // ~ or ~/something
    home = get_shell_home();
    if (home == nullptr) {
      fprintf(stderr, "%s: cannot determine the shell's home directory\n",
              progname);
      return nullptr;
    }
    rest = dir + 1;
  } else {
    // ~user or ~user/something: without this, "~root" became "$HOMEroot".
    home = home_of_named_user(dir, &rest);
    if (home == nullptr) {
      return nullptr;
    }
  }

  String path;
  return (asprintf(&path, "%s%s", home, rest) < 0) ? nullptr : path;
}

String collapse_user_directory(const ImmutableString dir) {
  const ImmutableString home = get_shell_home();
  if (home == nullptr) {
    return nullptr;
  }

  const size_t len = strlen(home);
  if (strncmp(dir, home, len) != 0) {
    return nullptr;
  }

  // The prefix must end at a path boundary
  if (dir[len] != '\0' && dir[len] != '/') {
    return nullptr;
  }

  String collapsed;
  return (asprintf(&collapsed, "~%s", dir + len) < 0) ? nullptr : collapsed;
}

String get_prompt(void) {
  ImmutableString user = get_current_username();
  if (user == nullptr) {
    user = "?";
  }

  char host[HOST_NAME_MAX + 1] = {0};
  if (gethostname(host, sizeof(host) - 1) != 0) {
    strcpy(host, "?");
  }

  AUTOFREE String directory = get_current_dir_name();
  AUTOFREE String collapsed =
      (directory != nullptr) ? collapse_user_directory(directory) : nullptr;

  ImmutableString shown = collapsed ? collapsed : directory ? directory : "?";

  String prompt;
  if (asprintf(&prompt, "<%s%s%s@%s%s%s:%s%s%s> ", BBLUE, user, COLOR_OFF,
               BCYAN, host, COLOR_OFF, BPURPLE, shown, COLOR_OFF) < 0) {
    return nullptr;
  }
  return prompt;
}
