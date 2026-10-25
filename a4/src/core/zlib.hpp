#pragma once

/// @file
/// @brief zlib compression of stored objects.

#include "core/commons.hpp"

namespace zlib {
/// @brief Compresses bytes into the zlib format used for stored objects.
/// @param data Bytes to compress.
/// @param compression_level 0 (none) to 9 (best); -1 is zlib's default (6).
/// @return The compressed bytes, or an error from zlib.
Result<Bytes> compress(BytesView data, int compression_level = -1);

/// @brief Decompresses zlib data of unknown original size.
/// @param compressed A complete zlib stream.
/// @return The original bytes, or an error if the data is corrupt or truncated.
Result<Bytes> decompress(BytesView compressed);
}
