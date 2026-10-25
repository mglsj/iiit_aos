#include "commands.h"

void echo(const ImmutableString *argv) {
  bool first = true;
  while (*(++argv) != nullptr) {
    printf(first ? "%s" : " %s", *argv);
    first = false;
  }
  printf("\n");
}
