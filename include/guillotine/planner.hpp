#pragma once
#include "types.hpp"

namespace guillotine {
// Inputs are never modified. Failure contains no partial certificate.
Result<Certificate> plan(const Request& request);
}
