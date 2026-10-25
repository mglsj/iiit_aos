#include "client_handler.hpp"
#include "common/text.hpp"

void ClientHandler::run(Connection &conn, const Bytes &first) {
  std::optional<Session> session;

  std::expected<Bytes, string> frame = first;
  while (frame) {
    if (!conn.send(_handle(*frame, session).encode()))
      break;
    frame = conn.recv();
  }

  // a disconnect counts as a logout
  _end_session(session);
  _replicator.push();
}

Response ClientHandler::_handle(const Bytes &frame,
                                std::optional<Session> &session) {
  auto req = Request::decode(frame);
  if (!req)
    return {.status = Status::BadRequest};

  const CommandSpec &spec = spec_of(req->cmd);
  std::span<const string> args = req->args;

  // client-added args such as piece hashes may exceed MAX_ARG_LEN
  bool for_tracker =
      spec.channel == Channel::User || spec.channel == Channel::Internal;
  if (!for_tracker || args.size() != spec.argc + spec.extra_args ||
      !valid_args(args.first(spec.argc)) ||
      !valid_args(args.subspan(spec.argc), MAX_FRAME_LEN))
    return {.status = Status::BadRequest};

  if (spec.needs_login && !session)
    return {.status = Status::NotLoggedIn};

  Response res = _dispatch(*req, session);

  // pushed before the reply, so the entry leaves before the client hears back
  _replicator.push();

  return res;
}

void ClientHandler::_end_session(std::optional<Session> &session) {
  if (session)
    _state.logout(session->user, session->addr);
  session.reset();
}

Response ClientHandler::_dispatch(const Request &req,
                                  std::optional<Session> &session) {
  using enum Command;

  const vector<string> &args = req.args;

  switch (req.cmd) {
  case Ping:
    return {.status = Status::Ok};

  case CreateUser:
    return {.status = _state.create_user(args[0], args[1])};

  case Login: {
    if (args[2].size() > MAX_ARG_LEN || !parse_address(args[2]))
      return {.status = Status::BadRequest};

    _end_session(session);

    Status s = _state.login(args[0], args[1], args[2]);
    if (s == Status::Ok)
      session = Session{.user = args[0], .addr = args[2]};
    return {.status = s};
  }

  case Logout:
    _end_session(session);
    return {.status = Status::Ok};

  case CreateGroup:
    return {.status = _state.create_group(args[0], session->user)};

  case JoinGroup:
    return {.status = _state.request_join(args[0], session->user)};

  case LeaveGroup:
    return {.status = _state.leave_group(args[0], session->user)};

  case ListGroups:
    return _state.list_groups();

  case ListRequests:
    return _state.list_requests(args[0], session->user);

  case AcceptRequest:
    return {.status = _state.accept_request(args[0], session->user, args[1])};

  case UploadFile: {
    auto meta = FileMeta::from_args(std::span(args).subspan(1));
    if (!meta)
      return {.status = Status::BadRequest};
    return {.status = _state.upload_file(args[0], session->user, *meta)};
  }

  case ListFiles:
    return _state.list_files(args[0], session->user);

  case DownloadFile:
    return _state.find_file(args[0], args[1], session->user);

  case StopShare:
    return {.status = _state.stop_share(args[0], args[1], session->user)};

  case ListSeeders:
    return _state.list_seeders(args[0], args[1], session->user);

  case ReportPieces: {
    auto count = parse_u64(args[2]);
    if (!count)
      return {.status = Status::BadRequest};
    return {.status =
                _state.report_pieces(args[0], args[1], session->user, *count)};
  }

  case PeerHello:
  case SyncRequest:
  case SyncEntries:
  case PieceMap:
  case GetPiece:
  case ShowDownloads:
    break;
  }

  return {.status = Status::BadRequest};
}
