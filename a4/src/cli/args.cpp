/// @file
/// @brief Implementation of the option parser.

#include "args.hpp"

#include <algorithm>
#include <format>

namespace {

/// @brief The key an option is stored under in ParsedArgs: its long name, else
/// its letter.
std::string key_of(const OptionSpec &option) {
  return option.long_name.empty() ? std::string(1, option.short_name)
                                  : std::string(option.long_name);
}

/// @brief The option as the user would type it, e.g. "-m" or "--name-only".
std::string display_name(const OptionSpec &option) {
  return option.long_name.empty() ? std::format("-{}", option.short_name)
                                  : std::format("--{}", option.long_name);
}

/// @brief Finds an option by its single-letter name.
/// @return The option, or nothing if the command does not accept it.
std::optional<const OptionSpec &> find_short(std::span<const OptionSpec> spec,
                                             char name) {
  const auto it = std::ranges::find(spec, name, &OptionSpec::short_name);
  if (it == spec.end())
    return std::nullopt;
  return *it;
}

/// @brief Finds an option by its long name.
/// @return The option, or nothing if the command does not accept it.
std::optional<const OptionSpec &> find_long(std::span<const OptionSpec> spec,
                                            std::string_view name) {
  const auto it = std::ranges::find(spec, name, &OptionSpec::long_name);
  if (it == spec.end())
    return std::nullopt;
  return *it;
}

} // namespace

std::optional<std::string_view>
ParsedArgs::value(const std::string &key) const {
  if (const auto list = lookup(values, key))
    return list->back();
  return std::nullopt;
}

std::span<const std::string_view>
ParsedArgs::all_values(const std::string &key) const {
  if (const auto list = lookup(values, key))
    return *list;
  return {};
}

Result<ParsedArgs> parse_args(Args args, std::span<const OptionSpec> spec) {
  ParsedArgs parsed;
  bool options_done = false;

  const auto record = [&](const OptionSpec &option,
                          std::optional<std::string_view> inline_value,
                          std::size_t &i) -> Result<void> {
    const std::string key = key_of(option);

    if (!option.takes_value) {
      if (inline_value)
        return std::unexpected(
            std::format("option '{}' takes no value", display_name(option)));
      parsed.flags.insert(key);
      return {};
    }

    if (inline_value) {
      parsed.values[key].push_back(*inline_value);
    } else if (i + 1 < args.size()) {
      parsed.values[key].push_back(args[++i]);
    } else {
      return std::unexpected(
          std::format("option '{}' requires a value", display_name(option)));
    }
    parsed.flags.insert(key);
    return {};
  };

  for (std::size_t i = 0; i < args.size(); i++) {
    const std::string_view arg = args[i];

    if (options_done || arg.size() < 2 || !arg.starts_with('-')) {
      parsed.positional.push_back(arg);
    } else if (arg == "--") {
      options_done = true;
    } else if (arg.starts_with("--")) {

      const std::string_view body = arg.substr(2);
      const auto eq = body.find('=');
      const std::string_view name = body.substr(0, eq);
      std::optional<std::string_view> inline_value;
      if (eq != std::string_view::npos)
        inline_value = body.substr(eq + 1);

      const auto option = find_long(spec, name);
      if (!option || name.empty())
        return std::unexpected(std::format("unknown option '--{}'", name));
      TRY_VOID(record(*option, inline_value, i));
    } else {
      for (std::size_t j = 1; j < arg.size(); j++) {
        const auto option = find_short(spec, arg[j]);
        if (!option || arg[j] == '\0')
          return std::unexpected(std::format("unknown option '-{}'", arg[j]));

        std::optional<std::string_view> inline_value;
        if (option->takes_value && j + 1 < arg.size())
          inline_value = arg.substr(j + 1);

        TRY_VOID(record(*option, inline_value, i));
        if (option->takes_value)
          break;
      }
    }
  }

  return parsed;
}
