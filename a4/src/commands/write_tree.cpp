/// @file
/// @brief The write-tree command.

#include "cli/args.hpp"
#include "cli/commands.hpp"
#include "objects/tree.hpp"

#include <filesystem>
#include <print>
#include <vector>

namespace {

constexpr std::string_view USAGE = "usage: mygit write-tree";

}

/// @brief `mygit write-tree`: stores the current folder as trees and prints the root hash.
///
/// Reads the working directory (not the index), skipping `.mygit` and empty folders.
/// @param args Must be empty.
/// @return Nothing, or an error if a file cannot be read or an object written.
Result<void> write_tree(Args args) {
  const auto parsed = TRY(parse_args(args, {}));
  if (parsed.positional.size() > 0)
    return std::unexpected(std::string(USAGE));

  std::vector<git::Object> pending;
  const auto tree =
      TRY(git::Tree::load_from_worktree(fs::current_path(), pending));
  for (const auto &object : pending)
    TRY_VOID(object.write());
  TRY_VOID(tree.write());

  std::println("{}", tree.hash().to_string());

  return {};
}
