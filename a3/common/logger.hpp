#pragma once

#include "common/types.hpp"

#include <atomic>
#include <cstdint>
#include <format>
#include <utility>

enum class Level : uint8_t { Debug, Info, Warn, Error };

class Logger {
public:
  // TOR_LOG=debug in the environment turns on debug lines
  static void init(string name);

  template <class... Args>
  static void debug(std::format_string<Args...> fmt, Args &&...args) {
    _log(Level::Debug, fmt, std::forward<Args>(args)...);
  }
  template <class... Args>
  static void info(std::format_string<Args...> fmt, Args &&...args) {
    _log(Level::Info, fmt, std::forward<Args>(args)...);
  }
  template <class... Args>
  static void warn(std::format_string<Args...> fmt, Args &&...args) {
    _log(Level::Warn, fmt, std::forward<Args>(args)...);
  }
  template <class... Args>
  static void error(std::format_string<Args...> fmt, Args &&...args) {
    _log(Level::Error, fmt, std::forward<Args>(args)...);
  }

private:
  template <class... Args>
  static void _log(Level level, std::format_string<Args...> fmt,
                   Args &&...args) {
    if (level >= _min.load())
      _write(level, std::format(fmt, std::forward<Args>(args)...));
  }

  static void _write(Level level, const string &message);

  static inline std::atomic<Level> _min = Level::Info;
  static inline string _name;
  static inline bool _color = false;
};
