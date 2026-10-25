#pragma once

/// @file
/// @brief File modes recorded in trees and the index.

#include <format>

#include "core/commons.hpp"

namespace git {

/// @brief The file modes Git records in trees and the index (octal values).
enum class Mode {
  /// @brief Regular file.
  FILE = 0100644,
  /// @brief Executable file.
  EXE = 0100755,
  /// @brief Symbolic link; the blob holds the link's target path.
  SYMLINK = 0120000,
  /// @brief Folder, i.e. a sub-tree.
  DIR = 040000,
};

/// @brief Converts a numeric mode read from a tree or the index.
/// @param mode The number, e.g. 0100644.
/// @return The matching Mode, or an error for any other value.
inline Result<Mode> get_mode(int mode) {
  switch (mode) {
  case static_cast<int>(Mode::FILE):
    return Mode::FILE;
  case static_cast<int>(Mode::EXE):
    return Mode::EXE;
  case static_cast<int>(Mode::SYMLINK):
    return Mode::SYMLINK;
  case static_cast<int>(Mode::DIR):
    return Mode::DIR;
  default:
    return std::unexpected(std::format("unsupported file mode {:o}", mode));
  }
}

}
