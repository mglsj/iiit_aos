#include "common/connection.hpp"
#include "common/error.hpp"
#include "common/protocol.hpp"

#include <array>
#include <sys/socket.h>
#include <unistd.h>

constexpr size_t LEN_SIZE = 4;

static std::expected<void, string> read_n(int fd, std::span<byte> buf) {
  size_t got = 0;

  while (got < buf.size()) {
    ssize_t r = ::read(fd, buf.data() + got, buf.size() - got);

    if (r < 0 && errno == EINTR)
      continue;
    if (r < 0)
      return errno_error("read");
    if (r == 0)
      return std::unexpected(got == 0 ? "connection closed"
                                      : "connection closed mid-frame");

    got += static_cast<size_t>(r);
  }

  return {};
}

static std::expected<void, string> send_n(int fd, std::span<const byte> buf,
                                          int flags) {
  size_t sent = 0;

  while (sent < buf.size()) {
    // MSG_NOSIGNAL turns SIGPIPE on a closed connection into an EPIPE error
    ssize_t w =
        ::send(fd, buf.data() + sent, buf.size() - sent, flags | MSG_NOSIGNAL);

    if (w < 0 && errno == EINTR)
      continue;
    if (w < 0)
      return errno_error("send");

    sent += static_cast<size_t>(w);
  }

  return {};
}

std::expected<void, string> Connection::send(std::span<const byte> frame) {
  if (frame.size() > MAX_FRAME_LEN)
    return std::unexpected("frame too large to send");

  Encoder len;
  len.put_u32(static_cast<uint32_t>(frame.size()));

  // MSG_MORE holds the length back so it leaves in one segment with the body
  std::scoped_lock lock(_send_mutex);
  return send_n(_socket.fd(), len.bytes(), frame.empty() ? 0 : MSG_MORE)
      .and_then([&] { return send_n(_socket.fd(), frame, 0); });
}

std::expected<Bytes, string> Connection::recv() {
  std::array<byte, LEN_SIZE> len_bytes;
  if (auto read = read_n(_socket.fd(), len_bytes); !read)
    return std::unexpected(read.error());

  uint32_t len = Decoder(len_bytes).get_u32();
  // checked before allocating, so a forged length cannot allocate gigabytes
  if (len > MAX_FRAME_LEN)
    return std::unexpected("frame length over the limit");

  Bytes frame(len);
  if (auto read = read_n(_socket.fd(), frame); !read)
    return std::unexpected(read.error());

  return frame;
}

std::expected<Response, string> Connection::request(const Request &req) {
  return send(req.encode())
      .and_then([&] { return recv(); })
      .and_then(Response::decode);
}
