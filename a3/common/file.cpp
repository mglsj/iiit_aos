#include "common/file.hpp"
#include "common/error.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

std::expected<File, string> File::_open_with(const string &path, int flags) {
  int fd = ::open(path.c_str(), flags, 0644);
  if (fd < 0)
    return errno_error(path);
  return File(fd);
}

std::expected<File, string> File::open_read(const string &path) {
  return _open_with(path, O_RDONLY);
}

std::expected<File, string> File::create(const string &path) {
  return _open_with(path, O_RDWR | O_CREAT | O_TRUNC);
}

std::expected<uint64_t, string> File::size() const {
  struct stat st;
  if (::fstat(_fd.get(), &st) != 0)
    return errno_error("fstat");
  if (!S_ISREG(st.st_mode))
    return std::unexpected("not a regular file");
  return static_cast<uint64_t>(st.st_size);
}

std::expected<void, string> File::resize(uint64_t size) {
  if (::ftruncate(_fd.get(), static_cast<off_t>(size)) != 0)
    return errno_error("ftruncate");
  return {};
}

std::expected<void, string> File::read_at(uint64_t offset,
                                          std::span<byte> buf) const {
  size_t got = 0;

  while (got < buf.size()) {
    ssize_t r = ::pread(_fd.get(), buf.data() + got, buf.size() - got,
                        static_cast<off_t>(offset + got));

    if (r < 0 && errno == EINTR)
      continue;
    if (r < 0)
      return errno_error("pread");
    // the file shrank after its size was read
    if (r == 0)
      return std::unexpected("file ended early");

    got += static_cast<size_t>(r);
  }

  return {};
}

std::expected<void, string> File::write_at(uint64_t offset,
                                           std::span<const byte> buf) {
  size_t written = 0;

  while (written < buf.size()) {
    ssize_t w = ::pwrite(_fd.get(), buf.data() + written, buf.size() - written,
                         static_cast<off_t>(offset + written));

    if (w < 0 && errno == EINTR)
      continue;
    if (w < 0)
      return errno_error("pwrite");

    written += static_cast<size_t>(w);
  }

  return {};
}
