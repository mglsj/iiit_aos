/// @file
/// @brief The commit command.

#include "cli/args.hpp"
#include "cli/commands.hpp"

#include <print>

#include "objects/commit.hpp"
#include "objects/tree.hpp"
#include "repo/identity.hpp"
#include "repo/index.hpp"
#include "repo/refs.hpp"
#include "repo/repo.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit commit [-m <message>]";

constexpr std::string_view DEFAULT_MESSAGE = "Default commit message";

constexpr OptionSpec SPEC[] = {{'m', "", true}};

}

/// @brief `mygit commit [-m <msg>]`: records the staged files as a new commit.
///
/// Builds trees from the index, uses the current HEAD commit as the parent,
/// writes the objects and then moves the branch (or detached HEAD). Without -m
/// the message is "Default commit message".
/// @param args Optional -m and its message.
/// @return Nothing, or an error if nothing is staged or nothing changed.
Result<void> commit(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));
  if (!parsed.positional.empty())
    return std::unexpected(std::string(USAGE));

  const fs::path git_dir = TRY(find_git_dir());
  const git::Index index = TRY(git::Index::load_from_index(git_dir / "index"));
  if (index.entries().empty())
    return std::unexpected("nothing to commit (use 'mygit add' first)");

  std::vector<git::Object> pending;
  const git::Tree tree = TRY(git::Tree::load_from_index(index, pending));

  const git::Head head = TRY(git::read_head(git_dir));
  std::vector<Sha1> parents;
  if (head.commit) {
    const git::Commit parent = TRY(git::Commit::load_from_objects(*head.commit));
    if (parent.tree_hash() == tree.hash())
      return std::unexpected("nothing to commit, working tree clean");
    parents.push_back(*head.commit);
  }

  std::string message(parsed.value("m").value_or(DEFAULT_MESSAGE));
  if (!message.ends_with('\n'))
    message += '\n';

  const git::Person person = git::current_person();
  const git::Commit commit(git::CommitData{
      .tree = tree.hash(),
      .parents = std::move(parents),
      .author = person,
      .committer = person,
      .message = std::move(message),
  });

  // Write objects before the ref, so a failure never leaves HEAD pointing at nothing.
  for (const git::Object &object : pending)
    TRY_VOID(object.write());
  TRY_VOID(tree.write());
  TRY_VOID(commit.write());
  TRY_VOID(git::advance_head(git_dir, commit.hash()));

  std::println("{}", commit.hash());
  return {};
}
