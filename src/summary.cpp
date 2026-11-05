#include "guillotine/summary.hpp"
#include "guillotine/verify.hpp"

namespace guillotine {
Result<Summary> summarize(const Request& request, const Certificate& certificate) {
    return verify_certificate(request, certificate);
}
}
