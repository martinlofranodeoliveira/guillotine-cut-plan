#pragma once
#include <istream>
#include <string_view>
#include <nlohmann/json.hpp>
#include "guillotine/types.hpp"

static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 &&
              NLOHMANN_JSON_VERSION_MINOR == 11 &&
              NLOHMANN_JSON_VERSION_PATCH == 2,
              "The CLI requires nlohmann/json 3.11.2.");

namespace guillotine::wire {
using Json = nlohmann::json;
inline constexpr std::size_t max_bytes = 524288;
inline constexpr std::size_t max_depth = 16;
Result<Json> parse(std::string_view text);
Result<Json> read(std::istream& stream);
Result<Request> decode_request(const Json& value);
Result<Certificate> decode_certificate(const Json& value);
Json encode_request(const Request& request);
Json encode_certificate(const Certificate& certificate);
Json encode_summary(const Summary& summary);
Json encode_error(const Error& error);
}
