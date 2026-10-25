#include "common/address.hpp"
#include "common/text.hpp"

#include <format>
#include <fstream>

std::expected<Address, string> parse_address(std::string_view text) {
  size_t colon = text.find(':');
  if (colon == std::string_view::npos)
    return std::unexpected(std::format("expected host:port, got \"{}\"", text));

  auto port = parse_u64(text.substr(colon + 1));
  if (!port || *port == 0 || *port > 65535)
    return std::unexpected(std::format("invalid port in \"{}\"", text));

  return Address{
      .host = string(text.substr(0, colon)),
      .port = static_cast<uint16_t>(*port),
  };
}

std::expected<vector<Address>, string> load_address_file(const string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return std::unexpected("cannot open " + path);

  vector<Address> addresses;

  string line;
  while (std::getline(file, line)) {
    trim_right(line);
    if (line.empty())
      continue;

    auto addr = parse_address(line);
    if (!addr)
      return std::unexpected(addr.error());
    addresses.push_back(std::move(*addr));
  }

  if (addresses.empty())
    return std::unexpected(path + " lists no addresses");

  return addresses;
}
