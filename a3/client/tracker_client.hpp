#pragma once

#include "common/address.hpp"
#include "common/connection.hpp"
#include "common/message.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <optional>

class TrackerClient {
public:
  TrackerClient(vector<Address> trackers, const Address &self)
      : _trackers(std::move(trackers)), _self(std::format("{}", self)) {}

  // tries every tracker once, so main can stop when none answers at startup
  std::expected<void, string> connect();

  std::expected<Response, string> request(Request req);

  void set_announcements(std::function<vector<Request>()> announcements) {
    _announcements = std::move(announcements);
  }

private:
  struct Credentials {
    string user;
    string password;
  };

  std::expected<void, string> _connect_any();
  std::expected<void, string> _open_connection();
  void _announce();

  vector<Address> _trackers;
  string _self;
  std::function<vector<Request>()> _announcements;

  // guards everything below and stays held from request to reply
  std::mutex _mutex;
  size_t _current = 0;
  std::optional<Connection> _conn;

  std::optional<Credentials> _login;
};
