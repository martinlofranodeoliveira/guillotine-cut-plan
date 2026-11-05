#pragma once
#include "types.hpp"

namespace guillotine {
// Verifies before reporting; never summarizes an unchecked certificate.
Result<Summary> summarize(const Request& request, const Certificate& certificate);
}
