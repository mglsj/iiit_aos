/// @file
/// @brief The checkout command.

#include "cli/args.hpp"
#include "cli/commands.hpp"

#include <format>
#include <map>
#include <print>

#include "core/file.hpp"
#include "objects/blob.hpp"
#include "objects/commit.hpp"
#include "objects/tree.hpp"
#include "repo/index.hpp"
#include "repo/refs.hpp"
#include "repo/repo.hpp"
#include "repo/worktree.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit checkout [-f] <commit_sha>";

constexpr OptionSpec SPEC[] = {{'f', "force", false}};

/// @brief Files of a commit, keyed by path.
using FileMap = std::map<std::string, git::FlatEntry>;

/// @brief The commit to check out, and the branch name if one was given.
struct Target {
  git::Commit commit;
  std::optional<std::string> branch;
};

/// @brief Resolves the argument of checkout: a full commit hash or a branch name.
/// @return The commit (and branch), or an error if it is neither.
Result<Target> resolve_target(const fs::path &git_dir, std::string_view arg) {
  if (const auto hash = Sha1::from_hex_string(arg))
    return Target{TRY(git::Commit::load_from_objects(*hash)), std::nullopt};

  const std::string name(arg);
  const auto tip = TRY(git::read_branch(git_dir, name));
  if (!tip)
    return std::unexpected(
        std::format("'{}' is not a commit hash or branch name", arg));
  return Target{TRY(git::Commit::load_from_objects(*tip)), name};
}

/// @brief Every file in a commit's snapshot, keyed by path.
Result<FileMap> files_of(const git::Commit &commit) {
  const git::Tree tree = TRY(git::Tree::load_from_objects(commit.tree_hash()));
  FileMap files;
  for (auto &entry : TRY(tree.flatten()))
    files.emplace(entry.path, std::move(entry));
  return files;
}

/// @brief Whether anything exists at a path (a broken symlink counts).
bool exists_on_disk(const fs::path &path) {
  std::error_code ec;
  return fs::exists(fs::symlink_status(path, ec));
}

/// @brief Whether @p path holds something the index does not track.
///
/// A folder counts only if a file inside it is untracked, so a tracked folder
/// can be replaced by a file of the same name.
bool holds_untracked(const fs::path &root, const fs::path &path,
                     const git::Index &index) {
  std::error_code ec;
  const auto status = fs::symlink_status(path, ec);
  if (!fs::exists(status))
    return false;
  if (!fs::is_directory(status))
    return true;

  for (fs::recursive_directory_iterator it(path, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (it->is_directory(ec) && !it->is_symlink(ec))
      continue;
    const std::string rel =
        it->path().lexically_relative(root).generic_string();
    if (!index.entries().contains(rel))
      return true;
  }
  return static_cast<bool>(ec);
}

/// @brief Refuses a switch that would destroy work.
///
/// Checks for staged changes, edited or deleted tracked files, and untracked
/// files where the target needs to write.
/// @param root The repository root.
/// @param index The current index.
/// @param current Files of the current commit.
/// @param target Files of the commit being checked out.
/// @return Nothing if it is safe, or an error naming the first problem.
Result<void> ensure_safe(const fs::path &root, const git::Index &index,
                         const FileMap &current, const FileMap &target) {
  const auto refuse = [](std::string_view why) {
    return std::unexpected(std::format(
        "{}\nhint: commit your changes first, or use -f to discard them",
        why));
  };

  if (index.entries().size() != current.size())
    return refuse("you have staged changes");

  for (const auto &[path, entry] : index.entries()) {
    const auto tracked = lookup(current, path);
    if (!tracked || tracked->hash != entry.hash || tracked->mode != entry.mode)
      return refuse("you have staged changes");

    const fs::path abs = root / path;
    if (!exists_on_disk(abs))
      return refuse(std::format("'{}' was deleted", path));
    const auto file = TRY(git::load_worktree_file(abs));
    if (!file || file->blob.hash() != entry.hash || file->mode != entry.mode)
      return refuse(std::format("'{}' has local modifications", path));
  }

  for (const auto &[path, entry] : target)
    if (!index.entries().contains(path) &&
        holds_untracked(root, root / path, index))
      return refuse(
          std::format("untracked file '{}' would be overwritten", path));

  return {};
}

/// @brief Deletes a path, then every parent folder left empty, up to @p root.
void remove_with_empty_parents(const fs::path &root, const fs::path &path) {
  std::error_code ec;
  fs::remove_all(path, ec);
  for (fs::path dir = path.parent_path(); dir != root && dir.has_parent_path();
       dir = dir.parent_path()) {
    if (!fs::is_empty(dir, ec) || ec || !fs::remove(dir, ec))
      break;
  }
}

/// @brief Clears what stands where a file will be written.
///
/// Removes a folder at the path, or a file where a parent folder is needed.
void clear_obstacles(const fs::path &root, const fs::path &path) {
  std::error_code ec;
  for (fs::path dir = path.parent_path(); dir != root;
       dir = dir.parent_path()) {
    const auto status = fs::symlink_status(dir, ec);
    if (fs::exists(status) && !fs::is_directory(status))
      fs::remove(dir, ec);
  }
  if (fs::is_directory(fs::symlink_status(path, ec)))
    fs::remove_all(path, ec);
}

/// @brief Writes one file or symlink of the target commit, with mode 0644 or 0755.
/// @param root The repository root.
/// @param entry Path and mode from the commit.
/// @param blob The file's content (for a symlink, its target).
Result<void> write_entry(const fs::path &root, const git::FlatEntry &entry,
                         const git::Blob &blob) {
  const fs::path path = root / entry.path;
  clear_obstacles(root, path);

  std::error_code ec;
  if (entry.mode == git::Mode::SYMLINK) {
    fs::remove(path, ec);
    fs::create_directories(path.parent_path(), ec);
    fs::create_symlink(blob.content(), path, ec);
    if (ec)
      return std::unexpected(std::format("cannot create link '{}': {}",
                                         path.string(), ec.message()));
    return {};
  }

  TRY_VOID(write_file(path, blob.content()));
  using fs::perms;
  const perms mode = entry.mode == git::Mode::EXE
                         ? perms::owner_all | perms::group_read |
                               perms::group_exec | perms::others_read |
                               perms::others_exec
                         : perms::owner_read | perms::owner_write |
                               perms::group_read | perms::others_read;
  fs::permissions(path, mode, ec);
  if (ec)
    return std::unexpected(
        std::format("cannot set permissions on '{}': {}", path.string(),
                    ec.message()));
  return {};
}

} // namespace

/// @brief `mygit checkout [-f] <commit_sha | branch>`: restores a commit's files.
///
/// Runs the safety checks (unless -f), loads every blob first, deletes files the
/// target does not have, writes the target's files, rebuilds the index and moves
/// HEAD (attached for a branch, detached for a hash). Untracked files are kept.
/// @param args Optional -f and one commit hash or branch name.
/// @return Nothing, or an error; on a logical error nothing on disk changes.
Result<void> checkout(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));
  if (parsed.positional.size() != 1)
    return std::unexpected(std::string(USAGE));

  const fs::path git_dir = TRY(find_git_dir());
  const fs::path root = worktree_root(git_dir);

  const Target target = TRY(resolve_target(git_dir, parsed.positional[0]));
  const git::Head head = TRY(git::read_head(git_dir));

  FileMap current;
  if (head.commit) {
    const git::Commit head_commit =
        TRY(git::Commit::load_from_objects(*head.commit));
    current = TRY(files_of(head_commit));
  }
  const FileMap wanted = TRY(files_of(target.commit));

  const git::Index old_index =
      TRY(git::Index::load_from_index(git_dir / "index"));
  if (!parsed.has("force"))
    TRY_VOID(ensure_safe(root, old_index, current, wanted));

  // Read every blob before changing the working tree, so a corrupt object
  // cannot leave it half restored.
  std::vector<git::Blob> blobs;
  blobs.reserve(wanted.size());
  for (const auto &[path, entry] : wanted)
    blobs.push_back(TRY(git::Blob::load_from_objects(entry.hash)));

  for (const auto &[path, entry] : current)
    if (!wanted.contains(path))
      remove_with_empty_parents(root, root / path);

  git::Index new_index;
  std::size_t i = 0;
  for (const auto &[path, entry] : wanted) {
    TRY_VOID(write_entry(root, entry, blobs[i++]));

    const auto file = TRY(git::load_worktree_file(root / path));
    if (file)
      new_index.upsert(git::make_index_entry(path, *file));
  }
  TRY_VOID(new_index.save(git_dir / "index"));

  if (target.branch) {
    TRY_VOID(git::set_head_branch(git_dir, *target.branch));
    std::println("Switched to branch '{}'", *target.branch);
  } else {
    TRY_VOID(git::set_head_detached(git_dir, target.commit.hash()));
    std::println("HEAD is now at {}", target.commit.hash());
  }
  return {};
}
