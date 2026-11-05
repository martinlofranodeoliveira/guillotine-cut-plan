#pragma once
#include <stdexcept>
#include <string>
#include <variant>
#include "guillotine/planner.hpp"
#include "guillotine/verify.hpp"

#define CHECK(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #__VA_ARGS__); } while (false)

namespace test {
using namespace guillotine;
template<class T> T value(Result<T> result) {
    if (const auto* error = std::get_if<Error>(&result))
        throw std::runtime_error(std::string(error_name(error->code)) + ": " + error->message);
    return std::get<T>(std::move(result));
}
template<class T> void error(const Result<T>& result, ErrorCode code) {
    CHECK(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).code == code);
}
inline Request fixture() {
    return {{10, 6}, 1, 2, {{"A", 6, 6, false}, {"B", 3, 6, false}}};
}
void geometry();
void adversarial();
void json();
}
