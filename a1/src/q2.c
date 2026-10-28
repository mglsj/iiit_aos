#include "commons.h"
#include "file.h"

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void print_permission(const char *file_name, const char *entity,
                      const char *permission, bool has_permission) {
  printf("%s has %s permissions on %s: %s\n", entity, permission, file_name,
         has_permission ? "\033[0;32mYes\033[0m" : "\033[0;31mNo\033[0m");
}

void print_permissions(const char *file_name, const mode_t permissions) {
  // printf("%04o\n", permissions & 07777);

  print_permission(file_name, "\033[1;34mUser\033[0m", "\033[1;34mread\033[0m",
                   permissions & S_IRUSR);
  print_permission(file_name, "\033[1;34mUser\033[0m", "\033[1;95mwrite\033[0m",
                   permissions & S_IWUSR);
  print_permission(file_name, "\033[1;34mUser\033[0m",
                   "\033[1;96mexecute\033[0m", permissions & S_IXUSR);

  print_permission(file_name, "\033[1;95mGroup\033[0m", "\033[1;34mread\033[0m",
                   permissions & S_IRGRP);
  print_permission(file_name, "\033[1;95mGroup\033[0m",
                   "\033[1;95mwrite\033[0m", permissions & S_IWGRP);
  print_permission(file_name, "\033[1;95mGroup\033[0m",
                   "\033[1;96mexecute\033[0m", permissions & S_IXGRP);

  print_permission(file_name, "\033[1;96mOther\033[0m", "\033[1;34mread\033[0m",
                   permissions & S_IROTH);
  print_permission(file_name, "\033[1;96mOther\033[0m",
                   "\033[1;95mwrite\033[0m", permissions & S_IWOTH);
  print_permission(file_name, "\033[1;96mOther\033[0m",
                   "\033[1;96mexecute\033[0m", permissions & S_IXOTH);
}

bool check_chunk_revesed(const int64_t size, const byte chunk1[size],
                         const byte chunk2[size]) {
  for (int i = 0; i < size; i++) {
    if (chunk1[i] != chunk2[size - 1 - i]) {
      return false;
    }
  }
  return true;
}

bool check_file_segment_reversed(const int old_fd, const int new_fd,
                                 const int64_t start_index,
                                 const int64_t size) {
  bool ans = true;

  int64_t chunk_size = min(size, MAX_CHUNK_SIZE);

  byte *chunk1 = safe_malloc(chunk_size * sizeof(byte));
  byte *chunk2 = safe_malloc(chunk_size * sizeof(byte));

  for (int64_t offset = 0; offset < size; offset += chunk_size) {
    int64_t bytes_left = size - offset;
    int64_t bytes_to_reverse = min(chunk_size, bytes_left);

    if (pread(old_fd, chunk2, bytes_to_reverse, start_index + offset) <
        bytes_to_reverse) {
      perror("Failed to read from old file");
      exit(EXIT_FAILURE);
    }

    if (pread(new_fd, chunk1, bytes_to_reverse,
              start_index + size - bytes_to_reverse - offset) <
        bytes_to_reverse) {
      perror("Failed to read from new file");
      exit(EXIT_FAILURE);
    }

    ans = check_chunk_revesed(bytes_to_reverse, chunk1, chunk2);

    if (!ans)
      break;
  }

  free(chunk1);
  free(chunk2);

  return ans;
}

bool check_chunk_equal(const int64_t size, const byte chunk1[size],
                       const byte chunk2[size]) {
  return memcmp(chunk1, chunk2, size) == 0;
}

bool check_file_segment_copied(const int old_fd, const int new_fd,
                               const int64_t start_index, const int64_t size) {
  bool ans = true;

  int64_t chunk_size = min(size, MAX_CHUNK_SIZE);

  byte *chunk1 = safe_malloc(chunk_size * sizeof(byte));
  byte *chunk2 = safe_malloc(chunk_size * sizeof(byte));

  for (int64_t offset = 0; offset < size; offset += chunk_size) {
    int64_t bytes_left = size - offset;
    int64_t bytes_to_reverse = min(chunk_size, bytes_left);

    if (pread(old_fd, chunk2, bytes_to_reverse, start_index + offset) <
        bytes_to_reverse) {
      perror("Failed to read from old file");
      exit(EXIT_FAILURE);
    }

    if (pread(new_fd, chunk1, bytes_to_reverse, start_index + offset) <
        bytes_to_reverse) {
      perror("Failed to read from new file");
      exit(EXIT_FAILURE);
    }

    ans = check_chunk_equal(bytes_to_reverse, chunk1, chunk2);

    if (!ans)
      break;
  }

  free(chunk1);
  free(chunk2);

  return ans;
}

bool check_block_wise_reversed(int old_fd, int new_fd, int64_t file_size,
                               int64_t block_size) {
  block_size = min(block_size, file_size);

  if (block_size == 1) {
    return check_file_segment_copied(old_fd, new_fd, 0, file_size);
  }

  int64_t total_blocks = ceil((double)file_size / block_size);

  if (total_blocks == 1) {
    return check_file_segment_reversed(old_fd, new_fd, 0, file_size);
  }

  int64_t bytes_left = file_size;
  if ((block_size >= MAX_CHUNK_SIZE) || (MAX_CHUNK_SIZE / block_size == 1)) {
    for (int64_t block = 0; bytes_left > 0; block++) {
      int64_t bytes_to_reverse = min(bytes_left, block_size);

      if (!check_file_segment_reversed(old_fd, new_fd, block * block_size,
                                       bytes_to_reverse))
        return false;

      bytes_left -= bytes_to_reverse;
    }
  } else {
    int64_t blocks_per_chunk = ceil((double)MAX_CHUNK_SIZE / block_size);
    int64_t total_chunks = ceil((double)total_blocks / blocks_per_chunk);

    int64_t chunk_size = blocks_per_chunk * block_size;
    byte *chunk1 = safe_malloc(chunk_size * sizeof(byte));
    byte *chunk2 = safe_malloc(chunk_size * sizeof(byte));

    for (int i = 0; i < total_chunks; i++) {
      int64_t bytes_to_copy = min(chunk_size, bytes_left);

      if (read(old_fd, chunk1, bytes_to_copy) < bytes_to_copy) {
        perror("Failed to read from input file");
        exit(EXIT_FAILURE);
      }

      if (read(new_fd, chunk2, bytes_to_copy) < bytes_to_copy) {
        perror("Failed to write to output file");
        exit(EXIT_FAILURE);
      }

      for (int64_t offset = 0; offset < bytes_to_copy; offset += block_size) {
        if (!check_chunk_revesed(min(block_size, bytes_to_copy - offset),
                                 chunk1 + offset, chunk2 + offset)) {
          free(chunk1);
          free(chunk2);
          return false;
        }
      }

      bytes_left -= bytes_to_copy;
    }

    free(chunk1);
    free(chunk2);
  }

  return true;
}

bool check_full_file_reversed(int old_fd, int new_fd, int64_t file_size) {
  return check_file_segment_reversed(old_fd, new_fd, 0, file_size);
}

bool check_partial_range_reversed(int old_fd, int new_fd, int64_t file_size,
                                  int64_t start_index, int64_t end_index) {
  return check_file_segment_reversed(old_fd, new_fd, 0, start_index) &&
         check_file_segment_copied(old_fd, new_fd, start_index,
                                   end_index - start_index + 1) &&
         check_file_segment_reversed(old_fd, new_fd, end_index + 1,
                                     file_size - end_index - 1);
}

bool is_valid(const int old_fd, const int new_fd, const int64_t size,
              const int mode, const char *argv[]) {
  switch (mode) {
  case 0: {
    int64_t block_size = atoll(argv[5]);
    if (block_size < 1) {
      block_size = 1;
      printf("WARNING: Block size less than 1, setting it to 1.");
    }
    return check_block_wise_reversed(old_fd, new_fd, size, block_size);
  }
  case 1: {
    return check_full_file_reversed(old_fd, new_fd, size);
  }
  case 2: {
    int64_t start_index = atoll(argv[5]);
    if (start_index < 0) {
      start_index = 0;
      printf("WARNING: Starting index less than 0, index set to 0");
    } else if (start_index >= size) {
      start_index = size - 1;
      printf("WARNING: Starting index greater than last index, index set to "
             "last index.");
    }
    int64_t end_index = atoll(argv[6]);
    if (end_index < start_index) {
      end_index = start_index;
      printf(
          "WARNING: End index less than start index, index set to start + 1.");
    } else if (end_index >= size) {
      end_index = size - 1;
      printf("WARNING: End index greater than last index, index set to file "
             "size.");
    }
    return check_partial_range_reversed(old_fd, new_fd, size, start_index,
                                        end_index);
  }
  }
  return false;
}

void verify_file(const int mode, const char *argv[]) {
  const char *new_file_name = argv[1];
  const char *old_file_name = argv[2];
  const char *directory = argv[3];

  int new_fd = open_file_for_read(new_file_name);
  struct stat new_st = get_file_stat(new_fd);

  int old_fd = open_file_for_read(old_file_name);
  struct stat old_st = get_file_stat(old_fd);

  struct stat directory_st = get_dir_stat(directory);

  printf("Directory is created: \033[0;32mYes\033[0m\n");

  if (new_st.st_size == old_st.st_size) {
    printf("Both Files Sizes are Same: \033[0;32mYes\033[0m\n");
    printf("\033[0;34mChecking file contents...\033[0m");
    fflush(stdout);

    printf("\rWhether file contents are correctly processed: %s\n",
           is_valid(old_fd, new_fd, old_st.st_size, mode, argv)
               ? "\033[0;32mYes\033[0m"
               : "\033[0;31mNo\033[0m");

    print_permissions("newfile", new_st.st_mode);
    print_permissions("oldfile", old_st.st_mode);
    print_permissions("directory", directory_st.st_mode);
  } else {
    printf("Both Files Sizes are Same: \033[0;31mNo\033[0m\n");
  }
}

/**
 * argv[]:
 *  0 -> process name
 *  1 -> new file path
 *  2 -> old file path
 *  3 -> directory
 *  4 -> flag
 *  5 -> block size / start index
 *  6 -> end index
 */
int main(const int argc, const char *argv[]) {
  switch (argc) {
  case 5: // flag 1
    if (strcmp(argv[4], "1") == 0) {
      verify_file(1, argv);
      return EXIT_SUCCESS;
    }
    break;
  case 6: // flag 0
    if (strcmp(argv[4], "0") == 0) {
      if (!is_str_integer(argv[5])) {
        printf("Usage: %s <newfilepath> <oldfilepath> <directory> 0 <block "
               "size>\n",
               argv[0]);
        return EXIT_FAILURE;
      };

      verify_file(0, argv);
      return EXIT_SUCCESS;
    }
    break;
  case 7: // flag 2
    if (strcmp(argv[4], "2") == 0) {
      if (!is_str_integer(argv[5]) || !is_str_integer(argv[6])) {
        printf("Usage: %s <newfilepath> <oldfilepath> <directory> 2 <start "
               "index> <end index>\n",
               argv[0]);
        return EXIT_FAILURE;
      };
      verify_file(2, argv);
      return EXIT_SUCCESS;
    }
    break;
  }

  printf("Usage: %s <newfilepath> <oldfilepath> <directory> <flag> "
         "[<blockSize>|<start> <end>]",
         argv[0]);
  return EXIT_FAILURE;
}
