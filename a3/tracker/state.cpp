#include "state.hpp"
#include "common/logger.hpp"
#include "common/text.hpp"

#include <algorithm>
#include <ranges>

static void log_entry(std::string_view verb, const LogEntry &entry) {
  bool pieces_only = std::ranges::all_of(
      entry.changes, [](const Change &c) { return c.op == Op::PiecesHeld; });

  // a download adds a PiecesHeld entry per piece, too many for the default
  // level
  if (pieces_only)
    Logger::debug("{} {}", verb, describe(entry));
  else
    Logger::info("{} {}", verb, describe(entry));
}

bool TrackerState::ingest(const vector<LogEntry> &entries) {
  std::scoped_lock lock(_mutex);

  bool gap = false;
  bool out_of_order = false;

  for (const auto &entry : entries) {
    Verdict v = _log.accept(entry);

    if (v == Verdict::New)
      log_entry("received", entry);
    else
      Logger::debug("dropped {}, verdict {}", describe(entry), enum_name(v));

    if (v == Verdict::Gap)
      gap = true;
    else if (v == Verdict::New && entry.stamp() > _last)
      _apply(entry);
    else if (v == Verdict::New)
      out_of_order = true;
  }

  // an entry older than the last applied one forces a replay in stamp order
  if (out_of_order)
    _replay();

  return gap;
}

void TrackerState::_commit(vector<Change> changes) {
  LogEntry entry = _log.append(std::move(changes), _last.timestamp);
  log_entry("committed", entry);
  _apply(entry);
}

void TrackerState::_replay() {
  vector<LogEntry> entries = _log.sorted();
  Logger::info("replaying {} entries in stamp order", entries.size());

  _users.clear();
  _groups.clear();
  _last = {};

  for (const auto &entry : entries)
    _apply(entry);
}

// skips a change to a user or group that an earlier entry may have deleted
void TrackerState::_apply(const LogEntry &entry) {
  using enum Op;

  for (const auto &change : entry.changes) {
    const string &a = change.args[0];
    const string &b = change.args[1];

    switch (change.op) {
    case UserCreated:
      // the first creation in stamp order wins
      _users.try_emplace(a, User{.password = b});
      break;
    case GroupCreated:
      _groups.try_emplace(a, Group{.owner = b, .members = {b}});
      break;
    case PendingAdded:
      if (auto group = _find_group(a))
        group->pending.insert(b);
      break;
    case PendingRemoved:
      if (auto group = _find_group(a))
        group->pending.erase(b);
      break;
    case MemberAdded:
      if (auto group = _find_group(a))
        group->members.insert(b);
      break;
    case MemberRemoved:
      if (auto group = _find_group(a)) {
        group->members.erase(b);
        group->remove_seeder_everywhere(b);
      }
      break;
    case OwnerChanged:
      if (auto group = _find_group(a))
        group->owner = b;
      break;
    case GroupDeleted:
      _groups.erase(a);
      break;
    case UserOnline:
      if (auto it = _users.find(a); it != _users.end())
        it->second.addr = b;
      break;
    case UserOffline:
      // a login from another address since then keeps the user online
      if (auto it = _users.find(a);
          it != _users.end() && it->second.addr == b) {
        it->second.addr.clear();
        for (auto &[_, group] : _groups)
          group.remove_seeder_everywhere(a);
      }
      break;
    case FileShared: {
      auto group = _find_group(a);
      auto meta = FileMeta::from_args(std::span(change.args).subspan(2));
      // skipped if the uploader left the group earlier in stamp order
      if (group && meta && group->members.contains(b))
        group->add_seeder(b, *meta);
      break;
    }
    case SeederRemoved:
      if (auto group = _find_group(a))
        group->remove_seeder(b, change.args[2]);
      break;
    case PiecesHeld: {
      auto group = _find_group(a);
      auto count = parse_u64(change.args[3]);
      if (group && count && group->members.contains(b))
        group->hold_pieces(b, change.args[2], static_cast<uint32_t>(*count));
      break;
    }
    }
  }

  _last = entry.stamp();
}

std::optional<Group &> TrackerState::_find_group(const string &gid) {
  auto it = _groups.find(gid);
  if (it == _groups.end())
    return std::nullopt;
  return it->second;
}

std::expected<Group *, Status>
TrackerState::_find_group_for(const string &gid, const string &uid) {
  if (!_users.contains(uid))
    return std::unexpected(Status::UserNotFound);

  auto group = _find_group(gid);
  if (!group)
    return std::unexpected(Status::GroupNotFound);

  return &*group;
}

std::expected<Group *, Status>
TrackerState::_find_owned_group(const string &gid, const string &owner) {
  return _find_group_for(gid, owner)
      .and_then([&](Group *group) -> std::expected<Group *, Status> {
        if (group->owner != owner)
          return std::unexpected(Status::NotOwner);
        return group;
      });
}

std::expected<Group *, Status>
TrackerState::_find_member_group(const string &gid, const string &uid) {
  return _find_group_for(gid, uid).and_then(
      [&](Group *group) -> std::expected<Group *, Status> {
        if (!group->members.contains(uid))
          return std::unexpected(Status::NotMember);
        return group;
      });
}

std::expected<SharedFile *, Status>
TrackerState::_find_shared_file(const string &gid, const string &name,
                                const string &uid) {
  return _find_member_group(gid, uid).and_then(
      [&](Group *group) -> std::expected<SharedFile *, Status> {
        auto it = group->files.find(name);
        if (it == group->files.end())
          return std::unexpected(Status::FileNotFound);
        return &it->second;
      });
}

Status TrackerState::create_user(const string &uid, const string &password) {
  std::scoped_lock lock(_mutex);

  if (_users.contains(uid))
    return Status::UserExists;

  _commit({{Op::UserCreated, {uid, password}}});
  return Status::Ok;
}

Status TrackerState::login(const string &uid, const string &password,
                           const string &addr) {
  std::scoped_lock lock(_mutex);

  auto it = _users.find(uid);
  if (it == _users.end())
    return Status::UserNotFound;

  if (it->second.password != password)
    return Status::BadCredentials;

  _commit({{Op::UserOnline, {uid, addr}}});
  return Status::Ok;
}

void TrackerState::logout(const string &uid, const string &addr) {
  std::scoped_lock lock(_mutex);

  auto it = _users.find(uid);
  if (it != _users.end() && it->second.addr == addr)
    _commit({{Op::UserOffline, {uid, addr}}});
}

Status TrackerState::create_group(const string &gid, const string &owner) {
  std::scoped_lock lock(_mutex);

  if (_groups.contains(gid))
    return Status::GroupExists;

  _commit({{Op::GroupCreated, {gid, owner}}});
  return Status::Ok;
}

Status TrackerState::request_join(const string &gid, const string &uid) {
  std::scoped_lock lock(_mutex);

  auto found = _find_group_for(gid, uid);
  if (!found)
    return found.error();
  Group *group = *found;

  if (group->members.contains(uid))
    return Status::AlreadyMember;

  if (group->pending.contains(uid))
    return Status::RequestPending;

  _commit({{Op::PendingAdded, {gid, uid}}});
  return Status::Ok;
}

Status TrackerState::accept_request(const string &gid, const string &owner,
                                    const string &uid) {
  std::scoped_lock lock(_mutex);

  auto owned = _find_owned_group(gid, owner);
  if (!owned)
    return owned.error();
  Group *group = *owned;

  if (!_users.contains(uid))
    return Status::UserNotFound;

  if (group->members.contains(uid))
    return Status::AlreadyMember;

  if (!group->pending.contains(uid))
    return Status::NoRequest;

  _commit({
      {Op::PendingRemoved, {gid, uid}},
      {Op::MemberAdded, {gid, uid}},
  });
  return Status::Ok;
}

Status TrackerState::leave_group(const string &gid, const string &uid) {
  std::scoped_lock lock(_mutex);

  auto found = _find_member_group(gid, uid);
  if (!found)
    return found.error();
  Group *group = *found;

  vector<Change> changes = {{Op::MemberRemoved, {gid, uid}}};
  Status status = Status::Ok;

  if (group->owner == uid) {
    // members is sorted, so both trackers pick the same new owner
    auto next = std::ranges::find_if(
        group->members, [&](const string &member) { return member != uid; });

    if (next == group->members.end()) {
      changes.push_back({Op::GroupDeleted, {gid, uid}});
      status = Status::GroupDeleted;
    } else {
      changes.push_back({Op::OwnerChanged, {gid, *next}});
    }
  }

  _commit(std::move(changes));
  return status;
}

Response TrackerState::list_groups() {
  std::scoped_lock lock(_mutex);

  return {
      .status = Status::Ok,
      .items = _groups | std::views::keys | std::ranges::to<vector>(),
  };
}

Response TrackerState::list_requests(const string &gid, const string &owner) {
  std::scoped_lock lock(_mutex);

  auto owned = _find_owned_group(gid, owner);
  if (!owned)
    return {.status = owned.error()};

  return {
      .status = Status::Ok,
      .items = std::ranges::to<vector>((*owned)->pending),
  };
}
