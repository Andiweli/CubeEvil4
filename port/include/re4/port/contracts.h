#pragma once
#include "re4/port/resources.h"
#include "re4/port/scheduler.h"
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
    // cRoomData::stopRelData saves state BEFORE epilog/unlink; restartRelData
    // links then restores state, WITHOUT a second prolog. Implementations must
    // explicitly snapshot native module state, never memcpy pointer-bearing BSS.
    virtual void stop(std::uint16_t fileTableIndex) = 0;
    virtual void restart(std::uint16_t fileTableIndex) = 0;
    virtual ModuleToken lifetime(std::uint16_t fileTableIndex) const = 0;
    virtual std::optional<RoomCallbacks> room(std::uint16_t roomId) const = 0;
};

// Phase-4 binding only. MRAM_READY is available now; BLOCK_CREATE must wait for
// a real consumer that owns cSmd/ObjMgr objects. References re-resolve by handle
// and byte offset after relocation; signed int GameCube pointer deltas are not ABI safe.
class BlockModelBinding {
  public:
    virtual ~BlockModelBinding() = default;
    virtual void create(std::uint8_t block, ResourceReference smd) = 0;
    virtual void destroy(std::uint8_t block) = 0;
    virtual void rebind(std::uint8_t block, ResourceReference movedData) = 0;
};

// Original PADStatus scalar fields (include/dolphin/pad.h), for a later input
// adapter. PadRead still owns Joy on/old/trg/rel, repeats and Key_type_tbl.
// No Android button mapping, event handling or pretend PADRead is implemented.
struct OriginalPadSample {
    std::uint16_t button = 0;
    std::int8_t stickX = 0, stickY = 0, substickX = 0, substickY = 0;
    std::uint8_t triggerLeft = 0, triggerRight = 0, analogA = 0, analogB = 0;
    std::int8_t error = -1;
};
static_assert(sizeof(OriginalPadSample) == 12, "Original PADStatus scalar ABI");
class PlatformGamepads {
  public:
    virtual ~PlatformGamepads() = default;
    // A frame-stable snapshot, sampled once at the gameplay tick, independent
    // of display frequency. Disconnect clears held axes/buttons and rumble.
    virtual OriginalPadSample sample(std::uint8_t port) = 0;
    virtual void rumble(std::uint8_t port, bool enabled) = 0;
};

// Import boundary only. Runtime DVD/ARAM asynchronous states are a separate
// required implementation, not represented by successful synchronous stubs.
class OriginalAssetSource {
  public:
    virtual ~OriginalAssetSource() = default;
    virtual std::vector<std::uint8_t> readFile(std::string_view originalPath) = 0;
};

} // namespace re4::port
