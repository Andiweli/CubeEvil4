#pragma once
#include "re4/port/contracts.h"
#include "re4/port/engine_loader.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace re4::port {
struct ModuleMetadata {
    std::uint32_t id = 0;
    std::uint16_t fileIndex = 0xffff;
    std::string name, object;
    std::vector<std::uint32_t> imports;
    std::uint8_t ctorSection = 0, dtorSection = 0, bssSection = 0;
};
struct RoomMetadata {
    std::uint16_t id = 0, fileIndex = 0;
    std::uint32_t module = 0;
    bool save = false, initCompiled = false, mainCompiled = false;
    std::string init, main;
};
struct ModuleCatalog {
    std::string sha256;
    std::vector<ModuleMetadata> modules;
    std::vector<RoomMetadata> rooms;
};
const ModuleCatalog &originalModuleCatalog(); // Generated from config and original tables.
struct RelModuleIndex {
    std::uint32_t id = 0, bssBytes = 0, exactBytes = 0;
    std::vector<std::uint32_t> imports;
    bool emptyConstructors = false, emptyDestructors = false;
};
// Validates original BE REL metadata only. No PPC relocation or code execution.
RelModuleIndex indexRelModule(const std::uint8_t *, std::size_t, const ModuleMetadata &);

class ModuleSnapshot {
  public:
    virtual ~ModuleSnapshot() = default;
};
class ModuleContext;
class NativeModuleState {
  public:
    virtual ~NativeModuleState() = default;
    virtual void prolog(ModuleContext &) = 0;
    virtual void epilog(ModuleContext &) = 0;
    // Explicit typed state: never memcpy a native BSS or a C++ object.
    virtual std::unique_ptr<ModuleSnapshot> save(ModuleContext &) const = 0;
    virtual void restore(ModuleContext &, const ModuleSnapshot &) = 0;
    virtual void invoke(ModuleContext &, std::uint16_t room, bool init) = 0;
    virtual void resetRoom(ModuleContext &, std::uint16_t room) = 0;
};
struct NativeModuleDefinition {
    std::uint32_t id;
    // Initialization dependencies, distinct from resident REL symbol imports.
    std::vector<std::uint32_t> activationDependencies;
    std::function<std::unique_ptr<NativeModuleState>()> create;
    std::string coverage; // Must identify partial original-code bindings / test adapters.
};
enum class ModulePhase { Inactive, Active, Stopped, Failed };
struct NativeModuleInfo {
    ModulePhase phase = ModulePhase::Inactive;
    std::uint64_t epoch = 0;
    std::string coverage;
    std::size_t reservations = 0;
};

class ModuleRuntime final : public NativeModuleRegistry {
  public:
    class Memory {
      public:
        virtual ~Memory() = default;
        virtual MemoryReservation reserve(std::uint32_t bytes) = 0;
        virtual void release(MemoryReservation &) = 0;
    };
    explicit ModuleRuntime(EngineLoader &, std::shared_ptr<Memory> = {});
    ~ModuleRuntime();
    ModuleRuntime(const ModuleRuntime &) = delete;
    ModuleRuntime &operator=(const ModuleRuntime &) = delete;
    void registerModule(NativeModuleDefinition); // Only while that module is inactive.
    void activate(std::uint16_t fileTableIndex) override;
    void deactivate(std::uint16_t fileTableIndex) override;
    void stop(std::uint16_t fileTableIndex) override;
    void restart(std::uint16_t fileTableIndex) override;
    ModuleToken lifetime(std::uint16_t fileTableIndex) const override;
    std::optional<RoomCallbacks> room(std::uint16_t roomId) const override;
    void activateId(std::uint32_t);
    void deactivateId(std::uint32_t);
    void stopId(std::uint32_t);
    void restartId(std::uint32_t);
    NativeModuleInfo info(std::uint32_t) const;
    void invoke(ModuleCallback);   // Reject inactive, missing, foreign and stale callbacks.
    void resetRoom(std::uint16_t); // Quiesce tasks, release work, rotate callback lifetime.
    ResourceLease roomWorkLease(std::uint16_t) const;
    TaskEntry bind(ModuleCallback); // Validity is checked again when actually executed.
    const std::vector<std::string> &trace() const;

  private:
    friend class ModuleContext;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Scoped original entry adapter. Work uses the supplied gameplay heap provider
// or the standalone MRAM allocator; stop retains owners and restore rebinds work.
class ModuleContext {
  public:
    using Work = std::map<std::uint16_t, MemoryReservation>;
    void registerRoom(std::uint16_t, RoomFunction init, RoomFunction main);
    void *calloc(std::uint32_t bytes, int releaseFlag, int heap, const char *, int);
    void observe(const std::string &);
    const Work &work() const;
    void restoreWork(const Work &);
    void *roomWork(std::uint16_t) const;
    NativeModuleState &state() const;
    static ModuleContext &active();
    struct Scope {
        explicit Scope(ModuleContext &);
        ~Scope();
        ModuleContext *previous;
    };

  private:
    friend class ModuleRuntime;
    struct Access;
    explicit ModuleContext(Access &, std::uint16_t room = 0xffff);
    Access &access_;
    std::uint16_t roomId_;
};
std::vector<NativeModuleDefinition> originalModuleDefinitions();
} // namespace re4::port
