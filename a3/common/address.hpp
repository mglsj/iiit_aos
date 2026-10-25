#pragma once

#include "common/types.hpp"

#include <cstdint>
#include <expected>
#include <format>
#include <string_view>

struct Address {
  string host;
  uint16_t port;
};

std::expected<Address, string> parse_address(std::string_view text);

std::expected<vector<Address>, string> load_address_file(const string &path);

template <> struct std::formatter<Address> : std::formatter<std::string_view> {
  auto format(const Address &addr, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}:{}", addr.host, addr.port);
  }
};
