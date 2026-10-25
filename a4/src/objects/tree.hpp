#pragma once

/// @file
/// @brief Tree objects: folders, their binary format, and how they are built.

#include <vector>

#include "objects/mode.hpp"
#include "objects/object.hpp"

namespace git {

class Index;

/// @brief One entry of a tree: a file, symlink or sub-tree.
struct TreeEntry {
  Mode mode;
  std::string name;
  Sha1 hash;
};

/// @brief Formats an entry like `ls-tree`: `"<mode> <type> <hash>
/// <name>"`.
/// @return For example `"040000 tree d855e2d6…	test_tree_1"`.
std::string to_listing(const TreeEntry &entry);

/// @brief A file or symlink anywhere below a tree, with its full path.
struct FlatEntry {
  /// @brief Path from the root tree, e.g. "src/main.cpp".
  std::string path;
  Mode mode;
  Sha1 hash;
};

/// @brief A folder: entries sorted by name, each naming a blob or a sub-tree.
///
/// Every Tree is valid: it can only be made by from_entries() (which checks and
/// sorts) or by parsing a stored tree.
class Tree : public Object {
public:
  /// @brief Parses a loaded object as a tree.
  /// @param object An object of type tree.
  /// @return The tree, or an error if it is another type or malformed.
  static Result<Tree> from_object(Object object);
  /// @brief Builds a new tree from entries in any order.
  ///
  /// Sorts them the way Git does (folders compare as "name/").
  /// @param entries The entries.
  /// @return The tree, or an error for an empty, ".", ".." or "/"-containing
  /// name,
  ///         or a duplicate name.
  static Result<Tree> from_entries(std::vector<TreeEntry> entries);

  /// @brief Loads and parses a stored tree.
  /// @param hash The tree's name.
  /// @return The tree, or an error if it is missing, corrupt or not a tree.
  static Result<Tree> load_from_objects(const Sha1 &hash);

  /// @brief Builds the tree of a folder on disk, recursively (used by
  /// write-tree).
  ///
  /// Skips `.mygit`, empty folders, and anything that is not a file or symlink.
  /// Blobs and sub-trees found on the way go into @p pending; the caller writes
  /// them before the tree, once the whole walk has succeeded.
  /// @param dir Folder to snapshot.
  /// @param[out] pending Receives every new blob and sub-tree.
  /// @return The root tree (not yet written), or an error.
  static Result<Tree> load_from_worktree(const fs::path &dir,
                                         std::vector<Object> &pending);
  /// @brief Builds nested trees from the staged paths in the index (used by
  /// commit).
  /// @param index The staging area.
  /// @param[out] pending Receives every sub-tree; the blobs were stored by add.
  /// @return The root tree (not yet written), or an error.
  static Result<Tree> load_from_index(const Index &index,
                                      std::vector<Object> &pending);

  /// @brief Lists every file and symlink below this tree, sorted by path.
  ///
  /// Loads sub-trees from the object store as needed.
  /// @return Entries with full paths such as "src/main.cpp", or an error if a
  ///         sub-tree cannot be loaded.
  Result<std::vector<FlatEntry>> flatten() const;

  /// @brief The entries, in sorted order.
  const std::vector<TreeEntry> &entries() const { return _entries; }

private:
  std::vector<TreeEntry> _entries;

  explicit Tree(std::vector<TreeEntry> entries)
      : Object(ObjectType::tree, _to_content(entries)),
        _entries(std::move(entries)) {}
  explicit Tree(Object &&object) : Object(std::move(object)) {}

  /// @brief Encodes entries in Git's format: `"<octal mode> <name>\0<20 raw
  /// hash bytes>"` each.
  static std::string _to_content(const std::vector<TreeEntry> &entries);
  /// @brief Decodes the content of a stored tree into _entries.
  /// @return Nothing, or an error for a malformed entry.
  Result<void> _parse_entries();
};

} // namespace git
