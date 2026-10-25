#pragma once

#include "common/address.hpp"
#include "common/connection.hpp"
#include "common/socket.hpp"

#include <condition_variable>
#include <cstddef>
#include <expected>
#include <functional>
#include <mutex>
#include <stop_token>

class TcpServer {
public:
  using Handler = std::function<void(Connection &)>;

  explicit TcpServer(Handler handler) : _handler(std::move(handler)) {}

  TcpServer(const TcpServer &) = delete("threads point back at it");
  TcpServer &operator=(const TcpServer &) = delete("threads point back at it");

  std::expected<void, string> listen(const Address &addr);

  std::expected<void, string> run(std::stop_token stop);

  void wait();

private:
  Handler _handler;
  Socket _listener;

  // guards _handlers
  std::mutex _mutex;
  std::condition_variable _idle;
  size_t _handlers = 0;
};
