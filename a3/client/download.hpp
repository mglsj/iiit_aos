#pragma once

#include "common/address.hpp"
#include "common/file.hpp"
#include "common/file_meta.hpp"
#include "library.hpp"
#include "piece_picker.hpp"
#include "tracker_client.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <mutex>
#include <stop_token>

vector<Address> find_peers(TrackerClient &tracker, const string &gid,
                           const string &name, const string &self);

enum class DownloadState { Running, Complete, Failed };

class Download {
public:
  Download(string gid, FileMeta meta, string path, File out,
           TrackerClient &tracker, Library &library, string self, bool shares);

  void run(vector<Address> peers, std::stop_token stop);

  string status_line();

  bool is(const string &gid, const string &name) const {
    return _gid == gid && _meta.name == name;
  }

  void cancel();

private:
  void _fetch_round(const vector<Address> &peers, std::stop_token stop);

  void _work(const vector<Address> &peers, std::atomic<size_t> &next_peer,
             std::stop_source &finished);

  std::expected<void, string> _fetch_from(const Address &peer,
                                          std::stop_source &finished);

  void _finish();

  void _give_up(string error);

  void _report(uint32_t pieces);

  uint32_t _done_count();
  bool _is_cancelled();

  const string _gid;
  const FileMeta _meta;
  const string _path;
  File _out;
  TrackerClient &_tracker;
  Library &_library;
  const string _self;
  // false for a copy of a file the library already serves from another path
  const bool _shares;
  PiecePicker _picker;

  std::stop_source _cancel;

  // guards _done, _state, _error and _cancelled
  std::mutex _mutex;
  uint32_t _done = 0;
  DownloadState _state = DownloadState::Running;
  string _error;
  bool _cancelled = false;
};
