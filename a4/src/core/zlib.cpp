/// @file
/// @brief Implementation of compress() and decompress() on top of zlib.

#include "core/zlib.hpp"
#include "core/commons.hpp"

#include <format>
#include <memory>
#include <zlib.h>

namespace {

constexpr std::size_t CHUNK_SIZE = 128 * 1024;

}

namespace zlib {

Result<Bytes> compress(BytesView data, int compression_level) {
  ::z_stream zs{};
  if (const int status = ::deflateInit(&zs, compression_level); status != Z_OK)
    return std::unexpected(
        std::format("deflateInit failed (zlib status {})", status));
  std::unique_ptr<::z_stream, decltype(&::deflateEnd)> guard(&zs, ::deflateEnd);

  // deflateBound() is an upper limit, so one Z_FINISH call is enough.
  Bytes compressed(::deflateBound(&zs, data.size()));
  zs.next_in = const_cast<::Bytef *>(
      reinterpret_cast<const ::Bytef *>(data.data()));
  zs.avail_in = static_cast<::uInt>(data.size());
  zs.next_out = reinterpret_cast<::Bytef *>(compressed.data());
  zs.avail_out = static_cast<::uInt>(compressed.size());

  if (::deflate(&zs, Z_FINISH) != Z_STREAM_END)
    return std::unexpected("cannot compress data");

  compressed.resize(zs.total_out);
  return compressed;
}

Result<Bytes> decompress(BytesView compressed) {
  ::z_stream zs{};
  if (const int status = ::inflateInit(&zs); status != Z_OK)
    return std::unexpected(
        std::format("inflateInit failed (zlib status {})", status));
  std::unique_ptr<::z_stream, decltype(&::inflateEnd)> guard(&zs, ::inflateEnd);

  zs.next_in = const_cast<::Bytef *>(
      reinterpret_cast<const ::Bytef *>(compressed.data()));
  zs.avail_in = static_cast<::uInt>(compressed.size());

  Bytes output;
  int status;
  do {
    const std::size_t used = zs.total_out;
    output.resize(used + CHUNK_SIZE);
    zs.next_out = reinterpret_cast<::Bytef *>(output.data()) + used;
    zs.avail_out = CHUNK_SIZE;

    status = ::inflate(&zs, Z_NO_FLUSH);
    switch (status) {
    case Z_OK:
    case Z_STREAM_END:
      break;
    // Git objects never use a preset dictionary.
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
      return std::unexpected("invalid compressed data");
    case Z_MEM_ERROR:
      return std::unexpected("out of memory while decompressing");
    case Z_BUF_ERROR:
      return std::unexpected("compressed data is truncated");
    default:
      return std::unexpected(std::format("decompression failed (zlib status {})", status));
    }
  } while (status != Z_STREAM_END);

  output.resize(zs.total_out);
  return output;
}
}
