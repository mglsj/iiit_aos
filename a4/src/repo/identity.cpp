/// @file
/// @brief Implementation of current_person().

#include "repo/identity.hpp"

#include <cstdlib>
#include <format>

#include "core/clock.hpp"

namespace {

/// @brief The first of several environment variables that is set and not empty.
/// @param names Variables to try, in order.
/// @param fallback Returned if none is set.
std::string env_or(std::initializer_list<const char *> names,
                   std::string fallback) {
  for (const char *name : names)
    if (const char *value = std::getenv(name); value && *value)
      return value;
  return fallback;
}

}

namespace git {

Person current_person() {
  const std::string user = env_or({"USER", "LOGNAME"}, "unknown");
  return Person{
      .name = env_or({"MYGIT_AUTHOR_NAME", "GIT_AUTHOR_NAME"}, user),
      .email = env_or({"MYGIT_AUTHOR_EMAIL", "GIT_AUTHOR_EMAIL"},
                      std::format("{}@localhost", user)),
      .seconds = now(),
      .timezone_offset = get_timezone_offset(),
  };
}

}
