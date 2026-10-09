#include "re4/port/module_verification.h"
#include "re4/port/modules.h"
#include "re4/port/sha256.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
using namespace re4::port;
void originalData(int argc, char **argv) {
    if (argc < 6)
        throw std::invalid_argument("data <root> <catalog> <disc> <module-id:path>...");
    auto files = std::make_shared<ImportedFiles>(argv[2], argv[3]);
    const auto disc = std::stoul(argv[4]);
    if (disc > 1)
        throw std::invalid_argument("disc");
    EngineLoader loader(files, static_cast<std::uint8_t>(disc));
    ModuleRuntime runtime(loader);
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"catalog_sha256\":\""
              << originalModuleCatalog().sha256 << "\",\"modules\":[";
    bool first = true;
    for (int i = 5; i < argc; ++i) {
        std::string argument(argv[i]);
        auto separator = argument.find(':');
        if (separator == std::string::npos)
            throw std::invalid_argument("module:path");
        const auto id = std::stoul(argument.substr(0, separator));
        const auto path = argument.substr(separator + 1);
        const auto &catalog = originalModuleCatalog();
        auto m = std::find_if(catalog.modules.begin(), catalog.modules.end(),
                              [&](const auto &value) { return value.id == id; });
        if (m == catalog.modules.end())
            throw std::invalid_argument("original module id");
        auto request = m->fileIndex != 0xffff && path.substr(path.size() - 4) == ".rel"
                           ? loader.requestFile(m->fileIndex, 0x104)
                           : loader.request(path);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (loader.status(request) == LoadStatus::Pending ||
               loader.status(request) == LoadStatus::Reading) {
            loader.frame({64 * 1024, std::chrono::nanoseconds(0)});
            std::this_thread::yield();
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("original REL read timeout");
        }
        std::shared_ptr<const LoadedFile> file;
        if (loader.readCheck(request, file) != 1 || !file)
            throw std::runtime_error("original REL file failed");
        const std::uint8_t *rel = file->bytes.data();
        std::size_t size = file->bytes.size();
        if (path.substr(path.size() - 4) == ".drs") {
            auto container = indexAssetContainer(rel, size);
            auto body = indexAssetBody(rel + container.bodyOffset, container.bodySize);
            if (!body.relOffset)
                throw std::runtime_error("original archive has no REL");
            rel += container.bodyOffset + body.relOffset;
            size = container.bodySize - body.relOffset;
        }
        RelModuleIndex index;
        try {
            index = indexRelModule(rel, size, *m);
        } catch (const std::exception &e) {
            throw std::runtime_error(path + ": " + e.what());
        }
        unsigned rejected = 0;
        for (std::size_t length :
             {std::size_t(0), std::size_t(0x4b), std::size_t(index.exactBytes - 1)}) {
            try {
                (void)indexRelModule(rel, length, *m);
            } catch (const std::invalid_argument &) {
                ++rejected;
            }
        }
        std::vector<std::uint8_t> bad(rel, rel + size);
        for (std::size_t p :
             {std::size_t(0xc), std::size_t(0x10), std::size_t(0x28), std::size_t(0x2c)}) {
            auto saved = std::vector<std::uint8_t>(bad.begin() + p, bad.begin() + p + 4);
            std::fill(bad.begin() + p, bad.begin() + p + 4, 0xff);
            try {
                (void)indexRelModule(bad.data(), bad.size(), *m);
            } catch (const std::invalid_argument &) {
                ++rejected;
            }
            std::copy(saved.begin(), saved.end(), bad.begin() + p);
        }
        if (rejected != 7)
            throw std::runtime_error("malformed REL bounds accepted");
        unsigned originalPairs = 0;
        if (m->name.substr(0, 2) == "st") {
            if (!index.emptyConstructors || !index.emptyDestructors)
                throw std::runtime_error(
                    "Original stage lifecycle list requires real constructor binding");
            runtime.activate(m->fileIndex);
            for (const auto &r : catalog.rooms)
                if (r.module == m->id && r.initCompiled) {
                    auto callbacks = *runtime.room(r.id);
                    runtime.invoke(callbacks.init);
                    runtime.invoke(callbacks.main);
                    ++originalPairs;
                    runtime.stop(m->fileIndex);
                    runtime.restart(m->fileIndex);
                    runtime.invoke(runtime.room(r.id)->main);
                }
            runtime.deactivate(m->fileIndex);
        }
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"id\":" << id << ",\"file_sha256\":\""
                  << sha256(file->bytes.data(), file->bytes.size()) << "\",\"rel_sha256\":\""
                  << sha256(rel, index.exactBytes) << "\",\"rel_bytes\":" << index.exactBytes
                  << ",\"bss_bytes\":" << index.bssBytes << ",\"rejected\":" << rejected
                  << ",\"original_pairs\":" << originalPairs << '}';
        loader.cancel(request);
        if (loader.memoryStats().mramBytes)
            throw std::runtime_error("original module read/work leaked MRAM");
    }
    std::cout << "]}\n";
}
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "data") {
            originalData(argc, argv);
            return 0;
        }
        if (argc != 3 || std::string(argv[1]) != "self-test")
            throw std::invalid_argument("Usage: re4_module_tool self-test <fixture-directory>");
        std::cout << re4::port::verifyModules(argv[2]) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
