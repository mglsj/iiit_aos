#pragma once

#include "common/codec.hpp"
#include "common/file_meta.hpp"
#include "common/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

enum class Op : uint8_t {
  UserCreated,
  GroupCreated,
  PendingAdded,
  MemberAdded,
  PendingRemoved,
  MemberRemoved,
  OwnerChanged,
  GroupDeleted,
  UserOnline,
  UserOffline,
  FileShared,
  SeederRemoved,
  PiecesHeld,
};

constexpr size_t op_argc(Op op) {
  using enum Op;

  switch (op) {
  case UserCreated:
  case GroupCreated:
  case PendingAdded:
  case MemberAdded:
  case PendingRemoved:
  case MemberRemoved:
  case OwnerChanged:
  case GroupDeleted:
  case UserOnline:
  case UserOffline:
    return 2;
  case FileShared:
    return 2 + FILE_META_ARGC;
  case SeederRemoved:
    return 3;
  case PiecesHeld:
    return 4;
  }
  return 0;
}

struct Change {
  Op op;
  vector<string> args;
};

struct Origin {
  uint32_t tracker_no;
  uint64_t incarnation;

  auto operator<=>(const Origin &) const = default;
};

struct Stamp {
  uint64_t timestamp;
  uint32_t tracker_no;
  uint64_t incarnation;
  uint64_t seq;

  auto operator<=>(const Stamp &) const = default;
};

using VersionVector = std::map<Origin, uint64_t>;

struct LogEntry {
  uint64_t seq;
  uint64_t timestamp;
  Origin origin;
  vector<Change> changes;

  Stamp stamp() const {
    return {timestamp, origin.tracker_no, origin.incarnation, seq};
  }
};

enum class Verdict { New, Duplicate, Gap };

class OperationLog {
public:
  explicit OperationLog(uint32_t tracker_no);

  LogEntry append(vector<Change> changes, uint64_t after);

  Verdict accept(const LogEntry &e);

  vector<LogEntry> missing_for(const VersionVector &vv) const;
  vector<LogEntry> own_since(uint64_t seq) const;
  uint64_t last_own_seq() const;
  VersionVector version_vector() const;

  vector<LogEntry> sorted() const;

private:
  // guards the entries, the version vector and the next se
  mutable std::mutex _mutex;
  Origin _origin;
  VersionVector _seen;
  vector<LogEntry> _entries;
  uint64_t _next_seq = 1;
};

constexpr size_t MIN_ENTRY_SIZE = 8 + 8 + 4 + 8 + 4;

string describe(const LogEntry &entry);

void put_entry(Encoder &e, const LogEntry &entry);
LogEntry get_entry(Decoder &d);

void put_version_vector(Encoder &e, const VersionVector &vv);
VersionVector get_version_vector(Decoder &d);
