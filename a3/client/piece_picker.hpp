#pragma once

#include "common/types.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>

// hands out the rarest missing piece a peer has, never one piece to two workers
class PiecePicker {
public:
  explicit PiecePicker(uint32_t count)
      : _pieces(count, Piece::Missing), _peers_with(count, 0) {}

  void add_peer(const string &map);

  void forget_peers();

  std::optional<uint32_t> next(const string &map, const vector<bool> &refused,
                               std::stop_token stop);

  [[nodiscard]] bool done(uint32_t index) pre(index < _pieces.size());

  void release(uint32_t index) pre(index < _pieces.size());

  bool complete();

private:
  enum class Piece : uint8_t { Missing, Fetching, Done };

  // guards everything below
  std::mutex _mutex;
  std::condition_variable_any _changed;
  // bumped on every change so waiters know to look again
  uint64_t _version = 0;

  vector<Piece> _pieces;
  vector<uint32_t> _peers_with;
  uint32_t _done = 0;
};
