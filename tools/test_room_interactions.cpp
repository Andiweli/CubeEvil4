// Real disc-asset validation; no game data or test switch is added to Android.
#include "room_interactions.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
}
re4::CollisionVec point(const re4::RoomObjectPlacement &p, float x, float y, float z) {
  re4::SmdPlacement pose;
  pose.position = p.position; pose.rotation = p.rotation; pose.scale = {1,1,1};
  const auto v = re4::TransformPoint(re4::PlacementMatrix(pose), {x,y,z});
  return {v[0],v[1],v[2]};
}
std::vector<std::uint8_t> asset(const re4::RoomArchive &a, const char *tag) {
  const auto *entry = a.find(tag);
  if (!entry) throw std::runtime_error("Original room subfile missing");
  return a.copy(entry - a.entries().data());
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 5) throw std::runtime_error("Usage: room_interactions_check R101 PL00 WEP01 CORE");
    const auto room = re4::RoomArchive::Load(argv[1]);
    const auto core = re4::RoomArchive::LoadDrs(argv[4]);
    const auto objects = re4::BuildRoomObjects(room, asset(core,"EFF"), 0x101);
    const re4::Sat sat(asset(room,"SAT")), eat(asset(room,"EAT"));
    const re4::SatCollision world(sat), effect(eat, re4::SatQueryMode::Effect);
    re4::RoomInteractions initial(0x101,room,objects.placements,argv[2],{},&effect);
    require(initial.doors().size() == 4 && initial.windows().size() == 5,
            "Original r101 contains four supported doors and five windows");
    require(initial.doors().back().placement.slot == 11 && initial.doors().back().lockHp >= 3,
            "R101Init side0 lock retains the source normal-strength hp");
    for (const auto &w : initial.windows()) {
      require(w.protectedByEvent == (w.placement.slot == 0 || w.placement.slot == 19),
              "Only original pre-house/pre-bell windows are protected");
      if (w.placement.id != 0x25) {
        bool intact = false, broken = false;
        for (const auto &draw : objects.mesh.draws) {
          if (draw.roomObjectSlot != int(w.placement.slot)) continue;
          intact |= draw.roomObjectVariant == 0;
          broken |= draw.roomObjectVariant == 1;
        }
        require(intact && broken, "Both real window BIN/TPL variants preload without placeholders");
      }
    }
    for (unsigned slot : {1u,2u,3u}) for (bool front : {true,false}) {
      re4::RoomInteractions interior(0x101,room,objects.placements,argv[2],{},&effect);
      re4::WalkProbe probe(world);
      probe.setObjectCollision(interior.collision());
      const auto &door = interior.doors().at(slot - 1);
      const auto start = point(door.placement,-650,0,front ? -650 : 650);
      const auto heading = door.placement.rotation[1] + (front ? 0 : 3.1415927f);
      std::cout << "Door " << slot << " " << (front ? "front" : "back") << " approach "
                << start[0] << "," << start[1] << "," << start[2] << '\n';
      require(probe.place(start), "Source door approach has real floor and a clear capsule");
      re4::LeonPreview player(argv[2],argv[3]);
      player.setPlacement(probe.position(),heading);
      const auto offer = interior.doorOffer(player.feet(),player.heading());
      require(offer && offer->slot == slot && !offer->kick && !offer->blocked,
              "Source facing/reach boundaries select the intended unlocked door");
      require(!interior.doorOffer(player.feet(),player.heading() + 1.5707963),
              "Sideways facing cannot activate a door");
      const auto from = point(door.placement,-650,1000,-650);
      const auto to = point(door.placement,-650,1000,650);
      require(bool(interior.collision()->line(from,to)), "Closed source leaf blocks crossing");
      require(interior.interact(player,probe) && player.action() == re4::PlayerAction::DoorOpen,
              "Door action starts original Leon PL_ARC1e");
      const auto ammo = player.magazine();
      for (unsigned i = 0; i < 65; ++i) {
        player.step(probe,0,0,false,false,1./30,{}, {}, [&]{interior.tick(player,probe);});
        bool finite = true;
        for (const auto &matrix : player.rig().bones())
          for (auto value : matrix) finite &= std::isfinite(value);
        require(finite, "Door actor pose is finite");
      }
      require(player.action() == re4::PlayerAction::Footwork && player.magazine() == ammo,
              "Authored door motion returns to footwork without consuming ammo");
      require(door.phase == re4::InteriorDoorPhase::Open &&
                std::abs(door.yaw - (front ? 1.5707f : -1.5707f)) < .001,
              "Door's own original FCV opens in the correct mirrored direction");
      require(!interior.collision()->line(from,to), "Open leaf really clears its original passage");
      const auto delta = interior.delta(int(slot));
      require(delta.has_value(), "Moving door supplies its model transform");
      const auto hinge = re4::TransformPoint(*delta,door.placement.position);
      for(unsigned k=0;k<3;++k)
        require(std::abs(hinge[k]-door.placement.position[k])<.01, "Door model rotates about authored hinge");
      player.setPlacement(point(door.placement,-650,0,-3000),heading);
      for(unsigned i=0;i<24;++i) interior.tick(player,probe);
      require(door.phase == re4::InteriorDoorPhase::Closed && door.yaw == 0,
              "Original proximity rule closes the unused door, including its model");
      require(bool(interior.collision()->line(from,to)), "Auto-closed leaf reactivates collision");
    }
    const auto &lock = initial.doors().back();
    const auto lockHit = initial.target(point(lock.placement,-1150,975,-1200),
                                         point(lock.placement,-1150,975,1200));
    require(lockHit && lockHit->kind == re4::InteriorHit::DoorLock,
            "Closest source cube resolves right-side lock before the iron leaf");
    require(initial.damage(*lockHit,1,1) && !lock.lockHp && initial.state().object(11,2),
            "One normal handgun lock hit runs original hp-zero and persistent flag rule");
    require(!initial.visible(-1,-1,11) && !initial.damage(*lockHit,1,1),
            "Broken lock hides and one shot cannot damage twice");
    unsigned serial = 2;
    for (std::size_t i=0;i<initial.windows().size();++i) {
      const auto &window = initial.windows()[i];
      const auto y = window.lowSat ? 0.f : 700.f;
      const auto ray = initial.target(point(window.placement,0,y,-100),
                                      point(window.placement,0,y,100));
      std::cout << "Window " << window.placement.slot << " target "
                << (ray ? int(ray->kind) : -1) << '\n';
      require(ray && ray->kind == re4::InteriorHit::Window && ray->index == i,
              "Original window hit rectangle resolves through genuine room EAT");
      const auto hp = window.hp;
      re4::RoomInteractions knifeTest(0x101,room,objects.placements,argv[2],{},&effect);
      const bool knifeDamage=knifeTest.damage(*ray,0x10,99);
      require(knifeDamage==!window.protectedByEvent &&
                  knifeTest.windows()[i].hp==(window.protectedByEvent?hp:0),
              "Original knife size group2 breaks every damage-enabled pane");
      require(!knifeTest.damage(*ray,0x10,99),
              "One knife attack cannot consume a pane twice");
      initial.damage(*ray,1,serial++);
      if (window.protectedByEvent) {
        require(window.hp == hp && !initial.state().object(window.placement.slot,1),
                "Protected window cannot skip its original room event by shooting");
      } else if(window.placement.id == 0) {
        require(window.hp == 500, "ET00 small-arms damage subtracts exactly 500 hp");
        require(!initial.damage(*ray,1,serial-1) && window.hp == 500,
                "Duplicate shot cannot consume the second half of window hp");
        initial.damage(*ray,1,serial++);
        require(window.hp == 0 && initial.state().object(window.placement.slot,1),
                "Second handgun shot breaks ET00 and commits its real etc bit");
        require(!initial.visible(int(window.placement.slot),0) &&
                  initial.visible(int(window.placement.slot),1),
                "Break switches exclusively to the authored replacement model");
      } else require(window.hp == 0, "ET25 is broken by its first group0 weapon hit");
    }
    const auto saved = initial.state();
    re4::RoomInteractions restored(0x101,room,objects.placements,argv[2],saved,&effect);
    require(!restored.doors().back().lockHp && restored.windows()[1].hp == 0,
            "A newly loaded r101 consumes saved lock/window flags");
    re4::RoomSaveState house; house.room.set(8);
    const auto later = re4::BuildRoomObjects(room,asset(core,"EFF"),0x101,&house);
    require(later.hiddenSlots.empty() && later.ladders == 3,
            "Ladder visibility reads the real house flag instead of a hard-coded profile");
    std::cout << "Original r101 interaction assets PASS: " << checks << " checks\n";
  } catch (const std::exception &e) {
    std::cerr << "Original r101 interaction FAIL: " << e.what() << '\n';
    return 1;
  }
}
