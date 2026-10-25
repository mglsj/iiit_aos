/// @file
/// @brief Implementation of reading and moving HEAD and branches.

#include "repo/refs.hpp"

#include <format>
#include <string_view>

#include "core/file.hpp"

namespace {

constexpr std::string_view BRANCH_PREFIX = "refs/heads/";
constexpr std::string_view SYMBOLIC_PREFIX = "ref: ";

/// @brief Removes trailing `\n` and `\r` characters.
std::string_view trim_newline(std::string_view text) {
  while (text.ends_with('\n') || text.ends_with('\r'))
    text.remove_suffix(1);
  return text;
}

/// @brief Whether a name is safe to use as a path under refs/heads.
///
/// Branch names become paths, so anything that could leave refs/heads (an
/// absolute path, "..", a NUL) is rejected.
bool valid_branch_name(std::string_view name) {
  return !name.empty() && !name.starts_with('/') && !name.ends_with('/') &&
         name.find("..") == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

/// @brief Writes `"<hash>\n"` to a ref file atomically.
Result<void> write_ref(const fs::path &file, const Sha1 &commit) {
  return write_file(file, std::format("{}\n", commit));
}

}

namespace git {

Result<std::optional<Sha1>> read_branch(const fs::path &git_dir,
                                        const std::string &name) {
  if (!valid_branch_name(name))
    return std::optional<Sha1>{};

  const fs::path file = git_dir / BRANCH_PREFIX / name;
  std::error_code ec;
  if (!fs::is_regular_file(file, ec))
    return std::optional<Sha1>{};

  const Bytes bytes = TRY(read_file(file));
  const Sha1 hash = TRY(Sha1::from_hex_string(trim_newline(as_text(bytes))));
  return std::optional<Sha1>(hash);
}

Result<Head> read_head(const fs::path &git_dir) {
  const Bytes bytes = TRY(read_file(git_dir / "HEAD"));
  const std::string_view text = trim_newline(as_text(bytes));

  if (!text.starts_with(SYMBOLIC_PREFIX))
    return Head{.branch = std::nullopt,
                .commit = TRY(Sha1::from_hex_string(text))};

  std::string_view ref = text.substr(SYMBOLIC_PREFIX.size());
  if (!ref.starts_with(BRANCH_PREFIX))
    return std::unexpected(std::format("unsupported HEAD '{}'", text));

  std::string branch(ref.substr(BRANCH_PREFIX.size()));
  const auto tip = TRY(read_branch(git_dir, branch));
  return Head{.branch = std::move(branch), .commit = tip};
}

Result<void> advance_head(const fs::path &git_dir, const Sha1 &commit) {
  const Head head = TRY(read_head(git_dir));
  if (head.branch)
    return write_ref(git_dir / BRANCH_PREFIX / *head.branch, commit);
  return set_head_detached(git_dir, commit);
}

Result<void> set_head_branch(const fs::path &git_dir, const std::string &name) {
  return write_file(git_dir / "HEAD",
                    std::format("{}{}{}\n", SYMBOLIC_PREFIX, BRANCH_PREFIX, name));
}

Result<void> set_head_detached(const fs::path &git_dir, const Sha1 &commit) {
  return write_ref(git_dir / "HEAD", commit);
}

}
