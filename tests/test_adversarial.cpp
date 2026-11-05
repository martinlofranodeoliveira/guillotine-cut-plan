#include "test.hpp"
#include "guillotine/summary.hpp"
#include <limits>

namespace test {
void adversarial() {
    const auto r = fixture();
    const auto good = value(plan(r));
    std::size_t a = 0, b = 0;
    for (std::size_t i = 0; i < good.nodes.size(); ++i) {
        if (good.nodes[i].item_id == "A") a = i;
        if (good.nodes[i].item_id == "B") b = i;
    }
    auto c = good; c.nodes[a].region.x = 5;
    error(verify_certificate(r, c), ErrorCode::out_of_bounds);
    c = good; c.nodes[b].item_id = "A";
    error(verify_certificate(r, c), ErrorCode::item_multiplicity);
    error(summarize(r, c), ErrorCode::item_multiplicity);
    c.nodes[a].region.x = std::numeric_limits<Unit>::max();
    error(verify_certificate(r, c), ErrorCode::out_of_bounds); // Precedence over duplicate ID.
    c = good; c.nodes[b].item_id = "unknown";
    error(verify_certificate(r, c), ErrorCode::item_multiplicity);
    c = good; c.nodes[b].rotation = 90;
    error(verify_certificate(r, c), ErrorCode::rotation_not_allowed);
    c = good; c.nodes[0].kerf.w = 0;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[0].offset = std::numeric_limits<Unit>::max();
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[0].second = no_node;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[0].first = 0;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[0].second = c.nodes[0].first;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[0].first = 9999;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.roots.push_back(c.roots[0]);
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.roots[0] = 9999;
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    Node extra; extra.region = {0, 0, 1, 1}; c.nodes.push_back(extra);
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes.resize(max_nodes + 1);
    error(verify_certificate(r, c), ErrorCode::resource_limit);
    c = good; c.nodes[b].region.x = 6; // In stock, but encroaches on the kerf.
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);
    c = good; c.nodes[a].first = 0; // Inactive field in a placement.
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);

    // Mutated root region: root region does not match stock dimensions.
    c = good;
    c.nodes[c.roots[0]].region = {0, 0, r.stock.width - 1, r.stock.height};
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);

    // Multi-node cycle with all placement IDs retained exactly once. Removing
    // a placement would instead exercise the earlier ITEM_MULTIPLICITY stage.
    c = good;
    const auto loop = static_cast<std::uint32_t>(c.nodes.size());
    c.nodes.push_back(c.nodes[0]);
    c.nodes[0].first = loop;
    c.nodes[loop].first = c.roots[0];
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);

    // Cut offset equals parent extent (offset >= extent is invalid).
    c = good;
    c.nodes[0].offset = 10; // region.w is 10
    error(verify_certificate(r, c), ErrorCode::certificate_invalid);

    // A valid horizontal-first tree is accepted: verification is not heuristic reproduction.
    Request other{{4, 4}, 0, 1, {{"square", 2, 2, false}}};
    Certificate alternate;
    alternate.roots = {0};
    alternate.nodes.resize(5);
    auto& root = alternate.nodes[0];
    root.kind = NodeKind::cut; root.region = {0, 0, 4, 4};
    root.axis = Axis::horizontal; root.offset = 2; root.first = 1; root.second = 2;
    root.kerf = {0, 2, 4, 0};
    auto& left = alternate.nodes[1];
    left.kind = NodeKind::cut; left.region = {0, 0, 4, 2}; left.offset = 2;
    left.first = 3; left.second = 4; left.kerf = {2, 0, 0, 2};
    alternate.nodes[2].region = {0, 2, 4, 2};
    alternate.nodes[3].kind = NodeKind::placement;
    alternate.nodes[3].region = {0, 0, 2, 2}; alternate.nodes[3].item_id = "square";
    alternate.nodes[4].region = {2, 0, 2, 2};
    CHECK(value(verify_certificate(other, alternate)) == Summary{1, 1, 2, 2, 4, 0, 12, 16});
}
}
