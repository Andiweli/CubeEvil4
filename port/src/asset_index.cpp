#include "re4/port/asset_index.h"
#include "re4/port/byte_order.h"
#include <limits>
#include <map>
#include <stdexcept>

namespace re4::port {
namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::invalid_argument(reason);
}
std::uint32_t word(const BigEndianView& view, std::size_t offset) {
    const auto value = view.u32(offset);
    require(value.has_value(), "truncated BE word");
    return *value;
}
bool span(std::size_t size, std::size_t offset, std::size_t bytes) {
    return offset <= size && bytes <= size - offset;
}
using Record = std::array<std::uint32_t, 8>;
std::vector<Record> records(const std::uint8_t* data, std::size_t size) {
    require(size >= 0x400, "truncated container table");
    BigEndianView view(data, size);
    std::vector<Record> result;
    for (std::size_t p = 0x20; p < 0x400; p += 0x20) {
        Record record{};
        for (std::size_t i = 0; i < 8; ++i) record[i] = word(view, p + i * 4);
        if (record[0] == 0xffffffffU) {
            for (std::size_t i = 1; i < 8; ++i) require(record[i] == 0, "invalid terminator");
            for (std::size_t i = p + 0x20; i < 0x400; ++i)
                require(data[i] == 0, "nonzero table padding");
            return result;
        }
        require(span(size, record[3], record[1]), "record outside file");
        result.push_back(record);
    }
    throw std::invalid_argument("missing container terminator");
}
}
const AssetSpan* AssetIndex::find(const char* tag, std::uint32_t occurrence) const noexcept {
    if (!tag) return nullptr;
    for (const auto& entry : entries)
        if (entry.tag[0] == static_cast<std::uint8_t>(tag[0]) &&
            entry.tag[1] == static_cast<std::uint8_t>(tag[1]) &&
            entry.tag[2] == static_cast<std::uint8_t>(tag[2]) && entry.occurrence == occurrence)
            return &entry;
    return nullptr;
}
AssetIndex indexAssetBody(const std::uint8_t* data, std::size_t size) {
    require(data && size >= 16 && size <= std::numeric_limits<std::uint32_t>::max(), "invalid body size");
    BigEndianView view(data, size);
    const auto count = word(view, 0);
    require(count > 0 && count <= (size - 16) / 8, "body count outside header");
    require(word(view, 8) == 0 && word(view, 12) == 0, "unknown body header variant");
    AssetIndex result;
    result.relOffset = word(view, 4);
    require(!result.relOffset || (result.relOffset <= size && result.relOffset % 32 == 0), "invalid REL offset");
    const std::size_t headerEnd = 16 + static_cast<std::size_t>(count) * 8;
    const std::size_t first = (headerEnd + 31) & ~std::size_t(31);
    require(word(view, 16) == first && first <= size, "invalid first body offset");
    for (std::size_t p = headerEnd; p < first; ++p) require(data[p] == 0, "nonzero body padding");
    const auto end = result.relOffset ? result.relOffset : static_cast<std::uint32_t>(size);
    result.entries.reserve(count);
    std::map<std::array<std::uint8_t, 3>, std::uint32_t> occurrences;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto start = word(view, 16 + static_cast<std::size_t>(i) * 4);
        const auto stop = i + 1 < count ? word(view, 20 + static_cast<std::size_t>(i) * 4) : end;
        require(start >= first && start % 32 == 0 && stop >= start && stop <= end, "invalid body entry span");
        AssetSpan entry{i, start, stop - start, 0, {}};
        for (std::size_t k = 0; k < 4; ++k) entry.tag[k] = data[16 + static_cast<std::size_t>(count) * 4 + i * 4 + k];
        entry.occurrence = occurrences[{entry.tag[0], entry.tag[1], entry.tag[2]}]++;
        result.entries.push_back(entry);
    }
    return result;
}
ContainerRecords indexAssetContainer(const std::uint8_t* data, std::size_t size) {
    require(data != nullptr && size <= std::numeric_limits<std::uint32_t>::max(), "invalid container size");
    const auto table = records(data, size);
    require(table.size() == 2, "unknown outer record count");
    const auto& body = table[0];
    const auto& sound = table[1];
    require(body[0] == 0 && body[2] == 0 && body[3] == 0x400,
        "invalid body record");
    // Compressed DAS record sizes are exact, not rounded. The following
    // record starts at the next 32-byte boundary (archive.py:load_das).
    const auto afterBody = (0x400ULL + body[1] + 31) & ~std::uint64_t(31);
    for (std::size_t i = 4; i < 8; ++i) require(body[i] == 0, "unknown body record parameters");
    require(sound[2] == 0, "invalid sound record");
    for (std::size_t i = 4; i < 8; ++i) require(sound[i] == 0, "unknown sound record parameters");
    if (sound[0] == 0xfffffffeU) {
        require(sound[1] == 0 && sound[3] == size && afterBody == size, "invalid empty sound record");
    } else {
        require(sound[0] == 4 && sound[3] == afterBody && sound[1] == size - sound[3], "invalid sound span");
        const auto bank = records(data + sound[3], sound[1]);
        std::size_t next = 0x400;
        for (const auto& r : bank) {
            require((r[0] == 1 || r[0] == 2) && r[2] == 0 && r[3] == next && r[6] == 0 && r[7] == 0,
                "unknown sound bank layout");
            const auto stop = next + r[1];
            next = (stop + 31) & ~std::size_t(31);
            require(next <= sound[1], "sound padding outside bank");
            for (auto p = stop; p < next; ++p) require(data[sound[3] + p] == 0, "nonzero sound padding");
        }
        require(next == sound[1], "unaccounted sound data");
    }
    return {body[3], body[1], sound[3], sound[1]};
}
} // namespace re4::port
