#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace re4::port {
// File offsets remain 32-bit BE values. They never become serialized pointers.
// The caller owns the immutable buffer for the lifetime of every indexed span.
struct AssetSpan {
    std::uint32_t index, offset, size, occurrence;
    std::array<std::uint8_t, 4> tag;
};
struct AssetIndex {
    std::vector<AssetSpan> entries;
    std::uint32_t relOffset = 0;
    const AssetSpan* find(const char* threeByteTag, std::uint32_t occurrence) const noexcept;
};
// read.cpp:GetDataExt / tools/drs.py:Drs. Strict file validation precedes use.
AssetIndex indexAssetBody(const std::uint8_t* data, std::size_t size);
struct ContainerRecords {
    std::uint32_t bodyOffset, bodySize, soundOffset, soundSize;
};
// tools/drs.py:read_records/Drs/SoundBank; validates the outer and sound spans.
ContainerRecords indexAssetContainer(const std::uint8_t* data, std::size_t size);
} // namespace re4::port
