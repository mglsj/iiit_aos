#pragma once

/// @file
/// @brief Reading files and symlinks from the working directory.

#include <sys/stat.h>

#include <optional>

#include "core/commons.hpp"
#include "objects/blob.hpp"
#include "objects/mode.hpp"

namespace git {

/// @brief A file or symlink read from disk.
struct WorktreeFile {
  /// @brief Mode::FILE, Mode::EXE or Mode::SYMLINK.
  Mode mode;
  /// @brief The content; for a symlink, its target path.
  Blob blob;
  /// @brief The lstat() data, copied into the index.
  struct stat st;
};

/// @brief Reads one path from disk with lstat(), without following symlinks.
/// @param path Path to read.
/// @return The file; nothing for sockets, pipes and devices (they are not
///         tracked); or an error if the path cannot be read.
Result<std::optional<WorktreeFile>> load_worktree_file(const fs::path &path);

/// @brief Whether a path is a folder itself, not a symlink to one.
bool is_real_directory(const fs::path &path);

}
