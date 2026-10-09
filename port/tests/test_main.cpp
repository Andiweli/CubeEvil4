#include "re4/port/verification.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc > 2) {
            std::cerr << "Usage: re4_core_tests [abi|rng|flags|math|endian]\n";
            return 2;
        }
        const auto report = re4::port::verifyOriginalCode(argc == 2 ? argv[1] : "");
        std::cout << report.text();
        return report.failures() == 0 && report.checks() > 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
