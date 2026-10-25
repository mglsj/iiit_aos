#pragma once

/// @file
/// @brief The command table and the shared error printer.

#include <array>
#include <print>
#include <string_view>

#include "core/commons.hpp"

/// @brief Prints an error to stderr with an "error: " prefix.
///
/// Text that starts with "usage:" is printed as is.
/// @param message The error message, without a prefix.
inline void print_error(std::string_view message) {
  if (message.starts_with("usage:"))
    std::println(stderr, "{}", message);
  else
    std::println(stderr, "error: {}", message);
}

/// @brief A command's entry point: takes its arguments, returns success or an error.
using CommandFn = Result<void> (*)(Args);

/// @brief One row of the command table: the name typed by the user and its function.
struct Command {
  std::string_view name;
  CommandFn run;
};

Result<void> init(Args);
Result<void> cat_file(Args);
Result<void> hash_object(Args);
Result<void> ls_tree(Args);
Result<void> write_tree(Args);
Result<void> add(Args);
Result<void> commit_tree(Args);
Result<void> commit(Args);
Result<void> log_history(Args);
Result<void> checkout(Args);

/// @brief Every command mygit understands. main() looks the name up here.
inline constexpr std::array COMMANDS = {
    Command{"add", add},
    Command{"cat-file", cat_file},
    Command{"hash-object", hash_object},
    Command{"init", init},
    Command{"ls-tree", ls_tree},
    Command{"write-tree", write_tree},
    Command{"commit-tree", commit_tree},
    Command{"commit", commit},
    Command{"log", log_history},
    Command{"checkout", checkout},
};
