#include "piece_picker.hpp"

#include <algorithm>

void PiecePicker::add_peer(const string &map) {
  std::scoped_lock lock(_mutex);

  for (size_t i = 0; i < map.size() && i < _pieces.size(); ++i)
    if (map[i] == '1')
      ++_peers_with[i];
}

void PiecePicker::forget_peers() {
  std::scoped_lock lock(_mutex);

  std::ranges::fill(_peers_with, 0);
}

std::optional<uint32_t> PiecePicker::next(const string &map,
                                          const vector<bool> &refused,
                                          std::stop_token stop) {
  std::unique_lock lock(_mutex);

  while (true) {
    std::optional<uint32_t> rarest;
    bool others_fetching = false;

    for (uint32_t i = 0; i < _pieces.size(); ++i) {
      if (map[i] != '1' || refused[i])
        continue;

      if (_pieces[i] == Piece::Fetching)
        others_fetching = true;
      else if (_pieces[i] == Piece::Missing &&
               (!rarest || _peers_with[i] < _peers_with[*rarest]))
        rarest = i;
    }

    if (rarest) {
      _pieces[*rarest] = Piece::Fetching;
      return rarest;
    }
    if (!others_fetching)
      return std::nullopt;

    // another worker's fetch may fail and put its piece back
    uint64_t seen = _version;
    if (!_changed.wait(lock, stop, [&] { return _version != seen; }))
      return std::nullopt;
  }
}

bool PiecePicker::done(uint32_t index) {
  std::scoped_lock lock(_mutex);

  _pieces[index] = Piece::Done;
  ++_done;
  ++_version;
  _changed.notify_all();
  return _done == _pieces.size();
}

void PiecePicker::release(uint32_t index) {
  std::scoped_lock lock(_mutex);

  _pieces[index] = Piece::Missing;
  ++_version;
  _changed.notify_all();
}

bool PiecePicker::complete() {
  std::scoped_lock lock(_mutex);

  return _done == _pieces.size();
}
