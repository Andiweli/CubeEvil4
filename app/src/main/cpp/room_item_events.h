#pragma once
#include "room_flow.h"
#include "room_pickups.h"
#include "r100_secondary.h"
namespace re4 {
// Native SceSetItemEvent/SceExecItemEvent owner for the authored shelves on
// the actual village route: R103Init, R113Init, then R106Init. AEV/ITA areas,
// CAM curves and SMD hinges remain original; no GC callbacks are executed.
class RoomItemEvents {
public:
  struct Shelf {
    RoomDoor area;
    unsigned itemArea{},flag{},cut{},left{},right{},steps{};
    float leftAngle{},rightAngle{};
    SmdPlacement leftPose,rightPose;
    std::unique_ptr<R100RoomCamera> camera;
  };
  RoomItemEvents(std::uint16_t room,const RoomArchive &archive,const Smd &smd,
                 ScenarioState &state,RoomPickups &pickups);
  const std::vector<Shelf> &shelves() const {return shelves_;}
  std::optional<std::size_t> offer(const CollisionVec &feet,double heading,
                 const std::optional<CollisionVec> &front={}) const;
  bool begin(std::size_t index);
  void tick30Hz();
  bool active() const {return active_.has_value();}
  const EventCamera *camera() const;
  std::optional<unsigned> lightArea() const;
  std::optional<Matrix34> delta(unsigned work) const;
  bool takeOpenSound();
  std::uint64_t ticks() const {return ticks_;}
private:
  std::uint16_t room_;
  ScenarioState &state_;
  RoomPickups &pickups_;
  std::vector<Shelf> shelves_;
  std::optional<std::size_t> active_;
  unsigned wait_{};
  bool waiting_{},sound_{};
  std::uint64_t ticks_{};
  static void step(Shelf &s);
};
}
