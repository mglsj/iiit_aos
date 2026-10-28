#include "commons.h"
#include "file.h"

#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Prints progress of operation dynamically
 */
void print_progress(double progress) {
  printf("\r[");
  int bars = progress / 2;
  for (int j = 0; j < bars; j++) {
    printf("=");
  }
  for (int j = bars; j < 50; j++) {
    printf(" ");
  }
  printf("] %.2f%%", progress);
  fflush(stdout);
}

/**
 * Reverses a chunk of bytes in place.
 */
void reverse_chunk(const int64_t size, byte chunk[size]) {
  for (int i = 0; i < size / 2; i++) {
    byte temp = chunk[i];
    chunk[i] = chunk[size - 1 - i];
    chunk[size - 1 - i] = temp;
  }
}

/**
 * Reverses a segment of input file and writes it to the output file.
 */
void reverse_file_segment(const int input_fd, const int output_fd,
                          const int64_t start_index, const int64_t size,
                          const double start_progress,
                          const double end_progress) {
  if (size <= 0)
    return;

  int64_t chunk_size = min(size, MAX_CHUNK_SIZE);
  byte *chunk = safe_malloc(chunk_size * sizeof(byte));

  int64_t offset = size - chunk_size;
  int64_t bytes_left = size;

  while (bytes_left) {
    int64_t bytes_to_reverse = min(chunk_size, bytes_left);

    if (pread(input_fd, chunk, bytes_to_reverse, start_index + offset) <
        bytes_to_reverse) {
      perror("Failed to read from input file");
      exit(EXIT_FAILURE);
    }

    reverse_chunk(bytes_to_reverse, chunk);

    if (write(output_fd, chunk, bytes_to_reverse) < bytes_to_reverse) {
      perror("Failed to write to output file");
      exit(EXIT_FAILURE);
    }

    bytes_left -= bytes_to_reverse;
    offset -= min(bytes_left, chunk_size);

    double cur_progress =
        start_progress + ((end_progress - start_progress) *
                          ((double)(size - bytes_left) / (double)size));
    print_progress(cur_progress);
  }

  free(chunk);
}

/**
 * Copies a segment of input file to the output file.
 */
void copy_file_segment(const int input_fd, const int output_fd,
                       const int64_t start_index, const int64_t size,
                       const double start_progress, const double end_progress) {
  if (size <= 0)
    return;

  int64_t chunk_size = min(size, MAX_CHUNK_SIZE);
  byte *chunk = safe_malloc(chunk_size * sizeof(byte));

  for (int64_t offset = 0; offset < size; offset += chunk_size) {
    int64_t bytes_left = size - offset;
    int64_t bytes_to_copy = min(chunk_size, bytes_left);

    if (pread(input_fd, chunk, bytes_to_copy, start_index + offset) <
        bytes_to_copy) {
      perror("Failed to read from input file");
      exit(EXIT_FAILURE);
    }

    if (write(output_fd, chunk, bytes_to_copy) < bytes_to_copy) {
      perror("Failed to write to output file");
      exit(EXIT_FAILURE);
    }

    double cur_progress =
        start_progress + ((end_progress - start_progress) *
                          ((double)(offset + bytes_to_copy) / (double)size));
    print_progress(cur_progress);
  }

  free(chunk);
}

/**
 * Reverses the file in block-wise manner.
 */
void block_wise_reversal(const int input_fd, const int output_fd,
                         const int64_t file_size, int64_t block_size) {
  block_size = min(block_size, file_size);

  if (block_size == 1) {
    copy_file_segment(input_fd, output_fd, 0, file_size, 0.0, 100.0);
    return;
  }

  int total_blocks = ceil((double)file_size / block_size);

  if (total_blocks == 1) {
    reverse_file_segment(input_fd, output_fd, 0, file_size, 0.0, 100.0);
    return;
  }

  printf("Reversing file in %d blocks of size %ld Bytes.\n", total_blocks,
         block_size);

  int64_t bytes_left = file_size;

  if ((block_size >= MAX_CHUNK_SIZE) || (MAX_CHUNK_SIZE / block_size == 1)) {
    for (int block = 0; bytes_left > 0; block++) {
      int64_t bytes_to_reverse = min(bytes_left, block_size);

      reverse_file_segment(
          input_fd, output_fd, block * block_size, bytes_to_reverse,
          ((double)block / (double)total_blocks) * 100.0,
          ((double)(block + 1) / (double)total_blocks) * 100.0);

      bytes_left -= bytes_to_reverse;
    }
  } else // manual optimization for small blocks
  {
    int64_t blocks_per_chunk = ceil((double)MAX_CHUNK_SIZE / block_size);
    int64_t total_chunks = ceil((double)total_blocks / blocks_per_chunk);

    int64_t chunk_size = blocks_per_chunk * block_size;
    byte *chunk = safe_malloc(chunk_size * sizeof(byte));

    for (int i = 0; i < total_chunks; i++) {
      int64_t bytes_to_copy = min(chunk_size, bytes_left);

      if (read(input_fd, chunk, bytes_to_copy) < bytes_to_copy) {
        perror("Failed to read from input file");
        exit(EXIT_FAILURE);
      }

      for (int64_t offset = 0; offset < bytes_to_copy; offset += block_size) {
        reverse_chunk(min(block_size, bytes_to_copy - offset), chunk + offset);
      }

      if (write(output_fd, chunk, bytes_to_copy) < bytes_to_copy) {
        perror("Failed to write to output file");
        exit(EXIT_FAILURE);
      }

      bytes_left -= bytes_to_copy;

      double cur_progress =
          (((double)(file_size - bytes_left)) / file_size) * 100.0;
      print_progress(cur_progress);
    }

    free(chunk);
  }
}

/**
 * Reverses the entire file.
 */
void full_file_reversal(int input_fd, int output_fd, int64_t file_size) {
  reverse_file_segment(input_fd, output_fd, 0, file_size, 0.0, 100.0);
}

/**
 * Reverses a partial range of the file.
 */
void partial_range_reversal(int input_fd, int output_fd, int64_t file_size,
                            int64_t start_index, int64_t end_index) {
  reverse_file_segment(input_fd, output_fd, 0, start_index, 0.0,
                       ((double)start_index / (double)file_size) * 100.0);

  // copy as is
  copy_file_segment(input_fd, output_fd, start_index,
                    end_index - start_index + 1,
                    ((double)start_index / (double)file_size) * 100.0,
                    ((double)end_index / (double)file_size) * 100.0);

  reverse_file_segment(input_fd, output_fd, end_index + 1,
                       file_size - end_index - 1,
                       ((double)end_index / (double)file_size) * 100.0, 100.0);
}

void file_reversal(const int mode, const char *argv[]) {
  // Function start time
  struct timeval start_time;
  gettimeofday(&start_time, nullptr);

  const char *input_file_name = argv[1];

  int input_fd = open_file_for_read(input_file_name);
  int64_t input_file_size = get_file_size(input_fd);

  if (input_file_size == 0) {
    printf("Input file is empty.\n");
    close(input_fd);
    exit(EXIT_FAILURE);
  }

  const char output_dir[] = "Assignment1";

  make_dir(output_dir);

  // Get input file name if path is passed.
  const char *base_name = strrchr(input_file_name, '/');
  base_name = base_name ? base_name + 1 : input_file_name;

  // Build output file name.
  char output_file[256] = {0};
  snprintf(output_file, 256, "%s/%d_%s", output_dir, mode, base_name);

  int output_fd = create_file(output_file);

  printf("Reversing file '%s' containing %ld Bytes.\n", input_file_name,
         input_file_size);

  // Reversal Logic
  switch (mode) {
  case 0: // Block wise reversal
  {
    int64_t block_size = atoll(argv[3]);
    if (block_size < 1) {
      block_size = 1;
      printf("WARNING: Block size less than 1, setting it to 1.");
    }
    block_wise_reversal(input_fd, output_fd, input_file_size, block_size);
  } break;
  case 1: // Full file reversal
  {
    full_file_reversal(input_fd, output_fd, input_file_size);
  } break;
  case 2: // Partial range reversal
  {
    int64_t start_index = atoll(argv[3]);
    if (start_index < 0) {
      start_index = 0;
      printf("WARNING: Starting index less than 0, index set to 0");
    } else if (start_index >= input_file_size) {
      start_index = input_file_size - 1;
      printf("WARNING: Starting index greater than last index, index set to "
             "last index.");
    }
    int64_t end_index = atoll(argv[4]);
    if (end_index < start_index) {
      end_index = start_index;
      printf(
          "WARNING: End index less than start index, index set to start + 1.");
    } else if (end_index >= input_file_size) {
      end_index = input_file_size - 1;
      printf("WARNING: End index greater than last index, index set to file "
             "size.");
    }
    partial_range_reversal(input_fd, output_fd, input_file_size, start_index,
                           end_index);
  } break;
  }

  printf("\nReversed file written to ./%s\n", output_file);

  close(input_fd);
  close(output_fd);

  // Function end time
  struct timeval end;
  gettimeofday(&end, nullptr);

  double elapsed = (end.tv_sec - start_time.tv_sec) +
                   ((end.tv_usec - start_time.tv_usec) / 1000000.0);
  double speed = (input_file_size / 1024.0 / 1024.0) / elapsed;

  printf("Operation took %.4fs [%.2fMB/s]\n", elapsed, speed);
}

int main(const int argc, const char *argv[]) {
  switch (argc) {
  case 3:
    // Full file reversal
    if (strcmp(argv[2], "1") == 0) {
      file_reversal(1, argv);
      return EXIT_SUCCESS;
    }
    break;
  case 4:
    // Block wise reversal
    if (strcmp(argv[2], "0") == 0) {
      if (!is_str_integer(argv[3])) {
        printf("Usage %s <input file> 0 <block size>.\n", argv[0]);
        return EXIT_FAILURE;
      };
      file_reversal(0, argv);
      return EXIT_SUCCESS;
    }
    break;
  case 5:
    // Partial range reversal
    if (strcmp(argv[2], "2") == 0) {
      if (!is_str_integer(argv[3]) || !is_str_integer(argv[4])) {
        printf("Usage %s <input file> 2 <start index> <end index>\n", argv[0]);
        return EXIT_FAILURE;
      };
      file_reversal(2, argv);
      return EXIT_SUCCESS;
    }
    break;
  }

  printf("Usage %s <input file> <flag> [additional arguments].\n", argv[0]);
  return EXIT_FAILURE;
}
