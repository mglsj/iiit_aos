#pragma once

#include "common/types.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <expected>
#include <sstream>
#include <string_view>

inline void trim_right(string &s) {
  while (!s.empty() && std::isspace(static_cast<byte>(s.back())))
    s.pop_back();
}

inline vector<string> tokenize(std::string_view s) {
  std::istringstream in{string(s)};

  vector<string> words;
  for (string word; in >> word;)
    words.push_back(word);
  return words;
}

inline std::expected<uint64_t, string> parse_u64(std::string_view text) {
  const char *end = text.data() + text.size();

  uint64_t value;
  auto result = std::from_chars(text.data(), end, value);
  if (!result || result.ptr != end)
    return std::unexpected("not a number: " + string(text));
  return value;
}
