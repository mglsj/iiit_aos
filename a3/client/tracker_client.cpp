#include "tracker_client.hpp"
#include "common/console.hpp"
#include "common/logger.hpp"

#include <format>

constexpr std::string_view NO_TRACKER = "Unable to connect to any trackers.";

std::expected<void, string> TrackerClient::connect() {
  std::scoped_lock lock(_mutex);
  return _connect_any();
}

std::expected<Response, string> TrackerClient::request(Request req) {
  std::scoped_lock lock(_mutex);

  if (req.cmd == Command::Login)
    req.args.push_back(_self);

  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!_conn) {
      if (auto opened = _open_connection(); !opened)
        return std::unexpected(opened.error());
    }

    auto res = _conn->request(req);
    if (!res) {
      Logger::warn("lost tracker {} at {}: {}", _current, _trackers[_current],
                   res.error());
      _conn.reset();
      _current = (_current + 1) % _trackers.size();
      continue;
    }

    // the tracker ends the old session on any login, even one that fails
    if (req.cmd == Command::Login || req.cmd == Command::Logout)
      _login.reset();
    if (req.cmd == Command::Login && res->status == Status::Ok) {
      _login = Credentials{.user = req.args[0], .password = req.args[1]};
      _announce();
    }

    return res;
  }

  return std::unexpected(string(NO_TRACKER));
}

std::expected<void, string> TrackerClient::_connect_any() {
  for (size_t i = 0; i < _trackers.size(); ++i) {
    size_t index = (_current + i) % _trackers.size();

    auto socket = connect_to(_trackers[index]);
    if (!socket) {
      Logger::warn("cannot reach tracker {} at {}: {}", index, _trackers[index],
                   socket.error());
      continue;
    }

    _conn.emplace(std::move(*socket));
    _current = index;
    Logger::info("connected to tracker {} at {}", index, _trackers[index]);
    return {};
  }

  return std::unexpected(string(NO_TRACKER));
}

std::expected<void, string> TrackerClient::_open_connection() {
  if (auto connected = _connect_any(); !connected)
    return connected;

  if (!_login)
    return {};

  auto res = _conn->request(
      {.cmd = Command::Login, .args = {_login->user, _login->password, _self}});

  if (!res) {
    _conn.reset();
    return std::unexpected(string(NO_TRACKER));
  }

  // for example, the new tracker has not yet heard of this user
  if (res->status != Status::Ok) {
    Console::print(stdout, std::format("Session lost after reconnecting: {}",
                                       status_text(res->status)));
    _login.reset();
    return {};
  }

  Logger::info("logged in again as {}", _login->user);
  _announce();
  if (!_conn)
    return std::unexpected(string(NO_TRACKER));
  return {};
}

// the tracker drops a user's files at logout, so every login lists them again
void TrackerClient::_announce() {
  if (!_announcements)
    return;

  for (const Request &req : _announcements()) {
    if (!_conn->request(req)) {
      _conn.reset();
      return;
    }
  }
}
