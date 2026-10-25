#pragma once

#include "common/message.hpp"
#include "common/socket.hpp"

#include <expected>
#include <mutex>
#include <span>

class Connection {
public:
  explicit Connection(Socket socket) : _socket(std::move(socket)) {}

  std::expected<void, string> send(std::span<const byte> frame);

  std::expected<Bytes, string> recv();

  std::expected<Response, string> request(const Request &req);

  void shutdown() { _socket.shutdown(); }

private:
  Socket _socket;
  std::mutex _send_mutex;
};
