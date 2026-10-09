#include "re4/port/asset_index.h"
#include "re4/port/yz2.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
std::vector<std::uint8_t> read(const char* path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("cannot open input");
    const auto size = stream.tellg();
    if (size < 0 || size > 64 * 1024 * 1024) throw std::runtime_error("input exceeds 64 MiB policy");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!data.empty() && !stream.read(reinterpret_cast<char*>(data.data()), size)) throw std::runtime_error("short read");
    return data;
}
void word(std::vector<std::uint8_t>& data, std::size_t p, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) data[p + i] = static_cast<std::uint8_t>(value >> (24 - 8 * i));
}
void selfTest() {
    std::vector<std::uint8_t> data(96, 0);
    word(data, 0, 3); word(data, 16, 64); word(data, 20, 64); word(data, 24, 96);
    data[28] = data[32] = 'M'; data[29] = data[33] = 'D'; data[30] = data[34] = 'T'; data[36] = 'X';
    const auto index = re4::port::indexAssetBody(data.data(), data.size());
    if (index.entries.size() != 3 || index.find("MDT", 1)->offset != 64 || index.find("MDT", 2) || index.entries[0].size)
        throw std::runtime_error("GetDataExt occurrence mismatch");
    unsigned rejected = 0;
    for (const auto& change : std::vector<std::pair<std::size_t, std::uint32_t>>{{0, 0xffffffffU}, {16, 0xffffffe0U}, {20, 32}, {24, 128}, {4, 32}, {8, 1}}) {
        auto bad = data; word(bad, change.first, change.second);
        try { (void)re4::port::indexAssetBody(bad.data(), bad.size()); }
        catch (const std::invalid_argument&) { ++rejected; }
    }
    for (std::size_t size = 0; size < 40; ++size) {
        try { (void)re4::port::indexAssetBody(data.data(), size); }
        catch (const std::invalid_argument&) { ++rejected; }
    }
    std::vector<std::uint8_t> container(0x460, 0);
    word(container, 0x24, 96); word(container, 0x2c, 0x400);
    word(container, 0x40, 0xfffffffeU); word(container, 0x4c, 0x460); word(container, 0x60, 0xffffffffU);
    (void)re4::port::indexAssetContainer(container.data(), container.size());
    word(container, 0x24, 95); // Actual compressed DAS variant: exact byte size.
    (void)re4::port::indexAssetContainer(container.data(), container.size());
    word(container, 0x24, 0xffffffe0U);
    try { (void)re4::port::indexAssetContainer(container.data(), container.size()); }
    catch (const std::invalid_argument&) { ++rejected; }
    if (rejected != 47) throw std::runtime_error("malformed-input tests failed");
    std::cout << "PASS asset_index: 51 checks; pointer=" << sizeof(void*) * 8 << "\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "self-test") { selfTest(); return 0; }
        if (argc < 3) throw std::runtime_error("usage: re4_asset_tool self-test | body FILE | container FILE | decode INPUT OUTPUT");
        auto data = read(argv[2]);
        const std::string mode = argv[1];
        if (mode == "decode" && argc == 4) {
            std::ifstream existing(argv[3], std::ios::binary | std::ios::ate);
            if (existing && existing.tellg() != 0) throw std::runtime_error("refusing to overwrite nonempty output");
            existing.close();
            re4::port::Yz2Decoder decoder(data.data(), data.size()); decoder.decode();
            const auto& out = decoder.output();
            std::ofstream stream(argv[3], std::ios::binary);
            if (!stream || !stream.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size())))
                throw std::runtime_error("cannot write decoded output");
            std::cout << "{\"size\":" << out.size() << ",\"consumed\":" << decoder.statistics().inputBytes << "}\n";
            return 0;
        }
        if (mode == "container") {
            const auto c = re4::port::indexAssetContainer(data.data(), data.size());
            std::cout << "{\"body_offset\":" << c.bodyOffset << ",\"body_size\":" << c.bodySize
                << ",\"sound_offset\":" << c.soundOffset << ",\"sound_size\":" << c.soundSize << "}\n";
        } else if (mode == "body") {
            const auto index = re4::port::indexAssetBody(data.data(), data.size());
            std::cout << "{\"rel_offset\":" << index.relOffset << ",\"entries\":[";
            bool first = true;
            for (const auto& e : index.entries) {
                if (!first) std::cout << ',';
                first = false;
                std::cout << '[' << e.index << ',' << e.offset << ',' << e.size << ',' << e.occurrence << ']';
            }
            std::cout << "]}\n";
        } else throw std::runtime_error("invalid command");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
