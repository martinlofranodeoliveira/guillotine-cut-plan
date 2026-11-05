#include "json_io.hpp"
#include "demo.hpp"
#include "guillotine/planner.hpp"
#include "guillotine/verify.hpp"
#include <fstream>
#include <iostream>
#include <new>
#include <string>

namespace {
using namespace guillotine;
int report(const Error& error) {
    std::cout << wire::encode_error(error).dump() << '\n';
    return 2;
}
int usage() {
    std::cerr << "Usage: cutplan plan|verify --input FILE|- [--human]\n"
                 "       cutplan demo [--json]\n";
    return 64;
}
int run(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string command = argv[1];
    if (command == "demo") {
        if (argc == 2) return guillotine::app::demo(false);
        if (argc == 3 && std::string(argv[2]) == "--json") return guillotine::app::demo(true);
        return usage();
    }
    if ((command != "plan" && command != "verify") || (argc != 4 && argc != 5) ||
        std::string(argv[2]) != "--input" || (argc == 5 && std::string(argv[4]) != "--human"))
        return usage();
    const bool human = argc == 5;
    Result<wire::Json> input;
    if (std::string(argv[3]) == "-") input = wire::read(std::cin);
    else {
        std::ifstream stream(argv[3], std::ios::binary);
        if (!stream) return report({ErrorCode::io_error, "Cannot open input file.", {}});
        input = wire::read(stream);
    }
    if (auto e = std::get_if<Error>(&input)) return report(*e);
    const auto& document = std::get<wire::Json>(input);
    if (command == "verify" && (!document.is_object() || document.size() != 2 ||
        !document.contains("request") || !document.contains("certificate")))
        return report({ErrorCode::invalid_request, "Verify requires exactly request and certificate.", {}});
    auto request_result = wire::decode_request(command == "plan" ? document : document.at("request"));
    if (auto e = std::get_if<Error>(&request_result)) return report(*e);
    const auto& request = std::get<Request>(request_result);
    auto certificate_result = command == "plan" ? plan(request) : wire::decode_certificate(document.at("certificate"));
    if (auto e = std::get_if<Error>(&certificate_result)) return report(*e);
    const auto& certificate = std::get<Certificate>(certificate_result);
    auto checked = verify_certificate(request, certificate);
    if (auto e = std::get_if<Error>(&checked)) return report(*e);
    const auto& summary = std::get<Summary>(checked);
    if (human) {
        std::cout << "PASS sheets=" << summary.sheets << " used_area=" << summary.used_area
                  << " kerf_area=" << summary.kerf_area << '\n';
    } else if (command == "plan") {
        std::cout << wire::Json{{"request", wire::encode_request(request)},
                               {"certificate", wire::encode_certificate(certificate)}}.dump() << '\n';
    } else {
        std::cout << wire::Json{{"ok", true}, {"summary", wire::encode_summary(summary)}}.dump() << '\n';
    }
    return 0;
}
}
int main(int argc, char** argv) {
    try {
        const int status = run(argc, argv);
        std::cout.flush();
        if (!std::cout) {
            std::cerr << "Output write failed.\n";
            return 2;
        }
        return status;
    } catch (const std::bad_alloc&) {
        std::cerr << "RESOURCE_LIMIT: allocation failed.\n";
        return 2;
    } catch (const std::exception&) {
        std::cerr << "Input/output or serialization failure.\n";
        return 2;
    }
}
