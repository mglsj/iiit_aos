/// @file
/// @brief Implementation of read_file() and write_file().

#include "core/file.hpp"

#include <format>
#include <fstream>
#include <system_error>

Result<Bytes> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec)
    return std::unexpected(
        std::format("cannot read '{}': {}", path.string(), ec.message()));

  std::ifstream file(path, std::ios::binary);
  if (!file)
    return std::unexpected(std::format("cannot open '{}'", path.string()));

  Bytes content(size);
  file.read(reinterpret_cast<char *>(content.data()),
            static_cast<std::streamsize>(size));
  if (file.gcount() != static_cast<std::streamsize>(size))
    return std::unexpected(std::format("cannot read '{}': file changed while reading", path.string()));

  return content;
}

Result<void> write_file(const fs::path &path, BytesView data) {
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec)
      return std::unexpected(std::format(
          "cannot create '{}': {}", path.parent_path().string(), ec.message()));
  }

  // Write to a temporary file and rename, so a crash never leaves a partial file.
  fs::path tmp = path;
  tmp += ".tmp";

  {
    std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
    if (!file)
      return std::unexpected(
          std::format("cannot open '{}' for writing", tmp.string()));

    file.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
    file.flush();
    if (!file) {
      fs::remove(tmp, ec);
      return std::unexpected(std::format("cannot write '{}'", tmp.string()));
    }
  }

  fs::rename(tmp, path, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return std::unexpected(
        std::format("cannot create '{}': {}", path.string(), ec.message()));
  }

  return {};
}
