#include "common/logger.hpp"
#include "common/console.hpp"

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <unistd.h>

void Logger::init(string name) {
  _name = std::move(name);
  _color = ::isatty(STDERR_FILENO) && !std::getenv("NO_COLOR");

  const char *level = std::getenv("TOR_LOG");
  if (level && std::string_view(level) == "debug")
    _min = Level::Debug;
}

constexpr std::string_view BLUE = "\x1b[34m";
constexpr std::string_view GREEN = "\x1b[32m";
constexpr std::string_view YELLOW = "\x1b[33m";
constexpr std::string_view RED = "\x1b[31m";
constexpr std::string_view BOLD = "\x1b[1m";
constexpr std::string_view NOT_BOLD = "\x1b[22m";
constexpr std::string_view RESET = "\x1b[0m";

struct Tag {
  std::string_view status;
  std::string_view color;
};

static Tag tag_of(Level level) {
  switch (level) {
  case Level::Debug:
    return {"DEBUG", BLUE};
  case Level::Info:
    return {"INFO", GREEN};
  case Level::Warn:
    return {"WARN", YELLOW};
  case Level::Error:
    return {"ERROR", RED};
  }
  return {"?", ""};
}

void Logger::_write(Level level, const string &message) {
  auto now = std::chrono::floor<std::chrono::milliseconds>(
      std::chrono::system_clock::now());

  Tag tag = tag_of(level);
  string padding(5 - tag.status.size(), ' ');

  if (_color)
    Console::print(stderr, std::format("{:%T} {}[{}{}{}]{}{} {}: {}", now,
                                       tag.color, BOLD, tag.status, NOT_BOLD,
                                       RESET, padding, _name, message));
  else
    Console::print(stderr, std::format("{:%T} [{}]{} {}: {}", now, tag.status,
                                       padding, _name, message));
}
