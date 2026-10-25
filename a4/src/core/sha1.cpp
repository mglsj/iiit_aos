/// @file
/// @brief Implementation of Sha1 on top of OpenSSL.

#include "core/sha1.hpp"

#include <algorithm>
#include <openssl/sha.h>

namespace {

constexpr std::string_view HEX_DIGITS = "0123456789abcdef";

int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

}

Sha1 Sha1::hash_data(BytesView data) {
  Sha1 sha1;

  SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(),
       reinterpret_cast<unsigned char *>(sha1._digest.data()));

  return sha1;
}

std::string Sha1::to_string() const {
  std::string result;
  result.reserve(SIZE * 2);

  for (const std::byte byte : _digest) {
    result += HEX_DIGITS[std::to_integer<std::size_t>(byte >> 4)];
    result += HEX_DIGITS[std::to_integer<std::size_t>(byte & std::byte(0x0f))];
  }

  return result;
}

Result<Sha1> Sha1::from_hex_string(std::string_view hex) {
  if (hex.size() != SIZE * 2)
    return std::unexpected(std::format("not a valid object name '{}'", hex));

  Sha1 sha1;
  for (std::size_t i = 0; i < SIZE; i++) {
    const int high = hex_value(hex[2 * i]);
    const int low = hex_value(hex[2 * i + 1]);
    if (high < 0 || low < 0)
      return std::unexpected(std::format("not a valid object name '{}'", hex));

    sha1._digest[i] = static_cast<std::byte>(high << 4 | low);
  }

  return sha1;
}

Result<Sha1> Sha1::from_bytes(BytesView raw) {
  if (raw.size() != SIZE)
    return std::unexpected(
        std::format("expected {} raw hash bytes, got {}", SIZE, raw.size()));

  Sha1 sha1;
  std::ranges::copy(raw, sha1._digest.begin());
  return sha1;
}
