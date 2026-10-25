#pragma once

#include "../include/commons.h"

typedef struct {
  bool long_format; // -l
  bool show_hidden; // -a
} Options;

Options parse_flags(size_t count, const ImmutableString flags[count]);
