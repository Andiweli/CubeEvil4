#include "re4/port/modules.h"
#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <set>
#include <thread>

namespace re4::port {
namespace {
std::atomic<std::uint64_t> identities{1};
thread_local ModuleContext *hookContext = nullptr;
} // namespace
struct ModuleContext::Access {
    EngineLoader *loader = nullptr;
    NativeModuleState *state = nullptr;
    std::uint32_t id = 0;
    Work work;
    std::vector<MemoryReservation> allocations;
    std::map<std::uint16_t, std::pair<RoomFunction, RoomFunction>> bindings;
    std::vector<std::string> *trace = nullptr;
};
ModuleContext::ModuleContext(Access &access, std::uint16_t room) : access_(access), roomId_(room) {}
ModuleContext::Scope::Scope(ModuleContext &c) : previous(hookContext) { hookContext = &c; }
ModuleContext::Scope::~Scope() { hookContext = previous; }
ModuleContext &ModuleContext::active() {
    if (auto *p = NativeScheduler::active().moduleCallContext())
        return *static_cast<ModuleContext *>(p);
    if (!hookContext)
        throw std::logic_error("No native module entry context");
    return *hookContext;
}
NativeModuleState &ModuleContext::state() const { return *access_.state; }
const ModuleContext::Work &ModuleContext::work() const { return access_.work; }
void ModuleContext::restoreWork(const Work &work) {
    for (const auto &p : work) {
        if (!p.second.data || std::none_of(access_.allocations.begin(), access_.allocations.end(),
                                           [&](const auto &owned) {
                                               return owned.data == p.second.data &&
                                                      owned.runtime == p.second.runtime;
                                           }))
            throw std::logic_error("Snapshot contains foreign or expired work");
    }
    access_.work = work;
}
void *ModuleContext::roomWork(std::uint16_t id) const {
    const auto it = access_.work.find(id);
    return it == access_.work.end() ? nullptr : it->second.data;
}
void ModuleContext::observe(const std::string &name) {
    access_.trace->push_back(std::to_string(access_.id) + ":" + name);
}
void ModuleContext::registerRoom(std::uint16_t room, RoomFunction init, RoomFunction main) {
    const auto &rows = originalModuleCatalog().rooms;
    auto metadata =
        std::find_if(rows.begin(), rows.end(), [&](const auto &r) { return r.id == room; });
    if (metadata == rows.end())
        throw std::logic_error("Uncatalogued original room registration");
    if (metadata->module != access_.id) {
        if (init || main)
            throw std::logic_error("Unresolved cross-module registration owner");
        return;
    }
    access_.bindings[room] = {init, main};
}
void *ModuleContext::calloc(std::uint32_t bytes, int flag, int heap, const char *, int) {
    if (flag != 1 || heap != 13 || roomId_ == 0xffff)
        throw std::logic_error("Unbound original heap mode or non-room allocation");
    auto memory = access_.loader->reserveMemory(bytes);
    access_.allocations.push_back(memory);
    access_.work[roomId_] = memory;
    observe("mem_calloc:" + std::to_string(bytes));
    return memory.data;
}

struct ModuleRuntime::Impl {
    struct Control {
        ModuleRuntime *owner;
    };
    struct Entry {
        NativeModuleDefinition definition;
        NativeModuleInfo info;
        ModuleToken token;
        std::unique_ptr<NativeModuleState> state;
        std::unique_ptr<ModuleSnapshot> snapshot;
        ModuleContext::Access access;
        std::unique_ptr<ModuleContext> context;
        std::map<std::pair<std::uint16_t, bool>, TaskEntry> calls;
        std::map<std::pair<std::uint16_t, bool>, std::unique_ptr<ModuleContext>> callContexts;
        std::set<std::uint16_t> initialized;
        std::set<std::uint16_t> savedInitialized;
    };
    EngineLoader &loader;
    const std::thread::id owner = std::this_thread::get_id();
    const std::uint64_t identity = identities.fetch_add(1);
    std::uint64_t epoch = 0;
    bool transitioning = false;
    std::map<std::uint32_t, Entry> entries;
    std::vector<std::string> trace;
    std::shared_ptr<Control> control;
    explicit Impl(EngineLoader &l, ModuleRuntime *r)
        : loader(l), control(std::make_shared<Control>(Control{r})) {}
    void check() const {
        if (owner != std::this_thread::get_id())
            throw std::logic_error("Module runtime owner thread required");
    }
    void transitionCheck() const {
        check();
        if (loader.scheduler().current() || loader.scheduler().moduleCallContext())
            throw std::logic_error("Module lifecycle transitions require the frame owner stack");
    }
    std::uint64_t next() {
        if (epoch == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Module callback epoch exhausted");
        return ++epoch;
    }
    Entry &entry(std::uint32_t id) {
        auto i = entries.find(id);
        if (i == entries.end())
            throw std::logic_error("Original module has no compiled native component: " +
                                   std::to_string(id));
        return i->second;
    }
    std::uint32_t fileId(std::uint16_t index) const {
        if (index == 0xffff)
            throw std::invalid_argument("Not a FileTbl module index");
        const auto &m = originalModuleCatalog().modules;
        auto i =
            std::find_if(m.begin(), m.end(), [&](const auto &v) { return v.fileIndex == index; });
        if (i == m.end())
            throw std::invalid_argument("Unknown original FileTbl module index");
        return i->id;
    }
    void release(Entry &e) {
        e.snapshot.reset();
        e.access.work.clear();
        for (auto &memory : e.access.allocations)
            loader.releaseMemory(memory);
        e.access.allocations.clear();
        e.calls.clear();
        e.callContexts.clear();
        e.initialized.clear();
        e.savedInitialized.clear();
        e.access.bindings.clear();
        e.state.reset();
        e.access.state = nullptr;
        e.context.reset();
    }
    void ensureNotUsed(std::uint32_t id) {
        for (const auto &p : entries)
            if (p.first != id && p.second.info.phase != ModulePhase::Inactive &&
                std::find(p.second.definition.activationDependencies.begin(),
                          p.second.definition.activationDependencies.end(),
                          id) != p.second.definition.activationDependencies.end())
                throw std::logic_error("Module still required by active/stopped dependent");
    }
};
ModuleRuntime::ModuleRuntime(EngineLoader &loader) : impl_(std::make_unique<Impl>(loader, this)) {
    for (auto definition : originalModuleDefinitions())
        registerModule(std::move(definition));
}
ModuleRuntime::~ModuleRuntime() {
    impl_->check();
    impl_->control->owner = nullptr;
    // Fail closed instead of releasing data still on a currently executing stack.
    std::vector<ModuleToken> tokens;
    for (const auto &p : impl_->entries)
        if (!(p.second.token == ModuleToken{}))
            tokens.push_back(p.second.token);
    if (!impl_->loader.scheduler().invalidateModules(tokens))
        std::terminate();
    std::vector<Impl::Entry *> reverse;
    for (auto &p : impl_->entries)
        reverse.push_back(&p.second);
    std::sort(reverse.begin(), reverse.end(),
              [](const auto *a, const auto *b) { return a->info.epoch > b->info.epoch; });
    for (auto *p : reverse) {
        auto &e = *p;
        try {
            if (e.info.phase == ModulePhase::Active)
                e.state->epilog(*e.context);
            impl_->release(e);
        } catch (...) {
            std::terminate();
        }
    }
}
void ModuleRuntime::registerModule(NativeModuleDefinition d) {
    impl_->check();
    const auto &m = originalModuleCatalog().modules;
    if (impl_->transitioning || !d.create || d.coverage.empty() ||
        std::none_of(m.begin(), m.end(), [&](const auto &v) { return v.id == d.id; }) ||
        impl_->entries.count(d.id))
        throw std::invalid_argument(
            "Duplicate, uncatalogued or incomplete native module definition");
    auto &e = impl_->entries[d.id];
    e.definition = std::move(d);
    e.info.coverage = e.definition.coverage;
}
void ModuleRuntime::activateId(std::uint32_t id) {
    impl_->transitionCheck();
    if (impl_->transitioning)
        throw std::logic_error("Reentrant module transition");
    auto &requested = impl_->entry(id);
    if (requested.info.phase == ModulePhase::Active)
        return;
    std::vector<std::uint32_t> order;
    std::set<std::uint32_t> visited, visiting;
    std::function<void(std::uint32_t)> visit = [&](auto n) {
        auto &e = impl_->entry(n);
        if (e.info.phase == ModulePhase::Active || visited.count(n))
            return;
        if (e.info.phase != ModulePhase::Inactive)
            throw std::logic_error("Stopped/failed dependency requires explicit recovery");
        if (!visiting.insert(n).second)
            throw std::logic_error("Cyclic native initialization dependency");
        for (auto dep : e.definition.activationDependencies)
            visit(dep);
        visiting.erase(n);
        visited.insert(n);
        order.push_back(n);
    };
    visit(id); // Resolve the entire initialization graph before changing state.
    impl_->transitioning = true;
    std::vector<std::uint32_t> started;
    try {
        for (auto n : order) {
            auto &e = impl_->entry(n);
            e.state = e.definition.create();
            if (!e.state)
                throw std::logic_error("Null native module state");
            started.push_back(n);
            e.access.loader = &impl_->loader;
            e.access.id = n;
            e.access.trace = &impl_->trace;
            e.access.state = e.state.get();
            e.context.reset(new ModuleContext(e.access));
            e.token = impl_->loader.scheduler().createModuleLifetime();
            e.info.epoch = impl_->next();
            e.state->prolog(*e.context);
            for (const auto &r : e.access.bindings)
                for (bool init : {false, true}) {
                    if (!(init ? r.second.first : r.second.second))
                        continue;
                    const auto roomId = r.first;
                    auto key = std::make_pair(roomId, init);
                    e.callContexts[key].reset(new ModuleContext(e.access, roomId));
                    auto *context = e.callContexts[key].get();
                    e.calls[key] = [&e, context, roomId, init] {
                        e.state->invoke(*context, roomId, init);
                    };
                }
            e.info.phase = ModulePhase::Active;
            impl_->trace.push_back(std::to_string(n) + ":active");
        }
        impl_->transitioning = false;
    } catch (...) {
        const auto originalFailure = std::current_exception();
        for (auto i = started.rbegin(); i != started.rend(); ++i) {
            auto &e = impl_->entry(*i);
            impl_->loader.scheduler().invalidateModule(e.token);
            e.token = {};
            try {
                if (e.info.phase == ModulePhase::Active)
                    e.state->epilog(*e.context);
                impl_->release(e);
                e.info.phase = ModulePhase::Inactive;
                e.info.epoch = impl_->next();
            } catch (...) {
                e.info.phase = ModulePhase::Failed;
            }
        }
        impl_->transitioning = false;
        std::rethrow_exception(originalFailure);
    }
}
void ModuleRuntime::stopId(std::uint32_t id) {
    impl_->transitionCheck();
    if (impl_->transitioning)
        throw std::logic_error("Reentrant module transition");
    auto &e = impl_->entry(id);
    if (e.info.phase == ModulePhase::Stopped)
        return;
    if (e.info.phase != ModulePhase::Active)
        throw std::logic_error("Stop requires active module");
    impl_->ensureNotUsed(id);
    if (!impl_->loader.scheduler().canInvalidateModules({e.token}))
        throw std::logic_error("Module code still executing");
    impl_->transitioning = true;
    try {
        if (!impl_->loader.scheduler().pauseModule(e.token))
            throw std::logic_error("Module became busy before state save");
        // cRoomData::stopRelData saves BEFORE DLL_Unlink invokes the epilog.
        e.snapshot = e.state->save(*e.context);
        if (!e.snapshot)
            throw std::logic_error("Module provided no explicit state snapshot");
        e.savedInitialized = e.initialized;
        e.state->epilog(*e.context);
        e.info.phase = ModulePhase::Stopped;
        impl_->trace.push_back(std::to_string(id) + ":stopped");
        impl_->transitioning = false;
    } catch (...) {
        if (!(e.token == ModuleToken{})) {
            impl_->loader.scheduler().invalidateModule(e.token);
            e.token = {};
        }
        e.info.phase = ModulePhase::Failed;
        impl_->transitioning = false;
        throw;
    }
}
void ModuleRuntime::restartId(std::uint32_t id) {
    impl_->transitionCheck();
    if (impl_->transitioning)
        throw std::logic_error("Reentrant module transition");
    auto &e = impl_->entry(id);
    if (e.info.phase == ModulePhase::Active)
        return;
    if (e.info.phase != ModulePhase::Stopped || !e.snapshot)
        throw std::logic_error("Restart requires saved module state");
    impl_->transitioning = true;
    try {
        // Equivalent of link THEN restore, with NO second prolog/constructor.
        e.state->restore(*e.context, *e.snapshot);
        e.initialized = e.savedInitialized;
        if (!impl_->loader.scheduler().resumeModule(e.token))
            throw std::logic_error("Stopped module lifetime was externally invalidated");
        e.info.phase = ModulePhase::Active;
        impl_->trace.push_back(std::to_string(id) + ":restarted");
        impl_->transitioning = false;
    } catch (...) {
        impl_->loader.scheduler().invalidateModule(e.token);
        e.token = {};
        e.info.phase = ModulePhase::Failed;
        impl_->transitioning = false;
        throw;
    }
}
void ModuleRuntime::deactivateId(std::uint32_t id) {
    impl_->transitionCheck();
    if (impl_->transitioning)
        throw std::logic_error("Reentrant module transition");
    auto &e = impl_->entry(id);
    if (e.info.phase == ModulePhase::Inactive)
        return;
    impl_->ensureNotUsed(id);
    if (!(e.token == ModuleToken{}) && !impl_->loader.scheduler().invalidateModule(e.token))
        throw std::logic_error("Module code still executing");
    e.token = {};
    impl_->transitioning = true;
    try {
        if (e.info.phase == ModulePhase::Active)
            e.state->epilog(*e.context);
        impl_->release(e);
        e.info.epoch = impl_->next();
        e.info.phase = ModulePhase::Inactive;
        impl_->trace.push_back(std::to_string(id) + ":inactive");
        impl_->transitioning = false;
    } catch (...) {
        e.info.phase = ModulePhase::Failed;
        impl_->transitioning = false;
        throw;
    }
}
void ModuleRuntime::activate(std::uint16_t f) { activateId(impl_->fileId(f)); }
void ModuleRuntime::deactivate(std::uint16_t f) { deactivateId(impl_->fileId(f)); }
void ModuleRuntime::stop(std::uint16_t f) { stopId(impl_->fileId(f)); }
void ModuleRuntime::restart(std::uint16_t f) { restartId(impl_->fileId(f)); }
ModuleToken ModuleRuntime::lifetime(std::uint16_t f) const {
    impl_->check();
    auto &e = impl_->entry(impl_->fileId(f));
    if (e.info.phase != ModulePhase::Active)
        throw std::logic_error("Inactive module lifetime");
    return e.token;
}
NativeModuleInfo ModuleRuntime::info(std::uint32_t id) const {
    impl_->check();
    auto &e = impl_->entry(id);
    auto info = e.info;
    info.reservations = e.access.allocations.size();
    return info;
}
std::optional<RoomCallbacks> ModuleRuntime::room(std::uint16_t id) const {
    impl_->check();
    auto meta =
        std::find_if(originalModuleCatalog().rooms.begin(), originalModuleCatalog().rooms.end(),
                     [&](const auto &r) { return r.id == id; });
    if (meta == originalModuleCatalog().rooms.end())
        return {};
    auto it = impl_->entries.find(meta->module);
    if (it == impl_->entries.end() || it->second.info.phase != ModulePhase::Active)
        return {};
    const auto &e = it->second;
    if (!e.calls.count({id, true}) || !e.calls.count({id, false}))
        return {};
    return RoomCallbacks{{impl_->identity, e.info.epoch, meta->module, id, true},
                         {impl_->identity, e.info.epoch, meta->module, id, false}};
}
void ModuleRuntime::invoke(ModuleCallback callback) {
    impl_->check();
    auto &e = impl_->entry(callback.module);
    if (callback.runtime != impl_->identity || callback.epoch != e.info.epoch ||
        e.info.phase != ModulePhase::Active || impl_->transitioning)
        throw std::logic_error("Stale, foreign or inactive module callback");
    auto call = e.calls.find({callback.room, callback.init});
    if (call == e.calls.end())
        throw std::logic_error("Original function is not compiled; dependencies remain unresolved");
    if (callback.init && !e.initialized.insert(callback.room).second)
        throw std::logic_error("Room already initialized; reset its lifetime first");
    try {
        impl_->loader.scheduler().callModule(
            e.token, call->second, e.callContexts.at({callback.room, callback.init}).get());
    } catch (...) {
        if (callback.init)
            e.initialized.erase(callback.room);
        throw;
    }
}
TaskEntry ModuleRuntime::bind(ModuleCallback callback) {
    impl_->check();
    auto control = impl_->control;
    return [control, callback] {
        if (!control->owner)
            throw std::logic_error("Destroyed module registry callback");
        control->owner->invoke(callback);
    };
}
void ModuleRuntime::resetRoom(std::uint16_t id) {
    impl_->transitionCheck();
    auto binding = room(id);
    if (!binding)
        throw std::logic_error("Room is not compiled/active");
    auto &e = impl_->entry(binding->init.module);
    if (!impl_->loader.scheduler().invalidateModule(e.token))
        throw std::logic_error("Room work is still executing");
    e.token = {};
    impl_->transitioning = true;
    try {
        // This reviewed component owns one plain work allocation per room. General
        // model/object destructors require their real consumer, not this data boundary.
        e.snapshot.reset();
        e.savedInitialized.clear();
        e.state->resetRoom(*e.context, id);
        auto p = e.access.work.find(id);
        if (p != e.access.work.end()) {
            const auto address = p->second.data;
            e.access.work.erase(p);
            for (auto it = e.access.allocations.begin(); it != e.access.allocations.end();) {
                if (it->data == address) {
                    impl_->loader.releaseMemory(*it);
                    it = e.access.allocations.erase(it);
                } else
                    ++it;
            }
        }
        e.initialized.erase(id);
        e.token = impl_->loader.scheduler().createModuleLifetime();
        e.info.epoch = impl_->next();
        impl_->transitioning = false;
    } catch (...) {
        e.info.phase = ModulePhase::Failed;
        impl_->transitioning = false;
        throw;
    }
}
ResourceLease ModuleRuntime::roomWorkLease(std::uint16_t id) const {
    impl_->check();
    auto binding = room(id);
    if (!binding)
        throw std::logic_error("Room is not compiled/active");
    const auto &e = impl_->entry(binding->init.module);
    auto p = e.access.work.find(id);
    if (p == e.access.work.end())
        throw std::logic_error("Room work is not initialized");
    return {p->second.owner, p->second.data, p->second.size};
}
const std::vector<std::string> &ModuleRuntime::trace() const {
    impl_->check();
    return impl_->trace;
}
} // namespace re4::port
