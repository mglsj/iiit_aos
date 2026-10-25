#pragma once

#include "common/connection.hpp"
#include "common/message.hpp"
#include "replicator.hpp"
#include "state.hpp"

#include <optional>

class ClientHandler {
public:
  ClientHandler(TrackerState &state, Replicator &replicator)
      : _state(state), _replicator(replicator) {}

  void run(Connection &conn, const Bytes &first);

private:
  struct Session {
    string user;
    string addr;
  };

  Response _handle(const Bytes &frame, std::optional<Session> &session);
  Response _dispatch(const Request &req, std::optional<Session> &session);
  void _end_session(std::optional<Session> &session);

  TrackerState &_state;
  Replicator &_replicator;
};
