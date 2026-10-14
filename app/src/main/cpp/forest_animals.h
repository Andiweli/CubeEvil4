#pragma once
#include "enemy_spawn.h"
#include "hit_geometry.h"
#include "leon_preview.h"
#include "original_enemy_drop.h"
#include <memory>
namespace re4 {
enum class CrowRoutine { Wait, Takeoff, Turn, DamageAir, DieNormal };
struct ForestAnimalSound {
  unsigned listIndex{}, block{8}, number{}, argument{0x23};
  CollisionVec position{};
  bool sharedCooldown{};
};
struct ForestAnimalDrop {
  unsigned listIndex{}, id{}, count{};
  CollisionVec position{};
  int effect{-1}; // Authored ITA eff_type overrides automatic item colour.
};
std::vector<EnemySpawn> ForestCrowSpawns(const EnemySpawns &spawns,
                                      std::uint16_t room=0x100);
class ForestCrow {
public:
  ForestCrow(ForestCrow &&) noexcept;
  ForestCrow &operator=(ForestCrow &&) noexcept;
  ~ForestCrow();
  ForestCrow(const ForestCrow &)=delete;
  const EnemySpawn &source() const {return source_;}
  const AnimationRig &rig() const {return *rig_;}
  CollisionVec feet() const;
  float heading() const {return heading_;}
  float scale() const {return scale_;}
  Matrix34 placement() const;
  unsigned wing() const {return wing_;}
  const void *gpuOwner(unsigned variant) const;
  CrowRoutine routine() const {return routine_;}
  unsigned hp() const {return hp_;}
  bool alive() const {return hp_>0;}
  bool visible() const {return !savedDead_;}
  unsigned routineStep() const {return routineStep_;}
  unsigned blendVariant() const {return blendVariant_;}
  unsigned motionClip() const {return clip_;}
  float flightHeight() const {return flyHeight_;}
private:
  friend class ForestAnimals;
  ForestCrow(const SatCollision &sat,const EnemySpawn &source,
      const SkinnedModel &body,const std::vector<AnimationBytes> &clips);
  void start(unsigned clip,unsigned flags=5,unsigned hokan=5);
  bool moveMotion(bool root);
  bool addSpeedAir(float heading);
  void tick(const CollisionVec &player,bool gunFired,
      std::vector<ForestAnimalSound> &sounds,
      std::vector<ForestAnimalDrop> &drops,const EnemyDropContext &context,
      EnemyDropState &dropState);
  void damage(unsigned weaponNo,float distanceSq);
  EnemySpawn source_;
  const SatCollision *sat_{};
  std::unique_ptr<AnimationRig> rig_;
  std::array<std::uint8_t,2> gpuOwners_{};
  std::array<float,3> position_{},speed_{};
  float heading_{},scale_{1},flyHeight_{},targetHeading_{},fallHeading_{},fallSpin_{},lastHeight_{};
  CrowRoutine routine_{CrowRoutine::Wait};
  unsigned routineStep_{},wing_{1},hp_{},clip_{},loops_{},turnTimer_{},turnDir_{},blendVariant_{},stateTimer_{};
  bool savedDead_{},dropEmitted_{},damageSound_{};
  std::optional<std::pair<unsigned,float>> pendingDamage_;
  bool pendingBlast_{};
  std::optional<AuthoredEnemyDrop> authoredDrop_;
  ScenarioState *scenario_{};
};
// The r100 EM23 set=0 crow paths: original Wait/Takeoff/Turn/air damage/
// death, original body and both interchangeable wings. r20a corpse paths
// cannot be selected by these ESL records and are deliberately not admitted.
class ForestAnimals {
public:
  ForestAnimals(const SatCollision &sat,const std::vector<std::uint8_t> &esl,
      const std::string &em23Drs,std::uint16_t room=0x100);
  const std::vector<ForestCrow> &animals() const {return animals_;}
  const StaticScene &mesh(unsigned wing) const;
  const std::vector<SkinCorner> &skin(unsigned wing) const;
  void step(double seconds,const CollisionVec &playerFeet,bool gunFired=false);
  void tick(const CollisionVec &playerFeet,bool gunFired=false);
  void suspendClock() {accumulator_=0;}
  std::uint64_t ticks() const {return ticks_;}
  void setDropContext(const EnemyDropContext &context) {dropContext_=context;}
  void setDropState(EnemyDropState *state) {sharedDropState_=state;}
  void setAuthoredDrops(const RoomArchive &room,ScenarioState &scenario);
  std::optional<AuthoredHitResult> target(const CollisionVec &from,
      const CollisionVec &to,unsigned weaponNo=2) const;
  // Submit a source player shot only once (serial is shared by all pellets).
  // The nearest crow per ray receives one pending hit; EAT clipping belongs
  // to the room owner before it submits a shot, as with EnemyGroup.
  void queueShot(const PlayerShot &shot);
  void queueMelee(const PlayerMeleeEvent &melee);
  void queueExplosion(const CollisionVec &center,float radius);
  std::vector<ForestAnimalSound> takeSounds();
  std::vector<ForestAnimalDrop> takeDrops();
  std::vector<unsigned> deadIndices() const;
  void restoreDead(const std::vector<unsigned> &indices);
  std::string describe() const;
private:
  std::array<StaticScene,2> meshes_;
  std::array<std::vector<SkinCorner>,2> skin_;
  std::vector<ForestCrow> animals_;
  std::vector<ForestAnimalSound> sounds_;
  std::vector<ForestAnimalDrop> drops_;
  EnemyDropContext dropContext_;
  EnemyDropState dropState_;
  EnemyDropState *sharedDropState_{};
  double accumulator_{};
  std::uint64_t ticks_{};
  std::uint16_t randomState_{0x2310};
  unsigned lastShotSerial_{},lastMeleeSerial_{};
  unsigned takeoffSoundWait_{};
  std::optional<std::pair<std::size_t,AuthoredHitResult>> nearest(
      const CollisionVec &from,const CollisionVec &to,unsigned weaponNo) const;
};
} // namespace re4
