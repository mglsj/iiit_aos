#pragma once

#include "download.hpp"

#include <expected>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

class Downloads {
public:
  Downloads(TrackerClient &tracker, Library &library, string self)
      : _tracker(tracker), _library(library), _self(std::move(self)) {}

  std::expected<void, string> start(const string &gid, FileMeta meta,
                                    string dest, std::stop_token stop);

  vector<string> status_lines();

  void cancel(const string &gid, const string &name);

  void join() { _threads.clear(); }

private:
  TrackerClient &_tracker;
  Library &_library;
  string _self;

  // guards _downloads, and is taken before a Download's own lock
  std::mutex _mutex;
  vector<std::unique_ptr<Download>> _downloads;

  // only the CLI thread touches this, so it needs no lock
  vector<std::jthread> _threads;
};
