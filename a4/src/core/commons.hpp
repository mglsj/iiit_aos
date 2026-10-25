#pragma once

/// @file
/// @brief Shared type aliases, the Result error type and the TRY macros.

#include <charconv>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

/// @brief Name of the repository folder that init creates in the worktree root.
const inline fs::path GIT_DIR(".mygit");

/// @brief Command-line arguments handed to a command (views into argv).
using Args = std::span<const std::string_view>;

/// @brief An owned buffer of raw bytes.
using Bytes = std::vector<std::byte>;
/// @brief A read-only view of raw bytes.
using BytesView = std::span<const std::byte>;

/// @brief Views the characters of a string as raw bytes, without copying.
/// @param text Text to view; it must outlive the returned view.
/// @return A byte view over the same memory.
inline BytesView as_byte_view(std::string_view text) {
  return std::as_bytes(std::span{text});
}

/// @brief Views raw bytes as characters, without copying.
///
/// The result can contain NUL bytes; use its size, never strlen().
/// @param bytes Bytes to view; they must outlive the returned view.
/// @return A string view over the same memory.
inline std::string_view as_text(BytesView bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

/// @brief Copies the characters of a string into a byte buffer.
/// @param text Text to copy.
/// @return A new buffer holding the same bytes.
inline Bytes to_bytes(std::string_view text) {
  const BytesView view = as_byte_view(text);
  return {view.begin(), view.end()};
}

/// @brief Either a value of type T or an error message.
///
/// Every function that can fail returns a Result instead of throwing. main()
/// prints the message of an error that reaches it.
template <class T> using Result = std::expected<T, std::string>;

/// @brief Looks up a key in a map without copying the value.
/// @param map Map to search.
/// @param key Key to find.
/// @return A reference to the mapped value, or nothing if the key is absent.
template <class Map>
std::optional<const typename Map::mapped_type &>
lookup(const Map &map, const typename Map::key_type &key) {
  const auto it = map.find(key);
  if (it == map.end())
    return std::nullopt;
  return it->second;
}

/// @brief Parses a whole string as an integer.
/// @tparam T Integer type to produce.
/// @param digits Text that must consist only of digits in @p base.
/// @param base Number base, e.g. 10, or 8 for file modes.
/// @return The number, or an error if the text is empty, has extra characters
///         or does not fit in T.
template <typename T>
inline Result<T> sv_to_num(std::string_view digits, int base = 10) {
  T number = 0;

  const auto [end, ec] = std::from_chars(
      digits.data(), digits.data() + digits.size(), number, base);
  if (digits.empty() || ec != std::errc{} ||
      end != digits.data() + digits.size())
    return std::unexpected("invalid number");

  return number;
}

/// @brief Unwraps a Result, or returns its error from the calling function.
///
/// Uses a GCC/Clang statement expression, so `TRY(f())` can be used as a value.
/// Do not nest TRY inside another TRY argument; the inner variable shadows.
/// @param expr An expression of type `Result<T>`.
#define TRY(expr)                                                              \
  __extension__({                                                              \
    auto &&_try_result = (expr);                                               \
    if (!_try_result)                                                          \
      return std::unexpected(std::move(_try_result).error());                  \
    std::move(*_try_result);                                                   \
  })

/// @brief Returns the error of a Result<void> from the calling function, if any.
/// @param expr An expression of type `Result<void>` (or any Result whose value is unused).
#define TRY_VOID(expr)                                                         \
  do {                                                                         \
    if (auto _try_result = (expr); !_try_result)                               \
      return std::unexpected(std::move(_try_result).error());                  \
  } while (0)
