#include "sort.h"

static int cmp_alphabetical(const void *a, const void *b) {
  return strcoll(((const FileInfo *)a)->name, ((const FileInfo *)b)->name);
}

void sort(Vector_FileInfo *files) {
  qsort(files->data, files->size, sizeof(FileInfo), cmp_alphabetical);
}
