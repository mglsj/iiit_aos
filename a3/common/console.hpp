#pragma once

#include "common/types.hpp"

#include <cstdio>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <termios.h>

class Console {
public:
  explicit Console(string prompt);
  ~Console();

  Console(const Console &) = delete("it restores the terminal once");
  Console &operator=(const Console &) = delete("it restores the terminal once");

  std::optional<string> read_line(std::stop_token stop = {});

  static void print(FILE *out, std::string_view line);

private:
  void _show_prompt();
  void _hide_prompt();
  std::optional<string> _take_line();
  std::optional<string> _take_key_line();
  bool _read_more(std::stop_token stop);

  termios _saved = {};
  string _pending; // bytes read but not yet handed out
  bool _in_escape = false;
  bool _ended = false;

  static std::mutex _mutex;
  static string _prompt;
  static string _typed;
  static bool _raw;
  static bool _showing;
};
