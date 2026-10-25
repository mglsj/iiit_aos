/// @file
/// @brief The add command.

#include "cli/commands.hpp"

#include <format>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cli/args.hpp"
#include "repo/index.hpp"
#include "objects/blob.hpp"
#include "repo/repo.hpp"
#include "repo/worktree.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit add <file>...";

/// @brief One path given to add, resolved against the repository.
struct Target {
  std::string arg;
  fs::path abs;
  std::string rel;
  bool on_disk;
};

/// @brief Resolves and checks one argument of add.
/// @param arg The path as typed, relative to @p cwd.
/// @param cwd The current folder.
/// @param root The repository root.
/// @param index The current index (a deleted file is valid if it is staged).
/// @return The target, or an error if it is outside the repository, inside
///         `.mygit`, or neither on disk nor in the index.
Result<Target> resolve(std::string_view arg, const fs::path &cwd,
                       const fs::path &root, const git::Index &index) {
  const fs::path abs = (cwd / arg).lexically_normal();
  std::string rel = abs.lexically_relative(root).generic_string();
  if (rel == ".")
    rel.clear();

  if (rel == ".." || rel.starts_with("../"))
    return std::unexpected(std::format(
        "'{}' is outside repository at '{}'", arg, root.string()));

  if (rel == GIT_DIR || rel.starts_with(GIT_DIR.string() + "/"))
    return std::unexpected(std::format("'{}' is inside the {} directory",
                                       arg, GIT_DIR.string()));

  std::error_code ec;
  const bool on_disk = fs::exists(fs::symlink_status(abs, ec));

  if (!on_disk && index.paths_under(rel).empty())
    return std::unexpected(
        std::format("pathspec '{}' did not match any files", arg));

  return Target{
      .arg = std::string(arg),
      .abs = abs,
      .rel = rel,
      .on_disk = on_disk,
  };
}

/// @brief Stores one file's blob and records it in the index.
/// @param abs Absolute path of the file.
/// @param rel Path relative to the repository root.
/// @param index Index to update.
/// @return Nothing, or an error if the file cannot be read or its blob written.
Result<void> stage_file(const fs::path &abs, const std::string &rel,
                        git::Index &index) {
  const std::optional<git::WorktreeFile> file =
      TRY(git::load_worktree_file(abs));
  if (!file)
    return {};

  TRY_VOID(file->blob.write());

  index.upsert(git::make_index_entry(rel, *file));
  return {};
}

/// @brief Stages every file below a folder, recursively.
///
/// Skips `.mygit`; a symlink to a folder is staged as a link. Also removes
/// index entries under the folder whose files no longer exist.
/// @param target The folder.
/// @param root The repository root.
/// @param index Index to update.
Result<void> stage_directory(const Target &target, const fs::path &root,
                             git::Index &index) {
  std::set<std::string> seen;

  std::error_code ec;
  fs::recursive_directory_iterator it(target.abs, ec);
  const fs::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    const fs::directory_entry &entry = *it;

    if (entry.path().filename() == GIT_DIR) {
      it.disable_recursion_pending();
      continue;
    }

    std::error_code status_ec;
    // symlink_status, so a link to a directory is staged as a link and not walked.
    const fs::file_status status = entry.symlink_status(status_ec);
    if (status_ec)
      return std::unexpected(std::format("cannot read '{}': {}",
                                         entry.path().string(),
                                         status_ec.message()));
    if (fs::is_directory(status))
      continue;

    const std::string rel =
        entry.path().lexically_relative(root).generic_string();
    TRY_VOID(stage_file(entry.path(), rel, index));
    seen.insert(rel);
  }
  if (ec)
    return std::unexpected(std::format("cannot read '{}': {}",
                                       target.abs.string(), ec.message()));

  for (const auto &path : index.paths_under(target.rel))
    if (!seen.contains(path))
      index.remove(path);

  return {};
}

}

/// @brief `mygit add <path>...`: stages files and folders for the next commit.
///
/// All paths are checked before anything is staged. Each file's blob is stored
/// immediately; a path that no longer exists is removed from the index. The
/// index is saved once at the end.
/// @param args One or more paths; "." means the current folder.
/// @return Nothing, or an error for a bad path or a read/write failure.
Result<void> add(Args args) {
  const auto parsed = TRY(parse_args(args, {}));
  if (parsed.positional.empty())
    return std::unexpected(std::string(USAGE));

  const fs::path git_dir = TRY(find_git_dir());
  const fs::path root = git_dir.parent_path().lexically_normal();

  std::error_code ec;
  const fs::path cwd = fs::current_path(ec);
  if (ec)
    return std::unexpected("cannot determine the current directory");

  git::Index index = TRY(git::Index::load_from_index(git_dir / "index"));

  std::vector<Target> targets;
  for (const std::string_view arg : parsed.positional)
    targets.push_back(TRY(resolve(arg, cwd, root, index)));

  for (const Target &target : targets) {
    if (!target.on_disk) {

      for (const auto &path : index.paths_under(target.rel))
        index.remove(path);
      continue;
    }

    std::error_code status_ec;
    const fs::file_status status = fs::symlink_status(target.abs, status_ec);
    if (fs::is_directory(status))
      TRY_VOID(stage_directory(target, root, index));
    else
      TRY_VOID(stage_file(target.abs, target.rel, index));
  }

  return index.save(git_dir / "index");
}
