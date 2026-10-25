/// @file
/// @brief Implementation of the index: binary encoding, validation, updates.

#include "repo/index.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>

#include "core/file.hpp"

namespace {

// Git index version 2: a header, sorted entries, then a SHA-1 of everything before it.
constexpr std::string_view SIGNATURE = "DIRC";
constexpr std::uint32_t VERSION = 2;

constexpr std::size_t HEADER_SIZE = 12;
constexpr std::size_t STAT_SIZE = 40;
constexpr std::size_t HASH_OFFSET = STAT_SIZE;
constexpr std::size_t FLAGS_OFFSET = HASH_OFFSET + Sha1::SIZE;
constexpr std::size_t FIXED_SIZE = FLAGS_OFFSET + 2;
constexpr std::size_t NAME_LENGTH_MAX = 0xFFF;

// Assume-valid, extended and merge-stage bits. This index never writes them.
constexpr std::uint16_t UNSUPPORTED_FLAGS = 0xF000;

/// @brief Appends a 16-bit big-endian integer.
void put_u16(std::string &out, std::uint16_t value) {
  out.push_back(static_cast<char>(value >> 8));
  out.push_back(static_cast<char>(value & 0xFF));
}

/// @brief Appends a 32-bit big-endian integer.
void put_u32(std::string &out, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

/// @brief Reads a 16-bit big-endian integer at @p pos. Callers check the bounds first.
std::uint16_t get_u16(std::string_view data, std::size_t pos) {
  return static_cast<std::uint16_t>(
      (static_cast<unsigned char>(data[pos]) << 8) |
      static_cast<unsigned char>(data[pos + 1]));
}

/// @brief Reads a 32-bit big-endian integer at @p pos. Callers check the bounds first.
std::uint32_t get_u32(std::string_view data, std::size_t pos) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < 4; i++)
    value = (value << 8) | static_cast<unsigned char>(data[pos + i]);
  return value;
}

/// @brief NUL bytes needed after an entry of @p length bytes.
///
/// Each entry ends with 1 to 8 NUL bytes so the next one starts on an 8-byte boundary.
std::size_t padding_for(std::size_t length) { return 8 - (length % 8); }

}

namespace git {

Result<Index> Index::load_from_index(const fs::path &file) {
  std::error_code ec;
  const bool exists = fs::exists(file, ec);
  if (ec)
    return std::unexpected(
        std::format("cannot read '{}': {}", file.string(), ec.message()));
  if (!exists)
    return Index{};

  const Bytes bytes = TRY(read_file(file));
  const std::string_view data = as_text(bytes);
  if (data.size() < HEADER_SIZE + Sha1::SIZE)
    return std::unexpected("index file is too short");
  if (data.substr(0, SIGNATURE.size()) != SIGNATURE)
    return std::unexpected("index file has a bad signature");
  if (get_u32(data, 4) != VERSION)
    return std::unexpected("unsupported index version");

  const std::string_view body = data.substr(0, data.size() - Sha1::SIZE);
  const std::string_view stored = data.substr(body.size());
  const Sha1 actual = Sha1::hash_data(as_byte_view(body));
  if (as_text(actual.raw()) != stored)
    return std::unexpected("index file is corrupt: checksum mismatch");

  const std::uint32_t count = get_u32(data, 8);
  Index result;
  std::size_t pos = HEADER_SIZE;

  for (std::uint32_t i = 0; i < count; i++) {
    if (pos + FIXED_SIZE > body.size())
      return std::unexpected("index file is corrupt: truncated entry");

    const std::uint16_t flags = get_u16(body, pos + FLAGS_OFFSET);
    if ((flags & UNSUPPORTED_FLAGS) != 0)
      return std::unexpected("index file uses unsupported entry flags");

    const std::size_t name_start = pos + FIXED_SIZE;
    const std::size_t nul = body.find('\0', name_start);
    if (nul == std::string_view::npos)
      return std::unexpected("index file is corrupt: unterminated path");

    const std::string_view name = body.substr(name_start, nul - name_start);
    const std::size_t name_length = flags & NAME_LENGTH_MAX;
    if (name.empty() ||
        (name_length != NAME_LENGTH_MAX && name_length != name.size()))
      return std::unexpected("index file is corrupt: bad path length");

    const std::size_t entry_length = FIXED_SIZE + name.size();
    const std::size_t next = pos + entry_length + padding_for(entry_length);
    if (next > body.size())
      return std::unexpected("index file is corrupt: truncated entry");

    IndexEntry entry;
    entry.ctime_sec = get_u32(body, pos);
    entry.ctime_nsec = get_u32(body, pos + 4);
    entry.mtime_sec = get_u32(body, pos + 8);
    entry.mtime_nsec = get_u32(body, pos + 12);
    entry.dev = get_u32(body, pos + 16);
    entry.ino = get_u32(body, pos + 20);
    entry.mode = TRY(get_mode(static_cast<int>(get_u32(body, pos + 24))));
    entry.uid = get_u32(body, pos + 28);
    entry.gid = get_u32(body, pos + 32);
    entry.size = get_u32(body, pos + 36);
    entry.hash = TRY(Sha1::from_bytes(
        as_byte_view(body.substr(pos + HASH_OFFSET, Sha1::SIZE))));
    entry.path = std::string(name);

    const std::string path = entry.path;
    if (!result._entries.try_emplace(path, std::move(entry)).second)
      return std::unexpected(
          std::format("index file has a duplicate entry '{}'", path));

    pos = next;
  }

  if (pos != body.size())
    return std::unexpected("index file has unexpected trailing data");

  return result;
}

Result<void> Index::save(const fs::path &file) const {
  std::string raw(SIGNATURE);
  put_u32(raw, VERSION);
  put_u32(raw, static_cast<std::uint32_t>(_entries.size()));

  for (const auto &item : _entries) {
    const IndexEntry &entry = item.second;

    const std::size_t start = raw.size();
    put_u32(raw, entry.ctime_sec);
    put_u32(raw, entry.ctime_nsec);
    put_u32(raw, entry.mtime_sec);
    put_u32(raw, entry.mtime_nsec);
    put_u32(raw, entry.dev);
    put_u32(raw, entry.ino);
    put_u32(raw, static_cast<std::uint32_t>(entry.mode));
    put_u32(raw, entry.uid);
    put_u32(raw, entry.gid);
    put_u32(raw, entry.size);
    raw += as_text(entry.hash.raw());

    const std::size_t name_length = std::min(entry.path.size(), NAME_LENGTH_MAX);
    put_u16(raw, static_cast<std::uint16_t>(name_length));
    raw += entry.path;
    raw.append(padding_for(raw.size() - start), '\0');
  }

  const Sha1 checksum = Sha1::hash_data(as_byte_view(raw));
  raw += as_text(checksum.raw());

  return write_file(file, raw);
}

IndexEntry make_index_entry(const std::string &path, const WorktreeFile &file) {
  const struct stat &st = file.st;
  return IndexEntry{
      .ctime_sec = static_cast<std::uint32_t>(st.st_ctim.tv_sec),
      .ctime_nsec = static_cast<std::uint32_t>(st.st_ctim.tv_nsec),
      .mtime_sec = static_cast<std::uint32_t>(st.st_mtim.tv_sec),
      .mtime_nsec = static_cast<std::uint32_t>(st.st_mtim.tv_nsec),
      .dev = static_cast<std::uint32_t>(st.st_dev),
      .ino = static_cast<std::uint32_t>(st.st_ino),
      .mode = file.mode,
      .uid = static_cast<std::uint32_t>(st.st_uid),
      .gid = static_cast<std::uint32_t>(st.st_gid),
      .size = static_cast<std::uint32_t>(st.st_size),
      .hash = file.blob.hash(),
      .path = path,
  };
}

void Index::upsert(IndexEntry entry) {
  const std::string path = entry.path;
  _entries.insert_or_assign(path, std::move(entry));
}

void Index::remove(const std::string &path) { _entries.erase(path); }

std::vector<std::string> Index::paths_under(std::string_view dir) const {
  std::vector<std::string> paths;
  for (const auto &item : _entries) {
    const std::string_view path = item.first;
    const bool match = dir.empty() || path == dir ||
                       (path.starts_with(dir) && path[dir.size()] == '/');
    if (match)
      paths.push_back(item.first);
  }
  return paths;
}

}
