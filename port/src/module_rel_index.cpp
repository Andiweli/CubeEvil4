#include "re4/port/byte_order.h"
#include "re4/port/modules.h"
#include <algorithm>
#include <set>
namespace re4::port {
RelModuleIndex indexRelModule(const std::uint8_t *data, std::size_t bytes,
                              const ModuleMetadata &m) {
    auto require = [](bool value) {
        if (!value)
            throw std::invalid_argument("Invalid/truncated original REL metadata");
    };
    require(data && bytes >= 0x4c && bytes <= 0xffffffffu);
    BigEndianView view(data, bytes);
    auto word = [&](std::size_t p) {
        auto value = view.u32(p);
        require(value.has_value());
        return *value;
    };
    auto bounded = [&](std::size_t p, std::size_t n) { require(p <= bytes && n <= bytes - p); };
    require(word(4) == 0 && word(8) == 0 && word(0x1c) == 3);
    const auto count = word(0xc), table = word(0x10);
    require(table == 0x4c && count > 0 && count <= (bytes - table) / 8);
    struct Section {
        std::uint32_t offset, bytes;
        bool code;
    };
    std::vector<Section> sections;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto offset = word(table + i * 8), size = word(table + i * 8 + 4);
        if (offset & ~3u)
            bounded(offset & ~3u, size);
        // SN's on-disc bssSection byte is zero; config identifies the actual
        // zero-offset .bss section (e.g. Sscrn section 22). OSLink fills runtime fields.
        else if (size)
            require(i == m.bssSection && size == word(0x20));
        sections.push_back({offset & ~3u, size, (offset & 1) != 0});
    }
    for (std::size_t entry = 0; entry < 3; ++entry) {
        auto section = data[0x30 + entry];
        auto offset = word(0x34 + entry * 4);
        if (!section && !offset)
            continue;
        require(section < sections.size() && sections[section].code &&
                offset < sections[section].bytes);
    }
    auto empty = [&](std::uint8_t i) {
        require(i < sections.size());
        const auto &s = sections[i];
        return s.offset && s.bytes == 4 && word(s.offset) == 0;
    };
    RelModuleIndex result;
    result.id = word(0);
    result.bssBytes = word(0x20);
    result.emptyConstructors = empty(m.ctorSection);
    result.emptyDestructors = empty(m.dtorSection);
    const auto offset = word(0x28), size = word(0x2c);
    bounded(offset, size);
    require(size && !(size % 8));
    std::set<std::uint32_t> identities;
    for (std::size_t p = offset; p < static_cast<std::size_t>(offset) + size; p += 8) {
        auto id = word(p), cursor = word(p + 4);
        require(identities.insert(id).second);
        result.imports.push_back(id);
        for (;;) {
            bounded(cursor, 8);
            const auto kind = data[cursor + 2];
            cursor += 8;
            if (kind == 203)
                break; // tools/relfile.py:R_DOLPHIN_END
        }
        result.exactBytes = std::max(result.exactBytes, cursor);
    }
    require(result.id == m.id);
    std::set<std::uint32_t> external;
    for (auto id : result.imports)
        if (id && id != result.id)
            external.insert(id);
    require(external == std::set<std::uint32_t>(m.imports.begin(), m.imports.end()));
    require(result.exactBytes >= offset + size);
    return result;
}
} // namespace re4::port
