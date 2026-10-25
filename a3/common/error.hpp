#pragma once

#include "common/types.hpp"

#include <cerrno>
#include <expected>
#include <format>
#include <string_view>
#include <system_error>

inline std::unexpected<string> errno_error(std::string_view what) {
  return std::unexpected(
      std::format("{}: {}", what, std::generic_category().message(errno)));
}
