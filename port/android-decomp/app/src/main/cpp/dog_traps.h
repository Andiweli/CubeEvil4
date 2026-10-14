#pragma once
#include "enemy_feedback.h"
#include "enemy_spawn.h"
#include "generic_event_actor.h"
#include "object_actions.h"
#include "room_state.h"
#include <set>

namespace re4 {
// Literal source proximity predicates. Coordinates use the trap's own frame.
bool OriginalBearTrapContact(const CollisionVec &trap,const CollisionVec &player,
                             bool alive,bool damaged);
bool OriginalTripwireContact(const CollisionVec &localPlayer,float wireLength);
struct DogTrapHit { std::size_t actor{}; AuthoredHitResult hit; CollisionVec source{}; unsigned targetResult{2}; };
struct DogTrapSound : EnemySoundEvent { unsigned enemyId{}; };
struct DogTrapEffect : EnemyEffectEvent { unsigned actor{}; bool player{},detached{}; };
class DogTraps {
public:
  enum class DogState { Trapped, Releasing, Running, Jumping, Leaving, Fading, Gone };
  DogTraps(const SatCollision &sat,const EnemySpawns &spawns,
           const std::string &em21Drs,const std::string &em2aDrs,
           ScenarioState &scenario,std::set<unsigned> *consumed=nullptr, std::uint16_t room=0x100);
  ~DogTraps();
  void installPlayerMotions(LeonPreview &player) const;
  // r100_trap_set is called only after the truck event, or on room re-entry
  // when RSF10 is already set. No generic "alive" ESL spawn substitute.
  void releaseAfterTruck();
  bool released() const { return released_; }
  const std::vector<std::unique_ptr<GenericEventActor>> &actors() const { return actors_; }
  const std::vector<EnemySpawn> &sources() const { return sources_; }
  DogState dogState() const { return dogState_; }
  bool rescued() const { return scenario_.scenario.test(12); }
  bool rescueAvailable(const LeonPreview &player) const;
  bool beginRescue(LeonPreview &player,WalkProbe &probe);
  // One owning EmMgr tick, before the one Leon tick. Enemies passed here
  // must be alive ids10..20; their source positions can also trigger wires.
  void tick(LeonPreview &player,WalkProbe &probe,
            const std::vector<CollisionVec> &livingGanados={});
  std::optional<DogTrapHit> target(const CollisionVec &from,const CollisionVec &to,
                                  unsigned weaponNo=1,float maxLenSq=1e16f) const;
  bool applyShot(const DogTrapHit &hit,unsigned serial,unsigned weaponNo);
  bool shoot(const PlayerShot &shot,float maxLenSq=1e16f);
  bool knife(const PlayerMeleeEvent &event);
  // Explosion/fire damage frees a trapped dog, without awarding SCF12.
  void applyBlast(const ObjectBlast &blast);
  std::vector<DogTrapSound> drainSounds();
  std::vector<DogTrapEffect> drainEffects();
  std::vector<Matrix34> effectParents(const DogTrapEffect &event,
                                     const LeonPreview &player) const;
  std::vector<ObjectBlast> drainBlasts();
  // The dog rescue jingle is SndStrReq(block1,cue0x0e), not a sample SE.
  bool takeRescueStreamRequest();
  std::string describe() const;
private:
  struct Trap;
  ScenarioState &scenario_;
  std::set<unsigned> *consumed_{};
  std::vector<EnemySpawn> sources_;
  std::vector<std::unique_ptr<GenericEventActor>> actors_;
  std::vector<std::unique_ptr<Trap>> traps_;
  std::vector<AnimationBytes> dogClips_,bearClips_,playerClips_;
  DogState dogState_{DogState::Trapped};
  SmdPlacement dogPlacement_;
  std::unique_ptr<WalkProbe> dogProbe_;
  unsigned dogPhase_{},dogTimer_{},dogMotion_{},dogMotionAge_{};
  float dogTilt_{},dogAlpha_=1;
  bool hasDog_{true};
  bool released_{},hurtEscape_{},rescueStream_{};
  std::uint16_t randomState_{0x2100};
  std::uint64_t serial_{},ticks_{};
  std::vector<DogTrapSound> sounds_;
  std::vector<DogTrapEffect> effects_;
  std::vector<ObjectBlast> blasts_;
  unsigned random();
  void dogTick();
  void startDog(unsigned clip,unsigned blend,unsigned flags);
  void moveDog(const RootMotionStep &step,bool collision);
  void escapeDog(bool hurt);
  void sound(unsigned actor,unsigned block,unsigned cue,bool replace=false);
  void effect(unsigned actor,unsigned sequence);
  void consume(unsigned listIndex);
  bool hitActor(std::size_t actor,unsigned weaponNo);
};
} // namespace re4
