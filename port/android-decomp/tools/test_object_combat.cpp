#include "archive.h"
#include "enemy_encounter.h"
#include "object_actions.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
using namespace re4;
unsigned checks{};
void check(bool value) {
  if (!value)
    throw std::runtime_error("Object combat check " +
                             std::to_string(checks + 1));
  ++checks;
}
std::vector<std::uint8_t> read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() < 0 || file.tellg() > 32 * 1024 * 1024)
    throw std::runtime_error("Object combat fixture path / size");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
    throw std::runtime_error("Object combat fixture read");
  return bytes;
}
// This flat floor isolates hit ordering. Both target shapes and placements
// remain those decoded from the real r400 ETS / original EM12 actor.
Sat flatFloor(float floor) {
  std::vector<std::uint8_t> bytes(20 + 48 + 12 + 12 + 40);
  auto half = [&](std::size_t offset, unsigned value) {
    bytes.at(offset) = value >> 8;
    bytes.at(offset + 1) = value;
  };
  auto word = [&](std::size_t offset, unsigned value) {
    half(offset, value >> 16);
    half(offset + 2, value);
  };
  bytes[0] = 32;
  half(2, 4);
  half(4, 1);
  half(6, 1);
  half(10, 2);
  half(12, 2);
  std::size_t offset = 20;
  for (float value : {-100000.f, floor, -100000.f, 100000.f, floor, -100000.f,
                     100000.f, floor, 100000.f, -100000.f, floor, 100000.f,
                     0.f, 1.f, 0.f}) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    word(offset, bits);
    offset += 4;
  }
  offset += 12;
  half(offset + 2, 2);
  half(offset + 4, 1);
  half(offset + 20 + 2, 3);
  half(offset + 20 + 4, 2);
  return Sat(bytes);
}
CollisionVec transform(const Matrix34 &matrix, const CollisionVec &point) {
  CollisionVec result{};
  for (unsigned row = 0; row < 3; ++row) {
    result[row] = matrix[row * 4 + 3];
    for (unsigned axis = 0; axis < 3; ++axis)
      result[row] += matrix[row * 4 + axis] * point[axis];
  }
  return result;
}
unsigned broken(const ObjectActions &objects) {
  return static_cast<unsigned>(std::count_if(
      objects.states().begin(), objects.states().end(),
      [](const auto &state) { return state.broken; }));
}
PlayerShot rayThroughBoth(const ObjectActions &objects,
                          const ObjectActionObject &object,
                          const EnemyEncounter &enemy, bool objectFirst) {
  for (double x : {0., -100., 100., -200., 200.})
    for (double y = 100; y < object.shapes.front().height; y += 100) {
      PlayerShot shot{1, transform(object.matrix, {x, y, -4000}),
                      transform(object.matrix, {x, y, 4000})};
      const auto actor = enemy.target(shot.from, shot.to);
      const auto container = objects.target(shot.from, shot.to);
      if (actor && container && container->slot == object.placement.slot &&
          ((container->hit.lenSq < actor->lenSq) == objectFirst))
        return shot;
    }
  throw std::runtime_error("No original crate / posed Ganado overlap ray");
}
void nearestTargets(const RoomArchive &room,
                    const std::vector<RoomObjectPlacement> &placements,
                    const EnemySpawn &spawn, const char *enemyDrs) {
  ObjectActions source(0x400, room, placements);
  check(source.objects().size() == 16);
  const auto selected = std::find_if(
      source.objects().begin(), source.objects().end(),
      [](const auto &object) { return object.boxType == 5; });
  check(selected != source.objects().end());
  const auto floor = flatFloor(selected->placement.position[1]);
  const SatCollision world(floor), effects(floor, SatQueryMode::Effect);
  ObjectActions objects(0x400, room, placements, &effects);
  auto at = transform(selected->matrix, {0, 0, 2200});
  EnemyEncounter behind(world, effects, spawn, enemyDrs, false,
                        EnemyEncounter::TestPlacement{
                            at, selected->placement.rotation[1]});
  const auto shot = rayThroughBoth(objects, *selected, behind, true);
  check(behind.target(shot.from, shot.to).has_value());
  behind.setObjectActions(&objects);
  check(!behind.target(shot.from, shot.to)); // Laser cannot pass intact box.
  behind.tick(behind.feet(), &shot);
  check(behind.shots() == 1 && behind.hits() == 0 && behind.hp() == 1000);
  check(broken(objects) == 1);
  behind.tick(behind.feet(), &shot);
  check(behind.shots() == 1 && behind.hits() == 0 && behind.hp() == 1000 &&
        broken(objects) == 1); // Same shot callback cannot hit through it.
  behind.setObjectActions(nullptr);
  auto later = shot;
  later.serial = 2;
  behind.tick(behind.feet(), &later);
  check(behind.hits() == 1 && behind.hp() == 1000);
  behind.tick(behind.feet());
  check(behind.hp() < 1000); // Retains the original next-tick damage order.

  ObjectActions farBox(0x400, room, placements, &effects);
  at = transform(selected->matrix, {0, 0, -2200});
  EnemyEncounter inFront(world, effects, spawn, enemyDrs, false,
                         EnemyEncounter::TestPlacement{
                             at, selected->placement.rotation[1]});
  const auto frontShot = rayThroughBoth(farBox, *selected, inFront, false);
  inFront.setObjectActions(&farBox);
  check(inFront.target(frontShot.from, frontShot.to).has_value());
  inFront.tick(inFront.feet(), &frontShot);
  check(inFront.hits() == 1 && broken(farBox) == 0);
  inFront.tick(inFront.feet());
  check(inFront.hp() < 1000 && broken(farBox) == 0);
}
void actualKnife(const RoomArchive &room,
                 const std::vector<RoomObjectPlacement> &placements,
                 const EnemySpawn &spawn, const char *enemyDrs,
                 const char *playerDrs, const char *weaponDrs) {
  ObjectActions objects(0x400, room, placements);
  const auto selected = std::find_if(
      objects.objects().begin(), objects.objects().end(),
      [](const auto &object) { return object.boxType == 5; });
  check(selected != objects.objects().end());
  const auto floor = flatFloor(selected->placement.position[1]);
  const SatCollision world(floor), effects(floor, SatQueryMode::Effect);
  const auto enemyAt = transform(selected->matrix, {0, 0, 2000});
  EnemyEncounter enemy(world, effects, spawn, enemyDrs, true,
                       EnemyEncounter::TestPlacement{
                           enemyAt, selected->placement.rotation[1]});
  enemy.setObjectActions(&objects);
  LeonPreview player(playerDrs, weaponDrs, enemyDrs, true);
  WalkProbe probe(world);
  check(probe.place(transform(selected->matrix, {0, 0, -800})));
  player.setPlacement(probe.position(), selected->placement.rotation[1]);
  PlayerActionInput input;
  input.knifeHeld = true;
  unsigned events{};
  for (unsigned tick = 0; tick < 120 && !broken(objects); ++tick) {
    input.fireHeld = tick >= 30;
    input.firePressed = tick == 30;
    player.step(probe, 0, 0, false, false, 1. / 30, input);
    if (player.lastMeleeEvent()) {
      const auto &event = *player.lastMeleeEvent();
      ++events;
      check(event.kind == PlayerMeleeKind::Knife && !event.rays.empty());
      enemy.queueMelee(event, player.feet());
      if (broken(objects)) {
        const auto states = objects.snapshot();
        enemy.queueMelee(event, player.feet());
        check(objects.snapshot().checksum == states.checksum);
      }
    }
  }
  check(events > 0 && broken(objects) == 1);
  check(enemy.knifeHits() == 0 && enemy.hp() == 1000);
  const unsigned completedSerial = player.lastMeleeEvent()->serial;
  // Zero time remains a read-only frame; no fabricated swing is generated.
  const auto before = objects.snapshot();
  enemy.stepPlayer(player, probe, 0, 0, false, false, 0, input);
  check(objects.snapshot().checksum == before.checksum);
  // Replacing Leon for a restored room retains the weapon event counters.
  // A fresh original slash must not reuse the previous cached attack serial.
  LeonPreview restored(playerDrs, weaponDrs, enemyDrs, true);
  restored.inheritInventory(player);
  WalkProbe restoredProbe(world);
  check(restoredProbe.place(player.feet()));
  restored.setPlacement(restoredProbe.position(), player.heading());
  input = {};
  input.knifeHeld = true;
  bool nextSlash{};
  for (unsigned tick = 0; tick < 120 && !nextSlash; ++tick) {
    input.fireHeld = tick >= 30;
    input.firePressed = tick == 30;
    restored.step(restoredProbe, 0, 0, false, false, 1. / 30, input);
    if (restored.lastMeleeEvent()) {
      check(restored.lastMeleeEvent()->serial == completedSerial + 1);
      nextSlash = true;
    }
  }
  check(nextSlash);
}
void ammoAndHealing(const char *playerDrs, const char *weaponDrs) {
  LeonPreview player(playerDrs, weaponDrs);
  check(player.magazine() == 10 && player.reserve() == 60);
  check(player.addHandgunAmmo(0) == 0 && player.reserve() == 60);
  check(player.addHandgunAmmo(80) == 80 && player.reserve() == 140);
  check(player.addHandgunAmmo(std::numeric_limits<unsigned>::max()) ==
        65535 - 150);
  check(player.magazine() + player.reserve() == 65535 &&
        player.addHandgunAmmo(1) == 0);
  LeonPreview next(playerDrs, weaponDrs);
  next.inheritInventory(player);
  check(next.magazine() == player.magazine() &&
        next.reserve() == player.reserve());
  check(player.life() == 1200 && player.heal(600) == 0);
  player.takeDamage(700, false);
  check(player.life() == 500 && player.heal(600) == 600 &&
        player.life() == 1100);
  check(player.heal(600) == 100 && player.life() == 1200);
  player.takeDamage(1, false);
  check(player.heal(std::numeric_limits<unsigned>::max()) == 1 &&
        player.life() == 1200);
  player.setFatalLife();
  check(player.life() == 0 && player.heal(600) == 0 && player.life() == 0);
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 6) {
      std::cerr << "Usage: object_combat_check EM12 r400 ESL00 PL00 WEP01\n";
      return 2;
    }
    const auto room = re4::RoomArchive::Load(argv[2]);
    const auto *ets = room.find("ETS");
    check(ets != nullptr);
    const auto placements = re4::ReadRoomObjectPlacements(
        room.copy(ets - room.entries().data()));
    const re4::EnemySpawns spawns(read(argv[3]));
    const auto *spawn = spawns.find(re4::EnemyEncounter::TestSpawn);
    check(spawn != nullptr);
    nearestTargets(room, placements, *spawn, argv[1]);
    actualKnife(room, placements, *spawn, argv[1], argv[4], argv[5]);
    ammoAndHealing(argv[4], argv[5]);
    std::cout << "Original r400 crate / posed Ganado nearest-hit ordering, "
                 "shot serial / next-tick damage, original animated knife "
                 "sweep, pause, checked ammo transfer and herb healing: "
              << checks << " checks PASS\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
