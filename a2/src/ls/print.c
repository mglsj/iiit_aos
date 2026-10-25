#define _GNU_SOURCE

#include "print.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

constexpr int MAX_COLUMNS = 256;

typedef struct {
  int valid;
  int line_len;
  int max_len[MAX_COLUMNS];
} column_info;

constexpr int MIN_COLUMN_WIDTH = 3; // 1 char + 2 spaces
constexpr time_t SIX_MONTHS = 365 * 24 * 60 * 60 / 2;

static bool stdout_is_terminal(void) { return isatty(STDOUT_FILENO) == 1; }

// Writes a name in its type color, left-aligned in a column `pad` wide.
static void print_padded_entry(ImmutableString name, ImmutableString color,
                               int pad) {
  static bool reset_color = true;

  if (!stdout_is_terminal() || strcmp(color, COLOR_REGULAR) == 0) {
    // no escapes
    printf("%-*s", pad, name);
    return;
  }

  AUTOFREE String entry = nullptr;

  const int len = asprintf(&entry, "%s%s%s%s", reset_color ? COLOR_RESET : "",
                           color, name, COLOR_RESET);
  if (len < 0) {
    return;
  }
  reset_color = false;

  const int invisible = len - (int)strlen(name);
  printf("%-*s", pad + invisible, entry);
}

static void print_padded_name(const FileInfo *file, int pad) {
  print_padded_entry(file->name, file->color, pad);
}

static void print_name(const FileInfo *file) { print_padded_name(file, 0); }

// The "-> target" half of a long-format symlink line.
static void print_link_target(const FileInfo *file) {
  if (file->link_broken) {
    printf("%s", file->link_target);
    return;
  }

  print_padded_entry(file->link_target, file->link_color, 0);
}

static void print_timestamp(const FileInfo *file) {
  const time_t stamp = file->st.st_mtime;

  struct tm *timeinfo = localtime(&stamp);
  if (timeinfo == nullptr) {
    printf("%-12s ", "?");
    return;
  }

  char filetime[32];
  const time_t now = time(nullptr);

  // For files modified a long time ago, show the year instead of the time.
  ImmutableString format =
      (stamp > now || now - stamp > SIX_MONTHS) ? "%b %e  %Y" : "%b %e %H:%M";

  if (strftime(filetime, sizeof(filetime), format, timeinfo) == 0) {
    printf("%-12s ", "?");
    return;
  }

  printf("%-12s ", filetime);
}

// ls -l
static void print_list(FileInfo *files, int count, Widths *widths) {
  if (widths->listing_a_directory)
    printf("total %d\n", widths->total_blocks);

  for (int i = 0; i < count; ++i) {
    printf("%s ", files[i].permission);
    printf("%*ld ", widths->nlink_width, files[i].st.st_nlink);
    printf("%-*s ", widths->ownername_width, files[i].owner_name);
    printf("%-*s ", widths->groupname_width, files[i].group_name);

    if (S_ISCHR(files[i].st.st_mode) || S_ISBLK(files[i].st.st_mode)) {
      char devices[32];
      snprintf(devices, sizeof(devices), "%d, %d", major(files[i].st.st_rdev),
               minor(files[i].st.st_rdev));
      printf("%*s ", widths->size_width, devices);
    } else {
      printf("%*ld ", widths->size_width, files[i].st.st_size);
    }

    print_timestamp(&files[i]);
    print_name(&files[i]);

    if (files[i].is_link) {
      printf(" -> ");
      print_link_target(&files[i]);
    }

    printf("\n");
  }
}

/* Picks the widest column count whose rows still fit the terminal. */
static int get_column_count(column_info *column_configs, FileInfo *files,
                            int file_count, Widths *widths) {
  int max_idx = widths->window_width / MIN_COLUMN_WIDTH - 1;
  int max_cols = (max_idx < file_count) ? max_idx : file_count;

  // column_configs / max_len are fixed-size: never index past them.
  if (max_cols > MAX_COLUMNS)
    max_cols = MAX_COLUMNS;
  if (max_cols < 1)
    max_cols = 1;

  for (int file_idx = 0; file_idx < file_count; ++file_idx) {
    for (int config_idx = 0; config_idx < max_cols; ++config_idx) {
      if (!column_configs[config_idx].valid)
        continue; // Skip invalid configurations

      int rows = (file_count + config_idx) / (config_idx + 1);

      // If too much allocated column/row space
      if ((rows * (config_idx + 1) - file_count) > rows) {
        column_configs[config_idx].valid = 0;
        continue;
      }

      // find file column
      int col = file_idx / rows;
      if (col >= MAX_COLUMNS)
        continue;

      int name_width = strlen(files[file_idx].name);

      // Check if the file name width exceeds the current column width
      if (name_width > column_configs[config_idx].max_len[col]) {
        column_configs[config_idx].line_len +=
            name_width - column_configs[config_idx].max_len[col];
        column_configs[config_idx].max_len[col] = name_width;
      }

      // Consider spaces as well
      if (column_configs[config_idx].line_len + (2 * config_idx) >
          widths->window_width)
        column_configs[config_idx].valid = 0;
    }
  }

  // Find the highest valid configuration
  int selected_config = max_cols - 1;
  while (selected_config >= 0 &&
         (!column_configs[selected_config].valid ||
          !column_configs[selected_config].max_len[selected_config]))
    selected_config--;

  return selected_config + 1; // Return the number of columns
}

// Plain ls
static void print_tabular(FileInfo *files, int file_count, Widths *widths) {
  column_info column_configs[MAX_COLUMNS] = {
      [0 ... MAX_COLUMNS - 1] = {.valid = 1, .line_len = 0, .max_len = {}}};

  // Off a terminal there are no columns to fit: one entry per line.
  int ncols = stdout_is_terminal()
                  ? get_column_count(column_configs, files, file_count, widths)
                  : 1;

  // A degenerate window width must still produce one usable column.
  if (ncols < 1)
    ncols = 1;

  int nrows = (file_count + ncols - 1) / ncols;

  for (int i = 0; i < nrows; i++) {
    for (int j = 0; j < ncols; j++) {
      int file_idx = i + j * nrows;

      // The layout can allocate more cells than there are files.
      if (file_idx >= file_count)
        break;

      if (j > 0)
        printf("  ");

      // Never pad the last name on a row
      bool last_in_row =
          (j == ncols - 1) || (i + (j + 1) * nrows >= file_count);

      print_padded_name(&files[file_idx],
                        last_in_row ? 0 : column_configs[ncols - 1].max_len[j]);
    }
    printf("\n");
  }
}

void print_ls(const Vector_FileInfo *files, Options options, Widths *widths) {
  if (files->size == 0)
    return;

  const int count = (int)files->size;

  if (options.long_format)
    print_list(files->data, count, widths);
  else
    print_tabular(files->data, count, widths);
}
