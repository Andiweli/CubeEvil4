#pragma once
#include "enemy_encounter.h"
#include <memory>
namespace re4 {
class RoomInteractions;
// A room's original manually released EM12 actors. All EmMgr actors tick
// before a single Leon tick; the optional first-house owner remains borrowed
// for compatibility with the existing bridge and explicit combat tests.
class EnemyGroup {
public:
  EnemyGroup(const SatCollision &sat,const SatCollision &eat,std::string enemyDrs);
  EnemyEncounter &spawn(const EnemySpawn &source);
  EnemyEncounter *find(unsigned listIndex);
  const EnemyEncounter *find(unsigned listIndex) const;
  const std::vector<std::unique_ptr<EnemyEncounter>> &actors() const { return actors_; }
  void setDifficulty(unsigned rank,bool hard=false);
  void setObjectActions(ObjectActions *objects);
  void setRoomInteractions(const RoomInteractions *interiors) { interiors_=interiors; }
  void setRoutes(std::shared_ptr<const EnemyRoutes> routes);
  void setFeedbackCamera(const CollisionVec &eye);
  void releaseR100Ambush();
  void releaseVillageAlarm();
  unsigned villageAlive() const;
  bool villageFoundPlayer() const;
  void releaseR100HouseAmbush();
  void releaseR100CliffActors();
  using ExternalTarget=std::function<std::optional<AuthoredHitResult>(
      const CollisionVec &,const CollisionVec &,unsigned)>;
  // Borrowed room crow/dog/trap targets compete with the same authored ray.
  void setExternalTarget(ExternalTarget target) { externalTarget_=std::move(target); }
  void setScriptSuspended(unsigned index,bool suspended);
  void suspendInput() { kickPending_.reset(); }
  bool kickAvailable(const LeonPreview &player,const EnemyEncounter *primary=nullptr) const;
  std::optional<AuthoredHitResult> target(const CollisionVec &from,const CollisionVec &to,
                                          unsigned weaponNo=1,const EnemyEncounter *primary=nullptr) const;
  // Dispatch one already authored shot; reactions stay queued until the next
  // EmMgr tick, exactly as the coupled player path below does.
  void queueShot(const PlayerShot &shot,EnemyEncounter *primary=nullptr);
  void queueMelee(const PlayerMeleeEvent &melee,const CollisionVec &player,
                  EnemyEncounter *primary=nullptr);
  // GetWepTargetList2 clears scenery normal for any actor/object target.
  // Capture before queued damage removes the same target from later queries.
  std::vector<std::optional<SatHit>> takeShotScenery(unsigned shotSerial);
  // Whether the original sorted/limited target list selected the borrowed
  // crow/dog owner for each ray, before queued actor/object damage.
  std::vector<bool> takeShotExternal(unsigned shotSerial);
  std::vector<bool> takeMeleeExternal(unsigned meleeSerial);
  void stepPlayer(LeonPreview &player,WalkProbe &probe,double turn,double forward,
                  bool runHeld,bool runPressed,double seconds,const PlayerActionInput &input,
                  const std::function<void()> &beforeWorldTick={},
                  const PlayerSimulationTick &afterWorldTick={},EnemyEncounter *primary=nullptr);
  // Event s20 cut2 explicitly permits indices4/5 to keep their authored
  // room walk/idle motions updating while the player remains suspended.
  void tickScriptActors(LeonPreview &player,WalkProbe &probe,const std::vector<unsigned> &only={});
  std::uint64_t worldTicks() const { return worldTicks_; }
  // PlWepHitCheck2 counts n!=0 on the primary ray, independently of whether
  // queued damage changes HP; secondary shotgun rays (flag4) never count.
  std::uint64_t accuracyHits() const { return accuracyHits_; }
  unsigned sharedAttackWait() const { return attackWait_; }
private:
  const SatCollision &sat_,&eat_;
  std::string enemyDrs_;
  std::vector<std::unique_ptr<EnemyEncounter>> actors_;
  ObjectActions *objects_{};
  const RoomInteractions *interiors_{};
  ExternalTarget externalTarget_;
  std::shared_ptr<const EnemyRoutes> routes_;
  unsigned rank_=5,attackWait_{};
  bool hard_{};
  std::uint64_t worldTicks_{};
  std::uint64_t accuracyHits_{};
  unsigned sceneryShotSerial_{};
  std::vector<std::optional<SatHit>> shotScenery_;
  std::vector<bool> shotExternal_;
  unsigned meleeExternalSerial_{};
  std::vector<bool> meleeExternal_;
  std::optional<unsigned> kickPending_;
  std::vector<EnemyEncounter*> ordered(EnemyEncounter *primary) const;
  void contacts(WalkProbe &playerProbe,const LeonPreview &player,const std::vector<EnemyEncounter*> &actors);
  void queuePlayerHit(const PlayerShot *shot,const PlayerMeleeEvent *melee,
                       const CollisionVec &playerFeet,const std::vector<EnemyEncounter*> &actors);
};
} // namespace re4
