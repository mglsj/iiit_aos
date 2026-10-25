#pragma once

/// @file
/// @brief The base Object: header, hash, loading and storing.

#include <optional>
#include <string>
#include <string_view>

#include "core/commons.hpp"
#include "core/sha1.hpp"

namespace git {

/// @brief The kinds of object Git stores. mygit never creates tags.
enum class ObjectType { blob, tree, commit, tag };

/// @brief The name used in object headers and by `cat-file -t`, e.g. "blob".
std::string_view to_string(ObjectType type);
/// @brief Parses a type name from an object header.
/// @return The type, or nothing if the name is unknown.
std::optional<ObjectType> parse_object_type(std::string_view name);

/// @brief An immutable stored object: a type, its content, and their hash.
///
/// The hash covers the `"<type> <size>\0"` header as well as the content, as in
/// Git, and is computed once in the constructor.
class Object {
public:
  /// @brief Reads, decompresses and checks the object named @p hash.
  /// @param hash The object's name.
  /// @return The object, or an error if it is missing, corrupt, or its header
  ///         or size does not match its content.
  static Result<Object> load_from_objects(const Sha1 &hash);

  /// @brief Compresses the object and stores it under `.mygit/objects/`.
  ///
  /// Does nothing if a file with this hash already exists, since it must hold
  /// the same bytes.
  /// @return Nothing, or an error if there is no repository or writing fails.
  Result<void> write() const;

  /// @brief The object's type.
  ObjectType type() const { return _type; }
  /// @brief The object's type as text, e.g. "tree".
  std::string_view type_name() const { return to_string(_type); }
  /// @brief Content size in bytes, without the header (what `cat-file -s`
  /// prints).
  std::size_t size() const { return _content.size(); }
  /// @brief The content bytes, without the header. May contain NUL bytes.
  const std::string &content() const { return _content; }
  /// @brief The object's SHA-1 name.
  const Sha1 &hash() const { return _hash; }

protected:
  /// @brief Creates an object and computes its hash.
  /// @param type Object type.
  /// @param content Content bytes, without the header.
  Object(ObjectType type, std::string content);

private:
  /// @brief The bytes that are hashed and stored: `"<type> <size>\0<content>"`.
  Bytes _serialize() const;
  std::string _content;
  ObjectType _type;
  Sha1 _hash;
};

} // namespace git
