#pragma once

#include <ctype.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t byte;

#define min(x, y) ((x) > (y) ? (y) : (x))

/**
 * Check if string is a integer.
 */
bool is_str_integer(const char *num) {
  if (num == nullptr)
    return false;

  if (*num == '-') {
    num++;
  }

  if (strlen(num) == 0)
    return false;

  for (const char *digit = num; *digit != '\0'; digit++) {
    if (isdigit(*digit))
      continue;
    else
      return false;
  }
  return true;
}
