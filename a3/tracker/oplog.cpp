#include "oplog.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <ranges>
#include <utility>

static uint64_t now_millis() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

OperationLog::OperationLog(uint32_t tracker_no)
    : _origin{.tracker_no = tracker_no, .incarnation = now_millis()} {}

LogEntry OperationLog::append(vector<Change> changes, uint64_t after) {
  std::scoped_lock lock(_mutex);

  uint64_t seq = _next_seq++;

  LogEntry e{
      .seq = seq,
      .timestamp = std::max(now_millis(), after + 1),
      .origin = _origin,
      .changes = std::move(changes),
  };

  _seen[_origin] = seq;
  _entries.push_back(e);
  return e;
}

Verdict OperationLog::accept(const LogEntry &e) {
  std::scoped_lock lock(_mutex);

  uint64_t &seen = _seen[e.origin];

  if (e.seq <= seen)
    return Verdict::Duplicate;
  if (e.seq != seen + 1)
    return Verdict::Gap;

  seen = e.seq;
  _entries.push_back(e);
  return Verdict::New;
}

vector<LogEntry> OperationLog::missing_for(const VersionVector &vv) const {
  std::scoped_lock lock(_mutex);

  auto missing = [&](const LogEntry &entry) {
    auto it = vv.find(entry.origin);
    uint64_t has = (it == vv.end()) ? 0 : it->second;
    return entry.seq > has;
  };

  return _entries | std::views::filter(missing) | std::ranges::to<vector>();
}

vector<LogEntry> OperationLog::own_since(uint64_t seq) const {
  std::scoped_lock lock(_mutex);

  auto own_and_newer = [&](const LogEntry &entry) {
    return entry.origin == _origin && entry.seq > seq;
  };

  return _entries | std::views::filter(own_and_newer) |
         std::ranges::to<vector>();
}

uint64_t OperationLog::last_own_seq() const {
  std::scoped_lock lock(_mutex);

  return _next_seq - 1;
}

VersionVector OperationLog::version_vector() const {
  std::scoped_lock lock(_mutex);

  return _seen;
}

vector<LogEntry> OperationLog::sorted() const {
  std::scoped_lock lock(_mutex);

  vector<LogEntry> entries = _entries;
  std::ranges::sort(entries, {}, &LogEntry::stamp);
  return entries;
}

static string describe(const Change &change) {
  std::span<const string> shown = change.args;
  if (change.op == Op::UserCreated)
    shown = shown.first(1);
  if (change.op == Op::FileShared)
    shown = shown.first(4);

  string text(enum_name(change.op));
  for (const string &arg : shown)
    text += " " + arg;
  return text;
}

string describe(const LogEntry &entry) {
  string text = std::format("entry {}.{}:", entry.origin.tracker_no, entry.seq);
  for (auto [i, change] : std::views::enumerate(entry.changes))
    text += (i == 0 ? " " : ", ") + describe(change);
  return text;
}

void put_entry(Encoder &e, const LogEntry &entry) {
  e.put_u64(entry.seq)
      .put_u64(entry.timestamp)
      .put_u32(entry.origin.tracker_no)
      .put_u64(entry.origin.incarnation)
      .put_u32(static_cast<uint32_t>(entry.changes.size()));

  for (const auto &change : entry.changes)
    e.put_u8(std::to_underlying(change.op)).put_strs(change.args);
}

LogEntry get_entry(Decoder &d) {
  LogEntry entry;
  entry.seq = d.get_u64();
  entry.timestamp = d.get_u64();
  entry.origin.tracker_no = d.get_u32();
  entry.origin.incarnation = d.get_u64();

  uint32_t n = d.get_count(1 + 4);
  for (uint32_t i = 0; i < n; ++i) {
    Change change;
    change.op = d.get_enum<Op>();
    change.args = d.get_strs();

    if (change.args.size() != op_argc(change.op))
      d.fail();

    entry.changes.push_back(std::move(change));
  }

  return entry;
}

void put_version_vector(Encoder &e, const VersionVector &vv) {
  e.put_u32(static_cast<uint32_t>(vv.size()));

  for (const auto &[origin, seq] : vv)
    e.put_u32(origin.tracker_no).put_u64(origin.incarnation).put_u64(seq);
}

VersionVector get_version_vector(Decoder &d) {
  VersionVector vv;

  uint32_t n = d.get_count(4 + 8 + 8);
  for (uint32_t i = 0; i < n; ++i) {
    Origin origin;
    origin.tracker_no = d.get_u32();
    origin.incarnation = d.get_u64();
    vv[origin] = d.get_u64();
  }

  return vv;
}
