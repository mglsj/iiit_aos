#pragma once

/// @file
/// @brief Blob objects: the content of one file.

#include "objects/object.hpp"

namespace git {

/// @brief The content of one file. Names and permissions live in trees, not here.
class Blob : public Object {
public:
  /// @brief Creates a blob from file content.
  /// @param content The file's bytes.
  explicit Blob(std::string content)
      : Object(ObjectType::blob, std::move(content)) {}

  /// @brief Reads a file from disk into a blob (follows symlinks).
  /// @param path File to read.
  /// @return The blob, or an error if the file cannot be read.
  static Result<Blob> load_from_worktree(const fs::path &path);
  /// @brief Loads a stored blob.
  /// @param hash The blob's name.
  /// @return The blob, or an error if it is missing, corrupt or not a blob.
  static Result<Blob> load_from_objects(const Sha1 &hash);

private:
  explicit Blob(Object &&object) : Object(std::move(object)) {}
};

}
