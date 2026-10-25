#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>
#include <utility>

// a gap in E's values returns 0, which fails the static_assert in get_enum
template <class E>
  requires std::is_scoped_enum_v<E>
consteval size_t enum_count() {
  auto values = std::meta::enumerators_of(^^E);
  for (size_t i = 0; i < values.size(); ++i)
    if (static_cast<size_t>(
            std::to_underlying(std::meta::extract<E>(values[i]))) != i)
      return 0;
  return values.size();
}

// the enumerator's name as written in the source, or "?" for any other value
template <class E>
  requires std::is_scoped_enum_v<E>
constexpr std::string_view enum_name(E value) {
  constexpr auto NAMES = [] {
    std::array<std::string_view, enum_count<E>()> names;
    auto values = std::meta::enumerators_of(^^E);
    for (size_t i = 0; i < names.size(); ++i)
      names[i] = std::define_static_string(std::meta::identifier_of(values[i]));
    return names;
  }();

  auto index = static_cast<size_t>(std::to_underlying(value));
  return index < NAMES.size() ? NAMES[index] : "?";
}
