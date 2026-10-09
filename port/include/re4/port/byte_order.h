#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace re4::port {

// Bounds-checked scalar decoding, not an invented RE4 archive structure.
// No alignment requirement, native pointer relocation, or whole-buffer swap.
class BigEndianView {
public:
    BigEndianView(const std::uint8_t* bytes, std::size_t size)
        : bytes_(bytes), size_(bytes ? size : 0) {}

    std::optional<std::uint16_t> u16(std::size_t offset) const {
        if (!contains(offset, 2)) return std::nullopt;
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes_[offset]) << 8) | bytes_[offset + 1]);
    }

    std::optional<std::uint32_t> u32(std::size_t offset) const {
        if (!contains(offset, 4)) return std::nullopt;
        return (static_cast<std::uint32_t>(bytes_[offset]) << 24)
            | (static_cast<std::uint32_t>(bytes_[offset + 1]) << 16)
            | (static_cast<std::uint32_t>(bytes_[offset + 2]) << 8)
            | static_cast<std::uint32_t>(bytes_[offset + 3]);
    }

    std::optional<float> f32(std::size_t offset) const {
        const auto word = u32(offset);
        if (!word) return std::nullopt;
        float result;
        static_assert(sizeof(result) == sizeof(*word));
        std::memcpy(&result, &*word, sizeof(result));
        return result;
    }

private:
    bool contains(std::size_t offset, std::size_t length) const {
        return offset <= size_ && length <= size_ - offset;
    }
    const std::uint8_t* bytes_;
    std::size_t size_;
};

} // namespace re4::port
