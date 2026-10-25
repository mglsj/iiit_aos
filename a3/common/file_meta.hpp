#pragma once

#include "common/protocol.hpp"
#include "common/sha1.hpp"
#include "common/types.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

constexpr uint32_t MAX_PIECES = PIECE_SIZE / SHA1_HEX_LEN;
constexpr uint64_t MAX_FILE_SIZE = uint64_t{MAX_PIECES} * PIECE_SIZE;

constexpr size_t FILE_META_ARGC = 4;

struct FileMeta {
  string name;
  uint64_t size = 0;
  string sha1 = {};
  string piece_hashes = {};

  uint32_t piece_count() const;
  uint32_t piece_len(uint32_t index) const pre(index < piece_count());
  std::string_view piece_hash(uint32_t index) const pre(index < piece_count());

  vector<string> to_args() const;

  static std::expected<FileMeta, string>
  from_args(std::span<const string> args);
};
