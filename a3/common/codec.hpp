#pragma once

#include "common/enums.hpp"
#include "common/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

class Encoder {
public:
  Encoder &put_u8(uint8_t v);
  Encoder &put_u32(uint32_t v);
  Encoder &put_u64(uint64_t v);
  Encoder &put_str(std::string_view s);
  Encoder &put_strs(const vector<string> &v);

  Encoder &put_raw(std::span<const byte> bytes);

  const Bytes &bytes() const { return _buffer; }
  Bytes take() { return std::move(_buffer); }

private:
  void _put_be(uint64_t v, int n);

  Bytes _buffer;
};

class Decoder {
public:
  explicit Decoder(std::span<const byte> buf) : _buf(buf) {}

  [[nodiscard]] uint8_t get_u8();
  [[nodiscard]] uint32_t get_u32();
  [[nodiscard]] uint64_t get_u64();
  [[nodiscard]] string get_str();
  [[nodiscard]] vector<string> get_strs();

  [[nodiscard]] uint32_t get_count(size_t min_size);

  template <class E>
    requires std::is_scoped_enum_v<E>
  [[nodiscard]] E get_enum() {
    static_assert(enum_count<E>() > 0, "enumerators must count up from 0");

    uint8_t v = get_u8();
    if (v >= enum_count<E>()) {
      _failed = true;
      return E{};
    }
    return static_cast<E>(v);
  }

  void fail() { _failed = true; }

  [[nodiscard]] bool ok() const { return !_failed; }

  [[nodiscard]] bool finished() const {
    return !_failed && _pos == _buf.size();
  }

private:
  std::span<const byte> _take(size_t n);
  uint64_t _get_be(size_t n);

  std::span<const byte> _buf;
  size_t _pos = 0;
  bool _failed = false;
};
