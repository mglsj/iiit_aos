#pragma once

#include "common/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

constexpr size_t SHA1_SIZE = 20;
constexpr size_t SHA1_HEX_LEN = 2 * SHA1_SIZE;

using Sha1Digest = std::array<byte, SHA1_SIZE>;

// streaming, so a large file is hashed one piece at a time
class Sha1 {
public:
  void update(std::span<const byte> data);

  Sha1Digest digest();

private:
  static constexpr size_t BLOCK_SIZE = 64;

  void _compress_block();

  std::array<uint32_t, 5> _state = {0x67452301, 0xEFCDAB89, 0x98BADCFE,
                                    0x10325476, 0xC3D2E1F0};
  std::array<byte, BLOCK_SIZE> _block = {};
  size_t _block_used = 0;
  uint64_t _message_len = 0;
};

string to_hex(std::span<const byte> bytes);
