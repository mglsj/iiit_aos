#pragma once

/// @file
/// @brief Locating the repository.

#include "core/commons.hpp"

/// @brief Finds the repository: `.mygit` in the current folder or the nearest parent.
/// @return The path of the `.mygit` folder, or "not a mygit repository".
Result<fs::path> find_git_dir();

/// @brief The working-tree root: the folder that contains `.mygit`.
/// @param git_dir Path returned by find_git_dir().
inline fs::path worktree_root(const fs::path &git_dir) {
  return git_dir.parent_path();
}
