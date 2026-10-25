#include "common/file_meta.hpp"
#include "common/text.hpp"

#include <algorithm>

static bool is_hex(std::string_view s) {
  return std::ranges::all_of(s, [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

uint32_t FileMeta::piece_count() const {
  if (size == 0)
    return 1;
  return static_cast<uint32_t>((size + PIECE_SIZE - 1) / PIECE_SIZE);
}

uint32_t FileMeta::piece_len(uint32_t index) const {
  uint64_t start = uint64_t{index} * PIECE_SIZE;
  return static_cast<uint32_t>(std::min<uint64_t>(PIECE_SIZE, size - start));
}

std::string_view FileMeta::piece_hash(uint32_t index) const {
  return std::string_view(piece_hashes)
      .substr(index * SHA1_HEX_LEN, SHA1_HEX_LEN);
}

vector<string> FileMeta::to_args() const {
  return {name, std::to_string(size), sha1, piece_hashes};
}

std::expected<FileMeta, string>
FileMeta::from_args(std::span<const string> args) {
  if (args.size() != FILE_META_ARGC)
    return std::unexpected("wrong number of file fields");

  auto size = parse_u64(args[1]);
  if (!size)
    return std::unexpected("file size is not a number");
  if (*size > MAX_FILE_SIZE)
    return std::unexpected("file is too large");

  FileMeta meta{
      .name = args[0],
      .size = *size,
      .sha1 = args[2],
      .piece_hashes = args[3],
  };

  if (meta.name.empty() || meta.name.size() > MAX_ARG_LEN ||
      meta.name.contains('/') || meta.name == "." || meta.name == "..")
    return std::unexpected("invalid file name");

  if (meta.sha1.size() != SHA1_HEX_LEN ||
      meta.piece_hashes.size() != size_t{meta.piece_count()} * SHA1_HEX_LEN ||
      !is_hex(meta.sha1) || !is_hex(meta.piece_hashes))
    return std::unexpected("hashes do not match the file size");

  return meta;
}
