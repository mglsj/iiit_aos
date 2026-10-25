#pragma once

#include "../include/commons.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define COLOR_RESET COLOR_OFF
#define COLOR_REGULAR COLOR_OFF
#define COLOR_EXECUTABLES "\x1b[01;32m"
#define COLOR_DIRECTORIES "\x1b[01;34m"
#define COLOR_LINK "\x1b[01;36m"
#define COLOR_FIFO "\x1b[40;33m"
#define COLOR_SOCKET "\x1b[01;35m"
#define COLOR_BLK_CHR "\x1b[40;33;01m"
#define COLOR_BROKEN_LINK "\x1b[40;31;01m"
#define COLOR_ARCHIVE "\x1b[01;31m"

constexpr size_t COLOR_LEN = 16;
constexpr size_t NAME_LEN = NAME_MAX + 1;
constexpr size_t PERMISSION_LEN = 11;

typedef struct {
  struct stat st;
  char name[NAME_LEN];
  char permission[PERMISSION_LEN];
  char color[COLOR_LEN];
  char extension[NAME_LEN];
  char owner_name[NAME_LEN];
  char group_name[NAME_LEN];

  // Symlink target, shown after "-> " in the long format
  bool is_link;
  bool link_broken;
  char link_target[NAME_LEN];
  char link_permission[PERMISSION_LEN];
  char link_color[COLOR_LEN];
} FileInfo;

VECTOR_DEFINE(FileInfo, nullptr)

void get_username_from_uid(int uid, String username);
void get_groupname_from_gid(int gid, String username);
void construct_permission_str(mode_t mode, char permission[PERMISSION_LEN]);
void get_file_extension(ImmutableString file_name, String extension);
void get_color(ImmutableString permission, ImmutableString extension,
               String color);
