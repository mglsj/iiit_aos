#pragma once

#include <unistd.h>
#include <utility>

class Fd {
public:
  Fd() = default;
  explicit Fd(int fd) : _fd(fd) {}
  ~Fd() { close(); }

  Fd(Fd &&other) noexcept : _fd(std::exchange(other._fd, -1)) {}
  Fd &operator=(Fd &&other) noexcept {
    if (this != &other) {
      close();
      _fd = std::exchange(other._fd, -1);
    }
    return *this;
  }

  Fd(const Fd &) = delete("two copies would both close it");
  Fd &operator=(const Fd &) = delete("two copies would both close it");

  int get() const { return _fd; }

  void close() {
    if (_fd >= 0) {
      ::close(_fd);
      _fd = -1;
    }
  }

private:
  int _fd = -1;
};
