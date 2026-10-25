#pragma once

#include "common/file_meta.hpp"
#include "common/message.hpp"
#include "common/types.hpp"
#include "oplog.hpp"

#include <expected>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>

struct User {
  string password;
  string addr = {};
};

struct SharedFile {
  FileMeta meta;
  std::map<string, uint32_t> seeders = {};
};

struct Group {
  string owner;
  std::set<string> members;
  std::set<string> pending = {};
  std::map<string, SharedFile> files = {};

  void add_seeder(const string &uid, const FileMeta &meta);
  void hold_pieces(const string &uid, const string &name, uint32_t count);
  void remove_seeder(const string &uid, const string &name);
  void remove_seeder_everywhere(const string &uid);
};

class TrackerState {
public:
  explicit TrackerState(OperationLog &log) : _log(log) {}

  bool ingest(const vector<LogEntry> &entries);

  [[nodiscard]] Status create_user(const string &uid, const string &password);

  [[nodiscard]] Status login(const string &uid, const string &password,
                             const string &addr);
  void logout(const string &uid, const string &addr);

  [[nodiscard]] Status create_group(const string &gid, const string &owner);
  [[nodiscard]] Status request_join(const string &gid, const string &uid);
  [[nodiscard]] Status accept_request(const string &gid, const string &owner,
                                      const string &uid);
  [[nodiscard]] Status leave_group(const string &gid, const string &uid);

  Response list_groups();
  Response list_requests(const string &gid, const string &owner);

  [[nodiscard]] Status upload_file(const string &gid, const string &uid,
                                   const FileMeta &meta);
  Response list_files(const string &gid, const string &uid);
  Response find_file(const string &gid, const string &name, const string &uid);
  [[nodiscard]] Status stop_share(const string &gid, const string &name,
                                  const string &uid);

  [[nodiscard]] Status report_pieces(const string &gid, const string &name,
                                     const string &uid, uint64_t count);

  Response list_seeders(const string &gid, const string &name,
                        const string &uid);

private:
  void _commit(vector<Change> changes);
  void _apply(const LogEntry &entry);

  void _replay();

  std::optional<Group &> _find_group(const string &gid);

  std::expected<Group *, Status> _find_group_for(const string &gid,
                                                 const string &uid);

  std::expected<Group *, Status> _find_owned_group(const string &gid,
                                                   const string &owner);

  std::expected<Group *, Status> _find_member_group(const string &gid,
                                                    const string &uid);

  std::expected<SharedFile *, Status>
  _find_shared_file(const string &gid, const string &name, const string &uid);

  struct OnlineSeeder {
    string uid;
    string addr;
    uint32_t pieces;
  };

  vector<OnlineSeeder> _online_seeders(const SharedFile &file);

  // guards _users, _groups and _last
  std::mutex _mutex;
  std::unordered_map<string, User> _users;
  std::unordered_map<string, Group> _groups;

  Stamp _last{};

  OperationLog &_log;
};
