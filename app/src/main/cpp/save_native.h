#pragma once
#include "save_game.h"
#include "room_state.h"
#include "inventory_case.h"
#include "room_pickups.h"
#include <optional>
namespace re4 {
struct NativeSaveLooseItem {
  std::uint8_t type{},area{};
  std::int8_t effect=-1;
  std::uint16_t room{},id{},count{};
  std::array<float,3> position{}; // Source units, before the pickup glow offset.
};
using NativeSaveEnemyFlags=std::array<std::array<std::uint32_t,8>,12>;
struct NativeSaveSnapshot {
  ScenarioState scenario;
  std::vector<CaseItem> inventory;
  unsigned caseSize{},equippedSlot=~0u;
  std::array<float,3> position{};
  float heading{};
  std::uint16_t room=0x100,previousRoom{},life=1200,maxLife=1200;
  std::uint16_t ashleyLife=600,ashleyMaxLife=600;
  std::uint8_t playerType{};
  std::uint8_t part{},previousPart{},jumpPoint{},terminal{},language=1;
  std::uint8_t chapter{},difficulty=5,gameLevel=5;
  std::uint16_t saveCount{},roundCount{};
  std::uint16_t roomContinues{},chapterContinues{},totalContinues{};
  std::optional<std::int32_t> saveKind;
  std::array<std::uint32_t,3> radioHeard{};
  std::uint32_t radioMessageNo=24;
  std::uint32_t playSeconds{},chapterKills{},totalKills{},chapterHits{},totalHits{},chapterShots{},totalShots{};
  std::int32_t difficultyPoint=5500;
  // Missing optional data preserves the corresponding original raw section.
  std::optional<NativeSaveEnemyFlags> enemyDeaths;
  int enemyListNo=0;
  // Original ESL is255*32; the saved global list has256 records. Either exact
  // length is accepted; the source sentinel remains zero for the short input.
  std::optional<std::vector<std::uint8_t>> enemyList;
  std::optional<std::vector<NativeSaveLooseItem>> looseItems;
  std::map<std::uint16_t,std::uint8_t> passedRooms;
};
const std::vector<std::uint16_t> &OriginalSaveRoomIds();
OriginalSaveSections NewOriginalSaveSections();
// Write/read explicit source big-endian offsets. A loaded basis retains all
// unsupported original fields, merchant/subscreen state and room BGM tables.
OriginalSaveSections EncodeNativeSave(const NativeSaveSnapshot &snapshot,
                                      const OriginalSaveSections *basis=nullptr);
NativeSaveSnapshot DecodeNativeSave(const OriginalSaveSections &sections);
// game.cpp::GameContinue(0), then GameAddPoint(LVADD_DIE), for the Leon campaign.
NativeSaveSnapshot RestoreNativeCheckpoint(const NativeSaveSnapshot &checkpoint,
                                          const NativeSaveSnapshot &current);
std::vector<NativeSaveLooseItem> CaptureOriginalPersistentDrops(
    const std::map<std::uint16_t,std::vector<RoomPickupRecord>> &drops);
std::map<std::uint16_t,std::vector<RoomPickupRecord>> RestoreOriginalPersistentDrops(
    const std::vector<NativeSaveLooseItem> &items);
OriginalSaveCardArt ParseOriginalSaveCardArt(const std::vector<std::uint8_t> &memcardDas,
                                          bool japanese=false);
OriginalSaveCardArt LoadOriginalSaveCardArt(const std::string &discFiles,bool japanese=false);
bool OriginalSaveEnemyDead(const NativeSaveEnemyFlags &flags,unsigned list,unsigned index);
void SetOriginalSaveEnemyDead(NativeSaveEnemyFlags &flags,unsigned list,unsigned index,bool dead=true);
} // namespace re4
