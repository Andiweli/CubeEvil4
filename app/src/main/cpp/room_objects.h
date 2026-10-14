#pragma once
#include "archive.h"
#include "scene.h"
#include "room_state.h"
#include <array>
#include <string>
#include <vector>

namespace re4 {
// Immutable GameCube ETS_DATA, from game/EtcModel.cpp / etc_model.h.
// Angles are already radians. These are authored room coordinates, not
// placements synthesized from the inspection camera or SAT polygons.
struct RoomObjectPlacement {
  unsigned id{}, slot{};
  std::size_t recordOffset{};
  std::array<float, 3> rotation{}, position{};
  std::array<std::uint8_t, 40> raw{};
};
std::vector<RoomObjectPlacement>
ReadRoomObjectPlacements(const std::vector<std::uint8_t> &ets);
// Original R100Init / r100_Car_pos_move: intact versus broken bridge.
// Only draw visibility changes; SAT and the original SMD disk data stay owned.
void ApplyRoomScriptVisibility(std::uint16_t room, const RoomSaveState &state,
                                StaticScene &scene);
void AppendStaticSceneLayer(StaticScene &scene, const StaticScene &layer);

struct RoomObjectAsset {
  std::string name;
  std::size_t headerOffset{}, dataOffset{}, extent{};
};
// ETM is a named, linear archive. It is not a DRS offset directory.
class RoomObjectArchive {
public:
  explicit RoomObjectArchive(std::vector<std::uint8_t> bytes);
  const std::vector<RoomObjectAsset> &assets() const { return assets_; }
  const RoomObjectAsset *find(const std::string &name) const;
  std::vector<std::uint8_t> copy(const std::string &name) const;

private:
  std::vector<std::uint8_t> bytes_;
  std::vector<RoomObjectAsset> assets_;
};

struct RoomObjectLayer {
  struct ScriptObject {
    enum Kind { VillagePyre, DoorLock } kind;
    unsigned binOccurrence{}, tplOccurrence{};
    int parentSlot = -1;
    std::array<float, 3> rotation{}, position{};
  };
  std::vector<RoomObjectPlacement> placements;
  // Source R101Init layout for the supplied room flags, USA effects.
  // Keep the original ETS records even when the current model is hidden.
  std::vector<unsigned> hiddenSlots;
  std::vector<ScriptObject> scriptObjects;
  StaticScene mesh;
  std::size_t rendered{}, unsupported{}, doors{}, windows{}, ladders{},
      containers{}, lights{};
  std::vector<std::string> warnings;
  std::string describe() const;
};
// Authored ETS/ETM placements + original EtcModel palette selection. Initial
// model poses only: door scripts, destruction, pickups and animals have their
// own runtime and are not implied by this geometry layer. r101 additionally
// applies the original USA R101Init layout: ladders 5/6/7 controlled by room
// flag 8, rack 17's scripted position, pyre, and the lock on door 11. Original
// broken-window variants are preloaded for selection by RoomInteractions.
// This layer does not execute campaign events or set their room flags.
RoomObjectLayer BuildRoomObjects(const RoomArchive &room,
                                 const std::vector<std::uint8_t> &coreEff,
                                 std::uint16_t roomId,
                                 const RoomSaveState *state = nullptr);
// Checked native BIN + TPL conversion, shared by ETS props and ITM pickups.
// Returns their original unanimated bind pose, or the authored standing
// ladder root pose when requested.
StaticScene BuildRoomModelScene(const std::vector<std::uint8_t> &bin,
                                const std::vector<std::uint8_t> &tpl,
                                bool ladder = false);
// Appends immutable geometry and textures to the existing room draw lists.
// Existing environment work numbers are never reused by these object draws.
void AppendRoomObjectScene(StaticScene &scene, const RoomObjectLayer &objects);
} // namespace re4
