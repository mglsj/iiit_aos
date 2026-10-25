#include "file_hash.hpp"
#include "common/file.hpp"

#include <format>
#include <functional>

static std::expected<uint64_t, string>
read_pieces(const string &path,
            std::function_ref<void(std::span<const byte>)> use) {
  auto file = File::open_read(path);
  if (!file)
    return std::unexpected(file.error());

  auto size = file->size();
  if (!size)
    return std::unexpected(std::format("{}: {}", path, size.error()));
  if (*size > MAX_FILE_SIZE)
    return std::unexpected(path + ": file is too large");

  FileMeta layout{.name = path, .size = *size};
  Bytes buf(PIECE_SIZE);

  for (uint32_t i = 0; i < layout.piece_count(); ++i) {
    std::span<byte> piece = std::span(buf).first(layout.piece_len(i));
    if (auto read = file->read_at(uint64_t{i} * PIECE_SIZE, piece); !read)
      return std::unexpected(std::format("{}: {}", path, read.error()));
    use(piece);
  }

  return *size;
}

std::expected<FileMeta, string> hash_file(const string &path) {
  Sha1 whole;
  string piece_hashes;

  auto size = read_pieces(path, [&](std::span<const byte> piece) {
    Sha1 one;
    one.update(piece);
    piece_hashes += to_hex(one.digest());
    whole.update(piece);
  });
  if (!size)
    return std::unexpected(size.error());

  return FileMeta{
      // npos + 1 wraps to 0 when the path has no slash
      .name = path.substr(path.rfind('/') + 1),
      .size = *size,
      .sha1 = to_hex(whole.digest()),
      .piece_hashes = std::move(piece_hashes),
  };
}

std::expected<string, string> sha1_of_file(const string &path) {
  Sha1 sha1;
  auto size = read_pieces(
      path, [&](std::span<const byte> piece) { sha1.update(piece); });
  if (!size)
    return std::unexpected(size.error());

  return to_hex(sha1.digest());
}
