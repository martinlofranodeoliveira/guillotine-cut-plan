#include "json_io.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <set>

namespace guillotine::wire {
namespace {
struct Gate final : nlohmann::json_sax<Json> {
    struct Frame { bool object; std::set<std::string> keys; };
    std::vector<Frame> stack;
    std::size_t tokens = 0;
    bool tick() { return ++tokens <= 32768; }
    bool null() override { return tick(); }
    bool boolean(bool) override { return tick(); }
    bool number_integer(number_integer_t value) override { return value >= 0 && tick(); }
    bool number_unsigned(number_unsigned_t) override { return tick(); }
    bool number_float(number_float_t, const string_t&) override { return false; }
    bool string(string_t& text) override { return text.size() <= 64 && tick(); }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override {
        if (!tick() || stack.size() >= max_depth) return false;
        stack.push_back({true, {}});
        return true;
    }
    bool key(string_t& key_text) override {
        return tick() && key_text.size() <= 64 && !stack.empty() && stack.back().object &&
            stack.back().keys.insert(key_text).second;
    }
    bool end_object() override {
        if (stack.empty() || !stack.back().object) return false;
        stack.pop_back();
        return tick();
    }
    bool start_array(std::size_t) override {
        if (!tick() || stack.size() >= max_depth) return false;
        stack.push_back({false, {}});
        return true;
    }
    bool end_array() override {
        if (stack.empty() || stack.back().object) return false;
        stack.pop_back();
        return tick();
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override {
        return false;
    }
};
struct DecodeFailure { Error error; };
[[noreturn]] void fail(ErrorCode code, const char* message) {
    throw DecodeFailure{{code, message, {}}};
}
void object(const Json& j, std::initializer_list<const char*> keys, ErrorCode code) {
    if (!j.is_object() || j.size() != keys.size()) fail(code, "Invalid or unknown object fields.");
    for (const auto key : keys) if (!j.contains(key)) fail(code, "A required field is missing.");
}
Unit natural(const Json& j, Unit low, Unit high, ErrorCode code) {
    Unit n = 0;
    if (j.type() == Json::value_t::number_unsigned) {
        n = j.get<Unit>();
    } else if (j.type() == Json::value_t::number_integer) {
        const auto signed_value = j.get<std::int64_t>();
        if (signed_value < 0) fail(code, "Expected a nonnegative integer token.");
        n = static_cast<Unit>(signed_value);
    } else {
        fail(code, "Expected an integer token, not a float, boolean or string.");
    }
    if (n < low || n > high) fail(code, "Integer is outside the field's range.");
    return n;
}
std::string text(const Json& j, ErrorCode code) {
    if (!j.is_string()) fail(code, "Expected a string.");
    const auto& s = j.get_ref<const std::string&>();
    if (s.empty() || s.size() > 64) fail(code, "String length must be in 1..64.");
    return s;
}
Rect rectangle(const Json& j) {
    constexpr auto code = ErrorCode::certificate_invalid;
    object(j, {"x", "y", "w", "h"}, code);
    const Unit top = std::numeric_limits<Unit>::max();
    return {natural(j.at("x"), 0, top, code), natural(j.at("y"), 0, top, code),
            natural(j.at("w"), 0, top, code), natural(j.at("h"), 0, top, code)};
}
Json rect_json(const Rect& r) {
    return {{"x", r.x}, {"y", r.y}, {"w", r.w}, {"h", r.h}};
}
std::uint32_t index(const Json& j) {
    return static_cast<std::uint32_t>(natural(j, 0, no_node - 1, ErrorCode::certificate_invalid));
}
Error allocation_error() { return {ErrorCode::resource_limit, "Allocation failed.", {}}; }
}

Result<Json> parse(std::string_view text_value) {
    try {
        if (text_value.size() > max_bytes)
            return Error{ErrorCode::resource_limit, "JSON exceeds 524288 bytes.", {}};
        // This lexical guard runs before either JSON pass can allocate nested containers.
        bool quoted = false, escaped = false;
        std::size_t depth = 0;
        for (char ch : text_value) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') quoted = false;
            } else if (ch == '"') {
                quoted = true;
            } else if (ch == '{' || ch == '[') {
                if (++depth > max_depth)
                    return Error{ErrorCode::invalid_json, "JSON nesting exceeds 16.", {}};
            } else if (ch == '}' || ch == ']') {
                if (depth == 0) return Error{ErrorCode::invalid_json, "Unbalanced JSON.", {}};
                --depth;
            } else if (ch == '-') {
                return Error{ErrorCode::invalid_json, "Negative numeric tokens are forbidden.", {}};
            }
        }
        Gate gate;
        if (!Json::sax_parse(text_value.begin(), text_value.end(), &gate))
            return Error{ErrorCode::invalid_json,
                "Invalid JSON, duplicate key, float token, excessive token count or string length.", {}};
        return Json::parse(text_value.begin(), text_value.end());
    } catch (const std::bad_alloc&) {
        return allocation_error();
    } catch (const Json::exception&) {
        return Error{ErrorCode::invalid_json, "Invalid JSON.", {}};
    }
}
Result<Json> read(std::istream& stream) {
    try {
        std::string input;
        std::array<char, 4096> buffer{};
        for (;;) {
            stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = stream.gcount();
            if (count > 0) {
                const auto size = static_cast<std::size_t>(count);
                if (size > max_bytes - input.size())
                    return Error{ErrorCode::resource_limit, "JSON exceeds 524288 bytes.", {}};
                input.append(buffer.data(), size);
            }
            if (stream.bad() || (stream.fail() && !stream.eof()))
                return Error{ErrorCode::io_error, "Input read failed.", {}};
            if (stream.eof()) break;
        }
        return parse(input);
    } catch (const std::bad_alloc&) {
        return allocation_error();
    } catch (const std::ios_base::failure&) {
        return Error{ErrorCode::io_error, "Input read failed.", {}};
    }
}
Result<Request> decode_request(const Json& j) {
    try {
        constexpr auto code = ErrorCode::invalid_request;
        object(j, {"stock", "kerf", "sheet_limit", "items"}, code);
        object(j.at("stock"), {"width", "height"}, code);
        Request r;
        r.stock = {natural(j.at("stock").at("width"), 1, max_dimension, code),
                   natural(j.at("stock").at("height"), 1, max_dimension, code)};
        r.kerf = natural(j.at("kerf"), 0, max_kerf, code);
        r.sheet_limit = static_cast<std::uint32_t>(natural(j.at("sheet_limit"), 1, max_sheets, code));
        const auto& items = j.at("items");
        if (!items.is_array()) fail(code, "Items must be an array.");
        if (items.size() > max_items) fail(ErrorCode::resource_limit, "At most 128 items are allowed.");
        r.items.reserve(items.size());
        for (const auto& value : items) {
            object(value, {"id", "width", "height", "allow_rotation"}, code);
            if (!value.at("allow_rotation").is_boolean()) fail(code, "allow_rotation must be boolean.");
            r.items.push_back({text(value.at("id"), code),
                natural(value.at("width"), 1, max_dimension, code),
                natural(value.at("height"), 1, max_dimension, code), value.at("allow_rotation").get<bool>()});
        }
        return r;
    } catch (const DecodeFailure& e) { return e.error; }
      catch (const std::bad_alloc&) { return allocation_error(); }
}
Result<Certificate> decode_certificate(const Json& j) {
    try {
        constexpr auto code = ErrorCode::certificate_invalid;
        object(j, {"roots", "nodes"}, code);
        const auto& roots = j.at("roots");
        const auto& nodes = j.at("nodes");
        if (!roots.is_array() || !nodes.is_array()) fail(code, "Roots and nodes must be arrays.");
        if (roots.size() > max_sheets || nodes.size() > max_nodes)
            fail(ErrorCode::resource_limit, "Certificate array limit exceeded.");
        Certificate c;
        c.roots.reserve(roots.size());
        c.nodes.reserve(nodes.size());
        for (const auto& root : roots) c.roots.push_back(index(root));
        for (const auto& value : nodes) {
            if (!value.is_object() || !value.contains("kind")) fail(code, "Node kind is required.");
            const auto kind = text(value.at("kind"), code);
            Node n;
            if (kind == "leaf") {
                object(value, {"kind", "region"}, code);
                n.kind = NodeKind::leaf;
            } else if (kind == "placement") {
                object(value, {"kind", "region", "item_id", "rotation"}, code);
                n.kind = NodeKind::placement;
                n.item_id = text(value.at("item_id"), code);
                n.rotation = natural(value.at("rotation"), 0, std::numeric_limits<Unit>::max(), code);
            } else if (kind == "cut") {
                object(value, {"kind", "region", "axis", "offset", "first", "second", "kerf"}, code);
                n.kind = NodeKind::cut;
                const auto axis = text(value.at("axis"), code);
                if (axis != "vertical" && axis != "horizontal") fail(code, "Unknown cut axis.");
                n.axis = axis == "vertical" ? Axis::vertical : Axis::horizontal;
                n.offset = natural(value.at("offset"), 0, std::numeric_limits<Unit>::max(), code);
                n.first = index(value.at("first"));
                n.second = value.at("second").is_null() ? no_node : index(value.at("second"));
                n.kerf = rectangle(value.at("kerf"));
            } else {
                fail(code, "Unknown node kind.");
            }
            n.region = rectangle(value.at("region"));
            c.nodes.push_back(std::move(n));
        }
        return c;
    } catch (const DecodeFailure& e) { return e.error; }
      catch (const std::bad_alloc&) { return allocation_error(); }
}
Json encode_request(const Request& r) {
    std::vector<const Item*> items;
    for (const auto& item : r.items) items.push_back(&item);
    std::sort(items.begin(), items.end(), [](const Item* a, const Item* b) { return a->id < b->id; });
    Json list = Json::array();
    for (const auto* item : items)
        list.push_back({{"id", item->id}, {"width", item->width}, {"height", item->height},
                        {"allow_rotation", item->allow_rotation}});
    return {{"stock", {{"width", r.stock.width}, {"height", r.stock.height}}},
            {"kerf", r.kerf}, {"sheet_limit", r.sheet_limit}, {"items", list}};
}
Json encode_certificate(const Certificate& c) {
    Json nodes = Json::array();
    for (const auto& n : c.nodes) {
        Json value = {{"region", rect_json(n.region)}};
        if (n.kind == NodeKind::leaf) value["kind"] = "leaf";
        else if (n.kind == NodeKind::placement) {
            value["kind"] = "placement";
            value["item_id"] = n.item_id;
            value["rotation"] = n.rotation;
        } else {
            value["kind"] = "cut";
            value["axis"] = n.axis == Axis::vertical ? "vertical" : "horizontal";
            value["offset"] = n.offset;
            value["first"] = n.first;
            value["second"] = n.second == no_node ? Json(nullptr) : Json(n.second);
            value["kerf"] = rect_json(n.kerf);
        }
        nodes.push_back(std::move(value));
    }
    return {{"roots", c.roots}, {"nodes", nodes}};
}
Json encode_summary(const Summary& s) {
    return {{"sheets", s.sheets}, {"placed_items", s.placed_items}, {"cuts", s.cuts},
        {"remaining_leaves", s.remaining_leaves}, {"used_area", s.used_area},
        {"kerf_area", s.kerf_area}, {"remaining_area", s.remaining_area}, {"stock_area", s.stock_area}};
}
Json encode_error(const Error& e) {
    return {{"ok", false}, {"error", {{"code", error_name(e.code)},
        {"message", e.message}, {"unplaced", e.unplaced}}}};
}
}
