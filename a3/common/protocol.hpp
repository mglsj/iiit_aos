#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

constexpr uint32_t PIECE_SIZE = 512 * 1024;
constexpr uint32_t MAX_FRAME_LEN = PIECE_SIZE + 4096;
constexpr size_t MAX_ARG_LEN = 256;

enum class Status : uint8_t {
  Ok,
  GroupDeleted,
  NotLoggedIn,
  BadRequest,
  UserExists,
  UserNotFound,
  BadCredentials,
  GroupExists,
  GroupNotFound,
  NotOwner,
  NotMember,
  AlreadyMember,
  RequestPending,
  NoRequest,
  FileNotFound,
  FileConflict,
  NotSharing,
  PieceUnavailable,
};

constexpr std::string_view status_text(Status s) {
  using enum Status;

  switch (s) {
  case Ok:
    return "";
  case GroupDeleted:
    return "Left the group. It had no other members, so it was deleted";
  case NotLoggedIn:
    return "Not logged in";
  case BadRequest:
    return "Invalid request";
  case UserExists:
    return "User already exists";
  case UserNotFound:
    return "User not found";
  case BadCredentials:
    return "Incorrect password";
  case GroupExists:
    return "Group already exists";
  case GroupNotFound:
    return "Group not found";
  case NotOwner:
    return "Only the group owner can do this";
  case NotMember:
    return "User is not a member of the group";
  case AlreadyMember:
    return "User is already a member of the group";
  case RequestPending:
    return "Join request already pending";
  case NoRequest:
    return "User has not requested to join the group";
  case FileNotFound:
    return "File not found in the group";
  case FileConflict:
    return "A different file with this name is already shared in the group";
  case NotSharing:
    return "You are not sharing this file";
  case PieceUnavailable:
    return "The peer does not have this piece";
  }
  return "Unknown status";
}
