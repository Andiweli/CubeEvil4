#include "re4/port/module_verification.h"
#include "re4/port/modules.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace re4::port {
namespace {
struct Checks {
    unsigned total = 0;
    void test(bool v, const char *why) {
        ++total;
        if (!v)
            throw std::runtime_error(why);
    }
    template <class F> void rejects(F f) {
        bool failed = false;
        try {
            f();
        } catch (const std::exception &) {
            failed = true;
        }
        test(failed, "Expected rejected module operation");
    }
};
const ModuleMetadata &named(const char *name) {
    const auto &modules = originalModuleCatalog().modules;
    auto p =
        std::find_if(modules.begin(), modules.end(), [&](const auto &m) { return m.name == name; });
    if (p == modules.end())
        throw std::logic_error("Missing original module metadata");
    return *p;
}
struct Counts {
    int constructors = 0, destructors = 0, prologs = 0, epilogs = 0, restores = 0, saved = 0,
        value = 7;
    bool failProlog = false, failSave = false, failRestore = false;
};
// Isolated adapter only: represents a pointer-bearing C++ state, never RE4 gameplay.
class ProbeState final : public NativeModuleState {
    struct Object {
        int value = 7;
    };
    struct Snapshot final : ModuleSnapshot {
        int value;
        std::shared_ptr<Object> object;
    };
    Counts &counts;
    std::shared_ptr<Object> object = std::make_shared<Object>();

  public:
    explicit ProbeState(Counts &c) : counts(c) { ++counts.constructors; }
    ~ProbeState() override { ++counts.destructors; }
    void prolog(ModuleContext &) override {
        ++counts.prologs;
        if (counts.failProlog)
            throw std::runtime_error("probe prolog");
    }
    void epilog(ModuleContext &) override {
        ++counts.epilogs;
        counts.value = 0;
        object.reset();
    }
    std::unique_ptr<ModuleSnapshot> save(ModuleContext &) const override {
        ++counts.saved;
        if (counts.failSave)
            throw std::runtime_error("probe save");
        auto s = std::make_unique<Snapshot>();
        s->value = counts.value;
        s->object = object;
        return s;
    }
    void restore(ModuleContext &, const ModuleSnapshot &snapshot) override {
        if (counts.failRestore)
            throw std::runtime_error("probe restore");
        const auto &s = dynamic_cast<const Snapshot &>(snapshot);
        counts.value = s.value;
        object = s.object;
        if (!object || object->value != 7)
            throw std::logic_error("explicit object ownership not restored");
        ++counts.restores;
    }
    void invoke(ModuleContext &, std::uint16_t, bool) override {
        throw std::logic_error("Probe has no gameplay callback");
    }
    void resetRoom(ModuleContext &, std::uint16_t) override {
        throw std::logic_error("Probe has no room work");
    }
};
NativeModuleDefinition probe(const char *name, Counts &c, std::vector<std::uint32_t> deps = {}) {
    return {named(name).id, std::move(deps), [&c] { return std::make_unique<ProbeState>(c); },
            "isolated native lifecycle adapter; no gameplay"};
}
} // namespace
std::string verifyModules(const std::string &directory) {
    Checks check;
    std::filesystem::create_directories(directory);
    const auto catalog = std::filesystem::path(directory) / "empty-catalog.tsv";
    {
        std::ofstream f(catalog);
        f << "RE4-IMPORT-CATALOG-1\n0\t1\t3\tfixture."
             "dat\tba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
    }
    std::filesystem::create_directories(std::filesystem::path(directory) / "blobs");
    {
        std::ofstream f(std::filesystem::path(directory) / "blobs" /
                            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                        std::ios::binary);
        f << "abc";
    }
    auto files = std::make_shared<ImportedFiles>(directory, catalog.string());
    EngineLoader loader(files);
    TaskEntry staleAfterDestruction;
    std::vector<std::string> semantic;
    const auto &metadata = originalModuleCatalog();
    check.test(metadata.modules.size() == 114 && metadata.rooms.size() == 122,
               "Generated original catalog coverage");
    for (const auto &m : metadata.modules)
        for (auto import : m.imports)
            check.test(std::any_of(metadata.modules.begin(), metadata.modules.end(),
                                   [&](const auto &n) { return n.id == import; }),
                       "Unresolved original import identity");
    {
        ModuleRuntime runtime(loader);
        check.rejects([&] { runtime.activate(0xffff); });
        check.rejects([&] { runtime.activateId(0xffffffff); });
        for (const auto &d : originalModuleDefinitions()) {
            runtime.activateId(d.id);
            auto epoch = runtime.info(d.id).epoch;
            runtime.activateId(d.id);
            check.test(runtime.info(d.id).epoch == epoch &&
                           runtime.info(d.id).phase == ModulePhase::Active,
                       "Duplicate prolog activation");
            runtime.stopId(d.id);
            runtime.stopId(d.id);
            check.test(runtime.info(d.id).phase == ModulePhase::Stopped,
                       "Original stage epilog / stop");
            runtime.restartId(d.id);
            runtime.restartId(d.id);
            check.test(runtime.info(d.id).phase == ModulePhase::Active &&
                           runtime.info(d.id).epoch == epoch,
                       "Original stage restart must retain activation identity");
            runtime.deactivateId(d.id);
        }
        const auto a = named("st1_2").fileIndex, b = named("st3_3").fileIndex;
        for (auto id : {std::uint16_t(0x10d), std::uint16_t(0x328)}) {
            auto file = id == 0x10d ? a : b;
            runtime.activate(file);
            auto callback = runtime.room(id);
            check.test(callback.has_value(), "Actual original callback binding missing");
            runtime.invoke(callback->init);
            runtime.invoke(callback->main);
            auto work = runtime.roomWorkLease(id);
            check.test(work.size == 1 && work.data[0] == 0 &&
                           !(reinterpret_cast<std::uintptr_t>(work.data) & 31u),
                       "Original native work allocation");
            const_cast<std::uint8_t *>(work.data)[0] = 0x5a; // isolated work-lifetime stimulus
            check.rejects([&] { runtime.invoke(callback->init); });
            runtime.stop(file);
            check.test(!runtime.room(id) && work.data[0] == 0x5a, "Stop discarded original work");
            check.rejects([&] { runtime.invoke(callback->main); });
            runtime.restart(file);
            auto resumed = *runtime.room(id);
            check.test(runtime.roomWorkLease(id).data == work.data &&
                           runtime.roomWorkLease(id).data[0] == 0x5a,
                       "Typed pointer owner restoration");
            runtime.invoke(
                callback->main); // Temporarily stopped references resume with the same activation.
            check.rejects([&] { runtime.invoke(resumed.init); });
            runtime.invoke(resumed.main);
            runtime.resetRoom(id);
            check.rejects([&] { runtime.invoke(resumed.main); });
            auto fresh = *runtime.room(id);
            runtime.invoke(fresh.init);
            check.test(runtime.roomWorkLease(id).data != work.data &&
                           runtime.roomWorkLease(id).data[0] == 0,
                       "Room lifetime did not renew");
            auto token = runtime.lifetime(file);
            int afterSleep = 0;
            TaskEntry original = runtime.bind(fresh.main);
            auto task = loader.scheduler().execScenario(
                5,
                [&] {
                    original();
                    loader.scheduler().sleep(1);
                    ++afterSleep;
                },
                0, 2, nullptr, token);
            loader.scheduler().runScenarios();
            check.test(task && loader.scheduler().state(*task), "Owned original task not sleeping");
            runtime.deactivate(file);
            loader.scheduler().runScenarios();
            check.test(!loader.scheduler().state(*task) && afterSleep == 0 &&
                           loader.scheduler().allocatedStacks() == 0,
                       "Deactivation retained module task stack");
            check.rejects([&] { original(); });
            check.test(work.data[0] == 0x5a, "Pinned work lease became dangling");
            work = {};
            check.test(loader.memoryStats().mramBytes == 0, "Module allocations leaked");
            semantic.push_back(std::to_string(id) +
                               ":init,main,stop,restart,reset,cancel,inactive");
        }
        runtime.activate(named("st1_0").fileIndex);
        check.test(runtime.room(0x120).has_value() && !runtime.room(0x100),
                   "Reviewed r120 entry or unported r100 coverage differs");
        check.rejects([&] {
            runtime.invoke(runtime.room(0x120)->init);
        }); // Imports absent in standalone module tests.
        auto missing = ModuleCallback{0, runtime.info(named("st1_0").id).epoch, named("st1_0").id,
                                      0x120, true};
        check.rejects([&] { runtime.invoke(missing); });
        runtime.deactivate(named("st1_0").fileIndex);
        // Real original registration functions ran; no callback placeholder executed.
        check.test(std::count_if(runtime.trace().begin(), runtime.trace().end(),
                                 [](const auto &s) {
                                     return s.find("mem_calloc:1") != std::string::npos;
                                 }) == 4,
                   "Original init allocation call count");
        check.test(std::any_of(runtime.trace().begin(), runtime.trace().end(),
                               [](const auto &s) {
                                   return s.find("cRoomData::execMainFunc") != std::string::npos;
                               }),
                   "Original room dispatcher not executed");
        Counts dependency, parent;
        runtime.registerModule(probe("Tools", dependency));
        runtime.registerModule(probe("Sscrn", parent, {named("Tools").id}));
        check.rejects([&] { runtime.registerModule(probe("Tools", dependency)); });
        runtime.activateId(named("Sscrn").id);
        check.test(dependency.prologs == 1 && parent.prologs == 1,
                   "Native initialization dependency not resolved");
        check.rejects([&] { runtime.deactivateId(named("Tools").id); });
        runtime.stopId(named("Sscrn").id);
        check.test(parent.saved == 1 && parent.epilogs == 1 && parent.value == 0,
                   "Snapshot was not captured before epilog");
        runtime.restartId(named("Sscrn").id);
        check.test(parent.value == 7 && parent.prologs == 1 && parent.constructors == 1 &&
                       parent.restores == 1,
                   "Restart reran native constructors/prolog or lost pointer object");
        runtime.deactivateId(named("Sscrn").id);
        runtime.deactivateId(named("Tools").id);
        check.test(parent.destructors == 1 && dependency.destructors == 1,
                   "Native objects not destroyed exactly once");
        parent.failProlog = true;
        check.rejects([&] { runtime.activateId(named("Sscrn").id); });
        check.test(runtime.info(named("Sscrn").id).phase == ModulePhase::Inactive &&
                       runtime.info(named("Tools").id).phase == ModulePhase::Inactive,
                   "Failed initialization graph partially active");
        parent.failProlog = false;
        runtime.activateId(named("Sscrn").id);
        parent.failSave = true;
        check.rejects([&] { runtime.stopId(named("Sscrn").id); });
        check.test(runtime.info(named("Sscrn").id).phase == ModulePhase::Failed,
                   "Failed snapshot was reported stopped");
        runtime.deactivateId(named("Sscrn").id);
        runtime.deactivateId(named("Tools").id);
        parent.failSave = false;
        runtime.activateId(named("Sscrn").id);
        runtime.stopId(named("Sscrn").id);
        parent.failRestore = true;
        check.rejects([&] { runtime.restartId(named("Sscrn").id); });
        check.test(runtime.info(named("Sscrn").id).phase == ModulePhase::Failed,
                   "Failed restore was reported active");
        runtime.deactivateId(named("Sscrn").id);
        runtime.deactivateId(named("Tools").id);
        Counts cycleA, cycleB, absent;
        runtime.registerModule(probe("t_camera", cycleA, {named("t_emlist").id}));
        runtime.registerModule(probe("t_emlist", cycleB, {named("t_camera").id}));
        check.rejects([&] { runtime.activateId(named("t_camera").id); });
        check.test(cycleA.constructors == 0 && cycleB.constructors == 0,
                   "Cyclic graph constructed partial state");
        runtime.registerModule(probe("t_esp", absent, {0xffffff}));
        check.rejects([&] { runtime.activateId(named("t_esp").id); });
        check.test(absent.constructors == 0, "Missing dependency constructed state");
        runtime.activate(a);
        auto main = runtime.room(0x10d)->main;
        staleAfterDestruction = runtime.bind(main);
        bool wrongThread = false;
        std::thread worker([&] {
            try {
                runtime.invoke(main);
            } catch (...) {
                wrongThread = true;
            }
        });
        worker.join();
        check.test(wrongThread, "Module runtime accepted I/O thread mutation");
        {
            ModuleRuntime other(loader);
            other.activate(a);
            check.rejects([&] { other.invoke(main); });
        }
        auto token = runtime.lifetime(a);
        int resumed = 0, contextValue = 19;
        bool executingProtected = false;
        TaskEntry nested = [&] {
            check.test(loader.scheduler().moduleCallContext() == &contextValue,
                       "Borrowed context missing");
            executingProtected = !loader.scheduler().invalidateModule(token);
            loader.scheduler().sleep(1);
            check.test(loader.scheduler().moduleCallContext() == &contextValue,
                       "Context changed across sleep");
            ++resumed;
        };
        auto foreign = loader.scheduler().execScenario(
            5, [&] { loader.scheduler().callModule(token, nested, &contextValue); });
        loader.scheduler().runScenarios();
        check.test(executingProtected && foreign && loader.scheduler().state(*foreign),
                   "Active nested call was invalidated");
        loader.scheduler().runScenarios();
        check.test(resumed == 1 && !loader.scheduler().moduleCallContext(),
                   "Nested original stack did not resume/release");
        foreign = loader.scheduler().execScenario(
            5, [&] { loader.scheduler().callModule(token, nested, &contextValue); });
        loader.scheduler().runScenarios();
        auto visits = loader.scheduler().state(*foreign)->sleepVisits;
        runtime.stop(a);
        for (int i = 0; i < 3; ++i)
            loader.scheduler().runScenarios();
        check.test(loader.scheduler().state(*foreign) &&
                       loader.scheduler().state(*foreign)->sleepVisits == visits &&
                       loader.scheduler().allocatedStacks() == 1,
                   "Temporary module stop lost/decremented borrowed stack");
        runtime.restart(a);
        loader.scheduler().runScenarios();
        check.test(resumed == 2 && !loader.scheduler().state(*foreign) &&
                       loader.scheduler().allocatedStacks() == 0,
                   "Temporary module restart did not resume exact stack");
        foreign = loader.scheduler().execScenario(
            5, [&] { loader.scheduler().callModule(token, nested, &contextValue); });
        loader.scheduler().runScenarios();
        runtime.deactivate(a);
        check.test(!loader.scheduler().state(*foreign) && !loader.scheduler().moduleCallContext() &&
                       loader.scheduler().allocatedStacks() == 0,
                   "Borrowed module lifetime cancellation leaked");
        runtime.activate(a);
        token = runtime.lifetime(a);
        bool directProtected = false;
        loader.scheduler().callModule(
            token, [&] { directProtected = !loader.scheduler().invalidateModule(token); });
        check.test(directProtected, "Direct callback was invalidated on owner stack");
        loader.scheduler().callModule(
            token, [&] { check.rejects([&] { runtime.activateId(named("st1_0").id); }); },
            &contextValue);
        auto captured = std::make_shared<int>(31);
        std::weak_ptr<int> retained = captured;
        auto replacement = bindTask([captured] { ++*captured; });
        TaskEntry chain = [&] { loader.scheduler().chain(replacement); };
        auto chained = loader.scheduler().execScenario(
            5, [&] { loader.scheduler().callModule(token, chain, &contextValue); });
        loader.scheduler().runScenarios();
        loader.scheduler().runScenarios();
        check.test(!loader.scheduler().state(*chained) && *captured == 32 &&
                       !loader.scheduler().moduleCallContext(),
                   "Task chain retained borrowed module context");
        replacement = {};
        captured.reset();
        check.test(retained.expired(), "Abandoned chain parameter retained callback capture");
        semantic.push_back("adapter:dependency,snapshot-before-epilog,typed-object,rollback,cycle,"
                           "stale,borrowed-sleep,pause-exact-resume,cancel,chain-release");
    }
    check.rejects([&] { staleAfterDestruction(); });
    check.test(loader.memoryStats().mramBytes == 0, "Destroyed registry leaked MRAM");
    std::ostringstream result;
    result << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"checks\":" << check.total
           << ",\"failed\":0,\"catalog_sha256\":\"" << metadata.sha256
           << "\",\"modules\":114,\"room_bindings\":122,\"original_registration_components\":14,"
              "\"original_init_main_pairs\":2,\"trace\":[";
    for (std::size_t i = 0; i < semantic.size(); ++i) {
        if (i)
            result << ',';
        result << '"' << semantic[i] << '"';
    }
    result << "]}";
    return result.str();
}
} // namespace re4::port
