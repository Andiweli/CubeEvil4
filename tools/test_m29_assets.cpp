#include "archive.h"
#include "leon_preview.h"
#include "object_actions.h"
#include "room_lighting.h"
#include "room_pickups.h"
#include "ports/rnd.h"
#include "grab_rules.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &room, const char *tag) {
  const auto *e = room.find(tag); check(e, "Original archive entry exists");
  return room.copy(e - room.entries().data());
}
void jumps(const char *path, const char *player, const char *weapon, unsigned expected) {
  const auto room = re4::RoomArchive::Load(path);
  const re4::Sat sat(entry(room, "SAT")); const re4::SatCollision world(sat);
  re4::WalkProbe probe(world); re4::LeonPreview leon(player, weapon);
  leon.setUnlimitedHealth(true);
  unsigned offered = 0, complete = 0; std::set<std::size_t> edges;
  for (std::size_t i = 0; i < sat.polygons.size(); ++i) {
    const auto &p = sat.polygons[i]; if (!(p.attributes & 0x100010)) continue;
    const auto normal = sat.normals[p.normal]; if (std::abs(normal[1]) > .5) continue;
    const auto triangle = world.triangle(i);
    for (double distance : {400., 600., 800.}) {
      re4::CollisionVec point{};
      for (const auto &corner : triangle) for (unsigned k = 0; k < 3; ++k) point[k] += corner[k] / 3;
      for (unsigned k = 0; k < 3; ++k) point[k] += normal[k] * distance;
      const auto floor = world.ground(point, 1000, 1500); if (!floor) continue;
      point[1] = floor->point[1]; if (!probe.place(point)) continue;
      leon.setPlacement(probe.position(), float(std::atan2(-normal[0], -normal[2])));
      const auto offer = leon.jumpOffer(probe); if (!offer) continue;
      ++offered; edges.insert(offer->wall.polygon); leon.beginJump(probe, *offer);
      unsigned ticks = 0;
      while (leon.jumpActive() && ticks++ < 260) {
        leon.step(probe, 1, 1, true, true, 1. / 30);
        check(leon.life() == 1200 && !leon.shots(), "Jump retains HP and ignores movement/fire overrides");
        if (ticks == 18) {
          const auto feet = leon.feet(); const auto bones = leon.rig().bones();
          const auto tick = leon.jumpState().ticks; leon.suspendClock();
          leon.step(probe, 1, 1, true, true, 0);
          check(leon.feet() == feet && leon.rig().bones() == bones && leon.jumpState().ticks == tick,
                "Paused jump keeps exact original pose and trajectory");
        }
      }
      if (!leon.jumpActive() && !leon.jumpState().failed && probe.position()[1] < point[1] - 500) ++complete;
      check(probe.ready() && world.ground(probe.position(), 1, 1).has_value(), "Landings use genuine SAT floors");
    }
  }
  check(offered && offered == complete && (!expected || offered == expected), "Every validated roof/tower offer completes");
  std::cout << path << ": jump approaches " << offered << '/' << complete << " · SAT edges " << edges.size() << '\n';
  leon.setPlacement(probe.position(), 0);
  const auto initial = leon.life(); RndInit(0x1974);
  (void)re4::GrabPlayerLifeDown(initial, 1200, true); const auto rng = RndState();
  RndInit(0x1974); leon.takeDamage(1200, true);
  check(leon.life() == 1200 && RndState() == rng, "Godmode preserves the source survival RNG call");
  leon.setFatalLife(); check(leon.alive() && leon.life() == 1200, "Fatal HP path remains protected");
  check(leon.applyBlast({true, 1200}, {leon.feet()[0], leon.feet()[1] + 500, leon.feet()[2]}, probe) &&
        leon.blastActive() && leon.life() == 1200, "Godmode still plays the genuine blast reaction");
  re4::LeonPreview replacement(player, weapon); replacement.inheritInventory(leon);
  check(replacement.unlimitedHealth() && replacement.life() == 1200, "Player replacement inherits unlimited health");
  re4::WalkProbe smoothProbe(world), originalProbe(world);
  check(smoothProbe.place(probe.position()) && originalProbe.place(probe.position()), "Interpolation fixture has SAT clearance");
  re4::LeonPreview smooth(player, weapon), original(player, weapon);
  smooth.setPlacement(smoothProbe.position(), 3.12f); original.setPlacement(originalProbe.position(), 3.12f);
  smooth.setRenderInterpolation(true);
  smooth.step(smoothProbe, 0, 0, false, false, 1. / 30);
  original.step(originalProbe, 0, 0, false, false, 1. / 30);
  unsigned visibleChanges = 0, physicsChanges = 0;
  auto oldVisible = smooth.renderHeading(), oldPhysics = smooth.heading();
  for (unsigned frame = 0; frame < 240; ++frame) {
    smooth.step(smoothProbe, .75, 0, false, false, 1. / 120);
    original.step(originalProbe, .75, 0, false, false, 1. / 120);
    check(smooth.heading() == original.heading() && smooth.feet() == original.feet() &&
          smooth.rig().bones() == original.rig().bones(), "Rendering interpolation does not change original 30Hz simulation");
    const auto visible = smooth.renderHeading();
    check(std::abs(std::remainder(double(visible) - oldVisible, 6.283185307179586)) < .07,
          "Heading interpolation follows the short arc across +/-pi");
    visibleChanges += std::abs(visible - oldVisible) > 1e-7;
    physicsChanges += std::abs(smooth.heading() - oldPhysics) > 1e-7;
    oldVisible = visible; oldPhysics = smooth.heading();
  }
  check(visibleChanges > physicsChanges * 3, "High-refresh rendering fills fixed-tick rotation gaps");
  smooth.suspendClock(); check(smooth.renderHeading() == smooth.heading(), "Pause discards outdated interpolation history");
}
void villageItems(const char *path) {
  const auto room = re4::RoomArchive::Load(path);
  const auto light = re4::ReadRoomLighting(room);
  check(light->cuts.size() == 20 && light->cuts[0].lights.size() == 15, "Original r101 LIT counts");
  const auto &fog = light->cuts[0].fog;
  check(fog.type == 2 && std::abs(fog.start + 7074.842285f) < .01 &&
        std::abs(fog.end - 175030.1875f) < .01 && fog.color == std::array<unsigned char, 4>{113,108,90,128},
        "r101 fog is taken verbatim from the imported USA disc");
  re4::ScenarioState state; re4::RoomPickups pickups(0x101, room, state);
  check(pickups.items().size() == 23, "Only the 23 waiting-type0 ITA records are activated");
  const auto mesh = re4::BuildRoomItems(room, pickups.renderPlacements());
  check(mesh.rendered && mesh.mesh.triangles, "Real ITM assets produce pickup geometry");
  unsigned shotgun = 0, ammo = 0, money = 0;
  for (std::size_t n = 0; n < pickups.items().size(); ++n) {
    const auto &item = pickups.items()[n];
    if (item.id == 0x2c) { check(item.count == 1 && (item.controlFlags & 2), "Shotgun keeps authored quad and one weapon count"); ++shotgun; }
    if (item.id == 4) ammo += item.count;
    if (item.id == 0x78 && item.trigger.recordIndex == 7) {
      const auto result = pickups.take(n);
      check(result && result->pesetas == 1000 && state.pesetas == 1000, "Authored 100 money units give 1000 pesetas"); ++money;
    } else check(pickups.take(n).has_value(), "Authored item transfers exactly once");
    check(!pickups.visible(int(n)) && !pickups.take(n), "Taken ITA item cannot render or transfer twice");
  }
  check(shotgun == 1 && money == 1 && ammo == 50 && state.carriedItems.at(0x2c) == 1,
        "Original shotgun and handgun quantities retained");
  re4::RoomPickups reopened(0x101, room, state);
  for (std::size_t n = 0; n < reopened.items().size(); ++n)
    check(!reopened.visible(int(n)) && !reopened.take(n), "Native item flags survive room reconstruction");
  state.room(0x101).setObject(0, 1);
  check(state.room(0x101).items.test(24), "ETC model words and ITA taken flags are independent");
  std::cout << "r101 " << pickups.describe() << " · triangles " << mesh.mesh.triangles << '\n';
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 5) throw std::runtime_error("Usage: m29_assets_check R101.dat R400.dat PL00.drs WEP01.drs");
    villageItems(argv[1]); jumps(argv[1], argv[3], argv[4], 58); jumps(argv[2], argv[3], argv[4], 0);
    std::cout << "M29 original-asset checks PASS: " << checks << '\n';
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
