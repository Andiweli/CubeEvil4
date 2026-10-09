#include "re4/port/engine_loader.h"
#include "re4/port/sha256.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
using namespace re4::port;
namespace {
unsigned checks = 0;
void check(bool value, const char *why) {
    ++checks;
    if (!value)
        throw std::runtime_error(why);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "expected bounded rejection");
}
void tick(EngineLoader &l) {
    l.frame({64 * 1024, std::chrono::nanoseconds(0)});
    std::this_thread::yield();
}
template <class F> void until(EngineLoader &l, F ready) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while (!ready()) {
        tick(l);
        if (std::chrono::steady_clock::now() > end)
            throw std::runtime_error("resource timeout");
    }
}
std::string bytesHash(const ResourceLease &r) { return sha256(r.data, r.size); }
void fixture(const std::filesystem::path &root) {
    std::filesystem::create_directories(root / "blobs");
    std::ofstream catalog(root / "catalog.tsv");
    catalog << "RE4-IMPORT-CATALOG-1\n";
    for (unsigned i = 0; i < 32; ++i) {
        std::vector<std::uint8_t> data(196611, static_cast<std::uint8_t>(i));
        auto hash = sha256(data.data(), data.size());
        std::ofstream out(root / "blobs" / hash, std::ios::binary);
        out.write(reinterpret_cast<const char *>(data.data()),
                  static_cast<std::streamsize>(data.size()));
        catalog << "0\t" << i << "\t" << data.size() << "\tfixture" << i << ".dat\t" << hash
                << '\n';
    }
    catalog << "0\t33\t196611\tbroken.dat\t" << std::string(64, '0') << '\n';
}
std::vector<unsigned> transfer(EngineLoader &l, ResourceHandle h, Residency destination) {
    l.loadResource(h, destination);
    std::vector<unsigned> states{static_cast<unsigned>(l.resourceInfo(h).state)};
    auto target =
        destination == Residency::Mram ? ResourceState::MramReady : ResourceState::AramReady;
    until(l, [&] {
        auto info = l.resourceInfo(h);
        if (info.error)
            throw std::runtime_error("transfer error");
        auto value = static_cast<unsigned>(info.state);
        if (states.back() != value)
            states.push_back(value);
        return info.state == target;
    });
    return states;
}
void selfTest(const std::filesystem::path &root) {
    fixture(root);
    auto files =
        std::make_shared<ImportedFiles>(root.u8string(), (root / "catalog.tsv").u8string());
    {
        EngineLoader l(files);
        l.setAramCompaction(false);
        auto h = l.registerResource("fixture0.dat"), other = l.registerResource("fixture1.dat");
        auto ref = l.reference(h, 0, 196611);
        rejects([&] { l.resolve(ref); });
        auto mram = transfer(l, h, Residency::Mram);
        check(mram == std::vector<unsigned>({1, 2}), "original MRAM states");
        auto lease = l.resolve(ref);
        auto hash = bytesHash(lease);
        check((reinterpret_cast<std::uintptr_t>(lease.data) & 31u) == 0, "32-byte MRAM alignment");
        auto cached = transfer(l, h, Residency::Aram);
        check(cached == std::vector<unsigned>({6, 4}), "original MRAM -> ARAM states");
        check(l.liveAllocations() == 0 && l.memoryStats().mramBytes > 0,
              "lease pins removed MRAM allocation");
        rejects([&] { l.resolve(ref); });
        check(bytesHash(lease) == hash, "old lease remains valid after ARAM move");
        auto restored = transfer(l, h, Residency::Mram);
        check(restored == std::vector<unsigned>({5, 2}), "original ARAM -> MRAM states");
        check(bytesHash(l.resolve(ref)) == hash, "handle resolves relocated MRAM bytes");
        auto external = l.reserveMemory(196611);
        l.loadResource(h, Residency::Mram, false, external);
        check(l.resolve(ref).data == external.data && bytesHash(l.resolve(ref)) == hash,
              "original MRAM copy/release ordering");
        rejects([&] { l.releaseMemory(external); });
        l.clearResource(h);
        l.releaseMemory(external);
        lease = {};
        check(l.memoryStats().mramBytes == 0, "all owned and pinned MRAM reclaimed");
        l.loadResource(h, Residency::Aram);
        check(l.resourceInfo(h).state == ResourceState::AramLoading, "DVD -> ARAM pending state");
        until(l, [&] { return l.resourceInfo(h).state == ResourceState::AramReady; });
        check(transfer(l, h, Residency::Mram) == std::vector<unsigned>({5, 2}),
              "DVD cached data restored");
        check(bytesHash(l.resolve(ref)) == hash, "DVD -> ARAM -> MRAM exact logical bytes");
        transfer(l, h, Residency::Aram);
        l.loadResource(h, Residency::Aram, false, {}, 0x900000);
        std::vector<unsigned> moved{7};
        until(l, [&] {
            auto s = static_cast<unsigned>(l.resourceInfo(h).state);
            if (moved.back() != s)
                moved.push_back(s);
            return s == 4;
        });
        check(moved == std::vector<unsigned>({7, 6, 4}) &&
                  l.resourceInfo(h).aramAddress == 0x900000,
              "original two-step ARAM relocation");
        transfer(l, h, Residency::Mram);
        check(bytesHash(l.resolve(ref)) == hash, "ARAM relocation hash");
        l.loadResource(other, Residency::Mram, true);
        l.loadResource(other, Residency::Aram);
        tick(l);
        check(l.memoryStats().transfers > 0, "partial transfer observable");
        l.clearResource(other);
        check(l.memoryStats().transfers == 0, "active transfer detached before free");
        l.loadResource(other, Residency::Aram, true);
        l.loadResource(other, Residency::Mram, true);
        check(bytesHash(l.resolve(l.reference(other, 0, 196611))) ==
                  files->find(0, "fixture1.dat")->sha256,
              "cancel/restart original synchronous wait");
        l.clearResource(other);
        l.clearResource(h);
        l.setResourceControl(true, true);
        l.loadResource(h, Residency::Mram);
        check(l.resourceInfo(h).state == ResourceState::NoData && l.resourceInfo(h).command == 1,
              "original held command");
        tick(l);
        check(l.resourceInfo(h).state == ResourceState::NoData, "controller command hold");
        l.setResourceControl(true);
        until(l, [&] { return l.resourceInfo(h).state == ResourceState::MramReady; });
        transfer(l, h, Residency::Aram);
        rejects([&] {
            l.loadResource(other, Residency::Aram, false, {}, l.resourceInfo(h).aramAddress);
        });
        rejects([&] { l.loadResource(h, Residency::Aram, false, {}, 0x800001); });
        rejects([&] { l.loadResource(h, Residency::Aram, false, {}, 0xCFFF00); });
        rejects([&] { l.reference(h, 0xffffffffu, 1); });
        auto foreign = h;
        ++foreign.runtime;
        rejects([&] { l.resourceInfo(foreign); });
        l.deleteResource(h);
        rejects([&] { l.resolve(ref); });
        auto reused = l.registerResource("fixture2.dat");
        check(reused.slot == h.slot && reused.generation != h.generation,
              "generation-safe reused data unit");
        rejects([&] { l.resourceInfo(h); });
        auto broken = l.registerResource("broken.dat");
        l.loadResource(broken, Residency::Mram);
        until(l, [&] { return l.resourceInfo(broken).error != 0; });
        tick(l);
        check(l.resourceInfo(broken).error != 0,
              "failure remains visible across controller passes");
        l.deleteResource(broken);
        l.deleteResource(reused);
        l.deleteResource(other);
        check(l.memoryStats().mramBytes == 0, "failure/clear memory cleanup");
    }
    {
        EngineLoader l(files);
        l.setAramCompaction(false);
        std::vector<ResourceHandle> units;
        for (unsigned i = 0; i < 17; ++i) {
            auto h = l.registerResource("fixture" + std::to_string(i) + ".dat");
            l.loadResource(h, Residency::Mram, true);
            units.push_back(h);
        }
        for (unsigned i = 0; i < 16; ++i)
            l.loadResource(units[i], Residency::Aram);
        check(l.memoryStats().transfers == 16, "16 real ARQ slots bounded");
        rejects([&] { l.loadResource(units[16], Residency::Aram); });
        check(l.resourceInfo(units[16]).state == ResourceState::MramReady &&
                  bytesHash(l.resolve(l.reference(units[16], 0, 196611))) ==
                      files->find(0, "fixture16.dat")->sha256,
              "queue exhaustion preserves source allocation");
        l.clearResource(units[0]); // Pending-head cancellation (original bug fixed).
        l.loadResource(units[0], Residency::Aram);
        unsigned gameplay = 0;
        auto task = l.scheduler().exec(1, [&] {
            for (;;) {
                ++gameplay;
                l.scheduler().sleep(1);
            }
        });
        until(l, [&] {
            for (unsigned i = 0; i < 16; ++i)
                if (l.resourceInfo(units[i]).state != ResourceState::AramReady)
                    return false;
            return true;
        });
        check(gameplay > 10, "gameplay continues during FIFO transfers");
        l.scheduler().kill(*task);
        auto gap = l.resourceInfo(units[1]).aramAddress;
        l.clearResource(units[1]);
        l.setAramCompaction(true);
        until(l, [&] {
            return l.resourceInfo(units[2]).aramAddress == gap &&
                   l.resourceInfo(units[2]).state == ResourceState::AramReady;
        });
        l.setAramCompaction(false);
        for (auto h : units) {
            l.clearResource(h);
            l.deleteResource(h);
        }
        check(l.memoryStats().mramBytes == 0 && l.memoryStats().transfers == 0,
              "multi-unit reclamation");
    }
    {
        // Sixteen concurrent repacks plus four resident units reserve 36 spans.
        // The original local AramArea[32] would overflow in this valid state.
        EngineLoader l(files);
        l.setAramCompaction(false);
        std::vector<ResourceHandle> units;
        for (unsigned i = 0; i < 20; ++i) {
            auto h = l.registerResource("fixture" + std::to_string(i) + ".dat");
            l.loadResource(h, Residency::Aram, true, {}, 0x800000 + i * 196704);
            units.push_back(h);
        }
        for (unsigned i = 0; i < 16; ++i)
            l.loadResource(units[i], Residency::Aram, false, {},
                           l.resourceInfo(units[i]).aramAddress + 32);
        auto extra = l.registerResource("fixture20.dat");
        l.loadResource(extra, Residency::Aram);
        check(l.resourceInfo(extra).state == ResourceState::AramLoading,
              "36 simultaneous ARAM reservation spans remain bounded");
        until(l, [&] {
            for (auto h : units)
                if (l.resourceInfo(h).state != ResourceState::AramReady)
                    return false;
            return l.resourceInfo(extra).state == ResourceState::AramReady;
        });
        for (unsigned i = 0; i < units.size(); ++i) {
            l.loadResource(units[i], Residency::Mram, true);
            check(bytesHash(l.resolve(l.reference(units[i], 0, 196611))) ==
                      files->find(0, "fixture" + std::to_string(i) + ".dat")->sha256,
                  "concurrent ARAM repack preserves every source");
            l.deleteResource(units[i]);
        }
        l.deleteResource(extra);
        check(l.memoryStats().mramBytes == 0 && l.memoryStats().transfers == 0,
              "concurrent repack ownership cleanup");
    }
    {
        MemoryLimits limits;
        limits.mramBytes = 32;
        limits.aramEnd = 0x800020;
        EngineLoader l(files, 0, {}, limits);
        auto h = l.registerResource("fixture0.dat");
        l.loadResource(h, Residency::Mram);
        check(l.resourceInfo(h).error == 1, "MRAM budget error");
        rejects([&] { l.loadResource(h, Residency::Aram, true); });
        check(l.resourceInfo(h).error == 4, "ARAM budget error");
        rejects([&] { l.reserveMemory(0xffffffffu); });
        l.deleteResource(h);
    }
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"checks\":" << checks
              << ",\"kind\":\"isolated_original_controller_and_native_memory_tests\"}\n";
}
void resources(std::shared_ptr<const ImportedFiles> files, unsigned disc, int count, char **paths) {
    EngineLoader l(files, static_cast<std::uint8_t>(disc));
    l.setAramCompaction(false);
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"resources\":[";
    for (int i = 0; i < count; ++i) {
        auto *f = files->find(static_cast<std::uint8_t>(disc), paths[i]);
        if (!f)
            throw std::runtime_error("missing real resource");
        auto h = l.registerResource(f->path);
        auto ref = l.reference(h, 0, f->size);
        auto a = transfer(l, h, Residency::Mram), b = transfer(l, h, Residency::Aram),
             c = transfer(l, h, Residency::Mram);
        check(bytesHash(l.resolve(ref)) == f->sha256,
              "real resource hash after ARAM cache restore");
        auto old = l.resolve(ref);
        auto reserved = l.reserveMemory(f->size);
        l.loadResource(h, Residency::Mram, false, reserved);
        check(bytesHash(l.resolve(ref)) == f->sha256 && bytesHash(old) == f->sha256,
              "real references after relocation");
        old = {};
        l.clearResource(h);
        l.releaseMemory(reserved);
        l.loadResource(h, Residency::Aram);
        tick(l);
        l.clearResource(h);
        transfer(l, h, Residency::Mram);
        check(bytesHash(l.resolve(ref)) == f->sha256, "real abort/reload hash");
        l.deleteResource(h);
        if (i)
            std::cout << ',';
        std::cout << "{\"path\":\"" << f->path << "\",\"sha256\":\"" << f->sha256
                  << "\",\"states\":[";
        bool first = true;
        for (auto *states : {&a, &b, &c})
            for (auto n : *states) {
                if (!first)
                    std::cout << ',';
                first = false;
                std::cout << n;
            }
        std::cout << "]}";
    }
    check(l.memoryStats().mramBytes == 0 && l.memoryStats().transfers == 0,
          "real resource final memory reclamation");
    std::cout << "],\"checks\":" << checks
              << ",\"file_cache_hits\":" << l.memoryStats().fileCacheHits << ",\"functions\":[";
    bool first = true;
    for (const auto &name : l.observedFunctions()) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << '"' << name << '"';
    }
    std::cout << "]}\n";
}
void rooms(std::shared_ptr<const ImportedFiles> files, unsigned disc, int count, char **ids) {
    EngineLoader l(files, static_cast<std::uint8_t>(disc));
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"rooms\":[";
    for (int i = 0; i < count; ++i) {
        auto id = std::stoul(ids[i], nullptr, 16);
        if (id > 0xffff)
            throw std::invalid_argument("room id overflow");
        check(l.loadRoom(static_cast<std::uint16_t>(id)), "real room starts");
        until(l, [&] { return !l.roomBusy(); });
        auto room = l.room();
        if (!room)
            throw std::runtime_error(l.roomError());
        auto bodyHash = sha256(room->body->data(), room->body->size());
        unsigned blockTransitions = 0;
        for (auto b : l.blockResources()) {
            auto f = l.resourceInfo(b.resource);
            auto ref = l.reference(b.resource, 0, f.size);
            auto lease = l.resolve(ref);
            auto hash = bytesHash(lease);
            l.setBlockResidency(b.number, Residency::Aram);
            until(l, [&] { return l.resourceInfo(b.resource).state == ResourceState::AramReady; });
            check(bytesHash(lease) == hash, "block lease survives deactivation");
            rejects([&] { l.resolve(ref); });
            l.setBlockResidency(b.number, Residency::Mram);
            until(l, [&] { return l.resourceInfo(b.resource).state == ResourceState::MramReady; });
            check(bytesHash(l.resolve(ref)) == hash, "block cache restore uses original bytes");
            l.setBlockResidency(b.number, Residency::Aram);
            tick(l);
            tick(l);
            l.clearBlock(b.number);
            check(l.memoryStats().transfers == 0, "block transfer abort frees queued source");
            lease = {};
            l.setBlockResidency(b.number, Residency::Mram);
            until(l, [&] { return l.resourceInfo(b.resource).state == ResourceState::MramReady; });
            check(bytesHash(l.resolve(ref)) == hash, "block abort/reload exact hash");
            ++blockTransitions;
        }
        l.cancelRoom();
        check(l.memoryStats().mramBytes == 0 && l.memoryStats().transfers == 0,
              "room MRAM/ARAM work reclaimed");
        auto oldHits = l.memoryStats().decodedCacheHits;
        check(l.loadRoom(static_cast<std::uint16_t>(id)), "repeat real room starts");
        until(l, [&] { return !l.roomBusy(); });
        auto repeated = l.room();
        check(repeated && sha256(repeated->body->data(), repeated->body->size()) == bodyHash,
              "repeat decoded integrity");
        bool cached = room->body->size() <= MemoryLimits{}.decodedCacheBytes;
        if (cached)
            check(repeated->body == room->body && l.memoryStats().decodedCacheHits > oldHits,
                  "immutable decoded cache reused without copy");
        auto functions = l.observedFunctions();
        if (i)
            std::cout << ',';
        std::cout << "{\"id\":\"" << ids[i] << "\",\"body_sha256\":\"" << bodyHash
                  << "\",\"block_transitions\":" << blockTransitions
                  << ",\"decoded_cache_reused\":" << (cached ? "true" : "false") << '}';
        l.cancelRoom();
        check(!room->body->empty(), "external room owner survives cache/controller release");
    }
    auto stats = l.memoryStats();
    l.trimCaches();
    check(l.memoryStats().fileCacheBytes == 0 && l.memoryStats().decodedCacheBytes == 0,
          "bounded cache trim");
    std::cout << "],\"checks\":" << checks << ",\"file_cache_hits\":" << stats.fileCacheHits
              << ",\"decoded_cache_hits\":" << stats.decodedCacheHits << "}\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "self-test")
            selfTest(std::filesystem::u8path(argv[2]));
        else if (argc >= 6 && std::string(argv[1]) == "resources")
            resources(std::make_shared<ImportedFiles>(argv[2], argv[3]),
                      static_cast<unsigned>(std::stoul(argv[4])), argc - 5, argv + 5);
        else if (argc >= 6 && std::string(argv[1]) == "rooms")
            rooms(std::make_shared<ImportedFiles>(argv[2], argv[3]),
                  static_cast<unsigned>(std::stoul(argv[4])), argc - 5, argv + 5);
        else
            throw std::invalid_argument(
                "self-test DIR | resources ROOT CATALOG DISC ORIGINAL_PATH...");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
