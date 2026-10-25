#include "app.hpp"
#include "common/logger.hpp"
#include "file_hash.hpp"
#include "peer_server.hpp"

App::App(vector<Address> trackers, const Address &self)
    : _self(self), _tracker(std::move(trackers), self),
      _peers([this](Connection &conn) { serve_peer(_library, conn); }) {
  _tracker.set_announcements([this] { return _library.announcements(); });
}

std::expected<void, string> App::start() {
  return _tracker.connect()
      .and_then([this] { return _peers.listen(_self); })
      .transform([this] {
        _accept = std::jthread([this] {
          if (auto ran = _peers.run(_stop.get_token()); !ran)
            Logger::error("peer server stopped: {}", ran.error());
        });
      });
}

void App::stop() {
  _stop.request_stop();
  if (_accept.joinable())
    _accept.join();
  _peers.wait();
  _downloads.join();
}

std::expected<Response, string> App::execute(Request req) {
  if (req.cmd == Command::UploadFile)
    return _upload(std::move(req));
  if (req.cmd == Command::DownloadFile)
    return _download(std::move(req));
  if (req.cmd == Command::ShowDownloads)
    return Response{.status = Status::Ok, .items = _downloads.status_lines()};

  // stops serving first, so the file is gone here whatever the tracker answers
  if (req.cmd == Command::StopShare) {
    _downloads.cancel(req.args[0], req.args[1]);
    _library.remove(req.args[0], req.args[1]);
  }

  return _tracker.request(std::move(req));
}

std::expected<Response, string> App::_upload(Request req) {
  string gid = req.args[0];
  string path = req.args[1];

  auto meta = hash_file(path);
  if (!meta)
    return std::unexpected(meta.error());

  req.args = {gid};
  req.args.append_range(meta->to_args());

  auto res = _tracker.request(std::move(req));
  if (res && res->status == Status::Ok)
    _library.add(gid, path, *meta, true);

  return res;
}

std::expected<Response, string> App::_download(Request req) {
  string gid = req.args[0];
  string dest = req.args[2];

  auto res = _tracker.request(std::move(req));
  if (!res || res->status != Status::Ok)
    return res;

  auto meta = FileMeta::from_args(res->items);
  if (!meta)
    return std::unexpected("Invalid response from the tracker: " +
                           meta.error());

  if (auto started = _downloads.start(gid, std::move(*meta), std::move(dest),
                                      _stop.get_token());
      !started)
    return std::unexpected(started.error());

  return Response{.status = Status::Ok};
}
