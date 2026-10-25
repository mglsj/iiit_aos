#define _GNU_SOURCE

#include "history.h"

#include <readline/history.h>

#define HISTORY_FILE ".shell_history"

static String history_path;

static void resolve_history_path(void) {
  const ImmutableString home = getenv("HOME");
  if (home == nullptr) {
    return;
  }

  if (asprintf(&history_path, "%s/%s", home, HISTORY_FILE) < 0) {
    history_path = nullptr;
  }
}

void init_history(void) {
  using_history();

  stifle_history(HISTORY_LIMIT);

  resolve_history_path();

  if (history_path != nullptr) {
    read_history(history_path);
  }
}

static void save_history(void) {
  if (history_path == nullptr) {
    return;
  }

  write_history(history_path);

  history_truncate_file(history_path, HISTORY_LIMIT);
}

void record_command(ImmutableString line) {
  add_history(line);
  save_history();
}

size_t history_count(void) {
  return (history_length > 0) ? (size_t)history_length : 0;
}

ImmutableString history_entry(size_t index) {
  HIST_ENTRY **entries = history_list();

  if (entries == nullptr || index >= history_count()) {
    return nullptr;
  }

  return entries[index]->line;
}
