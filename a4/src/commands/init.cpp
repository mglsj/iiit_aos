/// @file
/// @brief The init command.

#include <array>
#include <format>
#include <print>

#include "cli/args.hpp"
#include "cli/commands.hpp"
#include "core/file.hpp"

namespace {

constexpr std::string_view USAGE = "usage: mygit init";

constexpr std::array SUBDIRS = {
    "hooks", "info", "objects/info", "objects/pack", "refs/heads", "refs/tags",
};

}

/// @brief `mygit init`: creates the `.mygit` folder in the current folder.
///
/// Creates objects/, refs/heads/, refs/tags/, hooks/ and info/, and points HEAD
/// at refs/heads/main. Running it again keeps everything that exists.
/// @param args Must be empty.
/// @return Nothing, or an error if a folder or HEAD cannot be created.
Result<void> init(Args args) {
  const auto parsed = TRY(parse_args(args, {}));
  if (!parsed.positional.empty())
    return std::unexpected(std::string(USAGE));

  const fs::path head = GIT_DIR / "HEAD";
  std::error_code ec;
  const bool existed = fs::exists(head, ec);

  for (const auto *subdir : SUBDIRS) {
    fs::create_directories(GIT_DIR / subdir, ec);
    if (ec)
      return std::unexpected(std::format("cannot create '{}': {}",
                                         (GIT_DIR / subdir).string(),
                                         ec.message()));
  }

  if (!existed)
    TRY_VOID(write_file(head, "ref: refs/heads/main\n"));

  const fs::path root = fs::absolute(GIT_DIR, ec);
  std::println("{} mygit repository in {}/",
               existed ? "Reinitialized existing" : "Initialized empty",
               root.string());
  return {};
}
