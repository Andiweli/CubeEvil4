#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace re4 {
// Port derived from upstream game/yz2code.cpp + yz2asm.cpp; original provenance
// applies.
struct Yz2Result {
  std::vector<std::uint8_t> bytes;
  std::size_t payloadOffset{}, streamOffset{}, consumed{};
  std::uint32_t packedSize{};
};
// Accepts a raw Yz2 payload or the first MRAM part of a Capcom DVD container.
// All references are offsets into this call's output, never GameCube pointers.
Yz2Result DecodeYz2(const std::vector<std::uint8_t> &source,
                    std::size_t maxOutput = 64 * 1024 * 1024);
Yz2Result LoadYz2(const std::string &path,
                  std::size_t maxOutput = 64 * 1024 * 1024);
} // namespace re4
