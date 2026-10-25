#include "common/address.hpp"
#include "common/console.hpp"
#include "common/logger.hpp"
#include "common/text.hpp"
#include "console.hpp"
#include "server.hpp"

#include <csignal>
#include <expected>
#include <format>
#include <print>
#include <string_view>
#include <thread>
#include <unistd.h>

using std::println;

static std::expected<uint32_t, string> parse_tracker_no(std::string_view text,
                                                        size_t count) {
  auto value = parse_u64(text);
  if (!value || *value >= count)
    return std::unexpected(
        std::format("tracker number must be between 0 and {}", count - 1));
  return static_cast<uint32_t>(*value);
}

int main(int argc, char *argv[]) {
  std::signal(SIGPIPE, SIG_IGN);

  const string progname = argv[0];

  if (argc != 3) {
    println(stderr, "Usage: {} <tracker_info.txt> <tracker_no>", progname);
    return 1;
  }

  auto trackers = load_address_file(argv[1]);
  if (!trackers) {
    println(stderr, "{}: {}", progname, trackers.error());
    return 1;
  }

  auto tracker_no = parse_tracker_no(argv[2], trackers->size());
  if (!tracker_no) {
    println(stderr, "{}: {}", progname, tracker_no.error());
    return 1;
  }

  Logger::init(std::format("tracker {}", *tracker_no));
  // no prompt when stdin is a pipe, as in the tests
  Console console(::isatty(STDIN_FILENO) ? "> " : "");

  Server server(*trackers, *tracker_no);
  if (auto started = server.start(); !started) {
    Logger::error("{}", started.error());
    return 1;
  }

  Logger::info("listening on {}", (*trackers)[*tracker_no]);

  std::jthread input(console_loop, std::ref(server), std::ref(console));
  auto ran = server.run();
  if (!ran)
    Logger::error("{}", ran.error());

  server.stop();
  server.join();

  return ran ? 0 : 1;
}
