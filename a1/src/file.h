#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

constexpr auto MAX_CHUNK_SIZE = (10 * 1024 * 1024); // 10 MB

[[nodiscard]] void *safe_malloc(size_t n) {
  void *p = malloc(n);

  if (p == nullptr) {
    perror("[ERROR] Failed to allocate memory");
    exit(EXIT_FAILURE);
  }

  return p;
}

struct stat get_dir_stat(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) == -1) {
    perror("[ERROR] Failed to read dir stat");
    exit(EXIT_FAILURE);
  };
  return statbuf;
}

struct stat get_file_stat(int fd) {
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    perror("[ERROR] Failed to read file stat");
    exit(EXIT_FAILURE);
  };
  return statbuf;
}

size_t get_file_size(int fd) {
  auto statbuf = get_file_stat(fd);
  return statbuf.st_size;
}

int open_file_for_read(const char *file) {
  int fd = open(file, O_RDONLY);
  if (fd == -1) {
    perror("[ERROR] Failed to open file");
    exit(EXIT_FAILURE);
  }
  return fd;
}

int create_file(const char *file) {
  int fd = creat(file, 0600);
  if (fd == -1) {
    perror("[ERROR] Failed to create file");
    exit(EXIT_FAILURE);
  }
  return fd;
}

void make_dir(const char *dir) {
  if (mkdir(dir, 0700) == -1 && errno != EEXIST) {
    perror("[ERROR] Failed to create dir");
    exit(EXIT_FAILURE);
  }
}
