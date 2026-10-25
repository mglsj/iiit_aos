#include "console.hpp"
#include "common/logger.hpp"
#include "common/text.hpp"

void console_loop(Server &server, Console &console) {
  std::stop_token stop = server.stop_token();

  while (auto line = console.read_line(stop)) {
    trim_right(*line);

    if (*line == "quit") {
      server.stop();
      return;
    }
    if (!line->empty())
      Logger::warn("unknown command: {}", *line);
  }

  // stdin closed, and only quit stops the tracker
  if (!stop.stop_requested())
    Logger::info("console closed, the tracker keeps running");
}
