#include "server.hpp"

Server::Server(const vector<Address> &trackers, uint32_t self)
    : _trackers(trackers), _self(self), _log(self), _state(_log),
      _tcp([this](Connection &conn) { _route(conn); }),
      _replicator(_state, _log, _trackers, _self),
      _clients(_state, _replicator) {}

std::expected<void, string> Server::start() {
  return _tcp.listen(_trackers[_self]).transform([this] {
    _replicator.start(_stop.get_token());
  });
}

std::expected<void, string> Server::run() {
  return _tcp.run(_stop.get_token());
}

void Server::join() {
  _replicator.join();
  _tcp.wait();
}

void Server::_route(Connection &conn) {
  auto first = conn.recv();
  if (!first)
    return;

  if (frame_command(*first) == Command::PeerHello)
    _replicator.serve_inbound(conn, *first);
  else
    _clients.run(conn, *first);
}
