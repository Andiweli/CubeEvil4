#pragma once
#include "room_pickups.h"
#include "room_objects.h"
#include "scenario_collision.h"
#include "world_effects.h"
namespace re4 {
// Source r103/r113 well, Et1a/emitem and stage.cpp::subMissionSt1.
// Only authored geometry and original flag tables determine outcomes.
class VillageProps {
public:
  struct Medal {
    unsigned slot{};
    SmdPlacement original,pose;
    std::vector<Matrix34> parts;
  };
  struct Hit {
    enum Kind {Cover,Medallion} kind;
    std::size_t index{};
    AuthoredHitResult hit;
  };
  struct Cue {unsigned number{};CollisionVec position{};};
  struct Effect {unsigned sequence{};CollisionVec position{};};
  VillageProps(unsigned room,const RoomArchive &archive,const Smd &smd,
      ScenarioState &state,RoomPickups &pickups,ScenarioCollision &collision,
      SatCollision &sight,SatCollision &effect);
  const std::vector<Medal> &medals() const {return medals_;}
  bool hasWell() const {return hasWell_;}
  std::size_t pendant() const {return pendant_;}
  float lidAngle() const {return lidAngle_;}
  bool openOffer(const CollisionVec &feet,double heading,
      const std::optional<CollisionVec> &front={}) const;
  bool openLid();
  void tick30Hz();
  void initializeEffects(const RoomArchive &archive,const std::vector<std::uint8_t> &core);
  void tickEffects(const WorldEffectCamera &camera);
  const WorldEffectsData &wellEffectsData() const {return wellEffectsData_;}
  const WorldEffectsData &medalEffectsData() const {return medalEffectsData_;}
  std::vector<WorldEffectQuad> wellQuads(const WorldEffectCamera &camera) const;
  std::vector<WorldEffectQuad> medalQuads(const WorldEffectCamera &camera) const;
  std::vector<WorldEffectLine> medalLines() const;
  std::optional<Hit> target(const CollisionVec &from,const CollisionVec &to,
      unsigned weapon,float maxLenSq=1e16f) const;
  bool applyHit(const Hit &hit,unsigned serial,unsigned weapon);
  bool roomObjectVisible(int slot) const;
  std::optional<Matrix34> roomObjectDelta(int slot) const;
  bool scrollVisible(unsigned work) const;
  std::optional<Matrix34> scrollDelta(unsigned work) const;
  std::vector<Cue> takeSounds();
  std::vector<Effect> takeEffects();
  static unsigned syncMedallionFlags(ScenarioState &state);
private:
  void syncWell();
  void sound(unsigned number,const std::array<float,3> &position);
  unsigned room_{};
  ScenarioState &state_;
  RoomPickups &pickups_;
  ScenarioCollision &collision_;
  SatCollision &sight_,&effect_;
  std::vector<Medal> medals_;
  bool hasWell_{},closing_{},opening_{};
  unsigned bounce_{},lastCoverShot_{};
  std::size_t pendant_{},donor_{};
  SmdPlacement cover_,lid_;
  RoomDoor openArea_;
  float lidAngle_{},speed_{};
  std::vector<Cue> sounds_;
  std::vector<Effect> effects_;
  WorldEffectsData wellEffectsData_,medalEffectsData_;
  std::unique_ptr<WorldEffectsSimulation> wellEffects_,medalEffects_;
  std::uint64_t effectSerial_{};
};
} // namespace re4
