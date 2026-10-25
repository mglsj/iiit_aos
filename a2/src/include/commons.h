#pragma once

#include "colors.h"
#include "list.h"
#include "vector.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char *String;
typedef const char *ImmutableString;

static inline void auto_free_String(String *ptr) {
  if (ptr != nullptr && *ptr != nullptr) {
    free(*ptr);
  }
}

static inline void auto_fclose(FILE **file_pp) {
  if (file_pp != nullptr && *file_pp != nullptr) {
    fclose(*file_pp);
  }
}

#define AUTOFREE __attribute__((cleanup(auto_free_String)))
#define AUTOCLOSE __attribute__((cleanup(auto_fclose)))

#define STR_EQL(s, t) (strcmp(s, t) == 0)

#define max(a, b)                                                              \
  ({                                                                           \
    auto _max_a = (a);                                                         \
    auto _max_b = (b);                                                         \
    _max_a > _max_b ? _max_a : _max_b;                                         \
  })

#define min(a, b)                                                              \
  ({                                                                           \
    auto _min_a = (a);                                                         \
    auto _min_b = (b);                                                         \
    _min_a < _min_b ? _min_a : _min_b;                                         \
  })

VECTOR_DEFINE(int, nullptr)
VECTOR_DEFINE(char, nullptr)
VECTOR_DEFINE(String, free)

extern ImmutableString progname;
