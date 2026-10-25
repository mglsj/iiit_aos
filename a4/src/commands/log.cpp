/// @file
/// @brief The log command.

#include "cli/args.hpp"
#include "cli/commands.hpp"

#include <algorithm>
#include <print>
#include <set>
#include <sstream>

#include "core/clock.hpp"
#include "objects/commit.hpp"
#include "repo/refs.hpp"
#include "repo/repo.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit log";

/// @brief Prints one commit: hash, parents, author, committer, date and indented message.
void print_commit(const git::Commit &commit) {
  std::println("commit {}", commit.hash());
  for (const Sha1 &parent : commit.parents())
    std::println("Parent: {}", parent);
  std::println("Author: {} <{}>", commit.author().name, commit.author().email);
  std::println("Committer: {} <{}>", commit.committer().name,
               commit.committer().email);
  std::println("Date:   {}", format_timestamp(commit.committer().seconds,
                                              commit.committer().timezone_offset));
  std::println("");

  std::istringstream lines(commit.message());
  for (std::string line; std::getline(lines, line);)
    std::println("    {}", line);
  std::println("");
}

}

/// @brief `mygit log`: prints the history from HEAD, newest first.
///
/// Follows parent links; when several are pending (merges) it prints the one
/// with the newest committer time, and never prints a commit twice.
/// @param args Must be empty.
/// @return Nothing, or an error if there are no commits or one cannot be loaded.
Result<void> log_history(Args args) {
  const auto parsed = TRY(parse_args(args, {}));
  if (!parsed.positional.empty())
    return std::unexpected(std::string(USAGE));

  const fs::path git_dir = TRY(find_git_dir());
  const git::Head head = TRY(git::read_head(git_dir));
  if (!head.commit)
    return std::unexpected("your current branch has no commits yet");

  // With merge commits several lines of history are open, so always print the newest pending commit.
  std::vector<git::Commit> frontier;
  std::set<std::string> seen;
  const auto enqueue = [&](const Sha1 &hash) -> Result<void> {
    if (!seen.insert(hash.to_string()).second)
      return {};
    frontier.push_back(TRY(git::Commit::load_from_objects(hash)));
    return {};
  };

  TRY_VOID(enqueue(*head.commit));
  while (!frontier.empty()) {
    const auto newest = std::ranges::max_element(
        frontier, {}, [](const git::Commit &c) { return c.committer().seconds; });
    const git::Commit commit = std::move(*newest);
    frontier.erase(newest);

    print_commit(commit);
    for (const Sha1 &parent : commit.parents())
      TRY_VOID(enqueue(parent));
  }
  return {};
}
