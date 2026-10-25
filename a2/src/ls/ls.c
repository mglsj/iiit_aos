#include "ls.h"

#include "../executable.h"
#include "../helpers.h"
#include "filedata.h"
#include "parser.h"
#include "print.h"
#include "sort.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define CURR_DIR "."

static inline void auto_close_dir(DIR **dir_p) {
  if (dir_p != nullptr && *dir_p != nullptr) {
    closedir(*dir_p);
    *dir_p = nullptr;
  }
}

#define AUTOCLOSEDIR __attribute__((cleanup(auto_close_dir)))

static void read_link_data(int dirfd, FileInfo *file) {
  file->is_link = true;

  ssize_t len = readlinkat(dirfd, file->name, file->link_target,
                           sizeof(file->link_target) - 1);
  if (len < 0) {
    file->link_target[0] = '\0';
    file->link_broken = true;
    return;
  }
  file->link_target[len] = '\0';

  struct stat target;
  if (fstatat(dirfd, file->name, &target, 0) == -1) {
    // Dangling link
    file->link_broken = true;
    strcpy(file->color, COLOR_BROKEN_LINK);
    return;
  }

  char target_extension[256];
  construct_permission_str(target.st_mode, file->link_permission);
  get_file_extension(file->link_target, target_extension);
  get_color(file->link_permission, target_extension, file->link_color);
}

// Get file `name` info located at `dirfd`
static bool fill_file_info(int dirfd, ImmutableString name, FileInfo *entry,
                           Widths *widths) {
  memset(entry, 0, sizeof(*entry));

  if (fstatat(dirfd, name, &entry->st, AT_SYMLINK_NOFOLLOW) == -1) {
    return false;
  }

  snprintf(entry->name, sizeof(entry->name), "%s", name);
  get_username_from_uid(entry->st.st_uid, entry->owner_name);
  get_groupname_from_gid(entry->st.st_gid, entry->group_name);
  construct_permission_str(entry->st.st_mode, entry->permission);
  get_file_extension(entry->name, entry->extension);
  get_color(entry->permission, entry->extension, entry->color);

  if (entry->permission[0] == 'l') {
    read_link_data(dirfd, entry);
  }

  update_widths(widths, entry);
  return true;
}

// resolve ~
[[nodiscard]] static String resolve_operand(ImmutableString operand) {
  return (operand[0] == '~') ? expand_user_directory(operand) : strdup(operand);
}

static bool is_directory(ImmutableString operand) {
  AUTOFREE String path = resolve_operand(operand);
  struct stat st;

  return path != nullptr && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Collect the file or the files in directory
static bool collect_entries(ImmutableString operand, Vector_FileInfo *files,
                            Options options, Widths *widths) {
  AUTOFREE String path = resolve_operand(operand);
  if (path == nullptr) {
    return false;
  }

  struct stat operand_stat;
  if (stat(path, &operand_stat) == -1) {
    fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
    return false;
  }

  FileInfo entry;

  // listing file
  if (!S_ISDIR(operand_stat.st_mode)) {
    widths->listing_a_directory = false;

    if (!fill_file_info(AT_FDCWD, path, &entry, widths)) {
      fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
      return false;
    }

    if (!Vector_FileInfo_push_back(files, entry)) {
      fprintf(stderr, "ls: %s: %s\n", path, strerror(ENOMEM));
      return false;
    }

    return true;
  }

  // listing directory
  AUTOCLOSEDIR DIR *dir = opendir(path);
  if (dir == nullptr) {
    fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
    return false;
  }

  widths->listing_a_directory = true;

  const int dfd = dirfd(dir);
  struct dirent *file;

  while ((file = readdir(dir)) != nullptr) {
    if (file->d_name[0] == '.' && !options.show_hidden)
      continue;

    if (!fill_file_info(dfd, file->d_name, &entry, widths)) {
      fprintf(stderr, "ls: %s/%s: %s\n", path, file->d_name, strerror(errno));
      continue;
    }

    if (!Vector_FileInfo_push_back(files, entry)) {
      // OOM
      fprintf(stderr, "ls: %s: %s\n", path, strerror(ENOMEM));
      break;
    }
  }

  return true;
}

// List one. False if the operand could not be listed.
static bool list(ImmutableString operand, Options options, int window_width,
                 ImmutableString heading_separator) {
  Vector_FileInfo files;
  if (!Vector_FileInfo_init(&files)) {
    fprintf(stderr, "ls: out of memory\n");
    return false;
  }

  Widths widths = {.window_width = window_width};

  const bool listed = collect_entries(operand, &files, options, &widths);

  if (listed) {
    if (heading_separator != nullptr && is_directory(operand)) {
      printf("%s%s:\n", heading_separator, operand);
    }

    sort(&files);
    print_ls(&files, options, &widths);
  }

  Vector_FileInfo_free(&files);
  return listed;
}

void ls(const ImmutableString argv[]) {
  int argc = 0;
  while (argv[argc] != nullptr)
    argc++;

  ImmutableString *operands =
      calloc(argc > 0 ? argc : 1, sizeof(ImmutableString));
  if (operands == nullptr) {
    fprintf(stderr, "ls: out of memory\n");
    last_exit_status = 1;
    return;
  }

  int operand_count = 0;
  for (int i = 1; i < argc; i++)
    if (argv[i][0] != '-') // a path, not a flag
      operands[operand_count++] = argv[i];

  Options options = parse_flags(argc, argv);

  const int window_width = get_window_width();

  // Any operand that could not be listed fails the command as a whole
  bool listed = true;

  if (operand_count <= 1) {
    listed = list(operand_count == 0 ? CURR_DIR : operands[0], options,
                  window_width, nullptr);
  } else {
    for (int i = 0; i < operand_count; i++) {
      listed &= list(operands[i], options, window_width, i == 0 ? "" : "\n");
    }
  }

  if (!listed) {
    last_exit_status = 1;
  }

  free(operands);
}
