#pragma once

#include "filedata.h"

// Column widths
typedef struct {
  int window_width;
  int nlink_width;
  int ownername_width;
  int groupname_width;
  int size_width;
  int total_blocks;

  // False when listing file
  bool listing_a_directory;
} Widths;

int get_window_width(void);
void update_widths(Widths *widths, const FileInfo *file);
