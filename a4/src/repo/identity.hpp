#pragma once

/// @file
/// @brief Author and committer identity for new commits.

#include "objects/commit.hpp"

namespace git {

/// @brief Who is committing, and when.
///
/// Name from MYGIT_AUTHOR_NAME, GIT_AUTHOR_NAME, USER or LOGNAME; email from
/// MYGIT_AUTHOR_EMAIL, GIT_AUTHOR_EMAIL or `<user>@localhost`; the current time
/// and time zone.
Person current_person();

}
