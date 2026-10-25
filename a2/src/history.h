#pragma once

#include "include/commons.h"

constexpr int HISTORY_LIMIT = 20;
constexpr int HISTORY_DEFAULT_SHOWN = 10;

void init_history(void);
void record_command(ImmutableString line);
size_t history_count(void);
ImmutableString history_entry(size_t index);
