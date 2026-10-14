#pragma once
#include "enemy_spawn.h"
#include "room_flow.h"
#include "room_state.h"
#include <set>
#include <map>

namespace re4 {
constexpr unsigned R100FirstLookArea = 0x0a;
constexpr unsigned R100HouseFlagArea = 0x09;
constexpr std::array<unsigned,2> R100S20LiveActorIndices{4,5};
constexpr float R100FirstPlayerHeading = 1.75f;
// Original r120.cpp: FMV -> s00 (drive) -> s01 (stop/exit) -> room 100
// s40 (officers at the road). SCF59 cancels every remaining scene in this chain.
constexpr unsigned OriginalOpeningCancelFlag = 59;
constexpr float R100EntryPlayerHeading = 2.246f;
CollisionVec R120RoadJumpPosition();
CollisionVec R100EntryPlayerPosition();
void BeginOriginalOpening(ScenarioState &state,bool movieCanceled);
void CancelOriginalOpening(ScenarioState &state);
bool OriginalOpeningCanceled(const ScenarioState &state);
bool BeginR100Entry(RoomSaveState &state);
CollisionVec R100FirstPlayerPosition();
// R100Init's EmSetEvent record has sentinel 0xff, not an ESL list identity.
// afterLook selects r100_Sce_look's placement after the original s03 event.
EnemySpawn R100FirstGanadoSpawn(bool afterLook = true);
struct R100RoomProfile {
  bool firstPresent{}, firstActive{}, afterTruck{}, ambushReleased{}, houseSeen{};
};
R100RoomProfile R100Profile(const RoomSaveState &state);
bool BeginR100FirstEncounter(RoomSaveState &state);
bool CompleteR100FirstEncounter(ScenarioState &state);
// r100_WindowBreakCk: the authored flag area writes Room_flg bit64 while
// Leon is in the house. Once he leaves after s20, RSF4/first3 flag0x80 fire.
bool ReleaseR100HouseAmbush(RoomSaveState &state,bool playerInsideHouse);
// Selection only. The seven authored actors require their original model,
// weapon and scenario AI; this function never substitutes a test Ganado.
std::vector<const EnemySpawn *> R100AmbushSpawns(
    const EnemySpawns &spawns, const RoomSaveState &state,
    const std::set<unsigned> &deadIndices = {});
struct R100SecondaryOffer {
  enum Kind { EngineEvent, RoomCamera, HouseScare, RoomMessage, CorpseMessage } kind=EngineEvent;
  unsigned area{},phase{},cameraCut=~0u,message{};
  bool preservePlayerPosition{};
  std::optional<CollisionVec> playerPosition;
};
// Source campaign triggers; event execution is owned by OriginalEvent.
class R100Scenario {
public:
  explicit R100Scenario(const std::vector<std::uint8_t> &aev);
  bool firstLookHit(const CollisionVec &feet, double heading,
                    const std::optional<CollisionVec> &clippedFront = {}) const;
  const RoomDoor &firstLookArea() const { return firstLook_; }
  bool houseInsideHit(const CollisionVec &feet,double heading,
                       const std::optional<CollisionVec> &clippedFront={}) const;
  const RoomDoor &houseFlagArea() const { return house_; }
  // Authored area3 targets both PL and active enemies and latches Room_flg0.
  bool doorClosureFlagHit(const CollisionVec &feet, double heading) const;
  const RoomDoor &authoredArea(unsigned number) const;
  std::optional<R100SecondaryOffer> secondaryOffer(const RoomSaveState &state,
      const CollisionVec &feet,double heading,bool actionPressed,bool actionHeld,
      const std::optional<CollisionVec> &clippedFront={}, bool doorMessageEnabled=false) const;
  // Call only after accepting an offer. Exactly the source once flags;
  // ordinary officer interactions do not acquire an invented visited flag.
  bool beginSecondary(RoomSaveState &state,const R100SecondaryOffer &offer) const;
private:
  RoomDoor firstLook_,house_;
  std::map<unsigned,RoomDoor> areas_;
};
} // namespace re4
