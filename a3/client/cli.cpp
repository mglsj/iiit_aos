#include "cli.hpp"
#include "common/commands.hpp"
#include "common/text.hpp"

#include <cstdio>
#include <expected>
#include <print>
#include <ranges>

constexpr std::string_view INVALID_COMMAND = "Invalid command.";

static std::expected<Request, string>
parse_command(const vector<string> &tokens) {
  for (const CommandSpec &spec : all_specs()) {
    if (spec.channel != Channel::User && spec.channel != Channel::Local)
      continue;

    if (tokens.size() != 1 + spec.argc || tokens[0] != spec.name)
      continue;

    Request req{
        .cmd = spec.cmd,
        .args = tokens | std::views::drop(1) | std::ranges::to<vector>(),
    };

    if (!valid_args(req.args))
      return std::unexpected(string(INVALID_COMMAND));
    return req;
  }

  return std::unexpected(string(INVALID_COMMAND));
}

static void print_response(const CommandSpec &spec, const Response &res) {
  if (res.status != Status::Ok) {
    std::println("{}", status_text(res.status));
    return;
  }

  if (spec.cmd == Command::ListSeeders) {
    if (res.items.size() < 4)
      std::println("No peers are sharing this file right now");
    for (auto seeder : res.items | std::views::drop(1) | std::views::chunk(3))
      if (seeder.size() == 3)
        std::println("{} {} {}/{}", seeder[0], seeder[1], seeder[2],
                     res.items[0]);
    return;
  }

  if (!spec.success.empty())
    std::println("{}", spec.success);

  for (const auto &item : res.items)
    std::println("{}", item);
}

static void execute(App &app, const string &line) {
  auto req = parse_command(tokenize(line));
  if (!req) {
    std::println("{}", req.error());
    return;
  }

  auto res = app.execute(*req);
  if (!res) {
    std::println("{}", res.error());
    return;
  }

  print_response(spec_of(req->cmd), *res);
}

void run_cli(App &app, Console &console) {
  while (auto line = console.read_line()) {
    trim_right(*line);

    if (*line == "quit")
      break;
    if (!line->empty())
      execute(app, *line);
  }
}
