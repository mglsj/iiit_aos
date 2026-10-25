#pragma once

#include "common/fd.hpp"
#include "common/types.hpp"

#include <cstdint>
#include <expected>
#include <span>

class File {
public:
  static std::expected<File, string> open_read(const string &path);
  static std::expected<File, string> create(const string &path);

  std::expected<uint64_t, string> size() const;

  std::expected<void, string> resize(uint64_t size);

  std::expected<void, string> read_at(uint64_t offset,
                                      std::span<byte> buf) const;
  std::expected<void, string> write_at(uint64_t offset,
                                       std::span<const byte> buf);

private:
  explicit File(int fd) : _fd(fd) {}

  static std::expected<File, string> _open_with(const string &path, int flags);

  Fd _fd;
};
