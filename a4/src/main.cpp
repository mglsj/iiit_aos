/// @file
/// @brief Entry point: dispatches to the command named on the command line.

#include <algorithm>
#include <cstdlib>
#include <format>
#include <optional>
#include <string_view>
#include <vector>

#include "cli/commands.hpp"

namespace {

/// @brief Finds a command in the COMMANDS table by the name the user typed.
/// @return The command, or nothing if there is no command with that name.
std::optional<const Command &> find_command(std::string_view name) {
  const auto it = std::ranges::find(COMMANDS, name, &Command::name);
  if (it == COMMANDS.end())
    return std::nullopt;
  return *it;
}

/// @brief Runs the command named by the first argument.
/// @param args The command name followed by its arguments.
/// @return EXIT_SUCCESS, or EXIT_FAILURE after printing the error.
int execute(Args args) {
  const std::string_view name = args[0];

  const auto command = find_command(name);
  if (!command) {
    print_error(std::format("'{}' is not a mygit command", name));
    return EXIT_FAILURE;
  }

  if (const auto result = command->run(args.subspan(1)); !result) {
    if (!result.error().empty())
      print_error(result.error());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}

/// @brief Entry point: `mygit <command> [<args>]`.
/// @return 0 on success, 1 on any error or a usage mistake.
int main(int argc, char *argv[]) {
  const std::vector<std::string_view> args(argv, argv + argc);
  if (args.size() < 2) {
    print_error("usage: mygit <command> [<args>]");
    return EXIT_FAILURE;
  }

  return execute(Args(args).subspan(1));
}
