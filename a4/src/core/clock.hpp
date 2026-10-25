#pragma once

/// @file
/// @brief Time helpers for commit timestamps.

#include <chrono>
#include <format>
#include <string>
#include <string_view>

#include "core/commons.hpp"

/// @brief The current time as seconds since 1970-01-01 UTC (Unix time).
inline std::chrono::seconds now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());
}

/// @brief The system's current UTC offset.
/// @return An offset such as "+0530" or "-0700".
inline std::string get_timezone_offset() {
  const std::chrono::zoned_time zoned{std::chrono::current_zone(),
                                      std::chrono::system_clock::now()};
  return std::format("{:%z}", zoned);
}

/// @brief Formats a commit time in the zone it was written in, like Git's log.
///
/// A malformed offset is read as UTC.
/// @param seconds Unix time stored in the commit.
/// @param offset Offset stored in the commit, e.g. "+0000".
/// @return For example "Sat Oct 24 09:24:19 2026 +0000".
inline std::string format_timestamp(std::chrono::seconds seconds,
                                    const std::string &offset) {
  int minutes = 0;
  const auto hours = sv_to_num<int>(std::string_view(offset).substr(1, 2));
  const auto mins = sv_to_num<int>(std::string_view(offset).substr(3, 2));
  if (offset.size() == 5 && (offset[0] == '+' || offset[0] == '-') && hours &&
      mins) {
    minutes = *hours * 60 + *mins;
    if (offset[0] == '-')
      minutes = -minutes;
  }
  const std::chrono::sys_seconds local{seconds + std::chrono::minutes(minutes)};
  return std::format("{:%a %b %e %H:%M:%S %Y} {}", local, offset);
}
