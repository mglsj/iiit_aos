/// @file
/// @brief Implementation of find_git_dir().

#include "repo/repo.hpp"

#include <system_error>

Result<fs::path> find_git_dir() {
  std::error_code ec;
  fs::path dir = fs::current_path(ec);
  if (ec)
    return std::unexpected("cannot determine the current directory");

  while (true) {
    const fs::path candidate = dir / GIT_DIR;
    if (fs::is_directory(candidate, ec))
      return candidate;

    const fs::path parent = dir.parent_path();
    if (parent == dir)
      break;
    dir = parent;
  }

  return std::unexpected(
      "not a mygit repository (run 'init' first)");
}
