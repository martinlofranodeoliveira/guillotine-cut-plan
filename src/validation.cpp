#include "validation.hpp"
#include "checked.hpp"
#include <set>

namespace guillotine::detail {
std::optional<Error> validate_request(const Request& r) {
    const auto invalid = [](const char* text) {
        return Error{ErrorCode::invalid_request, text, {}};
    };
    if (r.stock.width == 0 || r.stock.height == 0 ||
        r.stock.width > max_dimension || r.stock.height > max_dimension)
        return invalid("Stock dimensions must be in 1..1000000.");
    if (r.kerf > max_kerf) return invalid("Kerf must be in 0..1000000.");
    if (r.sheet_limit == 0 || r.sheet_limit > max_sheets)
        return invalid("Sheet limit must be in 1..32.");
    if (r.items.size() > max_items) return invalid("At most 128 items are allowed.");
    Unit stock_area = 0, total = 0;
    if (!multiply(r.stock.width, r.stock.height, stock_area) ||
        !multiply(stock_area, r.sheet_limit, total))
        return Error{ErrorCode::arithmetic_overflow, "Stock area overflow.", {}};
    std::set<std::string> ids;
    for (const auto& item : r.items) {
        if (item.id.empty() || item.id.size() > 64)
            return invalid("Item IDs must contain 1..64 ASCII characters.");
        for (unsigned char ch : item.id) {
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                return invalid("Item IDs permit only ASCII letters, digits, underscore, hyphen and dot.");
        }
        if (!ids.insert(item.id).second) return invalid("Item IDs must be unique.");
        if (item.width == 0 || item.height == 0 ||
            item.width > max_dimension || item.height > max_dimension)
            return invalid("Item dimensions must be in 1..1000000.");
        Unit area = 0;
        if (!multiply(item.width, item.height, area) || !accumulate(total, area))
            return Error{ErrorCode::arithmetic_overflow, "Item area overflow.", {}};
    }
    return std::nullopt;
}
}
