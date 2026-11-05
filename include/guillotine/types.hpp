#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace guillotine {
using Unit = std::uint64_t;
inline constexpr Unit max_dimension = 1'000'000;
inline constexpr Unit max_kerf = 1'000'000;
inline constexpr std::size_t max_items = 128;
inline constexpr std::size_t max_sheets = 32;
inline constexpr std::size_t max_cuts = 256;
inline constexpr std::size_t max_nodes = 544;
inline constexpr std::uint32_t no_node = UINT32_MAX;

struct Rect {
    Unit x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Rect&) const = default;
};
struct Stock {
    Unit width = 0, height = 0;
    bool operator==(const Stock&) const = default;
};
struct Item {
    std::string id;
    Unit width = 0, height = 0;
    bool allow_rotation = false;
    bool operator==(const Item&) const = default;
};
struct Request {
    Stock stock;
    Unit kerf = 0;
    std::uint32_t sheet_limit = 1;
    std::vector<Item> items;
    bool operator==(const Request&) const = default;
};

enum class NodeKind { leaf, cut, placement };
enum class Axis { vertical, horizontal };
struct Node {
    Rect region;
    NodeKind kind = NodeKind::leaf;
    Axis axis = Axis::vertical;
    Unit offset = 0;
    std::uint32_t first = no_node, second = no_node;
    Rect kerf;
    std::string item_id;
    Unit rotation = 0;
    bool operator==(const Node&) const = default;
};
struct Certificate {
    std::vector<std::uint32_t> roots;
    std::vector<Node> nodes;
    bool operator==(const Certificate&) const = default;
};
struct Summary {
    Unit sheets = 0, placed_items = 0, cuts = 0, remaining_leaves = 0;
    Unit used_area = 0, kerf_area = 0, remaining_area = 0, stock_area = 0;
    bool operator==(const Summary&) const = default;
};

enum class ErrorCode {
    invalid_request, unplaceable, certificate_invalid, out_of_bounds,
    item_multiplicity, rotation_not_allowed, area_mismatch,
    arithmetic_overflow, resource_limit, invalid_json, io_error
};
inline const char* error_name(ErrorCode code) {
    switch (code) {
    case ErrorCode::invalid_request: return "INVALID_REQUEST";
    case ErrorCode::unplaceable: return "UNPLACEABLE";
    case ErrorCode::certificate_invalid: return "CERTIFICATE_INVALID";
    case ErrorCode::out_of_bounds: return "OUT_OF_BOUNDS";
    case ErrorCode::item_multiplicity: return "ITEM_MULTIPLICITY";
    case ErrorCode::rotation_not_allowed: return "ROTATION_NOT_ALLOWED";
    case ErrorCode::area_mismatch: return "AREA_MISMATCH";
    case ErrorCode::arithmetic_overflow: return "ARITHMETIC_OVERFLOW";
    case ErrorCode::resource_limit: return "RESOURCE_LIMIT";
    case ErrorCode::invalid_json: return "INVALID_JSON";
    case ErrorCode::io_error: return "IO_ERROR";
    }
    return "CERTIFICATE_INVALID";
}
struct Error {
    ErrorCode code;
    std::string message;
    std::vector<std::string> unplaced;
};
template<class T> using Result = std::variant<T, Error>;
}
