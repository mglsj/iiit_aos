/// @file
/// @brief Implementation of Commit: the text format and its parser.

#include "objects/commit.hpp"

#include <format>
#include <optional>
#include <string_view>

namespace {

/// @brief Parses `"Name <email> 1792833859 +0000"` from an author or committer line.
/// @param text The line without its "author " or "committer " key.
/// @return The person, or an error if the format is wrong.
Result<git::Person> parse_person(std::string_view text) {
  const auto bad = [&text] {
    return std::unexpected(std::format("malformed commit: invalid identity '{}'", text));
  };

  const auto open = text.find('<');
  const auto close = text.rfind('>');
  if (open == std::string_view::npos || close == std::string_view::npos ||
      close < open || open == 0)
    return bad();

  std::string_view stamp = text.substr(close + 1);
  if (!stamp.starts_with(' '))
    return bad();
  stamp.remove_prefix(1);

  const auto space = stamp.find(' ');
  if (space == std::string_view::npos)
    return bad();

  const auto seconds = TRY(sv_to_num<long long>(stamp.substr(0, space)));
  return git::Person{
      .name = std::string(text.substr(0, open - 1)),
      .email = std::string(text.substr(open + 1, close - open - 1)),
      .seconds = std::chrono::seconds(seconds),
      .timezone_offset = std::string(stamp.substr(space + 1)),
  };
}

}

namespace git {

Result<Commit> Commit::load_from_objects(const Sha1 &hash) {
  return from_object(TRY(Object::load_from_objects(hash)));
}

Result<Commit> Commit::from_object(Object object) {
  if (object.type() != ObjectType::commit)
    return std::unexpected(std::format("object {} is a {}, not a commit",
                                       object.hash(), object.type_name()));

  CommitData data = TRY(_parse(object.content()));
  return Commit(std::move(object), std::move(data));
}

Result<CommitData> Commit::_parse(std::string_view content) {
  const auto blank = content.find("\n\n");
  const std::string_view headers = content.substr(0, blank);
  const std::string_view message =
      blank == std::string_view::npos ? "" : content.substr(blank + 2);

  std::optional<Sha1> tree;
  std::vector<Sha1> parents;
  std::optional<Person> author;
  std::optional<Person> committer;

  std::string_view rest = headers;
  while (!rest.empty()) {
    const auto eol = rest.find('\n');
    const std::string_view line = rest.substr(0, eol);
    rest = eol == std::string_view::npos ? "" : rest.substr(eol + 1);

    const auto space = line.find(' ');
    if (space == std::string_view::npos)
      continue;
    const std::string_view key = line.substr(0, space);
    const std::string_view value = line.substr(space + 1);

    if (key == "tree")
      tree = TRY(Sha1::from_hex_string(value));
    else if (key == "parent")
      parents.push_back(TRY(Sha1::from_hex_string(value)));
    else if (key == "author")
      author = TRY(parse_person(value));
    else if (key == "committer")
      committer = TRY(parse_person(value));
  }

  if (!tree || !author || !committer)
    return std::unexpected("malformed commit: missing a required header");

  return CommitData{
      .tree = *tree,
      .parents = std::move(parents),
      .author = std::move(*author),
      .committer = std::move(*committer),
      .message = std::string(message),
  };
}

std::string Commit::_to_content(const CommitData &data) {
  std::string content = std::format("tree {}\n", data.tree);
  for (const Sha1 &parent : data.parents)
    content += std::format("parent {}\n", parent);

  const auto identity = [](std::string_view role, const Person &person) {
    return std::format("{} {} <{}> {} {}\n", role, person.name, person.email,
                       person.seconds.count(), person.timezone_offset);
  };
  content += identity("author", data.author);
  content += identity("committer", data.committer);
  content += std::format("\n{}", data.message);
  return content;
}

}
