#pragma once
#include "enemy_preview.h"
#include "enemy_spawn.h"
#include "enemy_feedback.h"
#include "hit_geometry.h"
#include "walk_probe.h"
#include "enemy_routes.h"
#include "enemy_awareness.h"
namespace re4 {
class ObjectActions;
class EnemyGroup;
struct ObjectBlast;
// The explicit ESL test actor or r100's authored first-house event actor.
// Approach/stop and selected reactions are a bounded native adapter, not
// cEm10's complete campaign state machine.
class EnemyEncounter {
public:
  enum class DamageSource { None, Bullet, Knife, Kick, Escape, Blast, Flash };
  static constexpr unsigned TestSpawn = 0x25;
  static CollisionVec TestPlayerPosition() { return {-82910, 860, -38480}; }
  struct TestPlacement {
    CollisionVec position;
    float heading;
  };
  static CollisionVec TerrainPlayerPosition() { return {-12400, 2576, 31080}; }
  static TestPlacement TerrainEnemyPlacement() {
    return {{-12400, 2274, 28080}, 0};
  }
  EnemyEncounter(const SatCollision &sat, const SatCollision &eat,
                 const EnemySpawn &spawn, const std::string &enemyDrs,
                 bool melee = false,
                 const std::optional<TestPlacement> &placement = {});
  void tick(const CollisionVec &player, const PlayerShot *shot = nullptr);
  void stepPlayer(LeonPreview &player, WalkProbe &probe, double turn,
                  double forward, bool runHeld, bool runPressed, double seconds,
                  const PlayerActionInput &input,
                  const std::function<void()> &beforeWorldTick = {},
                  const PlayerSimulationTick &afterWorldTick = {});
  // Only the original weapon13 Ganado branch. The room adapter applies
  // PlBombHitCk to Leon separately, and handles object chain damage itself.
  bool applyBlast(const ObjectBlast &blast, LeonPreview &player,
                  WalkProbe &probe);
  void queueProjectileImpact(const PlayerShot &shot) {queueShot(&shot);}
  std::optional<AuthoredHitResult> target(const CollisionVec &from,
                                        const CollisionVec &to,unsigned weapon=1) const;
  // Original GetWepTargetList2 scenery result before queued damage changes
  // actor eligibility. One entry per primary/secondary firearm ray.
  std::vector<std::optional<SatHit>> takeShotScenery(unsigned shotSerial);
  bool kickAvailable(const LeonPreview &player) const;
  void queueMelee(const PlayerMeleeEvent &event,
                  const CollisionVec &playerFeet);
  // Borrowed room runtime, replaced together with the room's collision data.
  // With no object runtime the existing encounter path remains unchanged.
  void setObjectActions(ObjectActions *objects) { objects_ = objects; }
  void suspendInput() { kickPending_ = false; }
  std::vector<EnemySoundEvent> drainSounds();
  CollisionVec soundPosition(EnemySoundChannel channel) const;
  std::vector<EnemyEffectEvent> drainEffects();
  void setFeedbackCamera(const CollisionVec &eye);
  const EnemyPreview &actor() const { return actor_; }
  Matrix34 placement() const;
  CollisionVec feet() const {
    if(r100Cliff_)return cliffPosition_;
    auto position = probe_.position();
    position[1] += airHeight_;
    return position;
  }
  float heading() const { return heading_; }
  float scale() const { return scale_; }
  unsigned hp() const { return hp_; }
  // em10_R1_Die_Cramp phase0 calls EmSetDieCnt after the fall completes.
  unsigned sourceKillCount() const { return sourceKillCount_; }
  unsigned maxHp() const { return maxHp_; }
  bool alive() const { return hp_ != 0; }
  bool deadSettled() const { return deadSettled_; }
  // r100_Sce_zombi_dead waits for EM_STATUS_ACTIVE. Standing Die_Normal
  // clears it after 0x49 ticks, or Die_Cramp clears it after an ended fall.
  bool scenarioDeathReady() const { return firstHouse_ || r100Ambush_ || villageGanado_ ? sourceDeathInactive_ : deadSettled_; }
  unsigned shots() const { return shots_; }
  unsigned hits() const { return hits_; }
  unsigned knifeHits() const { return knifeHits_; }
  unsigned kickHits() const { return kickHits_; }
  DamageSource lastDamageSource() const { return lastDamageSource_; }
  unsigned lastDamage() const { return lastDamage_; }
  bool lastHead() const { return lastHead_; }
  bool lastCritical() const { return lastCritical_; }
  std::uint16_t randomState() const { return randomState_; }
  std::string status() const;
  unsigned grabs() const { return grabs_; }
  unsigned escapes() const { return escapes_; }
  unsigned resistance() const { return resistance_; }
  bool firstHouseEncounter() const { return firstHouse_; }
  bool r100AmbushEncounter() const { return r100Ambush_; }
  bool foundPlayer() const { return foundPlayer_; }
  void setRoutes(std::shared_ptr<const EnemyRoutes> routes) { routes_=std::move(routes); }
  void setAwarenessInputs(bool playerFootstep,bool playerFired,bool somebodyDamaged=false,
      std::optional<CollisionVec> burstPosition={}) {
    playerFootstep_=playerFootstep;playerFired_=playerFired;somebodyDamaged_=somebodyDamaged;
    burstPosition_=burstPosition;
  }
  void setPeerAwareness(bool somebodyDamaged,bool otherFinding) {
    somebodyDamaged_=somebodyDamaged;otherFinding_=otherFinding;
  }
  bool r100HouseScare() const { return r100HouseScare_; }
  // Explicit transient EmSetFromList2(0x25,1) source routine5. The existing
  // diagnostic spawn keeps its older behavior until this is called.
  void beginR100HouseScare();
  bool r100CliffEncounter() const { return r100Cliff_; }
  // EmSetDie runs in phase2, after the initial scripted HP=0 wait.
  bool cliffFalling() const { return r100Cliff_ && cliffPhase_>=2; }
  void installR100CliffMotion(const std::vector<std::uint8_t> &motion);
  void releaseR100CliffActor();
  unsigned listIndex() const { return listIndex_; }
  bool scriptSuspended() const { return scriptSuspended_; }
  bool visible() const { return visible_; }
  bool sourceCollisionActive() const { return !keeperInvisible_ && !roofDamage_ && !blowFalling_; }
  GanadoPeerState sourcePeerState() const;
  void setVisible(bool value) { visible_=value; }
  void setScriptSuspended(bool value) { scriptSuspended_=value; }
  void releaseR100Ambush();
  void releaseVillageAlarm();
  void resetVillageSpawn(const EnemySpawn &source);
  bool villageGanado() const {return villageGanado_;}
  void releaseR100HouseAmbush();
  unsigned axeAttacks() const { return axeAttacks_; }
  unsigned axeHits() const { return axeHits_; }
  void setDifficulty(unsigned gameLevel, bool hardMode = false);

private:
  friend class EnemyGroup;
  const SatCollision &sat_;
  const SatCollision &eat_;
  WalkProbe probe_;
  EnemyPreview actor_;
  std::vector<AuthoredHitShape> shapes_;
  float heading_{}, scale_{};
  unsigned hp_{}, maxHp_{}, shots_{}, hits_{}, lastSerial_{}, lastDamage_{};
  bool hasSerial_{}, lastHead_{}, lastCritical_{}, deadSettled_{};
  unsigned sceneryShotSerial_{};
  std::vector<std::optional<SatHit>> shotScenery_;
  std::optional<AuthoredHitResult> pendingHit_;
  DamageSource pendingSource_{DamageSource::None};
  DamageSource lastDamageSource_{DamageSource::None};
  CollisionVec pendingFrom_{};
  unsigned knifeHits_{}, kickHits_{};
  bool melee_{}, kickPending_{}, headPromptTick_{}, downFacing_{true};
  bool firstHouse_{}, axeAttackHit_{}, hardMode_{}, sourceDeathInactive_{};
  bool villageGanado_{};
  bool r100Ambush_{}, scriptSuspended_{}, foundPlayer_{}, visible_=true;
  bool r100Cliff_{};
  bool r100HouseScare_{};
  unsigned houseScarePhase_=1,houseScareTimer_{};
  unsigned cliffPhase_=1,cliffTimer_{};
  CollisionVec cliffPosition_{};
  bool groupAttackBlocked_{};
  unsigned listIndex_{}, initialSet_{}, scriptWalkPhase_{};
  CollisionVec guardPosition_{};
  double guardRadius_{};
  std::shared_ptr<const EnemyRoutes> routes_;
  bool playerFootstep_{},playerFired_{},somebodyDamaged_{};
  bool otherFinding_{};
  std::optional<CollisionVec> burstPosition_;
  unsigned sightCount_{};
  unsigned sourceWalkType_{};
  unsigned dashWait_{},dashCycles_{},nearPlayerCount_{},sourceCharacter_{},blockedTicks_{};
  bool locomotionCycle_{};
  bool roofWaiting_{};
  unsigned roofTurnTimer_{};
  bool beginRoofDamage();
  bool tickRoofDamage();
  bool roofDamage_{};
  unsigned roofPhase_{},roofAirTicks_{};
  float roofTurnRemaining_{},roofVelocityY_{};
  float discoveryTurnTarget_{};
  bool discoveryVoiceDone_{};
  bool keeperLeft_{};
  bool keeper_{};
  bool keeperInvisible_{};
  bool sourceSight(const CollisionVec &player);
  bool detectPlayer(const CollisionVec &player,float playerHeading);
  bool tickDiscovery(const CollisionVec &player);
  unsigned axeAttacks_{}, axeHits_{}, axeTurnTimer_{}, gameLevel_=5;
  unsigned sourceDeathTimer_{};
  unsigned sourceKillCount_{};
  void countSourceDeath();
  // Original root Y is retained above the SAT floor. This selected flat-ground
  // adapter keeps WalkProbe's edge stop during walking. Dm_Roof owns
  // airborne root motion separately until the original landing gate.
  double airHeight_{};
  bool blowClamp_{}, blowLanded_{}, downState_{}, reactionGuard_{};
  bool blowFalling_{};
  unsigned blowFallPhase_{},blowAirTicks_{};
  CollisionVec blowFallSpeed_{};
  bool tickBlowFall();
  unsigned reactionTimer_{};
  unsigned damageTimer_{};
  unsigned blastSerial_{};
  bool hasBlastSerial_{};
  unsigned playerBlastSerial_{};
  unsigned flashCycles_{};
  bool tickFlash();
  std::uint16_t randomState_ = 0x4e19;
  unsigned grabs_{}, escapes_{}, resistance_{}, grabStartLife_{}, wakeTimer_{};
  unsigned attackWait_{};
  unsigned releaseTicks_{};
  bool wakeInitialized_{};
  std::optional<bool> releasePending_;
  bool chainsawCatch_{};
  unsigned chainsawPhase_{},chainsawTimer_{},chainsawTimer2_{};
  std::uint32_t flags_{};
  float originalScale_{}, catchTurn_{};
  CollisionVec catchAdjustment_{};
  ObjectActions *objects_{};
  std::vector<EnemySoundEvent> soundEvents_;
  std::vector<EnemyEffectEvent> effectEvents_;
  std::optional<CollisionVec> feedbackCamera_;
  std::uint64_t effectSerial_{};
  unsigned pendingWeaponNo_ = 1, pendingPowerLevel_{}, bloodBigEffectTimer_{};
  unsigned voiceDelay_{}, delayedVoiceCue_{}, breathTimer_ = 120;
  std::uint16_t feedbackRandom_ = 0x4e27;
  std::uint8_t previousSoundFree_{};
  void startMotion(EnemyMotion motion, bool mirrored = false,
                   double rootScale = 0);
  void soundTick();
  void sequenceFootstep();
  void emitSound(unsigned number, EnemySoundChannel channel,
                 bool replace = false, bool stop = false,
                 unsigned block = 8);
  void emitDamageFeedback();
  std::optional<AuthoredHitResult>
  targetWithoutObjects(const CollisionVec &from, const CollisionVec &to,
                        unsigned weaponNo = 1) const;
  void tickInside(const CollisionVec &player, const PlayerShot *shot);
  void applyPendingDamage();
  void queueShot(const PlayerShot *shot,bool skipObjects = false);
  void tickCombat(LeonPreview &player, WalkProbe &probe);
  void tickR100Cliff(const CollisionVec &player);
  void tickR100HouseScare();
  bool canCatch(const LeonPreview &player) const;
  bool axeAttackAvailable(const LeonPreview &player) const;
  void tickAxeAttack(LeonPreview &player);
  void tickChainsawCatch(LeonPreview &player);
  void beginChainsawCatch(LeonPreview &player);
  bool weaponAttackAvailable(const LeonPreview &player) const;
  void tickHoeAttack(LeonPreview &player);
  bool receiveWeaponHit(LeonPreview &player, const CollisionVec &point,
                        const CollisionVec &base, unsigned damage=380);
  unsigned voiceCue(unsigned slot) const { return GanadoVoiceCue(slot,actor_.type()); }
  void advanceEnemy(bool coupled = false);
};
} // namespace re4
