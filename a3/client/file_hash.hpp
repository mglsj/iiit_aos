#pragma once

#include "common/file_meta.hpp"

#include <expected>

std::expected<FileMeta, string> hash_file(const string &path);

std::expected<string, string> sha1_of_file(const string &path);
