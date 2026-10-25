#pragma once

#include "include/commons.h"

extern int last_exit_status;

bool is_executable(const ImmutableString path);

[[nodiscard]] String find_executable(const ImmutableString command);
[[nodiscard]] String resolve_command(const ImmutableString command);
