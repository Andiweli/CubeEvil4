#pragma once
#include "generic_event_actor.h"
#include "world_effects.h"
#include "room_flow.h"
namespace re4 {
class VillageBottles {
public:
 VillageBottles(const RoomArchive &room,const std::vector<std::uint8_t> &core);
 void throwAt(const DoorArea &area,const SatCollision &floor);
 void tick(LeonPreview &player,const SatCollision &eat,const WorldEffectCamera &camera);
 const GenericEventActor &model() const {return *model_;}
 std::vector<Matrix34> placements() const;
 const WorldEffectsData &coreData() const {return core_;}
 const WorldEffectsData &trailData() const {return trail_;}
 const WorldEffectsState &coreState() const {return coreSimulation_->state();}
 const WorldEffectsState &trailState() const {return trailSimulation_->state();}
 bool takeExplosion(){const bool result=explosion_;explosion_=false;return result;}
 unsigned throws() const {return throws_;}
 unsigned explosions() const {return explosions_;}
private:
 struct Bottle {CollisionVec pos{},speed{};float angle{},spin{};unsigned timer=210,flame{};std::uint64_t serial{};};
 std::unique_ptr<GenericEventActor> model_;
 WorldEffectsData core_,trail_;
 std::unique_ptr<WorldEffectsSimulation> coreSimulation_,trailSimulation_;
 std::vector<Bottle> bottles_;
 std::uint16_t random_=0x4e22;
 std::uint64_t serial_{};
 unsigned throws_{},explosions_{};
 bool explosion_{};
 void explode(Bottle &b,const SatCollision &floor);
};
}
