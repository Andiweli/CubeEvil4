#pragma once
#include "generic_event_actor.h"
#include "object_actions.h"
#include "world_effects.h"
#include <functional>
namespace re4 {
// Owned 30-Hz subweapons. Launching never submits a hitscan ray: only a
// finite flight segment or a due radial blast can damage the world.
class PlayerProjectiles {
public:
  using Target=std::function<std::optional<AuthoredHitResult>(const CollisionVec&,const CollisionVec&,unsigned)>;
  struct Cue {unsigned block{},number{};CollisionVec position{};};
  struct Draw {const GenericEventActor *model{};Matrix34 placement{};const AnimationRig *rig{};};
  PlayerProjectiles(const std::string &playerDrs,const std::vector<std::uint8_t> &core);
  ~PlayerProjectiles();
  void launch(const PlayerShot &shot,const LeonPreview &player,const SatCollision &eat);
  void tick(const SatCollision &eat,const WorldEffectCamera &camera,const Target &target,
            const std::function<void(const PlayerShot&)> &impact);
  void clear(); // cSubWep/cObjRocket/cEmMine::beginEvent and room teardown.
  std::vector<ObjectBlast> drainBlasts();
  std::vector<Cue> drainCues();
  std::vector<Draw> draws() const;
  const std::map<unsigned,std::unique_ptr<GenericEventActor>> &models() const {return models_;}
  unsigned modelGeneration() const {return unsigned(models_.size());}
  unsigned active() const {return unsigned(flights_.size());}
  const WorldEffectsData &effectsData() const {return effects_;}
  const WorldEffectsState &effectsState() const {return simulation_->state();}
private:
  struct Flight;
  RoomArchive player_;
  std::map<unsigned,std::unique_ptr<GenericEventActor>> models_;
  std::unique_ptr<SkinnedModel> rocketBody_;
  AnimationBytes rocketMotion_;
  std::vector<std::unique_ptr<Flight>> flights_;
  WorldEffectsData effects_;
  std::unique_ptr<WorldEffectsSimulation> simulation_;
  std::vector<ObjectBlast> blasts_;
  std::vector<Cue> cues_;
  unsigned blastSerial_{};
  std::uint64_t effectSerial_{};
  std::uint16_t random_=0x4e22;
  GenericEventActor &model(unsigned number);
  void burst(Flight &flight,const SatCollision &eat);
};
} // namespace re4
