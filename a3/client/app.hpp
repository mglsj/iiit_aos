#pragma once

#include "common/address.hpp"
#include "common/tcp_server.hpp"
#include "downloads.hpp"
#include "library.hpp"
#include "tracker_client.hpp"

#include <expected>
#include <stop_token>
#include <thread>

class App {
public:
  App(vector<Address> trackers, const Address &self);
  ~App() { stop(); }

  App(const App &) = delete("its threads point back at it");
  App &operator=(const App &) = delete("its threads point back at it");

  std::expected<void, string> start();

  std::expected<Response, string> execute(Request req);

  void stop();

private:
  std::expected<Response, string> _upload(Request req);
  std::expected<Response, string> _download(Request req);

  Address _self;

  std::stop_source _stop;

  TrackerClient _tracker;
  Library _library;
  Downloads _downloads{_tracker, _library, std::format("{}", _self)};
  TcpServer _peers;
  std::jthread _accept;
};
