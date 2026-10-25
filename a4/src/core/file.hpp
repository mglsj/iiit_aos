#pragma once

/// @file
/// @brief Binary-safe file reading and atomic file writing.

#include "core/commons.hpp"

/// @brief Reads a whole file as raw bytes.
/// @param path File to read.
/// @return Its exact bytes, or an error if it cannot be opened, or if fewer bytes
///         than its size could be read (for example, it changed meanwhile).
Result<Bytes> read_file(const fs::path &path);
/// @brief Writes a file atomically, creating its parent folders.
///
/// The data goes to `<path>.tmp`, which is then renamed over @p path, so readers
/// and crashes never see a half-written file.
/// @param path Destination file; replaced if it exists.
/// @param data Bytes to write.
/// @return Nothing, or an error if a folder, the temporary file or the rename fails.
Result<void> write_file(const fs::path &path, BytesView data);

/// @brief Writes text to a file atomically. See write_file(const fs::path &, BytesView).
/// @param path Destination file.
/// @param text Bytes to write, given as characters.
/// @return Nothing, or an error.
inline Result<void> write_file(const fs::path &path, std::string_view text) {
  return write_file(path, as_byte_view(text));
}
