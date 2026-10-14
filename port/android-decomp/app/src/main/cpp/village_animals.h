#pragma once
#include "enemy_spawn.h"
#include "generic_event_actor.h"
#include "hit_geometry.h"
#include "original_enemy_drop.h"
#include <memory>
namespace re4 {
enum class VillageAnimalRoutine { Wait, Moo, Peck, Walk, Dash, Jump, Hurt, Attack, Die, DieAir };
struct VillageAnimalHit {std::size_t actor{};AuthoredHitResult hit;};
struct VillageAnimalSound {unsigned listIndex{},number{},enemyId{};CollisionVec position{};};
struct VillageAnimalDrop {unsigned listIndex{},id{},count{};CollisionVec position{};int effect{-1};};
class VillageAnimal {
public:
  const EnemySpawn &source() const {return source_;}
  const GenericEventActor &model() const {return *model_;}
  CollisionVec feet() const {return {position_[0],position_[1],position_[2]};}
  VillageAnimalRoutine routine() const {return routine_;}
  unsigned hp() const {return hp_;}
  bool alive() const {return hp_!=0;}
  bool visible() const {return !savedDead_;}
  bool airborne() const {return airborne_;}
  float deathShade() const {return deathShade_;}
private:
  friend class VillageAnimals;
  struct Damage {unsigned serial{},weapon{},power{};AuthoredHitResult hit;CollisionVec from{};};
  EnemySpawn source_;
  std::unique_ptr<GenericEventActor> model_;
  std::array<float,3> position_{},speed_{};
  float heading_{},targetHeading_{};
  float deathShade_{1};
  unsigned hp_{},phase_{},loops_{},timer_{},turnTimer_{},escapeWait_{},stuck_{},damageWait_{},damageTotal_{};
  unsigned lastShot_{},lastMelee_{},breathTimer_{};
  VillageAnimalRoutine routine_{VillageAnimalRoutine::Wait};
  bool savedDead_{},airborne_{},eggLaid_{},attackHit_{},dropEmitted_{},burnFade_{},mirror_{};
  std::optional<Damage> pendingDamage_;
  std::optional<unsigned> pendingBlast_;
  std::optional<AuthoredEnemyDrop> authoredDrop_;
};
// Source EM26/EM28 routines for the three ordinary r101 ESL records.
// Original archive indices, posed hit capsules, random choices and 30-Hz
// motion remain independent of rendering and Ganado wave accounting.
class VillageAnimals {
public:
  VillageAnimals(const SatCollision &sat,const EnemySpawns &spawns,const std::string &emDir, std::uint16_t room=0x101);
  const std::vector<std::unique_ptr<VillageAnimal>> &animals() const {return animals_;}
  void tick(const CollisionVec &player,bool gunFired,bool alarm,bool bell,LeonPreview *leon=nullptr);
  std::optional<VillageAnimalHit> target(const CollisionVec &from,const CollisionVec &to,
      unsigned weapon=2,float maxLenSq=1e16f) const;
  void applyShot(const VillageAnimalHit &hit,unsigned serial,unsigned weapon,unsigned power,const CollisionVec &from);
  void queueExplosion(const CollisionVec &center,float radius,unsigned weapon);
  void setAuthoredDrops(const RoomArchive &room,ScenarioState &scenario);
  void setDropContext(const EnemyDropContext &context) {dropContext_=context;}
  void setDropState(EnemyDropState *state) {dropState_=state;}
  std::vector<VillageAnimalSound> takeSounds();
  std::vector<VillageAnimalDrop> takeDrops();
  std::vector<unsigned> deadIndices() const;
  void restoreDead(const std::vector<unsigned> &indices);
private:
  const SatCollision &sat_;
  std::vector<std::unique_ptr<VillageAnimal>> animals_;
  std::vector<VillageAnimalSound> sounds_;
  std::vector<VillageAnimalDrop> drops_;
  EnemyDropContext dropContext_;
  EnemyDropState localDropState_;
  EnemyDropState *dropState_{};
  ScenarioState *scenario_{};
  std::uint16_t randomState_{0x2628};
  bool previousBell_{};
  void start(VillageAnimal &a,unsigned clip,unsigned blend=0,unsigned flags=1);
  bool motion(VillageAnimal &a,bool ground=true);
  void place(VillageAnimal &a);
  bool air(VillageAnimal &a);
  void sound(const VillageAnimal &a,unsigned number);
  void damage(VillageAnimal &a);
  void die(VillageAnimal &a);
  void drop(VillageAnimal &a);
  void tickCow(VillageAnimal &a,const CollisionVec &player,LeonPreview *leon);
  void tickChicken(VillageAnimal &a,const CollisionVec &player,bool gunFired,bool alarm,bool burst);
};
}
