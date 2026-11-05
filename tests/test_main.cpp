#include "test.hpp"
#include "guillotine/summary.hpp"
#include "json_io.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

namespace {
using namespace guillotine;
using test::value;
void acceptance() {
    auto r = test::fixture();
    const auto original = r;
    const auto c = value(plan(r));
    CHECK(r == original);
    const auto s = value(summarize(r, c));
    CHECK(s == Summary{1, 2, 1, 0, 54, 6, 0, 60});
    CHECK(c.nodes[c.roots[0]].kerf == Rect{6, 0, 1, 6});
    bool a = false, b = false;
    for (const auto& n : c.nodes) {
        if (n.kind != NodeKind::placement) continue;
        if (n.item_id == "A") a = n.region == Rect{0, 0, 6, 6};
        if (n.item_id == "B") b = n.region == Rect{7, 0, 3, 6};
    }
    CHECK(a && b);
    r.items[1].width = 4;
    const auto two = value(plan(r));
    CHECK(value(verify_certificate(r, two)).sheets == 2);
    r.sheet_limit = 1;
    auto failure = plan(r);
    test::error(failure, ErrorCode::unplaceable);
    CHECK(std::get<Error>(failure).unplaced == std::vector<std::string>{"B"});

    r = {{4, 3}, 0, 1, {{"turn", 3, 4, false}}};
    test::error(plan(r), ErrorCode::unplaceable);
    r.items[0].allow_rotation = true;
    auto rotated = value(plan(r));
    CHECK(rotated.nodes.size() == 1);
    CHECK(rotated.nodes[0].region == Rect{0, 0, 4, 3});
    CHECK(rotated.nodes[0].rotation == 90);
    CHECK(value(verify_certificate(r, rotated)).used_area == 12);

    r = {{10, 6}, max_kerf, 1, {{"exact", 10, 6, false}}};
    auto exact = value(plan(r));
    CHECK(exact.nodes.size() == 1);
    CHECK(value(verify_certificate(r, exact)).cuts == 0);
    CHECK(value(verify_certificate(r, exact)).kerf_area == 0);
    r.items.clear();
    CHECK(value(plan(r)).nodes.empty());
    CHECK(value(summarize(r, value(plan(r)))) == Summary{});
}
void deterministic() {
    Request r{{8, 8}, 0, 3, {{"z", 4, 2, true}, {"B", 2, 4, true}, {"A", 3, 3, true}, {"a", 1, 1, false}}};
    std::sort(r.items.begin(), r.items.end(), [](const Item& a, const Item& b) { return a.id < b.id; });
    const auto canonical = wire::Json{{"request", wire::encode_request(r)},
        {"certificate", wire::encode_certificate(value(plan(r)))}}.dump();
    std::size_t count = 0;
    do {
        const auto c = value(plan(r));
        CHECK(value(verify_certificate(r, c)).placed_items == 4);
        CHECK((wire::Json{{"request", wire::encode_request(r)},
            {"certificate", wire::encode_certificate(c)}}.dump() == canonical));
        ++count;
    } while (std::next_permutation(r.items.begin(), r.items.end(),
        [](const Item& a, const Item& b) { return a.id < b.id; }));
    CHECK(count == 24);
    // Area and longest-side ties resolve by ASCII ID, not insertion order.
    r = {{4, 4}, 0, 2, {{"a", 4, 2, false}, {"A", 4, 2, false}}};
    auto c = value(plan(r));
    bool found = false;
    for (const auto& n : c.nodes) if (n.kind == NodeKind::placement && n.item_id == "A") {
        CHECK(n.region.x == 0 && n.region.y == 0);
        found = true;
    }
    CHECK(found);
}
void limits_and_gaps() {
    auto r = test::fixture();
    r.stock.width = 0;
    test::error(plan(r), ErrorCode::invalid_request);
    r = test::fixture(); r.kerf = max_kerf + 1;
    test::error(plan(r), ErrorCode::invalid_request);
    r = test::fixture(); r.sheet_limit = 33;
    test::error(plan(r), ErrorCode::invalid_request);
    r = test::fixture(); r.items[1].id = "A";
    test::error(plan(r), ErrorCode::invalid_request);
    r.items[1].id = "bad/name";
    test::error(plan(r), ErrorCode::invalid_request);
    r = test::fixture(); r.items[0].width = std::numeric_limits<Unit>::max();
    test::error(plan(r), ErrorCode::invalid_request);
    r = {{max_dimension, max_dimension}, 0, 32, {{"large", max_dimension, max_dimension, false}}};
    CHECK(value(summarize(r, value(plan(r)))).used_area == 1'000'000'000'000ULL);
    r = {{4, 3}, 2, 1, {{"gap", 3, 3, false}}};
    test::error(plan(r), ErrorCode::unplaceable);
    r.items[0].width = 2; // Gap equals kerf: cut, but no second leaf.
    auto c = value(plan(r));
    CHECK(c.nodes.size() == 2 && c.nodes[0].second == no_node);
    CHECK(value(summarize(r, c)).kerf_area == 6);
    r = {{3, 4}, 2, 1, {{"gap", 3, 3, false}}};
    test::error(plan(r), ErrorCode::unplaceable);
    // Maximum item count, including many fragments and adjacent zero-kerf pieces.
    r = {{16, 8}, 0, 32, {}};
    for (unsigned i = 0; i < 128; ++i) r.items.push_back({"piece_" + std::to_string(i), 1, 1, false});
    c = value(plan(r));
    CHECK(c.nodes.size() <= max_nodes);
    CHECK(value(summarize(r, c)).used_area == 128);
    r.items.push_back({"extra", 1, 1, false});
    test::error(plan(r), ErrorCode::invalid_request);
    // All 32 sheets are usable; the next item is a bounded heuristic failure.
    r = {{1, 1}, 1, 32, {}};
    for (unsigned i = 0; i < 32; ++i) r.items.push_back({"s" + std::to_string(i), 1, 1, false});
    CHECK(value(summarize(r, value(plan(r)))).sheets == 32);
    r.items.push_back({"zz", 1, 1, false});
    test::error(plan(r), ErrorCode::unplaceable);
}
}
int main() {
    try {
        acceptance();
        deterministic();
        limits_and_gaps();
        test::geometry();
        test::adversarial();
        test::json();
        std::cout << "PASS acceptance, determinism, limits, independent geometry, adversarial certificates, JSON\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
