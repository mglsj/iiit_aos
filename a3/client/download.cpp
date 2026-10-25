#include "download.hpp"
#include "common/connection.hpp"
#include "file_hash.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <format>
#include <ranges>
#include <thread>

constexpr int PEER_TIMEOUT_S = 10;

constexpr size_t MAX_WORKERS = 4;

constexpr auto RETRY_DELAY = std::chrono::seconds(2);
constexpr uint32_t MAX_IDLE_ROUNDS = 3;

static bool wait_unless_stopped(std::stop_token stop,
                                std::chrono::seconds delay) {
  std::mutex mutex;
  std::condition_variable_any never_notified;
  std::unique_lock lock(mutex);
  never_notified.wait_for(lock, stop, delay, [] { return false; });
  return !stop.stop_requested();
}

static bool piece_matches(const FileMeta &meta, uint32_t index,
                          std::span<const byte> piece) {
  if (piece.size() != meta.piece_len(index))
    return false;

  Sha1 sha1;
  sha1.update(piece);
  return to_hex(sha1.digest()) == meta.piece_hash(index);
}

vector<Address> find_peers(TrackerClient &tracker, const string &gid,
                           const string &name, const string &self) {
  auto res = tracker.request({
      .cmd = Command::ListSeeders,
      .args = {gid, name},
  });
  if (!res || res->status != Status::Ok)
    return {};

  vector<Address> peers;
  for (auto seeder : res->items | std::views::drop(1) | std::views::chunk(3)) {
    if (seeder.size() != 3)
      continue;
    const string &addr = seeder[1];
    if (addr == self)
      continue;
    if (auto peer = parse_address(addr))
      peers.push_back(std::move(*peer));
  }
  return peers;
}

Download::Download(string gid, FileMeta meta, string path, File out,
                   TrackerClient &tracker, Library &library, string self,
                   bool shares)
    : _gid(std::move(gid)), _meta(std::move(meta)), _path(std::move(path)),
      _out(std::move(out)), _tracker(tracker), _library(library),
      _self(std::move(self)), _shares(shares), _picker(_meta.piece_count()) {}

string Download::status_line() {
  std::scoped_lock lock(_mutex);

  switch (_state) {
  case DownloadState::Running:
    return std::format("[D] [{}] {} {}/{} pieces", _gid, _meta.name, _done,
                       _meta.piece_count());
  case DownloadState::Complete:
    return std::format("[C] [{}] {}", _gid, _meta.name);
  case DownloadState::Failed:
    break;
  }
  return std::format("[F] [{}] {}: {}", _gid, _meta.name, _error);
}

void Download::run(vector<Address> peers, std::stop_token stop) {
  uint32_t idle_rounds = 0;

  while (true) {
    uint32_t before = _done_count();
    _fetch_round(peers, stop);

    if (_is_cancelled())
      return _give_up("stopped by stop_share");
    if (_picker.complete())
      return _finish();
    if (stop.stop_requested())
      return _give_up("stopped");

    idle_rounds = _done_count() > before ? 0 : idle_rounds + 1;
    if (idle_rounds == MAX_IDLE_ROUNDS) {
      string error;
      {
        std::scoped_lock lock(_mutex);
        error = _error.empty() ? "no peer had every piece" : _error;
      }
      return _give_up(std::move(error));
    }

    if (!wait_unless_stopped(stop, RETRY_DELAY))
      return _give_up("stopped");
    peers = find_peers(_tracker, _gid, _meta.name, _self);
  }
}

void Download::_fetch_round(const vector<Address> &peers,
                            std::stop_token stop) {
  _picker.forget_peers();
  std::atomic<size_t> next_peer = 0;

  // the worker that writes the last piece stops the others' stalled fetches
  std::stop_source finished;
  std::stop_callback on_quit(stop, [&] { finished.request_stop(); });
  std::stop_callback on_cancel(_cancel.get_token(),
                               [&] { finished.request_stop(); });

  vector<std::jthread> workers;
  for (size_t w = 0; w < std::min(MAX_WORKERS, peers.size()); ++w)
    workers.emplace_back([&] { _work(peers, next_peer, finished); });
}

void Download::_work(const vector<Address> &peers,
                     std::atomic<size_t> &next_peer,
                     std::stop_source &finished) {
  for (size_t p; (p = next_peer++) < peers.size();) {
    if (finished.stop_requested())
      return;

    if (auto fetched = _fetch_from(peers[p], finished); !fetched) {
      std::scoped_lock lock(_mutex);
      _error = std::format("{}: {}", peers[p], fetched.error());
    }
  }
}

std::expected<void, string> Download::_fetch_from(const Address &peer,
                                                  std::stop_source &finished) {
  auto socket = connect_to(peer);
  if (!socket)
    return std::unexpected(socket.error());
  if (auto set = socket->set_recv_timeout(PEER_TIMEOUT_S); !set)
    return set;

  Connection conn(std::move(*socket));

  std::stop_callback on_stop(finished.get_token(), [&] { conn.shutdown(); });

  auto res = conn.request({
      .cmd = Command::PieceMap,
      .args = {_gid, _meta.name},
  });
  if (!res)
    return std::unexpected(res.error());
  if (res->status != Status::Ok)
    return std::unexpected(string(status_text(res->status)));
  if (res->items.size() != 1 || res->items[0].size() != _meta.piece_count())
    return std::unexpected("invalid piece map");

  const string map = std::move(res->items[0]);
  _picker.add_peer(map);

  // pieces this peer sent corrupted, which it is not asked for again
  vector<bool> refused(map.size(), false);

  while (auto i = _picker.next(map, refused, finished.get_token())) {
    res = conn.request({
        .cmd = Command::GetPiece,
        .args = {_gid, _meta.name, std::to_string(*i)},
    });
    if (!res) {
      _picker.release(*i);
      return std::unexpected(res.error());
    }

    Bytes piece;
    if (res->status == Status::Ok && res->items.size() == 1)
      piece.assign_range(res->items[0]);
    if (res->status != Status::Ok || !piece_matches(_meta, *i, piece)) {
      refused[*i] = true;
      _picker.release(*i);
      continue;
    }

    if (auto written = _out.write_at(uint64_t{*i} * PIECE_SIZE, piece);
        !written) {
      _picker.release(*i);
      return std::unexpected(written.error());
    }

    if (_shares)
      _library.mark_piece(_gid, _meta.name, *i);

    uint32_t done;
    {
      std::scoped_lock lock(_mutex);
      done = ++_done;
    }
    if (_picker.done(*i))
      finished.request_stop();

    // the first report makes this user a seeder, so others can fetch its pieces
    if (_shares)
      _report(done);
  }

  return {};
}

void Download::_report(uint32_t pieces) {
  (void)_tracker.request({
      .cmd = Command::ReportPieces,
      .args = {_gid, _meta.name, std::to_string(pieces)},
  });
}

void Download::_finish() {
  // the whole-file hash also catches a failed disk write
  auto written = sha1_of_file(_path);
  if (!written)
    return _give_up(written.error());
  if (*written != _meta.sha1)
    return _give_up("the whole file does not match its hash");

  std::scoped_lock lock(_mutex);
  _state = DownloadState::Complete;
}

// runs after the workers stop, so no piece report can follow the stop_share
void Download::_give_up(string error) {
  if (_shares) {
    _library.remove(_gid, _meta.name);

    if (_done_count() > 0)
      (void)_tracker.request({
          .cmd = Command::StopShare,
          .args = {_gid, _meta.name},
      });
  }

  std::scoped_lock lock(_mutex);
  _state = DownloadState::Failed;
  _error = std::move(error);
}

void Download::cancel() {
  {
    std::scoped_lock lock(_mutex);
    _cancelled = true;
  }
  _cancel.request_stop();
}

uint32_t Download::_done_count() {
  std::scoped_lock lock(_mutex);
  return _done;
}

bool Download::_is_cancelled() {
  std::scoped_lock lock(_mutex);
  return _cancelled;
}
