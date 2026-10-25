#pragma once

/// @file
/// @brief Commit objects: snapshots linked to their parents.

#include <chrono>
#include <vector>

#include "objects/object.hpp"

namespace git {

/// @brief An author or committer line: who and when.
struct Person {
  /// @brief Name, e.g. "Student".
  std::string name;
  /// @brief Email address, printed between `<` and `>`.
  std::string email;
  /// @brief Unix time of the commit.
  std::chrono::seconds seconds;
  /// @brief UTC offset at commit time, e.g. "+0530".
  std::string timezone_offset;
};

/// @brief Everything a commit records.
struct CommitData {
  /// @brief Root tree of the snapshot.
  Sha1 tree;
  /// @brief Previous commit(s): none for the first commit, one normally, more for a merge.
  std::vector<Sha1> parents;
  /// @brief Who wrote the change.
  Person author;
  /// @brief Who recorded the commit (mygit uses the same person).
  Person committer;
  /// @brief Commit message, ending in a newline.
  std::string message;
};

/// @brief A snapshot: one root tree, its parent commits, author, committer and message.
class Commit : public Object {
public:
  /// @brief Creates a new commit and computes its hash.
  /// @param data The commit's fields.
  explicit Commit(CommitData data)
      : Object(ObjectType::commit, _to_content(data)),
        _data(std::move(data)) {}

  /// @brief Parses a loaded object as a commit.
  /// @param object An object of type commit.
  /// @return The commit, or an error if it is another type or malformed.
  static Result<Commit> from_object(Object object);
  /// @brief Loads and parses a stored commit.
  /// @param hash The commit's name.
  /// @return The commit, or an error if it is missing, corrupt or not a commit.
  static Result<Commit> load_from_objects(const Sha1 &hash);

  /// @brief Hash of the root tree.
  const Sha1 &tree_hash() const { return _data.tree; }
  /// @brief Hashes of the parent commits.
  const std::vector<Sha1> &parents() const { return _data.parents; }
  /// @brief Who wrote the change, and when.
  const Person &author() const { return _data.author; }
  /// @brief Who recorded the commit, and when.
  const Person &committer() const { return _data.committer; }
  /// @brief The commit message.
  const std::string &message() const { return _data.message; }

private:
  CommitData _data;

  /// @brief Encodes a commit in Git's text format (tree, parents, author, committer, blank line, message).
  static std::string _to_content(const CommitData &data);
  /// @brief Decodes the text of a stored commit.
  ///
  /// Layout: header lines (tree, parent, author, committer), a blank line, then
  /// the message. tree, author and committer are required.
  /// @param content The commit's content.
  /// @return The fields, or an error for a missing or malformed header.
  static Result<CommitData> _parse(std::string_view content);

  Commit(Object &&object, CommitData data)
      : Object(std::move(object)), _data(std::move(data)) {}
};

}
