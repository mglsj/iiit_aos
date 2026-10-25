#pragma once

#include "client_handler.hpp"
#include "common/address.hpp"
#include "common/tcp_server.hpp"
#include "oplog.hpp"
#include "replicator.hpp"
#include "state.hpp"

#include <expected>
#include <stop_token>

class Server {
public:
  Server(const vector<Address> &trackers, uint32_t self);

  std::expected<void, string> start();
  std::expected<void, string> run();

  void stop() { _stop.request_stop(); }
  std::stop_token stop_token() const { return _stop.get_token(); }

  void join();

private:
  void _route(Connection &conn);

  vector<Address> _trackers;
  uint32_t _self;

  std::stop_source _stop;

  OperationLog _log;
  TrackerState _state;
  TcpServer _tcp;
  Replicator _replicator;
  ClientHandler _clients;
};
