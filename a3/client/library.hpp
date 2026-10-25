#pragma once

#include "common/file_meta.hpp"
#include "common/message.hpp"
#include "common/protocol.hpp"
#include "common/types.hpp"

#include <cstdint>
#include <expected>
#include <map>
#include <mutex>
#include <optional>
#include <utility>

class Library {
public:
  void add(const string &gid, const string &path, const FileMeta &meta,
           bool complete);
  void remove(const string &gid, const string &name);
  std::optional<string> path(const string &gid, const string &name) const;

  void mark_piece(const string &gid, const string &name, uint32_t index);

  vector<Request> announcements() const;

  std::optional<string> piece_map(const string &gid, const string &name) const;

  std::expected<Bytes, Status> read_piece(const string &gid, const string &name,
                                          uint64_t index) const;

private:
  struct LocalFile {
    string path;
    FileMeta meta;
    vector<bool> have;
  };

  using Key = std::pair<string, string>;

  // guards _files
  mutable std::mutex _mutex;
  std::map<Key, LocalFile> _files;
};
