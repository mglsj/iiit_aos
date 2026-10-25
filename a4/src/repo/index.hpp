#pragma once

/// @file
/// @brief The index (staging area) in Git's version 2 format.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/commons.hpp"
#include "core/sha1.hpp"
#include "objects/mode.hpp"
#include "repo/worktree.hpp"

namespace git {

/// @brief One staged path, laid out like a version 2 Git index entry.
///
/// The stat fields are recorded for Git compatibility; mygit compares hashes,
/// not timestamps.
struct IndexEntry {
  std::uint32_t ctime_sec = 0;
  std::uint32_t ctime_nsec = 0;
  std::uint32_t mtime_sec = 0;
  std::uint32_t mtime_nsec = 0;
  std::uint32_t dev = 0;
  std::uint32_t ino = 0;
  Mode mode = Mode::FILE;
  std::uint32_t uid = 0;
  std::uint32_t gid = 0;
  std::uint32_t size = 0;
  Sha1 hash{Sha1::Digest{}};
  std::string path;
};

/// @brief Builds an index entry from a file read from disk.
/// @param path Path relative to the repository root, with "/" separators.
/// @param file The file's mode, blob and lstat data.
IndexEntry make_index_entry(const std::string &path, const WorktreeFile &file);

/// @brief The staging area: staged paths, sorted, as stored in `.mygit/index`.
class Index {
public:
  /// @brief Reads and validates an index file.
  /// @param file Path of `.mygit/index`. A missing file gives an empty index.
  /// @return The index, or an error if the signature, version, checksum or any
  ///         entry is invalid.
  static Result<Index> load_from_index(const fs::path &file);

  /// @brief Writes the index in Git's version 2 format, with its SHA-1 trailer, atomically.
  /// @param file Path of `.mygit/index`.
  Result<void> save(const fs::path &file) const;

  /// @brief Adds an entry, or replaces the entry with the same path.
  void upsert(IndexEntry entry);

  /// @brief Removes the entry for a path, if there is one.
  void remove(const std::string &path);

  /// @brief Staged paths equal to @p dir or inside the folder @p dir.
  /// @param dir A path relative to the root; empty means every path.
  /// @return Matching paths. "src" matches "src/a.c" but not "src2/a.c".
  std::vector<std::string> paths_under(std::string_view dir) const;

  /// @brief Every entry, keyed and sorted by path.
  const std::map<std::string, IndexEntry> &entries() const { return _entries; }

private:
  std::map<std::string, IndexEntry> _entries;
};

}
