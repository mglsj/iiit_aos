#include "peer_server.hpp"
#include "common/commands.hpp"
#include "common/text.hpp"

static Response handle(const Library &library, const Bytes &frame) {
  auto req = Request::decode(frame);
  if (!req)
    return {.status = Status::BadRequest};

  const CommandSpec &spec = spec_of(req->cmd);
  const vector<string> &args = req->args;
  if (spec.channel != Channel::Peer || args.size() != spec.argc ||
      !valid_args(args))
    return {.status = Status::BadRequest};

  switch (req->cmd) {
  case Command::PieceMap: {
    auto map = library.piece_map(args[0], args[1]);
    if (!map)
      return {.status = Status::FileNotFound};
    return {.status = Status::Ok, .items = {std::move(*map)}};
  }

  case Command::GetPiece: {
    auto index = parse_u64(args[2]);
    if (!index)
      return {.status = Status::BadRequest};

    auto piece = library.read_piece(args[0], args[1], *index);
    if (!piece)
      return {.status = piece.error()};
    return {.status = Status::Ok, .items = {string(std::from_range, *piece)}};
  }

  // only the two peer commands pass the channel check above
  default:
    return {.status = Status::BadRequest};
  }
}

void serve_peer(const Library &library, Connection &conn) {
  while (auto frame = conn.recv())
    if (!conn.send(handle(library, *frame).encode()))
      break;
}
