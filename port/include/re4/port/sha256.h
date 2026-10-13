#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace re4::port {
// Integrity adapter; unrelated to gameplay. Streaming avoids a frame-sized hash stall.
class Sha256 {
  public:
    void update(const std::uint8_t *data, std::size_t size);
    std::string finish() const;

  private:
    void block(const std::uint8_t *data);
    std::array<std::uint32_t, 8> state_{{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f,
                                         0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    std::array<std::uint8_t, 64> pending_{};
    std::uint64_t bytes_ = 0;
    std::size_t used_ = 0;
};
std::string sha256(const std::uint8_t *data, std::size_t size);
} // namespace re4::port
