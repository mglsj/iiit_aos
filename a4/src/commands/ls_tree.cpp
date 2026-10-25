/// @file
/// @brief The ls-tree command.

#include "cli/args.hpp"
#include "cli/commands.hpp"
#include "objects/tree.hpp"

#include <print>

namespace {

constexpr std::string_view USAGE =
    "usage: mygit ls-tree [--name-only] <object>";

constexpr OptionSpec SPEC[] = {{0, "name-only", false}};

}

/// @brief `mygit ls-tree [--name-only] <tree_sha>`: lists the entries of one tree.
///
/// Prints `"<mode> <type> <hash>	<name>"` per entry, or only names with `--name-only`.
/// @param args Optional --name-only and one tree hash.
/// @return Nothing, or an error if the hash is not a tree.
Result<void> ls_tree(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));
  if (parsed.positional.size() != 1)
    return std::unexpected(std::string(USAGE));

  const auto hash = TRY(Sha1::from_hex_string(parsed.positional[0]));
  const auto tree = TRY(git::Tree::load_from_objects(hash));

  for (const auto &entry : tree.entries()) {
    if (parsed.has("name-only")) {
      std::println("{}", entry.name);
      continue;
    }

    std::println("{}", git::to_listing(entry));
  }

  return {};
}
