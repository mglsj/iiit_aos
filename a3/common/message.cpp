#include "common/message.hpp"

std::expected<Command, string> frame_command(std::span<const byte> frame) {
  Decoder d(frame);
  Command cmd = d.get_enum<Command>();

  if (!d.ok())
    return std::unexpected("unknown command byte");
  return cmd;
}

Bytes Request::encode() const { return begin_frame(cmd).put_strs(args).take(); }

std::expected<Request, string> Request::decode(std::span<const byte> frame) {
  Decoder d(frame);
  Request req{
      .cmd = d.get_enum<Command>(),
      .args = d.get_strs(),
  };

  if (!d.finished())
    return std::unexpected("malformed request");
  return req;
}

Bytes Response::encode() const {
  Encoder e;
  e.put_u8(std::to_underlying(status)).put_strs(items);
  return e.take();
}

std::expected<Response, string> Response::decode(std::span<const byte> frame) {
  Decoder d(frame);
  Response res{
      .status = d.get_enum<Status>(),
      .items = d.get_strs(),
  };

  if (!d.finished())
    return std::unexpected("malformed response");
  return res;
}
