#include "common/console.hpp"

#include <array>
#include <cctype>
#include <cerrno>
#include <print>
#include <sys/poll.h>
#include <unistd.h>
#include <utility>

constexpr char CTRL_C = 0x03;
constexpr char CTRL_D = 0x04;
constexpr char BACKSPACE = 0x08;
constexpr char ESCAPE = 0x1b;
constexpr char DELETE = 0x7f;

std::mutex Console::_mutex;
string Console::_prompt;
string Console::_typed;
bool Console::_raw = false;
bool Console::_showing = false;

Console::Console(string prompt) {
  std::scoped_lock lock(_mutex);
  _prompt = std::move(prompt);

  if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO) ||
      ::tcgetattr(STDIN_FILENO, &_saved) != 0)
    return;

  // without ICANON, ECHO and ISIG, every key reaches read() and nothing else
  termios keys = _saved;
  keys.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ISIG);
  keys.c_cc[VMIN] = 1;
  keys.c_cc[VTIME] = 0;
  _raw = ::tcsetattr(STDIN_FILENO, TCSANOW, &keys) == 0;
}

Console::~Console() {
  std::scoped_lock lock(_mutex);
  if (_raw)
    ::tcsetattr(STDIN_FILENO, TCSANOW, &_saved);
  _raw = false;
  _showing = false;
}

void Console::print(FILE *out, std::string_view line) {
  std::scoped_lock lock(_mutex);
  bool redraw = _raw && _showing;

  // \r and \x1b[2K move to the start of the line and clear it
  if (redraw) {
    std::print(stdout, "\r\x1b[2K");
    std::fflush(stdout);
  }
  std::println(out, "{}", line);
  std::fflush(out);
  if (redraw) {
    std::print(stdout, "{}{}", _prompt, _typed);
    std::fflush(stdout);
  }
}

std::optional<string> Console::read_line(std::stop_token stop) {
  _show_prompt();

  while (!_ended) {
    if (auto line = _raw ? _take_key_line() : _take_line()) {
      _hide_prompt();
      return line;
    }
    if (_ended)
      break;
    if (!_read_more(stop)) {
      // piped input may end without a newline, as a file can
      if (_ended && !_raw && !_pending.empty()) {
        _hide_prompt();
        return std::exchange(_pending, {});
      }
      break;
    }
  }

  _hide_prompt();
  return std::nullopt;
}

void Console::_show_prompt() {
  std::scoped_lock lock(_mutex);
  _typed.clear();
  _showing = true;
  std::print(stdout, "{}", _prompt);
  std::fflush(stdout);
}

void Console::_hide_prompt() {
  std::scoped_lock lock(_mutex);
  _showing = false;
}

std::optional<string> Console::_take_line() {
  size_t nl = _pending.find('\n');
  if (nl == string::npos)
    return std::nullopt;

  string line = _pending.substr(0, nl);
  _pending.erase(0, nl + 1);
  return line;
}

std::optional<string> Console::_take_key_line() {
  std::scoped_lock lock(_mutex);
  std::optional<string> line;

  size_t used = 0;
  while (used < _pending.size() && !line && !_ended) {
    char c = _pending[used++];

    // an arrow key sends ESC [ A, so a sequence ends at its first letter or ~
    if (_in_escape) {
      _in_escape =
          !(std::isalpha(static_cast<byte>(c)) && c != 'O') && c != '~';
      continue;
    }

    switch (c) {
    case '\r':
    case '\n':
      std::print(stdout, "\n");
      line = std::exchange(_typed, {});
      break;
    case CTRL_C:
      std::print(stdout, "^C\n");
      _typed.clear();
      line = "quit";
      break;
    case CTRL_D:
      if (_typed.empty()) {
        std::print(stdout, "\n");
        _ended = true;
      }
      break;
    case BACKSPACE:
    case DELETE:
      if (!_typed.empty()) {
        _typed.pop_back();
        std::print(stdout, "\b \b");
      }
      break;
    case ESCAPE:
      _in_escape = true;
      break;
    default:
      if (static_cast<byte>(c) >= 0x20) {
        _typed.push_back(c);
        std::print(stdout, "{}", c);
      }
    }
  }

  _pending.erase(0, used);
  std::fflush(stdout);
  return line;
}

// poll's timeout lets the loop notice a stop while nobody types
bool Console::_read_more(std::stop_token stop) {
  while (!stop.stop_requested()) {
    ::pollfd pfd{STDIN_FILENO, POLLIN, 0};
    int r = ::poll(&pfd, 1, 200);
    if (r == 0 || (r < 0 && errno == EINTR))
      continue;

    std::array<char, 256> buf;
    ssize_t n = (r < 0) ? -1 : ::read(STDIN_FILENO, buf.data(), buf.size());
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      _ended = true;
      return false;
    }

    _pending.append(buf.data(), static_cast<size_t>(n));
    return true;
  }
  return false;
}
