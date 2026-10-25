/// @file
/// @brief The cat-file command.

#include "cli/commands.hpp"

#include <format>
#include <print>
#include <string_view>

#include "cli/args.hpp"
#include "objects/tree.hpp"

namespace {

constexpr std::string_view USAGE =
    "usage: mygit cat-file (-p | -t | -s) <object>";

constexpr OptionSpec SPEC[] = {
    {'p', "", false},
    {'t', "", false},
    {'s', "", false},
};

}

/// @brief `mygit cat-file (-p | -t | -s) <sha>`: shows an object.
///
/// -p prints the content exactly (a tree as a readable listing), -t the type,
/// -s the content size in bytes.
/// @param args Exactly one of -p, -t, -s and one full hash.
/// @return Nothing, or an error for bad arguments or a missing/corrupt object.
Result<void> cat_file(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));

  const int modes = parsed.has("p") + parsed.has("t") + parsed.has("s");
  if (modes != 1 || parsed.positional.size() != 1)
    return std::unexpected(std::string(USAGE));

  const auto hash = TRY(Sha1::from_hex_string(parsed.positional[0]));
  const auto object = TRY(git::Object::load_from_objects(hash));

  if (parsed.has("p")) {
    if (object.type() == git::ObjectType::tree) {

      const auto tree = TRY(git::Tree::from_object(object));
      for (const auto &entry : tree.entries())
        std::println("{}", git::to_listing(entry));
    } else {
      std::print("{}", object.content());
    }
  } else if (parsed.has("t")) {
    std::println("{}", object.type_name());
  } else {
    std::println("{}", object.size());
  }

  return {};
}
