#pragma once

#include "include/commons.h"

void init_shell_home(void);

ImmutableString get_shell_home(void);

ImmutableString get_current_username(void);

[[nodiscard]] String get_prompt(void);

[[nodiscard]] String expand_user_directory(const ImmutableString dir);

[[nodiscard]] String collapse_user_directory(const ImmutableString dir);

[[nodiscard]] String strip_whitespace(ImmutableString s);

bool is_number(ImmutableString s);
