#include "replicator.hpp"
#include "common/logger.hpp"
#include "common/message.hpp"
#include "common/protocol.hpp"

#include <algorithm>
#include <generator>
#include <iterator>
#include <ranges>

using namespace std::chrono_literals;

constexpr auto RECONNECT_DELAY = 1s;

static Bytes hello_frame(uint32_t self) {
  return begin_frame(Command::PeerHello).put_u32(self).take();
}

static Bytes sync_request_frame(const VersionVector &vv) {
  Encoder e = begin_frame(Command::SyncRequest);
  put_version_vector(e, vv);
  return e.take();
}

static Bytes entries_frame(uint32_t count, const Encoder &entries) {
  return begin_frame(Command::SyncEntries)
      .put_u32(count)
      .put_raw(entries.bytes())
      .take();
}

// splits the entries into frames under MAX_FRAME_LEN, built one at a time
static std::generator<Bytes> entry_frames(const vector<LogEntry> &entries) {
  constexpr size_t HEADER_SIZE = 1 + 4;

  Encoder batch;
  uint32_t count = 0;

  for (const auto &entry : entries) {
    Encoder one;
    put_entry(one, entry);

    if (count > 0 && HEADER_SIZE + batch.bytes().size() + one.bytes().size() >
                         MAX_FRAME_LEN) {
      co_yield entries_frame(count, batch);
      batch = Encoder();
      count = 0;
    }

    batch.put_raw(one.bytes());
    ++count;
  }

  if (count > 0)
    co_yield entries_frame(count, batch);
}

static std::expected<void, string>
send_entries(Connection &conn, const vector<LogEntry> &entries) {
  for (const auto &frame : entry_frames(entries))
    if (auto sent = conn.send(frame); !sent)
      return sent;
  return {};
}

Replicator::Replicator(TrackerState &state, OperationLog &log,
                       const vector<Address> &trackers, uint32_t self)
    : _state(state), _log(log), _trackers(trackers), _self(self) {
  std::ranges::generate_n(std::back_inserter(_links), std::ssize(trackers),
                          [] { return std::make_unique<PeerLink>(); });
}

void Replicator::start(std::stop_token stop) {
  for (uint32_t peer = 0; peer < _trackers.size(); ++peer) {
    if (peer != _self)
      _threads.emplace_back(&Replicator::_sync_loop, this, stop, peer);
  }
}

void Replicator::join() { _threads.clear(); }

void Replicator::push() {
  uint64_t last = _log.last_own_seq();

  for (auto &link_ptr : _links) {
    PeerLink &link = *link_ptr;
    std::scoped_lock lock(link.mutex);

    // most commands log nothing, so this skips the log scan
    if (!link.conn || link.pushed >= last)
      continue;

    vector<LogEntry> entries = _log.own_since(link.pushed);
    if (entries.empty())
      continue;

    if (send_entries(*link.conn, entries))
      link.pushed = entries.back().seq;
    else
      // the sync thread sees the failure and reconnects
      link.conn->shutdown();
  }
}

void Replicator::serve_inbound(Connection &conn, std::span<const byte> hello) {
  Decoder d(hello);
  auto _ = d.get_enum<Command>();
  uint32_t peer = d.get_u32();

  if (!d.finished() || peer >= _links.size() || peer == _self)
    return;

  // the peer is back, so wake the sync thread for it now
  {
    PeerLink &link = *_links[peer];
    std::scoped_lock lock(link.mutex);
    link.wake_requested = true;
    link.wake.notify_all();
  }

  if (conn.send(sync_request_frame(_log.version_vector())))
    _receive(conn);
}

void Replicator::_sync_loop(std::stop_token stop, uint32_t peer) {
  PeerLink &link = *_links[peer];

  while (!stop.stop_requested()) {
    // a failed connect is normal while the peer is down, so it is not printed
    auto socket = connect_to(_trackers[peer]);

    if (socket) {
      Connection conn(std::move(*socket));

      std::stop_callback on_stop(stop, [&] { conn.shutdown(); });

      bool connected;
      {
        std::scoped_lock lock(link.mutex);
        connected = conn.send(hello_frame(_self)).has_value();

        // the peer's SyncRequest reply covers entries up to the cursor
        if (connected) {
          link.conn = &conn;
          link.pushed = _log.last_own_seq();
        }
      }

      if (connected) {
        Logger::info("connected to tracker {}", peer);
        _receive(conn);
        Logger::info("lost connection to tracker {}", peer);
      }

      std::scoped_lock lock(link.mutex);
      link.conn = nullptr;
    }

    _wait_before_reconnect(stop, link);
  }
}

void Replicator::_wait_before_reconnect(std::stop_token stop, PeerLink &link) {
  std::unique_lock lock(link.mutex);

  link.wake.wait_for(lock, stop, RECONNECT_DELAY,
                     [&] { return link.wake_requested; });
  link.wake_requested = false;
}

void Replicator::_receive(Connection &conn) {
  while (auto frame = conn.recv()) {
    Decoder d(*frame);
    Command cmd = d.get_enum<Command>();

    if (cmd == Command::SyncEntries) {
      vector<LogEntry> entries(d.get_count(MIN_ENTRY_SIZE));
      for (auto &entry : entries)
        entry = get_entry(d);

      if (!d.finished()) {
        Logger::warn("rejected malformed SyncEntries");
        continue;
      }

      bool gap = _state.ingest(entries);
      if (gap && !conn.send(sync_request_frame(_log.version_vector())))
        break;

    } else if (cmd == Command::SyncRequest) {
      VersionVector vv = get_version_vector(d);

      if (!d.finished()) {
        Logger::warn("rejected malformed SyncRequest");
        continue;
      }

      if (!send_entries(conn, _log.missing_for(vv)))
        break;

    } else {
      Logger::warn("unexpected message from peer tracker");
    }
  }
}
