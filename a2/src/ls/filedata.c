#include "filedata.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

void get_username_from_uid(int uid, String username) {
  struct passwd *pws = getpwuid(uid);

  if (pws == nullptr || pws->pw_name == nullptr) {
    snprintf(username, NAME_LEN, "%d", uid);
    return;
  }

  snprintf(username, NAME_LEN, "%s", pws->pw_name);
}

void get_groupname_from_gid(int gid, String groupname) {
  struct group *grp = getgrgid(gid);

  if (grp == nullptr || grp->gr_name == nullptr) {
    snprintf(groupname, NAME_LEN, "%d", gid);
    return;
  }

  snprintf(groupname, NAME_LEN, "%s", grp->gr_name);
}

void construct_permission_str(mode_t mode, char permission[PERMISSION_LEN]) {
  // Get the first character from file type
  permission[0] = '?';

  if (S_ISREG(mode))
    permission[0] = '-';
  else if (S_ISDIR(mode))
    permission[0] = 'd';
  else if (S_ISLNK(mode))
    permission[0] = 'l';
  else if (S_ISCHR(mode))
    permission[0] = 'c';
  else if (S_ISBLK(mode))
    permission[0] = 'b';
  else if (S_ISSOCK(mode))
    permission[0] = 's';
  else if (S_ISFIFO(mode))
    permission[0] = 'p';

  permission[1] = (mode & S_IRUSR) ? 'r' : '-';
  permission[2] = (mode & S_IWUSR) ? 'w' : '-';
  permission[3] = (mode & S_IXUSR) ? 'x' : '-';

  permission[4] = (mode & S_IRGRP) ? 'r' : '-';
  permission[5] = (mode & S_IWGRP) ? 'w' : '-';
  permission[6] = (mode & S_IXGRP) ? 'x' : '-';

  permission[7] = (mode & S_IROTH) ? 'r' : '-';
  permission[8] = (mode & S_IWOTH) ? 'w' : '-';
  permission[9] = (mode & S_IXOTH) ? 'x' : '-';
  permission[10] = '\0';
}

void get_file_extension(ImmutableString file_name, String extension) {
  ImmutableString p = strrchr(file_name, '.');

  if (p)
    snprintf(extension, NAME_LEN, "%s", p);
  else
    extension[0] = '\0';
}

static bool is_archive_file(ImmutableString extension) {
  return (strstr(extension, ".tar") || strstr(extension, ".taz") ||
          strstr(extension, ".xz") || strstr(extension, ".gz"));
}

void get_color(ImmutableString permission, ImmutableString extension,
               String color) {
  switch (permission[0]) {
  case '-':
    if (permission[3] == 'x' || permission[6] == 'x' || permission[9] == 'x')
      strcpy(color, COLOR_EXECUTABLES);
    else if (is_archive_file(extension))
      strcpy(color, COLOR_ARCHIVE);
    else
      strcpy(color, COLOR_REGULAR);
    break;
  case 'd':
    strcpy(color, COLOR_DIRECTORIES);
    break;
  case 'l':
    strcpy(color, COLOR_LINK);
    break;
  case 'c':
  case 'b':
    strcpy(color, COLOR_BLK_CHR);
    break;
  case 's':
    strcpy(color, COLOR_SOCKET);
    break;
  case 'p':
    strcpy(color, COLOR_FIFO);
    break;
  default:
    strcpy(color, COLOR_REGULAR);
    break;
  }
}
