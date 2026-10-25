/// @file
/// @brief Implementation of Object: object paths, header parsing, zlib storage.

#include "objects/object.hpp"

#include <format>

#include "core/commons.hpp"
#include "core/file.hpp"
#include "core/zlib.hpp"
#include "repo/repo.hpp"

namespace {

/// @brief Where an object lives: `.mygit/objects/<first 2 hex>/<other 38>`.
/// @return The path, or an error if no repository is found.
Result<fs::path> object_path(const Sha1 &hash) {
  const fs::path git_dir = TRY(find_git_dir());
  const std::string hex = hash.to_string();
  return git_dir / "objects" / hex.substr(0, 2) / hex.substr(2);
}

/// @brief The parsed "<type> <size>" header of a stored object.
struct Header {
  git::ObjectType type;
  std::size_t size;
};

/// @brief Parses an object header, e.g. "blob 11".
/// @param text The header without its terminating NUL.
/// @return The type and size, or an error for a malformed header.
Result<Header> parse_header(std::string_view text) {
  const auto space = text.find(' ');
  if (space == std::string_view::npos)
    return std::unexpected("invalid object header");

  const auto type = git::parse_object_type(text.substr(0, space));
  if (!type)
    return std::unexpected("unknown object type");

  const std::string_view digits = text.substr(space + 1);
  std::size_t size = TRY(sv_to_num<std::size_t>(digits));
  // const auto [end, ec] =
  //     std::from_chars(digits.data(), digits.data() + digits.size(), size);
  // if (digits.empty() || ec != std::errc{} ||
  //     end != digits.data() + digits.size())
  //   return std::unexpected("invalid object size");

  return Header{*type, size};
}

} // namespace

namespace git {

std::string_view to_string(ObjectType type) {
  switch (type) {
  case ObjectType::blob:
    return "blob";
  case ObjectType::tree:
    return "tree";
  case ObjectType::commit:
    return "commit";
  case ObjectType::tag:
    return "tag";
  }
  return "unknown";
}

std::optional<ObjectType> parse_object_type(std::string_view name) {
  for (const auto type : {ObjectType::blob, ObjectType::tree,
                          ObjectType::commit, ObjectType::tag}) {
    if (name == to_string(type))
      return type;
  }
  return std::nullopt;
}

Object::Object(ObjectType type, std::string content)
    : _content(std::move(content)), _type(type),
      _hash(Sha1::hash_data(_serialize())) {}

Bytes Object::_serialize() const {
  std::string raw = std::format("{} {}", type_name(), _content.size());
  raw.push_back('\0');
  raw += _content;

  return to_bytes(raw);
}

Result<Object> Object::load_from_objects(const Sha1 &hash) {

  const auto with_context = [&hash](std::string_view what) {
    return [&hash, what](const std::string &error) {
      return std::format("object {} {}: {}", hash, what, error);
    };
  };

  const fs::path path = TRY(object_path(hash));
  const auto compressed =
      TRY(read_file(path).transform_error([&hash](const std::string &) {
        return std::format("object {} not found", hash);
      }));
  const auto decompressed = TRY(
      zlib::decompress(compressed).transform_error(with_context("is corrupt")));
  const std::string_view raw = as_text(decompressed);

  const auto nul = raw.find('\0');
  if (nul == std::string_view::npos)
    return std::unexpected(
        std::format("object {} is corrupt: missing header", hash));

  const auto header = TRY(parse_header(raw.substr(0, nul))
                              .transform_error(with_context("is corrupt")));
  if (raw.size() - nul - 1 != header.size)
    return std::unexpected(
        std::format("object {} is corrupt: size mismatch", hash));

  return Object(header.type, std::string(raw.substr(nul + 1)));
}

Result<void> Object::write() const {
  const fs::path path = TRY(object_path(_hash));

  std::error_code ec;
  if (fs::exists(path, ec))
    return {};

  return write_file(path, TRY(zlib::compress(_serialize())));
}

} // namespace git
