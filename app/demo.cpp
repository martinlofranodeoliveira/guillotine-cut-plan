#include "demo.hpp"
#include "json_io.hpp"
#include "guillotine/planner.hpp"
#include "guillotine/verify.hpp"
#include <array>
#include <iostream>
#include <set>
#include <stdexcept>

namespace guillotine::app {
namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("Demo agreement failed.");
}
}
int demo(bool machine_readable) {
    const Request request{{10, 6}, 1, 2, {{"A", 6, 6, false}, {"B", 3, 6, false}}};
    auto result = plan(request);
    require(std::holds_alternative<Certificate>(result));
    const auto& c = std::get<Certificate>(result);
    require(c.roots.size() == 1 && c.nodes.size() == 3);
    require(c.roots[0] < c.nodes.size());
    const auto& cut = c.nodes[c.roots[0]];
    require(cut.kind == NodeKind::cut && cut.axis == Axis::vertical);
    require(cut.region == Rect{0, 0, 10, 6} && cut.offset == 6);
    require(cut.kerf == Rect{6, 0, 1, 6});
    require(cut.first < c.nodes.size() && cut.second < c.nodes.size());
    require(cut.first != cut.second && cut.first != c.roots[0] && cut.second != c.roots[0]);
    const auto& a = c.nodes[cut.first];
    const auto& b = c.nodes[cut.second];
    require(a.kind == NodeKind::placement && a.item_id == "A" && a.rotation == 0);
    require(b.kind == NodeKind::placement && b.item_id == "B" && b.rotation == 0);
    require(a.region == Rect{0, 0, 6, 6} && b.region == Rect{7, 0, 3, 6});
    // Independent discrete replay of this small fixture, not the library verifier.
    std::array<unsigned, 60> cells{};
    Unit independently_used = 0, independently_lost = 0;
    const auto paint = [&](const Rect& r, bool strip) {
        require(r.x <= 10 && r.y <= 6 && r.w <= 10 - r.x && r.h <= 6 - r.y);
        for (Unit y = r.y; y < r.y + r.h; ++y) {
            for (Unit x = r.x; x < r.x + r.w; ++x) {
                auto& cell = cells[static_cast<std::size_t>(y * 10 + x)];
                require(cell == 0);
                cell = strip ? 2U : 1U;
                if (strip) ++independently_lost; else ++independently_used;
            }
        }
    };
    paint(a.region, false);
    paint(cut.kerf, true);
    paint(b.region, false);
    for (auto cell : cells) require(cell != 0);
    require(independently_used == 54 && independently_lost == 6);
    auto verified = verify_certificate(request, c);
    require(std::holds_alternative<Summary>(verified));
    const auto& s = std::get<Summary>(verified);
    require(s.sheets == 1 && s.used_area == independently_used && s.kerf_area == independently_lost);
    require(s.stock_area == 60 && s.remaining_area == 0 && s.cuts == 1 && s.placed_items == 2);
    if (machine_readable)
        std::cout << wire::Json{{"ok", true}, {"summary", wire::encode_summary(s)}}.dump() << '\n';
    else
        std::cout << "PASS sheets=1 used_area=54 kerf_area=6\n";
    return 0;
}
}
