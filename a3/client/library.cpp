#include "library.hpp"
#include "common/file.hpp"
#include "common/logger.hpp"

#include <algorithm>

void Library::add(const string &gid, const string &path, const FileMeta &meta,
                  bool complete) {
  std::scoped_lock lock(_mutex);

  _files.insert_or_assign(
      Key{gid, meta.name},
      LocalFile{
          .path = path,
          .meta = meta,
          .have = vector<bool>(meta.piece_count(), complete),
      });
}

void Library::remove(const string &gid, const string &name) {
  std::scoped_lock lock(_mutex);

  _files.erase({gid, name});
}

std::optional<string> Library::path(const string &gid,
                                    const string &name) const {
  std::scoped_lock lock(_mutex);

  auto it = _files.find({gid, name});
  if (it == _files.end())
    return std::nullopt;
  return it->second.path;
}

void Library::mark_piece(const string &gid, const string &name,
                         uint32_t index) {
  std::scoped_lock lock(_mutex);

  auto it = _files.find({gid, name});
  if (it != _files.end() && index < it->second.have.size())
    it->second.have[index] = true;
}

vector<Request> Library::announcements() const {
  std::scoped_lock lock(_mutex);

  vector<Request> requests;
  for (const auto &[key, file] : _files) {
    const auto &[gid, name] = key;
    auto held = std::ranges::count(file.have, true);

    if (held == static_cast<long>(file.have.size())) {
      Request upload{.cmd = Command::UploadFile, .args = {gid}};
      upload.args.append_range(file.meta.to_args());
      requests.push_back(std::move(upload));
    } else if (held > 0) {
      requests.push_back({
          .cmd = Command::ReportPieces,
          .args = {gid, name, std::to_string(held)},
      });
    }
  }
  return requests;
}

std::optional<string> Library::piece_map(const string &gid,
                                         const string &name) const {
  std::scoped_lock lock(_mutex);

  auto it = _files.find({gid, name});
  if (it == _files.end())
    return std::nullopt;

  string map;
  for (bool has : it->second.have)
    map.push_back(has ? '1' : '0');
  return map;
}

std::expected<Bytes, Status> Library::read_piece(const string &gid,
                                                 const string &name,
                                                 uint64_t index) const {
  string path;
  uint32_t len = 0;

  // copies what the read needs, so the disk read runs without the lock
  {
    std::scoped_lock lock(_mutex);

    auto it = _files.find({gid, name});
    if (it == _files.end())
      return std::unexpected(Status::FileNotFound);

    const LocalFile &file = it->second;
    if (index >= file.have.size() || !file.have[index])
      return std::unexpected(Status::PieceUnavailable);

    path = file.path;
    len = file.meta.piece_len(static_cast<uint32_t>(index));
  }

  Bytes piece(len);
  auto read = File::open_read(path).and_then([&](const File &file) {
    return file.read_at(index * PIECE_SIZE, piece);
  });

  if (!read) {
    Logger::warn("cannot serve {}: {}", name, read.error());
    return std::unexpected(Status::PieceUnavailable);
  }

  return piece;
}
