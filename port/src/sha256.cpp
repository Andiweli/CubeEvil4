#include "re4/port/sha256.h"
#include <algorithm>
#include <stdexcept>
namespace re4::port {
namespace {
constexpr std::uint32_t k[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
} // namespace
void Sha256::block(const std::uint8_t *p) {
    std::uint32_t w[64];
    for (unsigned i = 0; i < 16; ++i)
        w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) |
               (std::uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
    for (unsigned i = 16; i < 64; ++i) {
        auto a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (rotr(a, 7) ^ rotr(a, 18) ^ (a >> 3)) + w[i - 7] +
               (rotr(b, 17) ^ rotr(b, 19) ^ (b >> 10));
    }
    auto a = state_[0], b = state_[1], c = state_[2], d = state_[3], e = state_[4], f = state_[5],
         g = state_[6], h = state_[7];
    for (unsigned i = 0; i < 64; ++i) {
        auto t =
            h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ ((~e) & g)) + k[i] + w[i];
        auto u = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t;
        d = c;
        c = b;
        b = a;
        a = t + u;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}
void Sha256::update(const std::uint8_t *p, std::size_t size) {
    if (size && !p)
        throw std::invalid_argument("null SHA input");
    bytes_ += size;
    while (size) {
        auto n = std::min(size, 64 - used_);
        std::copy_n(p, n, pending_.data() + used_);
        used_ += n;
        p += n;
        size -= n;
        if (used_ == 64) {
            block(pending_.data());
            used_ = 0;
        }
    }
}
std::string Sha256::finish() const {
    auto copy = *this;
    const auto bits = bytes_ * 8;
    std::uint8_t tail[128]{};
    tail[0] = 0x80;
    auto n = used_ < 56 ? 56 - used_ : 120 - used_;
    for (unsigned i = 0; i < 8; ++i)
        tail[n + i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    copy.update(tail, n + 8);
    std::string result;
    constexpr char digits[] = "0123456789abcdef";
    for (auto v : copy.state_)
        for (int i = 28; i >= 0; i -= 4)
            result += digits[(v >> i) & 15];
    return result;
}
std::string sha256(const std::uint8_t *p, std::size_t n) {
    Sha256 s;
    s.update(p, n);
    return s.finish();
}
} // namespace re4::port
