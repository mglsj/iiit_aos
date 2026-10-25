#include "app.hpp"
#include "cli.hpp"
#include "common/address.hpp"
#include "common/console.hpp"
#include "common/logger.hpp"

#include <csignal>
#include <print>

using std::println;

int main(int argc, char *argv[]) {
  std::signal(SIGPIPE, SIG_IGN);

  const string progname = argv[0];

  if (argc != 3) {
    println(stderr, "Usage: {} <ip:port> <tracker_info.txt>", progname);
    return 1;
  }

  auto self = parse_address(argv[1]);
  if (!self) {
    println(stderr, "{}: {}", progname, self.error());
    return 1;
  }

  auto trackers = load_address_file(argv[2]);
  if (!trackers) {
    println(stderr, "{}: {}", progname, trackers.error());
    return 1;
  }

  Logger::init(std::format("{}", *self));
  Console console("> ");

  App app(std::move(*trackers), *self);
  if (auto started = app.start(); !started) {
    Logger::error("{}", started.error());
    return 1;
  }

  run_cli(app, console);
  app.stop();

  return 0;
}
