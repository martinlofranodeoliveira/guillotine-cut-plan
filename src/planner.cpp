#include "guillotine/planner.hpp"
#include "checked.hpp"
#include "validation.hpp"
#include <algorithm>
#include <new>
#include <optional>
#include <tuple>

namespace guillotine {
namespace {
struct Ranked {
    const Item* item;
    Unit area, longest;
};
struct Leaf {
    std::uint32_t node, sheet;
};
struct Candidate {
    std::size_t leaf;
    Unit w, h, rotation;
    std::tuple<Unit, std::uint32_t, Unit, Unit, Unit, std::uint32_t> score;
};
bool fits(const Rect& leaf, Unit w, Unit h, Unit kerf) {
    if (w > leaf.w || h > leaf.h) return false;
    const Unit dx = leaf.w - w, dy = leaf.h - h;
    return (dx == 0 || dx >= kerf) && (dy == 0 || dy >= kerf);
}
}

Result<Certificate> plan(const Request& r) {
    try {
        if (auto error = detail::validate_request(r)) return *error;
        std::vector<Ranked> ordered;
        ordered.reserve(r.items.size());
        for (const auto& item : r.items) {
            Unit area = 0;
            if (!detail::multiply(item.width, item.height, area))
                return Error{ErrorCode::arithmetic_overflow, "Item area overflow.", {}};
            ordered.push_back({&item, area, std::max(item.width, item.height)});
        }
        std::sort(ordered.begin(), ordered.end(), [](const Ranked& a, const Ranked& b) {
            if (a.area != b.area) return a.area > b.area;
            if (a.longest != b.longest) return a.longest > b.longest;
            return a.item->id < b.item->id;
        });
        Certificate cert;
        cert.nodes.reserve(max_nodes);
        cert.roots.reserve(max_sheets);
        std::vector<Leaf> leaves;
        leaves.reserve(max_nodes);
        std::vector<std::string> unplaced;
        unplaced.reserve(r.items.size());

        for (const auto& ranked : ordered) {
            const Item& item = *ranked.item;
            std::optional<Candidate> best;
            const auto evaluate = [&](std::size_t index) -> bool {
                const auto leaf = leaves[index];
                const auto region = cert.nodes[leaf.node].region;
                Unit leaf_area = 0;
                if (!detail::multiply(region.w, region.h, leaf_area)) return false;
                for (Unit rotation : {Unit{0}, Unit{90}}) {
                    if (rotation == 90 && (!item.allow_rotation || item.width == item.height)) continue;
                    const Unit w = rotation == 0 ? item.width : item.height;
                    const Unit h = rotation == 0 ? item.height : item.width;
                    if (!fits(region, w, h, r.kerf)) continue;
                    Candidate candidate{index, w, h, rotation,
                        {leaf_area - ranked.area, leaf.sheet, region.y, region.x, rotation, leaf.node}};
                    if (!best || candidate.score < best->score) best = candidate;
                }
                return true;
            };
            for (std::size_t i = 0; i < leaves.size(); ++i) {
                if (!evaluate(i))
                    return Error{ErrorCode::arithmetic_overflow, "Leaf area overflow.", {}};
            }
            if (!best && cert.roots.size() < r.sheet_limit) {
                const Rect root{0, 0, r.stock.width, r.stock.height};
                const bool normal = fits(root, item.width, item.height, r.kerf);
                const bool rotated = item.allow_rotation && fits(root, item.height, item.width, r.kerf);
                if (normal || rotated) {
                    if (cert.nodes.size() >= max_nodes)
                        return Error{ErrorCode::resource_limit, "Node limit exceeded.", {}};
                    const auto node = static_cast<std::uint32_t>(cert.nodes.size());
                    const auto sheet = static_cast<std::uint32_t>(cert.roots.size());
                    Node n;
                    n.region = root;
                    cert.nodes.push_back(n);
                    cert.roots.push_back(node);
                    leaves.push_back({node, sheet});
                    if (!evaluate(leaves.size() - 1))
                        return Error{ErrorCode::arithmetic_overflow, "Leaf area overflow.", {}};
                }
            }
            if (!best) {
                unplaced.push_back(item.id);
                continue;
            }
            const auto chosen = *best;
            const Leaf selected = leaves[chosen.leaf];
            leaves.erase(leaves.begin() + static_cast<std::ptrdiff_t>(chosen.leaf));
            std::uint32_t current = selected.node;
            const auto split = [&](std::uint32_t index, Axis axis, Unit offset) -> Result<std::uint32_t> {
                const Rect region = cert.nodes[index].region;
                const Unit extent = axis == Axis::vertical ? region.w : region.h;
                Unit consumed = 0, edge = 0, tail = 0;
                const Unit origin = axis == Axis::vertical ? region.x : region.y;
                if (!detail::add(offset, r.kerf, consumed) || consumed > extent ||
                    !detail::add(origin, offset, edge) || !detail::add(edge, r.kerf, tail))
                    return Error{ErrorCode::arithmetic_overflow, "Split arithmetic failed.", {}};
                const Unit remainder = extent - consumed;
                const std::size_t children = remainder == 0 ? 1 : 2;
                if (cert.nodes.size() > max_nodes - children || leaves.size() >= max_nodes)
                    return Error{ErrorCode::resource_limit, "Fragmentation limit exceeded.", {}};
                Node cut;
                cut.region = region;
                cut.kind = NodeKind::cut;
                cut.axis = axis;
                cut.offset = offset;
                cut.first = static_cast<std::uint32_t>(cert.nodes.size());
                cut.kerf = axis == Axis::vertical
                    ? Rect{edge, region.y, r.kerf, region.h}
                    : Rect{region.x, edge, region.w, r.kerf};
                Node first;
                first.region = axis == Axis::vertical
                    ? Rect{region.x, region.y, offset, region.h}
                    : Rect{region.x, region.y, region.w, offset};
                cert.nodes.push_back(first);
                if (remainder != 0) {
                    cut.second = static_cast<std::uint32_t>(cert.nodes.size());
                    Node second;
                    second.region = axis == Axis::vertical
                        ? Rect{tail, region.y, remainder, region.h}
                        : Rect{region.x, tail, region.w, remainder};
                    cert.nodes.push_back(second);
                    leaves.push_back({cut.second, selected.sheet});
                }
                cert.nodes[index] = cut;
                return cut.first;
            };
            if (chosen.w < cert.nodes[current].region.w) {
                auto result = split(current, Axis::vertical, chosen.w);
                if (auto error = std::get_if<Error>(&result)) return *error;
                current = std::get<std::uint32_t>(result);
            }
            if (chosen.h < cert.nodes[current].region.h) {
                auto result = split(current, Axis::horizontal, chosen.h);
                if (auto error = std::get_if<Error>(&result)) return *error;
                current = std::get<std::uint32_t>(result);
            }
            cert.nodes[current].kind = NodeKind::placement;
            cert.nodes[current].item_id = item.id;
            cert.nodes[current].rotation = chosen.rotation;
        }
        if (!unplaced.empty())
            return Error{ErrorCode::unplaceable,
                "Heuristic left items unplaced; this does not prove infeasibility.", std::move(unplaced)};
        return cert;
    } catch (const std::bad_alloc&) {
        return Error{ErrorCode::resource_limit, "Allocation failed.", {}};
    }
}
}
