#pragma once

#include "include/commons.h"
#include "options.h"

bool is_builtin(const ImmutableString command_name);
void execute_builtin(Command *const restrict command_p);
void run_builtin_here(Command *const restrict command_p);
bool apply_redirects_child(Command *const restrict command_p);
void apply_redirects_only(Command *const restrict command_p);

typedef struct {
  ImmutableString name;
  void (*operation)(const ImmutableString *argv);
} Builtin;

extern Builtin builtins[];
