#include "state.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <ranges>

static bool same_content(const FileMeta &a, const FileMeta &b) {
  return a.size == b.size && a.sha1 == b.sha1;
}

void Group::add_seeder(const string &uid, const FileMeta &meta) {
  auto it = files.find(meta.name);

  if (it == files.end())
    it = files.emplace(meta.name, SharedFile{.meta = meta}).first;
  // after a partition, the first file in stamp order keeps the name
  else if (!same_content(it->second.meta, meta))
    return;

  it->second.seeders[uid] = meta.piece_count();
}

void Group::hold_pieces(const string &uid, const string &name, uint32_t count) {
  auto it = files.find(name);
  if (it == files.end())
    return;

  // the larger count wins, so reports arriving in any order agree
  uint32_t &held = it->second.seeders[uid];
  held = std::max(held, std::min(count, it->second.meta.piece_count()));
}

void Group::remove_seeder(const string &uid, const string &name) {
  auto it = files.find(name);
  if (it == files.end())
    return;

  it->second.seeders.erase(uid);
  if (it->second.seeders.empty())
    files.erase(it);
}

void Group::remove_seeder_everywhere(const string &uid) {
  for (auto it = files.begin(); it != files.end();) {
    it->second.seeders.erase(uid);
    it = it->second.seeders.empty() ? files.erase(it) : std::next(it);
  }
}

Status TrackerState::upload_file(const string &gid, const string &uid,
                                 const FileMeta &meta) {
  std::scoped_lock lock(_mutex);

  auto group = _find_member_group(gid, uid);
  if (!group)
    return group.error();

  if (auto it = (*group)->files.find(meta.name); it != (*group)->files.end()) {
    if (!same_content(it->second.meta, meta))
      return Status::FileConflict;
    if (auto held = it->second.seeders.find(uid);
        held != it->second.seeders.end() && held->second == meta.piece_count())
      return Status::Ok;
  }

  vector<string> args = {gid, uid};
  args.append_range(meta.to_args());

  _commit({{Op::FileShared, std::move(args)}});
  return Status::Ok;
}

Response TrackerState::list_files(const string &gid, const string &uid) {
  std::scoped_lock lock(_mutex);

  auto group = _find_member_group(gid, uid);
  if (!group)
    return {.status = group.error()};

  return {
      .status = Status::Ok,
      .items = (*group)->files | std::views::keys | std::ranges::to<vector>(),
  };
}

Response TrackerState::find_file(const string &gid, const string &name,
                                 const string &uid) {
  std::scoped_lock lock(_mutex);

  auto file = _find_shared_file(gid, name, uid);
  if (!file)
    return {.status = file.error()};

  return {.status = Status::Ok, .items = (*file)->meta.to_args()};
}

Status TrackerState::stop_share(const string &gid, const string &name,
                                const string &uid) {
  std::scoped_lock lock(_mutex);

  auto file = _find_shared_file(gid, name, uid);
  if (!file)
    return file.error();

  if (!(*file)->seeders.contains(uid))
    return Status::NotSharing;

  _commit({{Op::SeederRemoved, {gid, uid, name}}});
  return Status::Ok;
}

Status TrackerState::report_pieces(const string &gid, const string &name,
                                   const string &uid, uint64_t count) {
  std::scoped_lock lock(_mutex);

  auto file = _find_shared_file(gid, name, uid);
  if (!file)
    return file.error();

  if (count == 0 || count > (*file)->meta.piece_count())
    return Status::BadRequest;

  if (auto held = (*file)->seeders.find(uid);
      held != (*file)->seeders.end() && held->second >= count)
    return Status::Ok;

  _commit({{Op::PiecesHeld, {gid, uid, name, std::to_string(count)}}});
  return Status::Ok;
}

Response TrackerState::list_seeders(const string &gid, const string &name,
                                    const string &uid) {
  std::scoped_lock lock(_mutex);

  auto file = _find_shared_file(gid, name, uid);
  if (!file)
    return {.status = file.error()};

  Response res{
      .status = Status::Ok,
      .items = {std::to_string((*file)->meta.piece_count())},
  };
  for (const OnlineSeeder &seeder : _online_seeders(**file)) {
    res.items.push_back(seeder.uid);
    res.items.push_back(seeder.addr);
    res.items.push_back(std::to_string(seeder.pieces));
  }
  return res;
}

vector<TrackerState::OnlineSeeder>
TrackerState::_online_seeders(const SharedFile &file) {
  vector<OnlineSeeder> online;
  for (const auto &[uid, pieces] : file.seeders) {
    auto user = _users.find(uid);
    if (user != _users.end() && !user->second.addr.empty() && pieces > 0)
      online.push_back(
          {.uid = uid, .addr = user->second.addr, .pieces = pieces});
  }

  // stable, so equal counts keep user id order
  std::ranges::stable_sort(online, std::greater{}, &OnlineSeeder::pieces);
  return online;
}
