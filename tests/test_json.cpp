#include "test.hpp"
#include "json_io.hpp"
#include <sstream>

namespace test {
void json() {
    using wire::Json;
    error(wire::parse("{\"a\":1,\"a\":2}"), ErrorCode::invalid_json);
    error(wire::parse("{\"width\":1,\"\\u0077idth\":2}"), ErrorCode::invalid_json);
    error(wire::parse("{\"outer\":{\"x\":1,\"x\":2}}"), ErrorCode::invalid_json);
    for (const auto* text : {"1.0", "1e0", "1e999", "-1", "-0", "18446744073709551616", "{} trailing"})
        error(wire::parse(text), ErrorCode::invalid_json);
    error(wire::parse(std::string(17, '[') + "0" + std::string(17, ']')), ErrorCode::invalid_json);
    CHECK(value(wire::parse(std::string(16, '[') + "0" + std::string(16, ']'))).is_array());
    error(wire::parse(std::string(wire::max_bytes + 1, ' ')), ErrorCode::resource_limit);
    error(wire::parse(std::string("\"") + std::string(65, 'a') + "\""), ErrorCode::invalid_json);
    const std::string many_tokens = [] {
        std::string s = "[0";
        for (unsigned i = 0; i < 33000; ++i) s += ",0";
        return s + "]";
    }();
    error(wire::parse(many_tokens), ErrorCode::invalid_json);
    auto request_json = wire::encode_request(fixture());
    auto decoded = value(wire::decode_request(value(wire::parse(request_json.dump()))));
    CHECK(decoded == fixture());
    for (const auto& wrong : {Json(true), Json("10"), Json(1.0), Json(-1), Json(UINT64_MAX), Json(0)}) {
        auto j = request_json; j["stock"]["width"] = wrong;
        error(wire::decode_request(j), ErrorCode::invalid_request);
    }
    auto j = request_json; j["extra"] = 1;
    error(wire::decode_request(j), ErrorCode::invalid_request);
    j = request_json; j["items"][0]["allow_rotation"] = 1;
    error(wire::decode_request(j), ErrorCode::invalid_request);
    j = request_json; j["items"] = Json::array();
    for (unsigned i = 0; i < 129; ++i) j["items"].push_back(request_json["items"][0]);
    error(wire::decode_request(j), ErrorCode::resource_limit);
    const auto c = value(plan(decoded));
    const auto encoded = wire::encode_certificate(c);
    CHECK(value(wire::decode_certificate(value(wire::parse(encoded.dump())))) == c);
    j = encoded; j["nodes"][1]["region"]["x"] = UINT64_MAX;
    error(verify_certificate(decoded, value(wire::decode_certificate(j))), ErrorCode::out_of_bounds);
    j = encoded; j["nodes"][0]["axis"] = "diagonal";
    error(wire::decode_certificate(j), ErrorCode::certificate_invalid);
    j = encoded; j["nodes"][0]["first"] = true;
    error(wire::decode_certificate(j), ErrorCode::certificate_invalid);
    j = encoded; j["nodes"][1]["unexpected"] = 0;
    error(wire::decode_certificate(j), ErrorCode::certificate_invalid);
    j = encoded; j["nodes"] = Json::array();
    for (unsigned i = 0; i <= max_nodes; ++i) j["nodes"].push_back(encoded["nodes"][1]);
    error(wire::decode_certificate(j), ErrorCode::resource_limit);
    std::istringstream stream(request_json.dump());
    CHECK(value(wire::read(stream)) == request_json);
    std::istringstream too_big(std::string(wire::max_bytes + 1, ' '));
    error(wire::read(too_big), ErrorCode::resource_limit);
    std::istringstream broken("{}"); broken.setstate(std::ios::badbit);
    error(wire::read(broken), ErrorCode::io_error);
}
}
