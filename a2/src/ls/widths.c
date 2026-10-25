#include "widths.h"

#include "../include/commons.h"

#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#define FORMATTED_LEN(...) snprintf(nullptr, 0, __VA_ARGS__)

constexpr int DEFAULT_WINDOW_WIDTH = 80;

int get_window_width(void) {
  struct winsize w;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
    return w.ws_col;

  ImmutableString columns = getenv("COLUMNS");
  if (columns != nullptr) {
    int cols = atoi(columns);
    if (cols > 0)
      return cols;
  }

  return DEFAULT_WINDOW_WIDTH;
}

void update_widths(Widths *widths, const FileInfo *file) {
  widths->nlink_width =
      max(widths->nlink_width, FORMATTED_LEN("%ld", file->st.st_nlink));
  widths->ownername_width =
      max(widths->ownername_width, (int)strlen(file->owner_name));
  widths->groupname_width =
      max(widths->groupname_width, (int)strlen(file->group_name));

  // a device shows "major, minor" where a regular file shows its size
  const int size_width =
      (S_ISCHR(file->st.st_mode) || S_ISBLK(file->st.st_mode))
          ? FORMATTED_LEN("%d, %d", major(file->st.st_rdev),
                          minor(file->st.st_rdev))
          : FORMATTED_LEN("%ld", file->st.st_size);

  widths->size_width = max(widths->size_width, size_width);
  widths->total_blocks += (file->st.st_blocks / 2);
}
