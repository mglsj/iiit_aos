#include "commands.h"

#include "../executable.h"
#include "../helpers.h"
#include "../history.h"

void history(const ImmutableString *argv) {
  size_t requested = HISTORY_DEFAULT_SHOWN;

  if (argv[1] != nullptr) {
    if (argv[2] != nullptr) {
      fprintf(stderr, "%s: history: too many arguments\n", progname);
      last_exit_status = 1;
      return;
    }

    if (!is_number(argv[1])) {
      fprintf(stderr, "%s: history: %s: numeric argument required\n", progname,
              argv[1]);
      last_exit_status = 1;
      return;
    }

    requested = strtoul(argv[1], nullptr, 10);
  }

  const size_t count = history_count();
  const size_t shown = min(requested, count);

  for (size_t i = count - shown; i < count; i++) {
    ImmutableString line = history_entry(i);

    if (line != nullptr) {
      printf("%s\n", line);
    }
  }

  last_exit_status = 0;
}
