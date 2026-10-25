#include "common/socket.hpp"
#include "common/error.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

static std::expected<sockaddr_in, string> to_sockaddr(const Address &addr) {
  sockaddr_in sa = {};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(addr.port);

  if (::inet_pton(AF_INET, addr.host.c_str(), &sa.sin_addr) != 1)
    return std::unexpected("not an IPv4 address: " + addr.host);
  return sa;
}

static std::expected<Socket, string> new_socket() {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return errno_error("socket");
  return Socket(fd);
}

constexpr int SEND_TIMEOUT_S = 10;
constexpr int KEEPALIVE_IDLE_S = 10;
constexpr int KEEPALIVE_INTERVAL_S = 5;
constexpr int KEEPALIVE_COUNT = 3;

static void set_int_option(int fd, int level, int name, int value) {
  ::setsockopt(fd, level, name, &value, sizeof(value));
}

static void tune(const Socket &s) {
  timeval send_timeout{.tv_sec = SEND_TIMEOUT_S, .tv_usec = 0};
  ::setsockopt(s.fd(), SOL_SOCKET, SO_SNDTIMEO, &send_timeout,
               sizeof(send_timeout));

  // without it, Nagle's algorithm delays a pushed log entry by up to 40 ms
  set_int_option(s.fd(), IPPROTO_TCP, TCP_NODELAY, 1);

  set_int_option(s.fd(), SOL_SOCKET, SO_KEEPALIVE, 1);
  set_int_option(s.fd(), IPPROTO_TCP, TCP_KEEPIDLE, KEEPALIVE_IDLE_S);
  set_int_option(s.fd(), IPPROTO_TCP, TCP_KEEPINTVL, KEEPALIVE_INTERVAL_S);
  set_int_option(s.fd(), IPPROTO_TCP, TCP_KEEPCNT, KEEPALIVE_COUNT);
}

std::expected<void, string> Socket::set_recv_timeout(int seconds) {
  timeval timeout{.tv_sec = seconds, .tv_usec = 0};
  if (::setsockopt(fd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) !=
      0)
    return errno_error("setsockopt");
  return {};
}

void Socket::shutdown() {
  if (fd() >= 0)
    ::shutdown(fd(), SHUT_RDWR);
}

std::expected<Socket, string> connect_to(const Address &addr) {
  auto sa = to_sockaddr(addr);
  if (!sa)
    return std::unexpected(sa.error());

  auto s = new_socket();
  if (!s)
    return s;

  // set before connect, because SO_SNDTIMEO also limits a blocking connect
  tune(*s);
  auto *raw = reinterpret_cast<sockaddr *>(&*sa);
  if (::connect(s->fd(), raw, sizeof(*sa)) != 0) {
    // a timed-out connect leaves errno as EINPROGRESS
    if (errno == EINPROGRESS)
      return std::unexpected("connect: timed out");
    return errno_error("connect");
  }

  return s;
}

std::expected<Socket, string> accept_from(const Socket &listener) {
  while (true) {
    int fd = ::accept(listener.fd(), nullptr, nullptr);

    if (fd >= 0) {
      Socket s(fd);
      tune(s);
      return s;
    }

    // ECONNABORTED means a client gave up while it waited in the backlog
    if (errno != EINTR && errno != ECONNABORTED)
      return errno_error("accept");
  }
}

std::expected<Socket, string> listen_on(const Address &addr) {
  auto sa = to_sockaddr(addr);
  if (!sa)
    return std::unexpected(sa.error());

  auto s = new_socket();
  if (!s)
    return s;

  int yes = 1;
  if (::setsockopt(s->fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
    return errno_error("setsockopt");

  auto *raw = reinterpret_cast<sockaddr *>(&*sa);
  if (::bind(s->fd(), raw, sizeof(*sa)) != 0)
    return errno_error("bind");

  if (::listen(s->fd(), 16) != 0)
    return errno_error("listen");

  return s;
}
