#pragma once
#include "types.hpp"

namespace guillotine {
// Replays geometry independently of the planner's search and split code.
Result<Summary> verify_certificate(const Request& request, const Certificate& certificate);
}
