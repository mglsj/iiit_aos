#include "common/tcp_server.hpp"

#include <memory>
#include <thread>

std::expected<void, string> TcpServer::listen(const Address &addr) {
  return listen_on(addr).transform(
      // success branch of expected
      [this](Socket listener) { _listener = std::move(listener); });
}

std::expected<void, string> TcpServer::run(std::stop_token stop) {
  std::stop_callback on_stop(stop, [this] { _listener.shutdown(); });

  while (!stop.stop_requested()) {
    auto socket = accept_from(_listener);

    if (!socket && stop.stop_requested())
      break;
    if (!socket)
      return std::unexpected(socket.error());

    auto conn = std::make_unique<Connection>(std::move(*socket));

    {
      std::scoped_lock lock(_mutex);
      ++_handlers;
    }

    std::thread([this, stop, conn = std::move(conn)]() mutable {
      {
        // a callback added after a stop runs at once, so late connections wake
        std::stop_callback wake_conn(stop, [&] { conn->shutdown(); });
        _handler(*conn);
      }

      conn.reset();

      // notifying under the lock keeps wait() blocked until this thread is done
      std::scoped_lock lock(_mutex);
      --_handlers;
      _idle.notify_all();
    }).detach();
  }

  return {};
}

void TcpServer::wait() {
  std::unique_lock lock(_mutex);
  _idle.wait(lock, [this] { return _handlers == 0; });
}
