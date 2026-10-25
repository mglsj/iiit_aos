/// @file
/// @brief Implementation of Blob.

#include "objects/blob.hpp"

#include <format>

#include "core/file.hpp"

namespace git {

Result<Blob> Blob::load_from_worktree(const fs::path &path) {
  return read_file(path)
      .transform(
          [](const Bytes &bytes) { return Blob(std::string(as_text(bytes))); })
      .transform_error(
          [](const std::string &e) { return std::format("{}", e); });
}

Result<Blob> Blob::load_from_objects(const Sha1 &hash) {
  auto object = TRY(Object::load_from_objects(hash));
  if (object.type() != ObjectType::blob)
    return std::unexpected(
        std::format("object {} is a {}, not a blob", hash, object.type_name()));

  return Blob(std::move(object));
}

}
