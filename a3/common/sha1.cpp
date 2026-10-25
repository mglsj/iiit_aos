#include "common/sha1.hpp"

#include <algorithm>
#include <bit>

void Sha1::update(std::span<const byte> data) {
  _message_len += data.size();

  while (!data.empty()) {
    size_t copied = std::min(data.size(), BLOCK_SIZE - _block_used);
    std::copy_n(data.begin(), copied, _block.begin() + _block_used);
    _block_used += copied;
    data = data.subspan(copied);

    if (_block_used == BLOCK_SIZE) {
      _compress_block();
      _block_used = 0;
    }
  }
}

Sha1Digest Sha1::digest() {
  uint64_t message_bits = _message_len * 8;

  // padding is a 1 bit, zeros, then the bit length, to fill the last block
  const byte one_bit_then_zeros = 0x80;
  const byte zero_bits = 0x00;
  update({&one_bit_then_zeros, 1});
  while (_block_used != BLOCK_SIZE - 8)
    update({&zero_bits, 1});

  std::array<byte, 8> length_big_endian;
  for (size_t byte_index = 0; byte_index < 8; ++byte_index)
    length_big_endian[byte_index] =
        static_cast<byte>(message_bits >> (56 - 8 * byte_index));
  update(length_big_endian);

  Sha1Digest hash;
  for (size_t word_index = 0; word_index < 5; ++word_index)
    for (size_t byte_in_word = 0; byte_in_word < 4; ++byte_in_word)
      hash[4 * word_index + byte_in_word] =
          static_cast<byte>(_state[word_index] >> (24 - 8 * byte_in_word));
  return hash;
}

void Sha1::_compress_block() {
  // the message schedule, W in FIPS 180-4
  std::array<uint32_t, 80> schedule;
  for (size_t word = 0; word < 16; ++word)
    schedule[word] = uint32_t{_block[4 * word]} << 24 |
                     uint32_t{_block[4 * word + 1]} << 16 |
                     uint32_t{_block[4 * word + 2]} << 8 |
                     uint32_t{_block[4 * word + 3]};
  for (size_t word = 16; word < 80; ++word)
    schedule[word] = std::rotl(schedule[word - 3] ^ schedule[word - 8] ^
                                   schedule[word - 14] ^ schedule[word - 16],
                               1);

  // first to fifth are a to e in FIPS 180-4
  uint32_t first = _state[0];
  uint32_t second = _state[1];
  uint32_t third = _state[2];
  uint32_t fourth = _state[3];
  uint32_t fifth = _state[4];

  for (size_t round = 0; round < 80; ++round) {
    uint32_t mixed;
    uint32_t round_constant;

    if (round < 20) {
      // choose
      mixed = (second & third) | (~second & fourth);
      round_constant = 0x5A827999;
    } else if (round < 40) {
      mixed = second ^ third ^ fourth;
      round_constant = 0x6ED9EBA1;
    } else if (round < 60) {
      // majority
      mixed = (second & third) | (second & fourth) | (third & fourth);
      round_constant = 0x8F1BBCDC;
    } else {
      mixed = second ^ third ^ fourth;
      round_constant = 0xCA62C1D6;
    }

    // new_first is T in FIPS 180-4
    uint32_t new_first =
        std::rotl(first, 5) + mixed + fifth + round_constant + schedule[round];
    fifth = fourth;
    fourth = third;
    third = std::rotl(second, 30);
    second = first;
    first = new_first;
  }

  // adding makes each block depend on every block before it
  _state[0] += first;
  _state[1] += second;
  _state[2] += third;
  _state[3] += fourth;
  _state[4] += fifth;
}

string to_hex(std::span<const byte> bytes) {
  constexpr char HEX_DIGITS[] = "0123456789abcdef";

  string hex;
  hex.reserve(2 * bytes.size());
  for (byte value : bytes) {
    hex.push_back(HEX_DIGITS[value >> 4]);
    hex.push_back(HEX_DIGITS[value & 0xf]);
  }
  return hex;
}
