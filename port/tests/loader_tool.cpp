#include "re4/port/engine_loader.h"
#include "re4/port/sha256.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
using namespace re4::port;
namespace {
unsigned checks = 0;
void check(bool value, const char *reason) {
    ++checks;
    if (!value)
        throw std::runtime_error(reason);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "expected rejection");
}
void spin(EngineLoader &loader, const std::function<bool()> &done) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!done()) {
        loader.frame({64 * 1024, std::chrono::nanoseconds(0)});
        if (std::chrono::steady_clock::now() > end)
            throw std::runtime_error("engine load timed out");
        std::this_thread::yield();
    }
}
void file(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw std::runtime_error("fixture write failed");
}
void word(std::vector<std::uint8_t> &b, std::size_t p, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b[p + i] = static_cast<std::uint8_t>(n >> (24 - i * 8));
}
std::string bodyJson(const AssetIndex &index) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (const auto &e : index.entries) {
        if (!first)
            out << ',';
        first = false;
        out << '[' << e.index << ',' << e.offset << ',' << e.size << ',' << e.occurrence << ",\"";
        for (unsigned i = 0; i < 3; ++i)
            out << static_cast<char>(e.tag[i]);
        out << "\"]";
    }
    out << ']';
    return out.str();
}
void selfTest(const std::filesystem::path &directory) {
    std::filesystem::create_directories(directory / "blobs");
    const std::vector<std::uint8_t> bytes(300 * 1024, 0x63);
    auto hash = sha256(bytes.data(), bytes.size());
    file(directory / "blobs" / hash, bytes);
    auto catalog = directory / "catalog.tsv";
    {
        std::ofstream out(catalog);
        out << "RE4-IMPORT-CATALOG-1\n";
        for (unsigned i = 0; i < 3; ++i)
            out << "0\t" << i + 1 << "\t" << bytes.size() << "\tfixture" << i << ".dat\t" << hash
                << '\n';
        out << "0\t9\t" << bytes.size() << "\tbad.dat\t" << std::string(64, '0') << '\n';
        out << "0\t10\t" << bytes.size() << "\tbgm/bio4str.hed\t" << hash << '\n';
        out << "0\t11\t" << bytes.size() << "\tst1/r1fe.das\t" << hash << '\n';
        out << "0\t12\t0\tempty.dat\t" << sha256(nullptr, 0) << '\n';
    }
    file(directory / "blobs" / sha256(nullptr, 0), {});
    auto source = std::make_shared<ImportedFiles>(directory.u8string(), catalog.u8string());
    check(sha256(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA empty");
    check(sha256(reinterpret_cast<const std::uint8_t *>("abc"), 3) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA abc");
    Sha256 split;
    split.update(bytes.data(), 1);
    split.update(bytes.data() + 1, bytes.size() - 1);
    check(split.finish() == hash, "SHA streaming");
    check(source->find(0, "FIXTURE0.DAT") && source->find(0, "fixture0.dat")->fstIndex == 1,
          "case-insensitive original path");
    check(!source->find(1, "fixture0.dat"), "disc isolation");
    rejects([&] { source->find(0, "../fixture0.dat"); });
    rejects([&] { source->find(0, "st1//file"); });
    {
        EngineLoader loader(source);
        auto a = loader.request("fixture0.dat", 0x100, 9),
             b = loader.request("fixture1.dat", 0x100, 1),
             c = loader.request("fixture2.dat", 0x100, 1);
        std::vector<unsigned> order;
        bool ended[3]{};
        LoadRequest requests[] = {a, b, c};
        unsigned gameplay = 0;
        loader.scheduler().exec(1, [&] {
            for (unsigned i = 0; i < 100; ++i) {
                ++gameplay;
                loader.scheduler().sleep(1);
            }
            loader.scheduler().exit();
        });
        spin(loader, [&] {
            for (unsigned i = 0; i < 3; ++i)
                if (!ended[i] && loader.status(requests[i]) == LoadStatus::Complete) {
                    std::shared_ptr<const LoadedFile> result;
                    check(loader.readCheck(requests[i], result) == 1, "completion poll");
                    check(result->bytes == bytes, "asynchronous original bytes");
                    ended[i] = true;
                    order.push_back(i);
                }
            return order.size() == 3;
        });
        check(order == std::vector<unsigned>({1, 2, 0}),
              "original lower-priority number first/FIFO ties");
        check(gameplay > 1, "gameplay runs during I/O");
        check(loader.status(a) == LoadStatus::Released, "poll releases queue");
        std::shared_ptr<const LoadedFile> result;
        check(loader.readCheck(a, result) == -5, "stale completed request");
        auto pending = loader.request("fixture0.dat");
        check(loader.cancel(pending), "pending cancel");
        check(loader.readCheck(pending, result) == -4, "pending cancellation visible");
        auto active = loader.request("fixture1.dat");
        spin(loader, [&] { return loader.status(active) == LoadStatus::Reading; });
        check(loader.suspend(active), "suspend current");
        for (unsigned i = 0; i < 5; ++i)
            loader.frame({1, std::chrono::nanoseconds(0)});
        check(loader.status(active) == LoadStatus::Reading, "suspended read unpublished");
        check(loader.resume(active), "resume current");
        spin(loader, [&] { return loader.status(active) == LoadStatus::Complete; });
        check(loader.readCheck(active, result) == 1, "resumed result");
        auto cancelled = loader.request("fixture0.dat");
        spin(loader, [&] { return loader.status(cancelled) == LoadStatus::Reading; });
        check(loader.cancel(cancelled), "active cancel");
        spin(loader, [&] { return loader.status(cancelled) == LoadStatus::Cancelled; });
        check(loader.readCheck(cancelled, result) == -4, "active cancellation visible");
        auto normal = loader.request("fixture0.dat", 0);
        spin(loader, [&] { return loader.status(normal) == LoadStatus::Complete; });
        check(loader.readCheck(normal, result) == 1, "original slot3 DVD path");
        auto missing = loader.request("missing.dat");
        spin(loader, [&] { return loader.status(missing) == LoadStatus::Failed; });
        check(loader.readCheck(missing, result) == -1, "missing-file error");
        file(directory / "blobs" / std::string(64, '0'), bytes);
        auto bad = loader.request("bad.dat");
        spin(loader, [&] { return loader.status(bad) == LoadStatus::Failed; });
        check(loader.readCheck(bad, result) == -6, "hash corruption rejected");
        auto header = loader.request("fixture0.dat", 0x8100);
        spin(loader, [&] { return loader.status(header) == LoadStatus::Failed; });
        check(loader.readCheck(header, result) == -6, "invalid container rejected");
        auto empty = loader.request("empty.dat");
        spin(loader, [&] { return loader.status(empty) == LoadStatus::Complete; });
        check(loader.readCheck(empty, result) == 1 && result->bytes.empty(),
              "zero-length file is owned and valid");
        check(loader.loadRoom(0x1fe), "isolated invalid room container starts");
        spin(loader, [&] { return !loader.roomBusy(); });
        check(loader.roomStage() == RoomLoadStage::Failed && !loader.roomError().empty(),
              "room error terminates original wait instead of hanging");
        loader.cancelRoom();
        auto range = loader.request("fixture0.dat", 0x100, 4, bytes.size() - 32, 32);
        spin(loader, [&] { return loader.status(range) == LoadStatus::Complete; });
        check(loader.readCheck(range, result) == 1 && result->bodyOffset == bytes.size() - 32 &&
                  result->bodySize == 32,
              "original offset/length request");
        auto overflow = loader.request("fixture0.dat", 0x100, 4, 0xffffffff, 32);
        spin(loader, [&] { return loader.status(overflow) == LoadStatus::Failed; });
        check(loader.readCheck(overflow, result) == -6, "offset overflow rejected");
        auto sync = loader.request("fixture0.dat", 1);
        check(loader.readCheck(sync, result) == 1, "explicit original synchronous mode");
        auto saved = loader.request("fixture0.dat");
        spin(loader, [&] { return loader.status(saved) == LoadStatus::Reading; });
        auto nested = loader.request("fixture1.dat", 1);
        check(loader.readCheck(nested, result) == 1,
              "synchronous request preserves existing async queue");
        spin(loader, [&] { return loader.status(saved) == LoadStatus::Complete; });
        check(loader.readCheck(saved, result) == 1, "saved async request resumes");
        auto discard = loader.request("fixture0.dat", 0x41);
        check(loader.status(discard) == LoadStatus::Released, "original automatic-release mode");
        auto numbered = loader.requestFile(0);
        spin(loader, [&] { return loader.status(numbered) == LoadStatus::Complete; });
        check(loader.readCheck(numbered, result) == 1 && result->identity.path == "bgm/bio4str.hed",
              "original FileTbl/DvdRead identity");
        rejects([&] { loader.requestFile(0xffff); });
        rejects([&] { loader.request("fixture0.dat", 8); });
        auto named = loader.request("fixture0.dat");
        auto foreign = named;
        ++foreign.runtime;
        check(!loader.cancel(foreign), "foreign runtime handle rejected");
        loader.cancel(named);
        loader.readCheck(named, result);
        auto old = loader.request("fixture0.dat");
        loader.cancel(old);
        loader.readCheck(old, result);
        for (unsigned i = 0; i < 260; ++i) {
            auto r = loader.request("fixture0.dat");
            check(loader.cancel(r), "recycle cancel");
            check(loader.readCheck(r, result) == -4, "recycle release");
        }
        check(!loader.cancel(old), "generation prevents recycled-ID cancel");
        std::vector<LoadRequest> full;
        for (unsigned i = 0; i < 16; ++i)
            full.push_back(loader.request("fixture0.dat"));
        rejects([&] { loader.request("fixture0.dat"); });
        loader.cancelAll();
        for (auto r : full)
            check(loader.status(r) == LoadStatus::Released, "cancel-all releases all");
        check(loader.liveAllocations() == 0, "no unowned MRAM allocations");
    }
    for (unsigned hz : {60, 90, 120}) {
        EngineLoader l(source);
        FrameViAdapter clock(l);
        clock.onDisplayFrame(0);
        for (unsigned i = 1; i <= hz; ++i)
            clock.onDisplayFrame(static_cast<std::int64_t>(1000000000ULL * i / hz));
        check(clock.retraces() == 60 && clock.gameplayFrames() == 30,
              "display-independent original 60/2 cadence");
        clock.reset();
        clock.onDisplayFrame(10000000000LL);
        check(clock.gameplayFrames() == 30, "pause discards elapsed wall time");
        rejects([&] { clock.onDisplayFrame(1); });
    }
    std::vector<std::uint8_t> mdt(32, 0);
    word(mdt, 8, 28);
    rejects([&] { englishMessage(mdt.data(), mdt.size(), 0); });
    mdt.resize(48);
    word(mdt, 8, 28);
    word(mdt, 32, 1);
    word(mdt, 36, 12);
    auto msg = englishMessage(mdt.data(), mdt.size(), 0);
    check(msg.offset == 40 && msg.size == 8, "bounded original English message lookup");
    rejects([&] { englishMessage(mdt.data(), mdt.size(), 1); });
    // Deterministic worker barrier: abort while the worker owns private input.
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, unblock = false;
    {
        EngineLoader l(source, 0, [&](std::uint64_t) {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            cv.notify_one();
            cv.wait(lock, [&] { return unblock; });
        });
        auto r = l.request("fixture0.dat");
        l.frame();
        {
            std::unique_lock<std::mutex> lock(mutex);
            check(cv.wait_for(lock, std::chrono::seconds(3), [&] { return entered; }),
                  "I/O worker reached barrier");
        }
        auto start = std::chrono::steady_clock::now();
        l.cancelAll();
        check(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250),
              "cancellation does not join worker");
        {
            std::lock_guard<std::mutex> lock(mutex);
            unblock = true;
        }
        cv.notify_one();
        check(l.status(r) == LoadStatus::Released, "cancelled destination lifetime separated");
        auto retry = l.request("fixture1.dat");
        spin(l, [&] { return l.status(retry) == LoadStatus::Complete; });
        std::shared_ptr<const LoadedFile> result;
        check(l.readCheck(retry, result) == 1 && result->bytes == bytes,
              "restart after cancelled worker");
    }
    std::cout << "{\"checks\":" << checks << ",\"pointer_bits\":" << sizeof(void *) * 8
              << ",\"trace\":\"priority:1,2,0;pending-cancel;read-suspend-resume;active-cancel;"
                 "retry;slot3;hash-error;generation;frame60/2\"}\n";
}
void rooms(const char *root, const char *catalog, unsigned disc, int argc, char **ids) {
    auto source = std::make_shared<ImportedFiles>(root, catalog);
    EngineLoader loader(source, static_cast<std::uint8_t>(disc));
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"rooms\":[";
    bool first = true, blocksCancelled = false;
    for (int n = 0; n < argc; ++n) {
        auto id = std::stoul(ids[n], nullptr, 16);
        if (id > 0xffff)
            throw std::invalid_argument("room id overflow");
        check(loader.loadRoom(static_cast<std::uint16_t>(id)), "room task starts");
        unsigned gameplay = 0;
        auto task = loader.scheduler().exec(1, [&] {
            for (;;) {
                ++gameplay;
                loader.scheduler().sleep(1);
            }
        });
        spin(loader, [&] { return !loader.roomBusy(); });
        if (task)
            loader.scheduler().kill(*task);
        auto room = loader.room();
        if (!room)
            throw std::runtime_error(loader.roomError());
        check(gameplay > 1, "gameplay during real load");
        check(loader.liveAllocations() == room->blocks.size(),
              "one owning MRAM allocation per block");
        check(loader.scheduler().allocatedStacks() == 0, "finished engine task stacks released");
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"id\":\"" << ids[n] << "\",\"source_sha256\":\""
                  << sha256(room->source->bytes.data(), room->source->bytes.size())
                  << "\",\"body_sha256\":\"" << sha256(room->body->data(), room->body->size())
                  << "\",\"body_size\":" << room->body->size()
                  << ",\"entries\":" << bodyJson(room->index) << ",\"blocks\":[";
        for (std::size_t i = 0; i < room->blocks.size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &b = *room->blocks[i];
            std::cout << "{\"path\":\"" << b.identity.path << "\",\"size\":" << b.bytes.size()
                      << ",\"sha256\":\"" << sha256(b.bytes.data(), b.bytes.size())
                      << "\",\"entries\":" << bodyJson(room->blockIndices[i]) << '}';
        }
        std::cout << "],\"english_mdt\":{";
        auto *mdt = room->find("MDT");
        if (!mdt)
            std::cout << "\"status\":\"missing\",\"count\":0";
        else
            try {
                auto *p = room->body->data() + mdt->offset;
                auto count = englishMessageCount(p, mdt->size);
                for (std::uint32_t i = 0; i < count; ++i)
                    englishMessage(p, mdt->size, i);
                std::cout << "\"status\":\"" << (count ? "present" : "empty")
                          << "\",\"count\":" << count;
            } catch (const std::invalid_argument &) {
                std::cout << "\"status\":\"open\"";
            }
        std::cout << '}';
        auto functions = loader.observedFunctions();
        auto hadBlocks = !room->blocks.empty();
        auto retained = room;
        loader.cancelRoom();
        check(loader.liveAllocations() == 0, "room/DC allocations released");
        check(!retained->body->empty(), "archive spans pinned after room release");
        if (n == 0) {
            check(loader.loadRoom(static_cast<std::uint16_t>(id)), "start cancellable room read");
            loader.frame({1, std::chrono::nanoseconds(0)});
            loader.cancelRoom();
            check(loader.scheduler().allocatedStacks() == 0, "room read abort releases stacks");
            check(loader.loadRoom(static_cast<std::uint16_t>(id)), "restart for decode abort");
            spin(loader, [&] {
                return loader.roomStage() == RoomLoadStage::Decoding || !loader.roomBusy();
            });
            check(loader.roomStage() == RoomLoadStage::Decoding, "real decoder reached");
            loader.cancelRoom();
            check(loader.liveAllocations() == 0 && loader.scheduler().allocatedStacks() == 0,
                  "decode abort ownership");
        }
        if (hadBlocks && !blocksCancelled) {
            check(loader.loadRoom(static_cast<std::uint16_t>(id)), "start block abort");
            spin(loader,
                 [&] { return loader.roomStage() == RoomLoadStage::Blocks || !loader.roomBusy(); });
            check(loader.roomStage() == RoomLoadStage::Blocks, "original block queue reached");
            loader.cancelRoom();
            check(loader.liveAllocations() == 0 && loader.scheduler().allocatedStacks() == 0,
                  "block abort frees destinations safely");
            blocksCancelled = true;
        }
        std::cout << ",\"flow\":[";
        bool firstFunction = true;
        for (const auto &function : functions) {
            if (!firstFunction)
                std::cout << ',';
            firstFunction = false;
            std::cout << '\"' << function << '\"';
        }
        std::cout << "]}";
    }
    std::cout << "],\"checks\":" << checks << "}\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "self-test")
            selfTest(std::filesystem::u8path(argv[2]));
        else if (argc >= 6 && std::string(argv[1]) == "rooms")
            rooms(argv[2], argv[3], static_cast<unsigned>(std::stoul(argv[4])), argc - 5, argv + 5);
        else
            throw std::invalid_argument(
                "usage: re4_loader_tool self-test DIR | rooms ROOT CATALOG DISC HEX_ROOM...");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
