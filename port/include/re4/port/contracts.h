#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace re4::port {

// Proposed native boundaries. No implementations or GameCube API stubs are
// supplied. See docs/runtime-boundaries.md before adapting original call sites.
using RoomFunction = void (*)(); // ROOM_DATA::pInit/pMain in room_data.h
struct RoomCallbacks {
    RoomFunction init;
    RoomFunction main;
};

class NativeModuleRegistry {
public:
    virtual ~NativeModuleRegistry() = default;
    // fileTableIndex is ROOM_DATA::rel_file, not OSModuleHeader's module ID.
    virtual void activate(std::uint16_t fileTableIndex) = 0;
    virtual void deactivate(std::uint16_t fileTableIndex) = 0;
    virtual std::optional<RoomCallbacks> room(std::uint16_t roomId) const = 0;
};

// A typed native entry, NOT a drop-in reinterpret_cast of TaskFunc. Integer
// and other original callback signatures require audited typed trampolines.
struct TaskEntry {
    void (*function)(void*);
    void* argument;
};

class StackfulScheduler {
public:
    virtual ~StackfulScheduler() = default;
    virtual void exec(int slot, TaskEntry entry) = 0;
    virtual void sleep(int ticks) = 0;
    virtual void chain(TaskEntry entry) = 0;
    virtual void kill(int slot) = 0;
    virtual void runTick() = 0;
};

// Import boundary only. Runtime DVD/ARAM asynchronous states are a separate
// required implementation, not represented by successful synchronous stubs.
class OriginalAssetSource {
public:
    virtual ~OriginalAssetSource() = default;
    virtual std::vector<std::uint8_t> readFile(std::string_view originalPath) = 0;
};

} // namespace re4::port
