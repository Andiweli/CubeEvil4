#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include "re4/port/scheduler.h"

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

// Import boundary only. Runtime DVD/ARAM asynchronous states are a separate
// required implementation, not represented by successful synchronous stubs.
class OriginalAssetSource {
public:
    virtual ~OriginalAssetSource() = default;
    virtual std::vector<std::uint8_t> readFile(std::string_view originalPath) = 0;
};

} // namespace re4::port
