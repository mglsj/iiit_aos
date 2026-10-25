#include "common/codec.hpp"

void Encoder::_put_be(uint64_t v, int n) {
  for (int shift = (n - 1) * 8; shift >= 0; shift -= 8)
    _buffer.push_back(static_cast<byte>(v >> shift));
}

Encoder &Encoder::put_u8(uint8_t v) {
  _put_be(v, 1);
  return *this;
}

Encoder &Encoder::put_u32(uint32_t v) {
  _put_be(v, 4);
  return *this;
}

Encoder &Encoder::put_u64(uint64_t v) {
  _put_be(v, 8);
  return *this;
}

Encoder &Encoder::put_str(std::string_view s) {
  put_u32(static_cast<uint32_t>(s.size()));
  _buffer.append_range(s);
  return *this;
}

Encoder &Encoder::put_strs(const vector<string> &v) {
  put_u32(static_cast<uint32_t>(v.size()));
  for (const auto &s : v)
    put_str(s);
  return *this;
}

Encoder &Encoder::put_raw(std::span<const byte> bytes) {
  _buffer.append_range(bytes);
  return *this;
}

std::span<const byte> Decoder::_take(size_t n) {
  // compared as n > size - pos because pos + n could overflow
  if (_failed || n > _buf.size() - _pos) {
    _failed = true;
    return {};
  }

  std::span<const byte> out = _buf.subspan(_pos, n);
  _pos += n;
  return out;
}

uint64_t Decoder::_get_be(size_t n) {
  uint64_t v = 0;
  for (byte b : _take(n))
    v = (v << 8) | b;
  return v;
}

uint8_t Decoder::get_u8() { return static_cast<uint8_t>(_get_be(1)); }

uint32_t Decoder::get_u32() { return static_cast<uint32_t>(_get_be(4)); }

uint64_t Decoder::get_u64() { return _get_be(8); }

string Decoder::get_str() {
  uint32_t len = get_u32();
  return string(std::from_range, _take(len));
}

vector<string> Decoder::get_strs() {
  uint32_t n = get_count(4);

  vector<string> v;
  v.reserve(n);
  for (uint32_t i = 0; i < n; ++i)
    v.push_back(get_str());
  return v;
}

uint32_t Decoder::get_count(size_t min_size) {
  uint32_t n = get_u32();

  if (_failed || (min_size > 0 && n > (_buf.size() - _pos) / min_size)) {
    _failed = true;
    return 0;
  }

  return n;
}
