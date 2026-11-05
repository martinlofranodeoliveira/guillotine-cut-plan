#pragma once
#include <limits>
#include "guillotine/types.hpp"

namespace guillotine::detail {
inline bool add(Unit a, Unit b, Unit& out) {
    if (b > std::numeric_limits<Unit>::max() - a) return false;
    out = a + b;
    return true;
}
inline bool multiply(Unit a, Unit b, Unit& out) {
    if (a != 0 && b > std::numeric_limits<Unit>::max() / a) return false;
    out = a * b;
    return true;
}
inline bool accumulate(Unit& total, Unit amount) {
    Unit next = 0;
    if (!add(total, amount, next)) return false;
    total = next;
    return true;
}
}
