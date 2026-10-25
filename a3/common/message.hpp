#pragma once

#include "common/codec.hpp"
#include "common/commands.hpp"
#include "common/protocol.hpp"
#include "common/types.hpp"

#include <expected>
#include <span>
#include <utility>

inline Encoder begin_frame(Command cmd) {
  Encoder e;
  e.put_u8(std::to_underlying(cmd));
  return e;
}

std::expected<Command, string> frame_command(std::span<const byte> frame);

struct Request {
  Command cmd;
  vector<string> args;

  Bytes encode() const;
  static std::expected<Request, string> decode(std::span<const byte> frame);
};

struct Response {
  Status status;
  vector<string> items = {};

  Bytes encode() const;
  static std::expected<Response, string> decode(std::span<const byte> frame);
};
