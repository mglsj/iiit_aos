#include "parser.h"

Options parse_flags(size_t count, const ImmutableString flags[count]) {
  Options options = {0};

  for (size_t i = 0; i < count; ++i) {
    if (flags[i][0] != '-') {
      continue; // not a flag
    }

    for (int j = 1; flags[i][j] != '\0'; j++) {
      switch (flags[i][j]) {
      case 'a':
        options.show_hidden = true;
        break;
      case 'l':
        options.long_format = true;
        break;
      default:
        break; // unsupported flag
      }
    }
  }

  return options;
}
