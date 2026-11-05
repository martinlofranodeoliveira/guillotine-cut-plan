#include "guillotine/verify.hpp"
#include "checked.hpp"
#include "validation.hpp"
#include <map>
#include <new>

namespace guillotine {
namespace {
Error invalid(const char* text) {
    return {ErrorCode::certificate_invalid, text, {}};
}
bool within(const Rect& r, const Stock& s, bool positive) {
    Unit right = 0, bottom = 0;
    return (!positive || (r.w != 0 && r.h != 0)) &&
        detail::add(r.x, r.w, right) && detail::add(r.y, r.h, bottom) &&
        right <= s.width && bottom <= s.height;
}
bool inactive_cut_fields(const Node& n) {
    return n.axis == Axis::vertical && n.offset == 0 &&
        n.first == no_node && n.second == no_node && n.kerf == Rect{};
}
}

Result<Summary> verify_certificate(const Request& r, const Certificate& c) {
    try {
        if (auto error = detail::validate_request(r)) return *error;
        if (c.nodes.size() > max_nodes || c.roots.size() > max_sheets || c.roots.size() > r.sheet_limit)
            return Error{ErrorCode::resource_limit, "Certificate size or sheet limit exceeded.", {}};
        // Bounds precede multiplicity; neither depends on graph reachability.
        for (const auto& node : c.nodes) {
            if (!within(node.region, r.stock, true) ||
                (node.kind == NodeKind::cut && !within(node.kerf, r.stock, false)))
                return Error{ErrorCode::out_of_bounds, "A region lies outside stock or is empty.", {}};
        }
        std::map<std::string, const Item*> items;
        std::map<std::string, std::size_t> seen_items;
        for (const auto& item : r.items) items.emplace(item.id, &item);
        for (const auto& node : c.nodes) {
            if (node.kind != NodeKind::placement) continue;
            if (!items.contains(node.item_id) || ++seen_items[node.item_id] != 1)
                return Error{ErrorCode::item_multiplicity, "Unknown or repeated item ID.", {}};
        }
        if (seen_items.size() != items.size())
            return Error{ErrorCode::item_multiplicity, "Every requested item must appear exactly once.", {}};
        for (const auto& node : c.nodes) {
            if (node.kind != NodeKind::placement) continue;
            const auto& item = *items.at(node.item_id);
            if (node.rotation != 0 && (node.rotation != 90 || !item.allow_rotation))
                return Error{ErrorCode::rotation_not_allowed, "Placement rotation is not permitted.", {}};
            const Unit w = node.rotation == 0 ? item.width : item.height;
            const Unit h = node.rotation == 0 ? item.height : item.width;
            if (node.region.w != w || node.region.h != h)
                return invalid("Placement dimensions disagree with item and rotation.");
        }
        std::vector<bool> visited(c.nodes.size(), false);
        std::vector<std::uint32_t> work;
        work.reserve(max_nodes);
        const Rect stock{0, 0, r.stock.width, r.stock.height};
        for (auto root : c.roots) {
            if (root >= c.nodes.size() || c.nodes[root].region != stock)
                return invalid("A sheet root is missing or does not equal stock.");
            work.push_back(root);
        }
        Summary s;
        s.sheets = static_cast<Unit>(c.roots.size());
        std::size_t reached = 0;
        while (!work.empty()) {
            const auto index = work.back();
            work.pop_back();
            if (index >= c.nodes.size() || visited[index])
                return invalid("Repeated reference, cycle or invalid node index.");
            visited[index] = true;
            ++reached;
            const Node& n = c.nodes[index];
            Unit area = 0;
            if (!detail::multiply(n.region.w, n.region.h, area))
                return Error{ErrorCode::arithmetic_overflow, "Region area overflow.", {}};
            if (n.kind == NodeKind::leaf) {
                if (!inactive_cut_fields(n) || !n.item_id.empty() || n.rotation != 0)
                    return invalid("Unused leaf contains active fields.");
                ++s.remaining_leaves;
                if (!detail::accumulate(s.remaining_area, area))
                    return Error{ErrorCode::arithmetic_overflow, "Remaining area overflow.", {}};
            } else if (n.kind == NodeKind::placement) {
                if (!inactive_cut_fields(n)) return invalid("Placement contains cut fields.");
                ++s.placed_items;
                if (!detail::accumulate(s.used_area, area))
                    return Error{ErrorCode::arithmetic_overflow, "Used area overflow.", {}};
            } else if (n.kind == NodeKind::cut) {
                if (!n.item_id.empty() || n.rotation != 0 ||
                    (n.axis != Axis::vertical && n.axis != Axis::horizontal))
                    return invalid("Invalid cut fields.");
                ++s.cuts;
                if (s.cuts > max_cuts)
                    return Error{ErrorCode::resource_limit, "Cut count exceeded.", {}};
                const bool vertical = n.axis == Axis::vertical;
                const Unit extent = vertical ? n.region.w : n.region.h;
                const Unit origin = vertical ? n.region.x : n.region.y;
                Unit consumed = 0, start = 0, after = 0;
                if (n.offset == 0 || n.offset >= extent ||
                    !detail::add(n.offset, r.kerf, consumed) || consumed > extent ||
                    !detail::add(origin, n.offset, start) || !detail::add(start, r.kerf, after))
                    return invalid("Cut does not leave a positive first child and full kerf.");
                const Unit rest = extent - consumed;
                const Rect strip = vertical
                    ? Rect{start, n.region.y, r.kerf, n.region.h}
                    : Rect{n.region.x, start, n.region.w, r.kerf};
                const Rect first = vertical
                    ? Rect{n.region.x, n.region.y, n.offset, n.region.h}
                    : Rect{n.region.x, n.region.y, n.region.w, n.offset};
                if (n.kerf != strip) return invalid("Kerf strip is not the full cut strip.");
                if (n.first >= c.nodes.size() || c.nodes[n.first].region != first)
                    return invalid("First child does not partition its parent.");
                if (rest == 0) {
                    if (n.second != no_node) return invalid("Zero-sized second child must be omitted.");
                } else {
                    const Rect second = vertical
                        ? Rect{after, n.region.y, rest, n.region.h}
                        : Rect{n.region.x, after, n.region.w, rest};
                    if (n.second >= c.nodes.size() || c.nodes[n.second].region != second)
                        return invalid("Second child does not partition its parent.");
                    if (work.size() >= max_nodes) return invalid("Traversal bound exceeded.");
                    work.push_back(n.second);
                }
                if (work.size() >= max_nodes) return invalid("Traversal bound exceeded.");
                work.push_back(n.first);
                Unit strip_area = 0;
                if (!detail::multiply(strip.w, strip.h, strip_area) ||
                    !detail::accumulate(s.kerf_area, strip_area))
                    return Error{ErrorCode::arithmetic_overflow, "Kerf area overflow.", {}};
            } else {
                return invalid("Unknown node kind.");
            }
        }
        if (reached != c.nodes.size()) return invalid("Certificate contains unreachable nodes.");
        Unit one_sheet = 0, accounted = 0;
        if (!detail::multiply(r.stock.width, r.stock.height, one_sheet) ||
            !detail::multiply(one_sheet, s.sheets, s.stock_area) ||
            !detail::add(s.used_area, s.kerf_area, accounted) ||
            !detail::accumulate(accounted, s.remaining_area))
            return Error{ErrorCode::arithmetic_overflow, "Material accounting overflow.", {}};
        if (accounted != s.stock_area)
            return Error{ErrorCode::area_mismatch, "Material is not conserved.", {}};
        return s;
    } catch (const std::bad_alloc&) {
        return Error{ErrorCode::resource_limit, "Allocation failed.", {}};
    }
}
}
