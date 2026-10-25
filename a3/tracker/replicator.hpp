#pragma once

#include "common/address.hpp"
#include "common/connection.hpp"
#include "oplog.hpp"
#include "state.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>

class Replicator {
public:
  Replicator(TrackerState &state, OperationLog &log,
             const vector<Address> &trackers, uint32_t self);

  void start(std::stop_token stop);

  void join();

  void push();

  void serve_inbound(Connection &conn, std::span<const byte> hello);

private:
  struct PeerLink {
    std::mutex mutex;
    Connection *conn = nullptr;
    uint64_t pushed = 0;

    std::condition_variable_any wake;
    bool wake_requested = false;
  };

  void _sync_loop(std::stop_token stop, uint32_t peer);
  void _wait_before_reconnect(std::stop_token stop, PeerLink &link);
  void _receive(Connection &conn);

  TrackerState &_state;
  OperationLog &_log;

  vector<Address> _trackers;
  uint32_t _self;

  vector<std::unique_ptr<PeerLink>> _links;
  vector<std::jthread> _threads;
};
