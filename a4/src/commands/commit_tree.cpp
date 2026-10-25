/// @file
/// @brief The commit-tree plumbing command.

#include "cli/args.hpp"
#include "cli/commands.hpp"

#include <print>

#include "objects/commit.hpp"
#include "objects/tree.hpp"
#include "repo/identity.hpp"

namespace {

constexpr std::string_view USAGE =
    "usage: mygit commit-tree <tree> [-p <parent>]... [-m <message>]";

constexpr OptionSpec SPEC[] = {{'m', "", true}, {'p', "", true}};

}

/// @brief `mygit commit-tree <tree> [-p <parent>]... [-m <msg>]`: writes a commit for a tree.
///
/// A plumbing command: it does not move HEAD or any branch.
/// @param args A tree hash, any number of -p parents, an optional message.
/// @return Nothing, or an error if the tree or a parent is not valid.
Result<void> commit_tree(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));
  if (parsed.positional.size() != 1)
    return std::unexpected(std::string(USAGE));

  const Sha1 tree_hash = TRY(Sha1::from_hex_string(parsed.positional[0]));
  const git::Tree tree = TRY(git::Tree::load_from_objects(tree_hash));

  std::vector<Sha1> parents;
  for (const std::string_view parent : parsed.all_values("p")) {
    const Sha1 hash = TRY(Sha1::from_hex_string(parent));
    TRY_VOID(git::Commit::load_from_objects(hash));
    parents.push_back(hash);
  }

  std::string message(parsed.value("m").value_or(""));
  if (!message.empty() && !message.ends_with('\n'))
    message += '\n';

  const git::Person person = git::current_person();
  const git::Commit commit(git::CommitData{
      .tree = tree.hash(),
      .parents = std::move(parents),
      .author = person,
      .committer = person,
      .message = std::move(message),
  });
  TRY_VOID(commit.write());

  std::println("{}", commit.hash());
  return {};
}
