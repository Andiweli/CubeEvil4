#include "types.h"
#include "rnd.h"
#include "flag.h"
#include "re4/port/byte_order.h"
#include "re4/port/contracts.h"
#include "re4/port/original_math.h"
#include "re4/port/verification.h"
#include "scheduler_verification.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace re4::port {
namespace {

std::mutex verificationMutex;

void expect(CheckGroup& group, bool passed, std::string_view label) {
    ++group.checks;
    if (!passed) {
        ++group.failures;
        group.diagnostics.emplace_back(label);
    }
}

u32 floatBits(f32 value) {
    u32 result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void checkAbi(CheckGroup& group) {
    expect(group, sizeof(u32) == 4 && sizeof(s32) == 4, "GameCube integer words stay 32-bit");
    expect(group, sizeof(std::uintptr_t) == sizeof(void*), "uintptr_t holds native addresses");
    expect(group, sizeof(void*) == 4 || sizeof(void*) == 8, "32/64-bit native pointer width");
    expect(group, std::numeric_limits<u32>::max() == 0xFFFFFFFFu, "u32 unsigned range");
    expect(group, std::numeric_limits<s32>::min() == (-2147483647 - 1), "s32 signed range");
    expect(group, floatBits(1.0f) == 0x3F800000u, "IEEE binary32 representation");
    auto word = std::make_unique<u32>(0x80000000u);
    const auto address = reinterpret_cast<std::uintptr_t>(word.get());
    expect(group, reinterpret_cast<u32*>(address) == word.get(), "native pointer round-trip");
    expect(group, FlagBitChk(word.get(), 0) == 0x80000000u,
        "original flag function uses a real native heap pointer");
    u32 value = 0xFFFFFFFFu;
    value += 1u;
    expect(group, value == 0, "u32 arithmetic wraps at 32 bits on both ABIs");
}

void checkRng(CheckGroup& group) {
    // Fixed vectors were calculated with widened integer arithmetic from the
    // source's recurrence, including its equal-state escape. These are not
    // claimed to be independently captured GameCube execution traces.
    struct Vector { u16 seed; std::array<u8, 16> bytes; };
    const std::array<Vector, 3> vectors{{
        {0x0000, {0x01,0x81,0x41,0x21,0x11,0x09,0x05,0x03,0x02,0x81,0x40,0x9F,0x4E,0xA5,0x50,0xA5}},
        {0x0D37, {0xA8,0xF5,0x9B,0x6E,0xD7,0x8B,0x65,0x52,0xC8,0x03,0xA0,0xEE,0x15,0xA8,0xF1,0x95}},
        {0xFFFF, {0xFE,0x7D,0x3C,0x9B,0x4A,0xA1,0x4C,0xA1,0x4B,0x20,0x8A,0xBF,0x59,0x26,0x8C,0xBF}}
    }};
    for (const auto& vector : vectors) {
        RndInit(vector.seed);
        for (const auto expected : vector.bytes)
            expect(group, Rnd() == expected, "Rnd fixed byte vector");
    }

    RndInit(0x0D37);
    expect(group, floatBits(fRand0_1()) == 0x3E5FAD40u, "fRand0_1 exact float bits");
    expect(group, Rnd() == 0x6E, "fRand0_1 consumes exactly three bytes");
    RndInit(0x0D37);
    expect(group, floatBits(fRand1_1()) == 0xBF102960u, "fRand1_1 exact float bits");
    expect(group, Rnd() == 0x6E, "fRand1_1 consumes exactly three bytes");

    const std::array<u32, 8> seeds{
        0x3C6EF35F,0x47502932,0xD1CCF6E9,0xAAF95334,
        0x6252E503,0x9F2EC686,0x57FE6C2D,0xA3D95FA8};
    const std::array<u32, 8> bits0{
        0x3F5DE6BE,0x3F205264,0x3F19EDD2,0x3F72A668,
        0x3F25CA06,0x3EBB1A18,0x3F7CD85A,0x3F32BF50};
    const std::array<u32, 8> bits1{
        0x3F3BCD7C,0x3E814990,0x3E4F6E90,0x3F654CD0,
        0x3E972818,0xBE89CBD0,0x3F79B0B4,0x3ECAFD40};
    u32 seed0 = 0;
    u32 seed1 = 0;
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        const auto value0 = fRandSeed0_1(&seed0);
        const auto value1 = fRandSeed1_1(&seed1);
        expect(group, seed0 == seeds[i] && seed1 == seeds[i], "LCG 32-bit overflow vector");
        expect(group, floatBits(value0) == bits0[i], "fRandSeed0_1 exact float bits");
        expect(group, floatBits(value1) == bits1[i], "fRandSeed1_1 exact float bits");
    }

    std::array<u8, 4096> replay{};
    RndInit(0x0D37);
    for (auto& value : replay) value = Rnd();
    RndInit(0x0D37);
    for (const auto value : replay) expect(group, Rnd() == value, "reseeded RNG replay");
    RndInit(0x0D37);
    for (int i = 0; i < 4096; ++i) {
        const auto value0 = fRand0_1();
        const auto value1 = fRand1_1();
        expect(group, value0 >= 0.0f && value0 < 1.0f, "global RNG [0,1) range");
        expect(group, value1 >= -1.0f && value1 < 1.0f, "global RNG [-1,1) range");
    }
}

void checkFlags(CheckGroup& group) {
    // Read the original inline function from the unchanged copied flag.h.
    // Pointer-narrowing FlagOn/Off/Xor macros are deliberately not invoked.
    auto words = std::make_unique<std::array<u32, 3>>(
        std::array<u32, 3>{0x81000001u, 0x40000001u, 0x80000000u});
    struct Probe { u32 flag; u32 result; };
    const std::array<Probe, 13> probes{{
        {0,0x80000000u},{1,0},{7,0x01000000u},{8,0},{30,0},{31,1},
        {32,0},{33,0x40000000u},{62,0},{63,1},{64,0x80000000u},{65,0},{95,0}
    }};
    for (const auto& probe : probes)
        expect(group, FlagBitChk(words->data(), probe.flag) == probe.result,
            "original top-bit numbering and cross-word boundaries");
    const std::array<u8, 4> diskWord{0x81,0x00,0x00,0x01};
    const auto decoded = BigEndianView(diskWord.data(), diskWord.size()).u32(0);
    expect(group, decoded.has_value(), "big-endian flag word decoded");
    if (decoded) {
        u32 nativeWord = *decoded;
        expect(group, FlagBitChk(&nativeWord, 7) == 0x01000000u,
            "original flag test on decoded GameCube word");
    }
}

void checkMath(CheckGroup& group) {
    f32 endpoints[2]{2.0f, 6.0f};
    f32 tangents[2]{4.0f, 4.0f};
    expect(group, hermite(endpoints, tangents, 0.0f) == 2.0f, "Hermite first endpoint");
    expect(group, hermite(endpoints, tangents, 1.0f) == 6.0f, "Hermite second endpoint");
    expect(group, hermite(endpoints, tangents, 0.5f) == 4.0f, "Hermite linear midpoint");
    expect(group, hermite(endpoints, tangents, -0.5f) == 0.0f, "original extrapolation retained");
    f32 cubic[2]{0.0f, 1.0f};
    f32 derivative[2]{0.0f, 3.0f};
    expect(group, hermite(cubic, derivative, 0.25f) == 0.015625f, "Hermite t^3 at 1/4");
    expect(group, hermite(cubic, derivative, 0.5f) == 0.125f, "Hermite t^3 at 1/2");
    expect(group, hermite(cubic, derivative, 0.75f) == 0.421875f, "Hermite t^3 at 3/4");
    expect(group, IPOW(3.0f, 0) == 1.0f, "IPOW zero exponent");
    expect(group, IPOW(2.0f, 10) == 1024.0f, "IPOW positive exponent");
    expect(group, IPOW(-2.0f, 3) == -8.0f, "IPOW odd exponent");
    expect(group, IPOW(-2.0f, 4) == 16.0f, "IPOW even exponent");
    expect(group, IPOW(0.5f, 4) == 0.0625f, "IPOW fractional base");
    expect(group, IPOW(0.0f, 0) == 1.0f, "original zero-to-zero behavior");
    expect(group, IPOW(2.0f, -1) == 1.0f, "original negative-exponent behavior retained");
}

void checkEndian(CheckGroup& group) {
    const std::array<u8, 19> bytes{
        0xAA,0x12,0x34,0x89,0xAB,0xCD,0xEF,
        0x3F,0x80,0x00,0x00,0x80,0x00,0x00,0x00,0x7F,0x80,0x00,0x00};
    const BigEndianView view(bytes.data(), bytes.size());
    expect(group, view.u16(1) == std::optional<std::uint16_t>(std::uint16_t{0x1234}), "unaligned BE16");
    expect(group, view.u32(3) == std::optional<std::uint32_t>(0x89ABCDEFu), "unaligned BE32");
    expect(group, view.f32(7) == std::optional<float>(1.0f), "BE float 1.0");
    const auto negativeZero = view.f32(11);
    expect(group, negativeZero && floatBits(*negativeZero) == 0x80000000u, "BE negative zero bits");
    const auto infinity = view.f32(15);
    expect(group, infinity && std::isinf(*infinity) && *infinity > 0, "BE infinity");
    expect(group, !view.u32(16), "truncated scalar rejected");
    expect(group, !view.u16(bytes.size()), "one-past-end rejected");
    expect(group, !view.u32(std::numeric_limits<std::size_t>::max()), "overflow offset rejected");
    expect(group, !BigEndianView(nullptr, 128).u16(0), "null buffer rejected");
    expect(group, bytes[0] == 0xAA && bytes[3] == 0x89, "source bytes remain unchanged");
}

} // namespace

int VerificationReport::checks() const {
    int result = 0;
    for (const auto& group : groups) result += group.checks;
    return result;
}

int VerificationReport::failures() const {
    int result = 0;
    for (const auto& group : groups) result += group.failures;
    return result;
}

std::string VerificationReport::text() const {
    std::ostringstream out;
    out << "RE4 original-code verification\n"
        << "pointer=" << sizeof(void*) * 8 << " bits; long=" << sizeof(long) * 8
        << " bits; u32=" << sizeof(u32) * 8 << " bits\n";
    for (const auto& group : groups) {
        out << (group.failures ? "FAIL " : "PASS ") << group.name << ": "
            << group.checks << " checks, " << group.failures << " failures\n";
        for (const auto& diagnostic : group.diagnostics) out << "  " << diagnostic << '\n';
    }
    out << "TOTAL: " << checks() << " checks, " << failures() << " failures\n";
    return out.str();
}

VerificationReport verifyOriginalCode(std::string_view requested) {
    std::lock_guard<std::mutex> lock(verificationMutex);
    struct Group { const char* name; void (*run)(CheckGroup&); };
    const std::array<Group, 9> available{{
        {"abi",checkAbi},{"rng",checkRng},{"flags",checkFlags},
        {"math",checkMath},{"endian",checkEndian},
        {"scheduler_stack",checkSchedulerStack},{"scheduler_order",checkSchedulerOrder},
        {"scheduler_lifecycle",checkSchedulerLifecycle},{"scheduler_modules",checkSchedulerModules}
    }};
    VerificationReport report;
    for (const auto& group : available) {
        if (requested.empty() || requested == group.name) {
            report.groups.push_back(CheckGroup{group.name, 0, 0, {}});
            group.run(report.groups.back());
        }
    }
    if (report.groups.empty()) throw std::invalid_argument("Unknown verification group");
    return report;
}

} // namespace re4::port
