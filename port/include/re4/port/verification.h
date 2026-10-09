#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace re4::port {

struct CheckGroup {
    std::string name;
    int checks = 0;
    int failures = 0;
    std::vector<std::string> diagnostics;
};

struct VerificationReport {
    std::vector<CheckGroup> groups;
    int checks() const;
    int failures() const;
    std::string text() const;
};

// One shared suite is used by CTest, the Android Activity and instrumentation.
// Serialized because the original global RNG is deliberately not thread-safe.
VerificationReport verifyOriginalCode(std::string_view group = {});

} // namespace re4::port
