#include "common/commands.hpp"
#include "common/enums.hpp"
#include "common/file_meta.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace {

using enum Command;

constexpr CommandSpec SPECS[] = {
    {
        .cmd = Ping,
        .channel = Channel::User,
        .name = "ping",
        .success = "Pong",
    },
    {
        .cmd = CreateUser,
        .channel = Channel::User,
        .name = "create_user",
        .argc = 2,
        .success = "User created successfully",
    },
    {
        .cmd = Login,
        .channel = Channel::User,
        .name = "login",
        .argc = 2,
        .extra_args = 1,
        .success = "Login successful",
    },
    {
        .cmd = Logout,
        .channel = Channel::User,
        .name = "logout",
        .needs_login = true,
        .success = "Logged out",
    },
    {
        .cmd = CreateGroup,
        .channel = Channel::User,
        .name = "create_group",
        .argc = 1,
        .needs_login = true,
        .success = "Group created successfully",
    },
    {
        .cmd = JoinGroup,
        .channel = Channel::User,
        .name = "join_group",
        .argc = 1,
        .needs_login = true,
        .success = "Join request sent successfully",
    },
    {
        .cmd = LeaveGroup,
        .channel = Channel::User,
        .name = "leave_group",
        .argc = 1,
        .needs_login = true,
        .success = "User left the group successfully",
    },
    {
        .cmd = ListGroups,
        .channel = Channel::User,
        .name = "list_groups",
        .needs_login = true,
    },
    {
        .cmd = ListRequests,
        .channel = Channel::User,
        .name = "list_requests",
        .argc = 1,
        .needs_login = true,
    },
    {
        .cmd = AcceptRequest,
        .channel = Channel::User,
        .name = "accept_request",
        .argc = 2,
        .needs_login = true,
        .success = "User added to the group successfully",
    },
    {
        .cmd = UploadFile,
        .channel = Channel::User,
        .name = "upload_file",
        .argc = 2,
        // the client swaps the path for the name, size and hashes
        .extra_args = FILE_META_ARGC - 1,
        .needs_login = true,
        .success = "File uploaded successfully",
    },
    {
        .cmd = ListFiles,
        .channel = Channel::User,
        .name = "list_files",
        .argc = 1,
        .needs_login = true,
    },
    {
        // the tracker ignores the destination path, which only the client uses
        .cmd = DownloadFile,
        .channel = Channel::User,
        .name = "download_file",
        .argc = 3,
        .needs_login = true,
        .success = "Download started",
    },
    {
        .cmd = StopShare,
        .channel = Channel::User,
        .name = "stop_share",
        .argc = 2,
        .needs_login = true,
        .success = "Stopped sharing the file",
    },
    {.cmd = PeerHello, .channel = Channel::Tracker},
    {.cmd = SyncRequest, .channel = Channel::Tracker},
    {.cmd = SyncEntries, .channel = Channel::Tracker},
    {.cmd = PieceMap, .channel = Channel::Peer, .argc = 2},
    {.cmd = GetPiece, .channel = Channel::Peer, .argc = 3},
    {
        .cmd = ShowDownloads,
        .channel = Channel::Local,
        .name = "show_downloads",
    },
    {
        .cmd = ListSeeders,
        .channel = Channel::User,
        .name = "list_seeders",
        .argc = 2,
        .needs_login = true,
    },
    {
        .cmd = ReportPieces,
        .channel = Channel::Internal,
        .argc = 3,
        .needs_login = true,
    },
};

constexpr bool specs_match_enum() {
  if (std::size(SPECS) != enum_count<Command>())
    return false;

  for (auto [i, spec] : std::views::enumerate(SPECS))
    if (spec.cmd != static_cast<Command>(i))
      return false;

  return true;
}

static_assert(specs_match_enum(), "SPECS must list every Command in order");

bool is_control(byte c) { return c < 0x20 || c == 0x7f; }

} // namespace

const CommandSpec &spec_of(Command cmd) {
  return SPECS[std::to_underlying(cmd)];
}

std::span<const CommandSpec> all_specs() { return SPECS; }

bool valid_args(std::span<const string> args, size_t max_len) {
  return std::ranges::all_of(args, [&](std::string_view arg) {
    return !arg.empty() && arg.size() <= max_len &&
           std::ranges::none_of(arg, is_control);
  });
}
