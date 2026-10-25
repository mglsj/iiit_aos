#pragma once

/// @file
/// @brief HEAD and branches.

#include <optional>
#include <string>

#include "core/commons.hpp"
#include "core/sha1.hpp"

namespace git {

/// @brief What HEAD points to: a branch (HEAD names a ref) or a detached commit.
struct Head {
  /// @brief Branch name, e.g. "main"; empty when HEAD is detached.
  std::optional<std::string> branch;
  /// @brief Current commit; empty on a branch with no commits yet.
  std::optional<Sha1> commit;
};

/// @brief Reads HEAD, following `"ref: refs/heads/<name>"` to the branch's commit.
/// @param git_dir The `.mygit` folder.
/// @return The branch and/or commit, or an error if HEAD is missing or malformed.
Result<Head> read_head(const fs::path &git_dir);

/// @brief Moves the current branch to @p commit, or HEAD itself when detached.
///
/// Called by commit after the new commit has been written.
/// @param git_dir The `.mygit` folder.
/// @param commit The new commit.
Result<void> advance_head(const fs::path &git_dir, const Sha1 &commit);

/// @brief Attaches HEAD to a branch: writes `"ref: refs/heads/<name>"`.
/// @param git_dir The `.mygit` folder.
/// @param name Branch name.
Result<void> set_head_branch(const fs::path &git_dir, const std::string &name);
/// @brief Detaches HEAD: writes the commit hash into HEAD directly.
/// @param git_dir The `.mygit` folder.
/// @param commit Commit to point at.
Result<void> set_head_detached(const fs::path &git_dir, const Sha1 &commit);

/// @brief Reads the commit a branch points to.
/// @param git_dir The `.mygit` folder.
/// @param name Branch name; names that could escape refs/heads are rejected.
/// @return The commit, nothing if the branch does not exist (or the name is
///         invalid), or an error if the branch file is malformed.
Result<std::optional<Sha1>> read_branch(const fs::path &git_dir,
                                        const std::string &name);

}
