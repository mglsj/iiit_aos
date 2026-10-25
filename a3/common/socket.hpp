#pragma once

#include "common/address.hpp"
#include "common/fd.hpp"

#include <expected>

class Socket {
public:
  Socket() = default;
  explicit Socket(int fd) : _fd(fd) {}

  int fd() const { return _fd.get(); }

  std::expected<void, string> set_recv_timeout(int seconds);

  void shutdown();

private:
  Fd _fd;
};

std::expected<Socket, string> connect_to(const Address &addr);

std::expected<Socket, string> accept_from(const Socket &listener);

std::expected<Socket, string> listen_on(const Address &addr);
