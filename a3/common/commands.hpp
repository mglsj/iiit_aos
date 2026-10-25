#pragma once

#include "common/protocol.hpp"
#include "common/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

enum class Command : uint8_t {
  Ping,
  CreateUser,
  Login,
  Logout,
  CreateGroup,
  JoinGroup,
  LeaveGroup,
  ListGroups,
  ListRequests,
  AcceptRequest,
  UploadFile,
  ListFiles,
  DownloadFile,
  StopShare,
  PeerHello,
  SyncRequest,
  SyncEntries,
  PieceMap,
  GetPiece,
  ShowDownloads,
  ListSeeders,
  ReportPieces,
};

enum class Channel : uint8_t {
  User,
  Internal,
  Tracker,
  Peer,
  Local,
};

struct CommandSpec {
  Command cmd;
  Channel channel;
  std::string_view name = {};
  size_t argc = 0;
  size_t extra_args = 0;
  bool needs_login = false;
  std::string_view success = {};
};

std::span<const CommandSpec> all_specs();

const CommandSpec &spec_of(Command cmd)
    pre(std::to_underlying(cmd) < all_specs().size());

bool valid_args(std::span<const string> args, size_t max_len = MAX_ARG_LEN);
