/// @file
/// @brief Implementation of load_worktree_file().

#include "repo/worktree.hpp"

#include <format>

namespace git {

Result<std::optional<WorktreeFile>> load_worktree_file(const fs::path &path) {
  // lstat, so a symlink is stored as a link and never followed.
  struct stat st{};
  if (lstat(path.c_str(), &st) != 0)
    return std::unexpected(
        std::format("cannot stat '{}'", path.string()));

  if (S_ISLNK(st.st_mode)) {
    std::error_code ec;
    const fs::path target = fs::read_symlink(path, ec);
    if (ec)
      return std::unexpected(std::format("cannot read link '{}': {}",
                                         path.string(), ec.message()));
    return std::optional<WorktreeFile>(
        WorktreeFile{Mode::SYMLINK, Blob(target.string()), st});
  }

  if (S_ISREG(st.st_mode)) {
    const Mode mode = (st.st_mode & S_IXUSR) ? Mode::EXE : Mode::FILE;
    Blob blob = TRY(Blob::load_from_worktree(path));
    return std::optional<WorktreeFile>(WorktreeFile{mode, std::move(blob), st});
  }

  // Sockets, pipes and devices are not tracked.
  return std::optional<WorktreeFile>{};
}

bool is_real_directory(const fs::path &path) {
  std::error_code ec;
  return fs::is_directory(fs::symlink_status(path, ec));
}

}
