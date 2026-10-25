#pragma once

/// @file
/// @brief A small command-line option parser.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "core/commons.hpp"

/// @brief Declares one option a command accepts.
struct OptionSpec {
  /// @brief Single-letter name, used as `-x`; 0 if there is none.
  char short_name;
  /// @brief Long name, used as `--name`; empty if there is none.
  std::string_view long_name;
  /// @brief Whether the option needs a value, as in `-m msg`.
  bool takes_value;
};

/// @brief The result of parse_args().
///
/// Options are keyed by their long name if they have one, otherwise by their
/// short letter (e.g. "name-only", "m").
struct ParsedArgs {
  /// @brief Every option that was given.
  std::set<std::string> flags;

  /// @brief Values of options that take one, in order; repeats are kept.
  std::map<std::string, std::vector<std::string_view>> values;
  /// @brief Arguments that are not options, in order.
  std::vector<std::string_view> positional;

  /// @brief Whether an option was given.
  /// @param key Long name, or the short letter as a string.
  bool has(const std::string &key) const { return flags.contains(key); }

  /// @brief The last value given for an option.
  /// @param key Option key.
  /// @return The value, or nothing if the option was not given.
  std::optional<std::string_view> value(const std::string &key) const;
  /// @brief Every value given for an option, e.g. each `-p` of commit-tree.
  /// @param key Option key.
  /// @return The values in order; empty if the option was not given.
  std::span<const std::string_view> all_values(const std::string &key) const;
};

/// @brief Splits command arguments into options and positional arguments.
///
/// Supports `-x`, clustered flags (`-pw`), `-m value` and `-mvalue`,
/// `--name`, `--name=value`, and `--` to end option parsing.
/// @param args The command's arguments (without the program and command name).
/// @param spec The options this command accepts.
/// @return The parsed arguments, or an error for an unknown option or a missing
///         or unexpected value.
Result<ParsedArgs> parse_args(Args args, std::span<const OptionSpec> spec);
