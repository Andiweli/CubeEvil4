#pragma once
#include "room_flow.h"
#include "room_state.h"
#include "enemy_spawn.h"
#include <map>
#include <optional>
#include <set>
namespace re4 {
enum class VillageEvent { Binoculars,House,Bell };
struct VillageCue {
  enum Kind { Spawn,Alarm,RemoveGanados,StartBell,BellComplete,FirstWaveCamera,TowerBottle,CombatStart,CombatStop } kind;
  std::vector<unsigned> indices;
  unsigned camera{};
};
// Native, pointer-free port of st1/r101.cpp. All deadlines use source 30-Hz ticks.
class R101Scenario {
public:
  explicit R101Scenario(const std::vector<std::uint8_t> &aev);
  static const std::vector<unsigned> &actorIndices();
  std::vector<unsigned> initialSpawns(const EnemySpawns &esl,ScenarioState &state,
      const std::set<unsigned> &dead={}) const;
  void resume(const ScenarioState &state,unsigned alive);
  void tick(ScenarioState &state,const CollisionVec &feet,double heading,
      unsigned alive,bool foundPlayer,bool eventPaused=false,bool canStartEvent=true);
  std::optional<VillageEvent> offer(const ScenarioState &state,const CollisionVec &feet,
      double heading,bool pressed,bool held,const std::optional<CollisionVec> &front={}) const;
  bool binocularAvailable(const ScenarioState &state,const CollisionVec &feet,
      double heading,const std::optional<CollisionVec> &front={}) const;
  void begin(VillageEvent event,ScenarioState &state,unsigned alive);
  void finish(VillageEvent event,ScenarioState &state);
  void finishWaveCamera();
  std::vector<VillageCue> takeCues();
  const DoorArea &towerTarget() const {return areas_.at(0x15).area;}
  unsigned battleTicks() const {return battleTicks_;}
  unsigned entryTicks() const {return entryTicks_;}
  int kills(unsigned alive) const {return baseline_-int(alive);}
private:
  void alarm(ScenarioState &state,unsigned alive);
  void startBell(ScenarioState &state);
  void spawn(unsigned first,unsigned last,bool addToCount=true);
  bool hit(unsigned area,const CollisionVec &feet,double heading,
      const std::optional<CollisionVec> &front={}) const;
  std::map<unsigned,RoomDoor> areas_;
  std::vector<VillageCue> cues_;
  std::optional<VillageEvent> pendingEvent_;
  unsigned battleTicks_{},entryTicks_{},countDelay_{},waveDelay_{},bellDelay_{},towerTicks_{};
  int baseline_{},houseDifference_{};
  bool counting_{},wavePending_{},bellPending_{},cameraPending_{};
  bool postBellCombat_{};
};
} // namespace re4
