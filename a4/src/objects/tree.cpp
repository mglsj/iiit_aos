/// @file
/// @brief Implementation of Tree: sorting, parsing, building from disk or the index.

#include "objects/tree.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string_view>
#include <vector>

#include "objects/blob.hpp"
#include "repo/index.hpp"
#include "repo/worktree.hpp"

namespace {

/// @brief The string an entry sorts by: Git sorts a folder as if its name ended in "/".
std::string sort_key(const git::TreeEntry &entry) {
  return entry.mode == git::Mode::DIR ? entry.name + "/" : entry.name;
}

/// @brief Builds one tree level from sorted index entries, recursing into folders.
///
/// @p rows are sorted by path, so all files of one folder are adjacent and can
/// be grouped in a single pass.
/// @param rows Index entries below the current folder, sorted by path.
/// @param prefix_len Length of the current folder's path prefix, e.g. 4 for "src/".
/// @param[out] pending Receives every sub-tree built.
/// @return The tree for this level, or an error.
Result<git::Tree> build_level(const std::vector<const git::IndexEntry *> &rows,
                              std::size_t prefix_len,
                              std::vector<git::Object> &pending) {
  std::vector<git::TreeEntry> entries;

  std::size_t i = 0;
  while (i < rows.size()) {
    const std::string_view path = rows[i]->path;
    const std::string_view rest = path.substr(prefix_len);
    const std::size_t slash = rest.find('/');

    if (slash == std::string_view::npos) {
      entries.push_back({
          .mode = rows[i]->mode,
          .name = std::string(rest),
          .hash = rows[i]->hash,
      });
      i++;
      continue;
    }

    const std::string_view folder = rest.substr(0, slash);
    const std::size_t child_prefix = prefix_len + slash + 1;
    const std::string_view folder_prefix = path.substr(0, child_prefix);

    std::vector<const git::IndexEntry *> children;
    while (i < rows.size() &&
           std::string_view(rows[i]->path).starts_with(folder_prefix)) {
      children.push_back(rows[i]);
      i++;
    }

    const git::Tree subtree = TRY(build_level(children, child_prefix, pending));
    pending.push_back(subtree);

    entries.push_back({
        .mode = git::Mode::DIR,
        .name = std::string(folder),
        .hash = subtree.hash(),
    });
  }

  return git::Tree::from_entries(std::move(entries));
}

/// @brief Appends every file below @p tree to @p out, recursively.
/// @param tree Tree to walk.
/// @param prefix Path of @p tree, ending in "/" (empty for the root).
/// @param[out] out Receives the entries.
/// @return Nothing, or an error if a sub-tree cannot be loaded.
Result<void> flatten_into(const git::Tree &tree, const std::string &prefix,
                          std::vector<git::FlatEntry> &out) {
  for (const auto &entry : tree.entries()) {
    std::string path = prefix + entry.name;
    if (entry.mode == git::Mode::DIR) {
      const git::Tree subtree = TRY(git::Tree::load_from_objects(entry.hash));
      TRY_VOID(flatten_into(subtree, path + "/", out));
    } else {
      out.push_back({.path = std::move(path), .mode = entry.mode, .hash = entry.hash});
    }
  }
  return {};
}

}

namespace git {

std::string to_listing(const TreeEntry &entry) {
  const ObjectType type =
      entry.mode == Mode::DIR ? ObjectType::tree : ObjectType::blob;
  return std::format("{:06o} {} {}\t{}", static_cast<int>(entry.mode),
                     to_string(type), entry.hash, entry.name);
}

Result<Tree> Tree::load_from_objects(const Sha1 &hash) {
  return from_object(TRY(Object::load_from_objects(hash)));
}

Result<Tree> Tree::from_object(Object object) {
  if (object.type() != ObjectType::tree)
    return std::unexpected(std::format("object {} is a {}, not a tree",
                                       object.hash(), object.type_name()));

  Tree tree(std::move(object));
  TRY_VOID(tree._parse_entries());
  return tree;
}

Result<void> Tree::_parse_entries() {
  std::string_view rest = Object::content();

  while (!rest.empty()) {
    const auto space = rest.find(' ');
    if (space == std::string_view::npos)
      return std::unexpected("malformed tree entry: missing mode");

    const int mode_num = TRY(sv_to_num<int>(rest.substr(0, space), 8));
    const Mode mode = TRY(get_mode(mode_num));
    rest.remove_prefix(space + 1);

    const auto nul = rest.find('\0');
    if (nul == std::string_view::npos)
      return std::unexpected("malformed tree entry: missing name");

    std::string name(rest.substr(0, nul));
    rest.remove_prefix(nul + 1);

    if (rest.size() < Sha1::SIZE)
      return std::unexpected("malformed tree entry: truncated hash");

    Sha1 hash = TRY(Sha1::from_bytes(as_byte_view(rest.substr(0, Sha1::SIZE))));
    rest.remove_prefix(Sha1::SIZE);

    _entries.push_back({.mode = mode, .name = std::move(name), .hash = hash});
  }

  return {};
}

Result<Tree> Tree::from_entries(std::vector<TreeEntry> entries) {
  for (const auto &entry : entries) {
    if (entry.name.empty() || entry.name == "." || entry.name == ".." ||
        entry.name.find_first_of("/\0", 0, 2) != std::string::npos)
      return std::unexpected(
          std::format("malformed tree entry: invalid name '{}'", entry.name));
  }

  std::ranges::sort(entries, [](const auto &a, const auto &b) {
    return sort_key(a) < sort_key(b);
  });

  for (std::size_t i = 1; i < entries.size(); i++)
    if (entries[i - 1].name == entries[i].name)
      return std::unexpected(
          std::format("malformed tree entry: duplicate name '{}'", entries[i].name));

  return Tree(std::move(entries));
}

Result<std::vector<FlatEntry>> Tree::flatten() const {
  std::vector<FlatEntry> result;
  TRY_VOID(flatten_into(*this, "", result));
  return result;
}

Result<Tree> Tree::load_from_worktree(const fs::path &dir,
                                      std::vector<Object> &pending) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return std::unexpected(
        std::format("'{}' is not a directory", dir.string()));

  const auto read_error = [&dir](const std::error_code &error) {
    return std::format("cannot read '{}': {}", dir.string(),
                       error.message());
  };

  std::vector<TreeEntry> entries;
  fs::directory_iterator it(dir, ec);
  if (ec)
    return std::unexpected(read_error(ec));

  for (const fs::directory_iterator end; !ec && it != end; it.increment(ec)) {
    const fs::directory_entry &entry = *it;
    const fs::path name = entry.path().filename();

    if (is_real_directory(entry.path())) {
      if (name == GIT_DIR)
        continue;

      const Tree subtree = TRY(Tree::load_from_worktree(entry.path(), pending));
      // Like Git, skip empty directories.
      if (subtree.entries().empty())
        continue;
      pending.push_back(subtree);

      entries.push_back({
          .mode = Mode::DIR,
          .name = name.string(),
          .hash = subtree.hash(),
      });
      continue;
    }

    const std::optional<WorktreeFile> file =
        TRY(load_worktree_file(entry.path()));
    if (file) {
      pending.push_back(file->blob);

      entries.push_back({
          .mode = file->mode,
          .name = name.string(),
          .hash = file->blob.hash(),
      });
    }
  }
  if (ec)
    return std::unexpected(read_error(ec));

  return from_entries(std::move(entries));
}

Result<Tree> Tree::load_from_index(const Index &index,
                                   std::vector<Object> &pending) {
  std::vector<const IndexEntry *> rows;
  rows.reserve(index.entries().size());
  for (const auto &item : index.entries())
    rows.push_back(&item.second);

  return build_level(rows, 0, pending);
}

std::string Tree::_to_content(const std::vector<TreeEntry> &entries) {
  std::string content = "";

  for (auto const &entry : entries) {
    content += std::format("{:o} {}", static_cast<int>(entry.mode), entry.name);
    content.push_back('\0');
    content += as_text(entry.hash.raw());
  }
  return content;
}

}
