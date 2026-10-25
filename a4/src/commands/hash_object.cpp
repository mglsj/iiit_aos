/// @file
/// @brief The hash-object command.

#include "cli/commands.hpp"

#include <format>
#include <print>

#include "cli/args.hpp"
#include "objects/blob.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit hash-object [-w] <file>...";

constexpr OptionSpec SPEC[] = {{'w', "", false}};

}

/// @brief `mygit hash-object [-w] <file>...`: prints the blob hash of each file.
///
/// With -w, also stores each blob. Without it, nothing is written and no
/// repository is needed. A failing file is reported and the rest still run.
/// @param args Optional -w, then one or more files.
/// @return Nothing, or an empty error (already printed) if any file failed.
Result<void> hash_object(Args args) {
  const auto parsed = TRY(parse_args(args, SPEC));
  if (parsed.positional.empty())
    return std::unexpected(std::string(USAGE));

  bool success = true;

  for (const fs::path file_path : parsed.positional) {
    auto blob = git::Blob::load_from_worktree(file_path);
    if (!blob) {
      print_error(blob.error());
      success = false;
      continue;
    }

    if (parsed.has("w")) {
      auto write_result = blob->write();
      if (!write_result) {
        print_error(write_result.error());
        success = false;
        continue;
      }
    }

    std::println("{}", blob->hash());
  }

  if (success)
    return {};
  else
    return std::unexpected("");
}
