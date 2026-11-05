#include "test.hpp"
#include <map>
#include <set>
#include <vector>

namespace test {
namespace {
// Small-domain oracle: reconstructs the tree and paints every material cell.
// It never calls the production verifier and does not use its geometry helpers.
struct Piece { std::size_t sheet; Rect rect; };
bool overlaps(const Rect& a, const Rect& b) {
    return a.w && a.h && b.w && b.h && a.x < b.x + b.w && b.x < a.x + a.w &&
        a.y < b.y + b.h && b.y < a.y + a.h;
}
void oracle(const Request& r, const Certificate& c) {
    CHECK(r.stock.width <= 10 && r.stock.height <= 10);
    std::vector<Piece> placements, strips;
    std::vector<unsigned> cells(c.roots.size() * 100, 0);
    std::set<std::string> ids;
    std::set<std::uint32_t> reached;
    std::map<std::string, Item> items;
    for (const auto& item : r.items) items.emplace(item.id, item);
    const auto paint = [&](std::size_t sheet, const Rect& box, unsigned tag) {
        CHECK(box.x <= r.stock.width && box.y <= r.stock.height);
        CHECK(box.w <= r.stock.width - box.x && box.h <= r.stock.height - box.y);
        for (Unit y = box.y; y < box.y + box.h; ++y)
            for (Unit x = box.x; x < box.x + box.w; ++x) {
                auto& cell = cells[sheet * 100 + static_cast<std::size_t>(y * 10 + x)];
                CHECK(cell == 0);
                cell = tag;
            }
    };
    struct Work { std::uint32_t node; std::size_t sheet; Rect expected; };
    std::vector<Work> work;
    for (std::size_t i = 0; i < c.roots.size(); ++i)
        work.push_back({c.roots[i], i, {0, 0, r.stock.width, r.stock.height}});
    while (!work.empty()) {
        const auto entry = work.back(); work.pop_back();
        CHECK(entry.node < c.nodes.size() && reached.insert(entry.node).second);
        const auto& n = c.nodes[entry.node];
        CHECK(n.region == entry.expected && n.region.w && n.region.h);
        const auto box = entry.expected;
        if (n.kind == NodeKind::placement) {
            CHECK(items.contains(n.item_id) && ids.insert(n.item_id).second);
            const auto& item = items.at(n.item_id);
            CHECK(n.rotation == 0 || (n.rotation == 90 && item.allow_rotation));
            CHECK(box.w == (n.rotation == 0 ? item.width : item.height));
            CHECK(box.h == (n.rotation == 0 ? item.height : item.width));
            placements.push_back({entry.sheet, box});
            paint(entry.sheet, box, 1);
        } else if (n.kind == NodeKind::leaf) {
            paint(entry.sheet, box, 3);
        } else {
            CHECK(n.kind == NodeKind::cut);
            Rect low = box, high = box, lost = box;
            Unit remainder = 0;
            if (n.axis == Axis::vertical) {
                CHECK(n.offset > 0 && n.offset < box.w && r.kerf <= box.w - n.offset);
                low.w = n.offset;
                lost.x = box.x + n.offset; lost.w = r.kerf;
                remainder = box.w - n.offset - r.kerf;
                high.x = lost.x + r.kerf; high.w = remainder;
            } else {
                CHECK(n.axis == Axis::horizontal);
                CHECK(n.offset > 0 && n.offset < box.h && r.kerf <= box.h - n.offset);
                low.h = n.offset;
                lost.y = box.y + n.offset; lost.h = r.kerf;
                remainder = box.h - n.offset - r.kerf;
                high.y = lost.y + r.kerf; high.h = remainder;
            }
            CHECK(n.kerf == lost);
            strips.push_back({entry.sheet, lost});
            paint(entry.sheet, lost, 2);
            work.push_back({n.first, entry.sheet, low});
            if (remainder) work.push_back({n.second, entry.sheet, high});
            else CHECK(n.second == no_node);
        }
    }
    CHECK(reached.size() == c.nodes.size() && ids.size() == r.items.size());
    for (std::size_t s = 0; s < c.roots.size(); ++s)
        for (Unit y = 0; y < r.stock.height; ++y)
            for (Unit x = 0; x < r.stock.width; ++x)
                CHECK(cells[s * 100 + static_cast<std::size_t>(y * 10 + x)] != 0);
    for (std::size_t i = 0; i < placements.size(); ++i) {
        for (std::size_t j = i + 1; j < placements.size(); ++j)
            if (placements[i].sheet == placements[j].sheet)
                CHECK(!overlaps(placements[i].rect, placements[j].rect));
        for (const auto& strip : strips)
            if (placements[i].sheet == strip.sheet) CHECK(!overlaps(placements[i].rect, strip.rect));
    }
}
}
void geometry() {
    std::size_t pairs = 0;
    for (Unit aw = 1; aw <= 3; ++aw) for (Unit ah = 1; ah <= 3; ++ah)
        for (Unit bw = 1; bw <= 3; ++bw) for (Unit bh = 1; bh <= 3; ++bh) {
            Request r{{3, 3}, 0, 2, {{"A", aw, ah, false}, {"B", bw, bh, false}}};
            auto c = value(plan(r));
            oracle(r, c);
            CHECK(value(verify_certificate(r, c)).used_area == aw * ah + bw * bh);
            ++pairs;
        }
    CHECK(pairs == 81);
    oracle(fixture(), value(plan(fixture())));
    // More than two items exercises repeated fragmentation and multiple roots.
    // Each item fits fresh stock with a full kerf, so four sheets suffice.
    std::size_t fragmented = 0;
    for (Unit kerf = 0; kerf <= 2; ++kerf) {
        for (Unit seed = 0; seed < 32; ++seed) {
            Request r{{7, 7}, kerf, 4, {}};
            for (Unit i = 0; i < 4; ++i)
                r.items.push_back({"mixed_" + std::to_string(i),
                    1 + (seed + 3 * i) % 4, 1 + (seed / 4 + 5 * i) % 4, i % 2 == 0});
            oracle(r, value(plan(r)));
            ++fragmented;
        }
    }
    CHECK(fragmented == 96);
    for (Unit kerf = 0; kerf <= 3; ++kerf)
        for (Unit w = 1; w <= 3; ++w) for (Unit h = 1; h <= 3; ++h) {
            Request r{{3, 3}, kerf, 2, {{"A", w, h, true}, {"B", 1, 1, true}}};
            auto result = plan(r);
            if (auto c = std::get_if<Certificate>(&result)) oracle(r, *c);
            else error(result, ErrorCode::unplaceable);
        }
    // Oracle itself detects a moved rectangle, independently of verify_certificate.
    auto bad = value(plan(fixture()));
    for (auto& n : bad.nodes) if (n.kind == NodeKind::placement && n.item_id == "B") n.region.x = 6;
    bool rejected = false;
    try { oracle(fixture(), bad); } catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
}
}
