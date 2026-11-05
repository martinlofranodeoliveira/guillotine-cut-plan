#pragma once
#include <optional>
#include "guillotine/types.hpp"

namespace guillotine::detail {
std::optional<Error> validate_request(const Request& request);
}
