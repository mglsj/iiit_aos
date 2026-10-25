#include "downloads.hpp"

#include <format>
#include <optional>
#include <sys/stat.h>

static bool is_directory(const string &path) {
  struct stat st;
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool same_file(const string &a, const string &b) {
  struct stat sa, sb;
  return ::stat(a.c_str(), &sa) == 0 && ::stat(b.c_str(), &sb) == 0 &&
         sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

std::expected<void, string> Downloads::start(const string &gid, FileMeta meta,
                                             string dest,
                                             std::stop_token stop) {
  if (is_directory(dest))
    dest += "/" + meta.name;

  // a download onto the file the library serves would truncate it
  std::optional<string> held = _library.path(gid, meta.name);
  if (held && same_file(*held, dest))
    return std::unexpected("Already sharing or downloading this file there");

  vector<Address> peers = find_peers(_tracker, gid, meta.name, _self);
  if (peers.empty())
    return std::unexpected("No peers are sharing this file right now");

  auto file = File::create(dest);
  if (!file)
    return std::unexpected(file.error());
  // the full size up front lets each piece be written at its own offset
  if (auto sized = file->resize(meta.size); !sized)
    return std::unexpected(std::format("{}: {}", dest, sized.error()));

  if (!held)
    _library.add(gid, dest, meta, false);

  auto download = std::make_unique<Download>(gid, std::move(meta),
                                             std::move(dest), std::move(*file),
                                             _tracker, _library, _self, !held);
  Download &started = *download;
  {
    std::scoped_lock lock(_mutex);
    _downloads.push_back(std::move(download));
  }

  _threads.emplace_back(
      [&started, peers = std::move(peers), stop] { started.run(peers, stop); });
  return {};
}

vector<string> Downloads::status_lines() {
  std::scoped_lock lock(_mutex);

  vector<string> lines;
  for (const auto &download : _downloads)
    lines.push_back(download->status_line());
  return lines;
}

void Downloads::cancel(const string &gid, const string &name) {
  std::scoped_lock lock(_mutex);

  for (const auto &download : _downloads)
    if (download->is(gid, name))
      download->cancel();
}
