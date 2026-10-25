#pragma once

/// @file
/// @brief SHA-1 hashes: computing, parsing and printing them.

#include <array>
#include <format>
#include <string>
#include <string_view>

#include "core/commons.hpp"

/// @brief A 20-byte SHA-1 digest: the name of every object.
///
/// Stored as raw bytes; to_string() gives the usual 40-character hex form.
class Sha1 {
public:
  /// @brief Digest length in bytes (40 hex characters).
  static constexpr std::size_t SIZE = 20;
  /// @brief Raw storage for one digest.
  using Digest = std::array<std::byte, SIZE>;

  /// @brief Wraps an existing digest.
  /// @param digest The 20 raw bytes.
  explicit Sha1(const Digest &digest) noexcept : _digest(digest) {}

  /// @brief Computes the SHA-1 of some bytes (via OpenSSL).
  /// @param data Bytes to hash.
  /// @return Their digest.
  static Sha1 hash_data(BytesView data);
  /// @brief Parses a hash written as 40 hex digits (either case).
  /// @param hex The hash text; short prefixes are not accepted.
  /// @return The hash, or "not a valid object name" for any other input.
  static Result<Sha1> from_hex_string(std::string_view hex);
  /// @brief Builds a hash from 20 raw bytes, as stored in trees and the index.
  /// @param raw Exactly Sha1::SIZE bytes.
  /// @return The hash, or an error if the length is wrong.
  static Result<Sha1> from_bytes(BytesView raw);

  /// @brief The raw digest.
  const Digest &get_bytes() const { return _digest; }
  /// @brief The raw digest as a byte view.
  BytesView raw() const { return _digest; }
  /// @brief Formats the hash as 40 lower-case hex digits.
  /// @return For example "95d09f2b10159347eece71399a7e2e907ea3df4f".
  std::string to_string() const;

  bool operator==(const Sha1 &) const = default;

private:
  Sha1() = default;

  Digest _digest{};
};

/// @brief Lets std::format and std::println print a Sha1 as hex, e.g. `std::format("{}", hash)`.
template <> struct std::formatter<Sha1> : std::formatter<std::string_view> {
  auto format(const Sha1 &hash, std::format_context &ctx) const {
    return std::formatter<std::string_view>::format(hash.to_string(), ctx);
  }
};
