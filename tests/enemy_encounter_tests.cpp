#include "archive.h"
#include "enemy_encounter.h"
#include "ports/rnd.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
using re4::CollisionVec;
using re4::EnemyEncounter;
using re4::EnemyMotion;
using re4::EnemySpawn;
using re4::Matrix34;
using re4::PlayerShot;
unsigned checks = 0;
void check(bool value) {
  if (!value)
    std::cerr << "Failed encounter check " << checks + 1 << '\n';
  assert(value);
  ++checks;
}
template <class Function> void rejects(Function action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected);
}
std::vector<std::uint8_t> read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() < 0 || file.tellg() > 32 * 1024 * 1024)
    throw std::runtime_error("Enemy fixture size/path");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
    throw std::runtime_error("Enemy fixture read");
  return bytes;
}
re4::Sat flatFixture() {
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
  for (float value : {-100000.f, 0.f, -100000.f, 100000.f, 0.f, -100000.f,
                     100000.f, 0.f, 100000.f, -100000.f, 0.f, 100000.f,
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
  return re4::Sat(bytes);
}
void actorContact() {
  const auto sat = flatFixture();
  const re4::SatCollision world(sat);
  re4::WalkProbe large(world), split(world);
  const re4::ActorBlocker blocker{{0, 0, 0}, 400, 1600};
  check(large.place({-3000, 0, 0}) && split.place({-3000, 0, 0}));
  large.setActorBlocker(blocker);
  split.setActorBlocker(blocker);
  large.moveDelta({6000, 0, 0});
  for (unsigned i = 0; i < 20; ++i)
    split.moveDelta({300, 0, 0});
  check(large.position()[0] <= -700 && large.position()[0] > -700.1);
  check(split.position()[0] <= -700 && split.position()[0] > -700.1);
  check(std::abs(split.position()[0] - large.position()[0]) < .1);
  auto before = large.position();
  large.moveDelta({-1000, 0, 0});
  check(std::abs(large.position()[0] - before[0] + 1000) < .00001);
  check(large.place({-700, 0, 0}));
  large.moveDelta({0, 0, 1000});
  check(std::abs(large.position()[0] + 700) < .00001 &&
        std::abs(large.position()[2] - 1000) < .00001);
  check(large.place({-3000, 0, 700}));
  large.moveDelta({6000, 0, 0});
  // A true tangent remains clear instead of stopping at the circle's side.
  check(large.position()[0] == 3000 && large.position()[2] == 700);
  for (double actorY : {1600., 2000., -1600.}) {
    check(large.place({-3000, 0, 0}));
    large.setActorBlocker(re4::ActorBlocker{{0, actorY, 0}, 400, 1600});
    large.moveDelta({6000, 0, 0});
    check(large.position()[0] == 3000);
  }
  check(large.place({-3000, 0, 0}));
  large.setActorBlocker(blocker);
  for (double radius : {0., -1., double(NAN), double(INFINITY), 1000001.})
    rejects([&] { large.setActorBlocker(
        re4::ActorBlocker{{0, 0, 0}, radius, 1600}); });
  for (double height : {0., -1., 799., double(NAN), double(INFINITY), 1000001.})
    rejects([&] { large.setActorBlocker(
        re4::ActorBlocker{{0, 0, 0}, 400, height}); });
  rejects([&] { large.setActorBlocker(
      re4::ActorBlocker{{NAN, 0, 0}, 400, 1600}); });
  large.moveDelta({6000, 0, 0});
  check(large.position()[0] <= -700); // Invalid replacement retained blocker.
  large.setActorBlocker(std::nullopt);
  large.moveDelta({6000, 0, 0});
  check(large.position()[0] > 5000);
}
re4::Sat resource(const re4::RoomArchive &archive, const char *tag) {
  const auto *entry = archive.find(tag);
  if (!entry)
    throw std::runtime_error("Enemy fixture room resource");
  return re4::Sat(archive.copy(entry - archive.entries().data()));
}
CollisionVec transform(const Matrix34 &matrix, CollisionVec point) {
  CollisionVec result{};
  for (unsigned row = 0; row < 3; ++row) {
    result[row] = matrix[row * 4 + 3];
    for (unsigned axis = 0; axis < 3; ++axis)
      result[row] += matrix[row * 4 + axis] * point[axis];
  }
  return result;
}
CollisionVec hitCenter(const EnemyEncounter &enemy, bool head) {
  const auto shape = re4::NormalGanadoHitShapes().at(head ? 0 : 1);
  CollisionVec offset{shape.offset[0], shape.offset[1], shape.offset[2]};
  offset[1] += shape.height / 2;
  const auto joint = enemy.actor().rig().jointMatrices().at(shape.partsNo - 1);
  return transform(enemy.placement(), transform(joint, offset));
}
// Aim through the actual current posed authored part, then verify the closest
// capsule is in the intended category. No invented enemy sphere is involved.
PlayerShot aimedRay(const EnemyEncounter &enemy, bool head, unsigned serial) {
  const auto center = hitCenter(enemy, head);
  const auto shapes = re4::NormalGanadoHitShapes();
  for (unsigned axis : {0u, 2u, 1u})
    for (double side : {1., -1.}) {
      PlayerShot ray{serial, center, center};
      ray.from[axis] -= side * 800;
      ray.to[axis] += side * 800;
      const auto hit = re4::HitAuthoredShapes(
          shapes, enemy.actor().rig().jointMatrices(), enemy.placement(),
          ray.from, ray.to, 1600 * 1600);
      if (hit && (shapes[hit->partIndex].partsNo == 5) == head)
        return ray;
    }
  throw std::runtime_error("No ray through current authored hit category");
}
void finite(const EnemyEncounter &enemy) {
  check(enemy.hp() <= enemy.maxHp());
  for (auto value : enemy.feet())
    check(std::isfinite(value));
  check(std::isfinite(enemy.heading()) && std::isfinite(enemy.scale()));
  for (const auto &joint : enemy.actor().rig().jointMatrices())
    for (float value : joint)
      check(std::isfinite(value));
}
void spawnAndMovement(const re4::SatCollision &world,
                      const re4::SatCollision &bullets, const EnemySpawn &spawn,
                      const char *drs) {
  RndInit(0x1234);
  EnemyEncounter enemy(world, bullets, spawn, drs);
  check(RndState() == 0x1234 && enemy.randomState() == 0x6303);
  check(enemy.hp() == 1000 && enemy.maxHp() == 1000 && enemy.alive());
  check(enemy.feet()[0] == spawn.position[0] &&
        enemy.feet()[2] == spawn.position[2] &&
        std::abs(enemy.feet()[1] - spawn.position[1]) <= 250);
  check(enemy.scale() >= 1.02f && enemy.scale() <= 1.04f);
  check(enemy.actor().mesh().triangles == 3013 &&
        enemy.actor().rig().jointMatrices().size() == 34);
  const auto original = enemy.feet();
  const auto player = EnemyEncounter::TestPlayerPosition();
  const double before = std::hypot(player[0] - original[0],
                                   player[2] - original[2]);
  for (unsigned i = 0; i < 240; ++i) {
    enemy.tick(player);
    check(RndState() == 0x1234);
    check(world.ground(enemy.feet(), 1, 1).has_value());
  }
  const double after = std::hypot(player[0] - enemy.feet()[0],
                                  player[2] - enemy.feet()[2]);
  check(after < before - 1000 && after >= 1300 && after <= 1900);
  check(enemy.actor().motion() == EnemyMotion::Idle);
  check(enemy.hp() == 1000 && enemy.shots() == 0 && enemy.hits() == 0);
  finite(enemy);
  auto invalid = spawn;
  invalid.hp = 0;
  rejects([&] { EnemyEncounter rejected(world, bullets, invalid, drs); });
  invalid = spawn;
  invalid.type = 1;
  rejects([&] { EnemyEncounter rejected(world, bullets, invalid, drs); });
  invalid = spawn;
  invalid.flags |= 0x20000000;
  rejects([&] { EnemyEncounter rejected(world, bullets, invalid, drs); });
  invalid = spawn;
  invalid.room = 0x300;
  rejects([&] { EnemyEncounter rejected(world, bullets, invalid, drs); });
}
void queuedDamage(const re4::SatCollision &world,
                   const re4::SatCollision &bullets, const EnemySpawn &spawn,
                   const char *drs) {
  EnemyEncounter enemy(world, bullets, spawn, drs);
  const auto ray = aimedRay(enemy, false, 1);
  check(enemy.target(ray.from, ray.to).has_value());
  auto player = enemy.feet(); // Keep movement out of hit scheduling checks.
  enemy.tick(player, &ray);
  check(enemy.shots() == 1 && enemy.hits() == 1 && enemy.hp() == 1000);
  check(enemy.lastDamage() == 0); // Original EmMgr already ran this tick.
  auto blocked = ray;
  blocked.serial = 2;
  enemy.tick(player, &blocked);
  check(enemy.hp() == 865 && enemy.lastDamage() == 135 &&
        !enemy.lastHead() && !enemy.lastCritical());
  check(enemy.shots() == 2 && enemy.hits() == 1);
  check(enemy.actor().motion() == EnemyMotion::HitBody);
  check(enemy.randomState() == 0x2440); // LifeDownSet2 consumes two bytes.
  auto again = aimedRay(enemy, false, 3);
  enemy.tick(player, &again);
  check(enemy.hits() == 2 && enemy.hp() == 865);
  enemy.tick(player, &again); // Duplicate serial cannot create another hit.
  check(enemy.hp() == 730 && enemy.hits() == 2 && enemy.shots() == 3);
  enemy.tick(player, &ray); // Stale earlier serial is equally ignored.
  check(enemy.hp() == 730 && enemy.hits() == 2 && enemy.shots() == 3);
  for (unsigned i = 0; i < 120; ++i)
    enemy.tick(enemy.feet());
  check(enemy.actor().motion() == EnemyMotion::Idle && enemy.hp() == 730);
  finite(enemy);
  const auto oldHp = enemy.hp(), oldShots = enemy.shots();
  const auto oldFeet = enemy.feet();
  const auto oldPose = enemy.actor().rig().jointMatrices();
  rejects([&] { enemy.tick({NAN, 0, 0}); });
  check(enemy.hp() == oldHp && enemy.shots() == oldShots &&
        enemy.feet() == oldFeet && enemy.actor().rig().jointMatrices() == oldPose);
  auto malformed = ray;
  malformed.serial = 4;
  malformed.to[0] = std::numeric_limits<double>::infinity();
  rejects([&] { enemy.tick(player, &malformed); });
  check(enemy.hp() == oldHp && enemy.shots() == oldShots);
}
void headsAndDeath(const re4::SatCollision &world,
                    const re4::SatCollision &bullets, const EnemySpawn &spawn,
                    const char *drs) {
  EnemyEncounter head(world, bullets, spawn, drs);
  auto ray = aimedRay(head, true, 1);
  head.tick(head.feet(), &ray);
  check(head.hp() == 1000 && head.hits() == 1);
  head.tick(head.feet());
  check(head.hp() == 838 && head.lastDamage() == 162 &&
        head.lastHead() && !head.lastCritical());
  check(head.randomState() == 0x4420);
  check(head.actor().motion() == EnemyMotion::HitHead);
  for (unsigned i = 0; i < 120; ++i)
    head.tick(head.feet());
  check(head.actor().motion() == EnemyMotion::Idle);

  EnemyEncounter death(world, bullets, spawn, drs);
  unsigned serial = 0;
  while (death.alive() && serial < 20) {
    ray = aimedRay(death, false, ++serial);
    const auto oldHp = death.hp();
    death.tick(death.feet(), &ray);
    check(death.hp() == oldHp);
    death.tick(death.feet());
    check(death.hp() == (oldHp > 135 ? oldHp - 135 : 0));
    death.tick(death.feet());
  }
  check(!death.alive() && death.hp() == 0 && death.lastDamage() == 55);
  check(death.hits() == 8 && death.actor().motion() == EnemyMotion::Death);
  check(!death.target(ray.from, ray.to));
  for (unsigned i = 0; i < 240 && !death.deadSettled(); ++i)
    death.tick(death.feet());
  check(death.deadSettled());
  const auto pose = death.actor().rig().jointMatrices();
  const auto feet = death.feet();
  const auto frame = death.actor().rig().curveFrame();
  check(frame >= 101); // Advance original SEQ; sample(max) would loop.
  for (unsigned i = 0; i < 120; ++i)
    death.tick(EnemyEncounter::TestPlayerPosition());
  check(death.actor().rig().jointMatrices() == pose && death.feet() == feet &&
        death.actor().rig().curveFrame() == frame && death.hp() == 0);
  finite(death);

  // A high-HP test fixture lets the original byte stream reach a critical;
  // production keeps the actual ESL HP=1000, already asserted above.
  auto highHp = spawn;
  highHp.hp = 65535;
  EnemyEncounter critical(world, bullets, highHp, drs);
  bool seenCritical = false;
  for (unsigned i = 1; i <= 100 && critical.alive(); ++i) {
    const auto saved = RndState();
    RndInit(critical.randomState());
    const bool expected = Rnd() % 12 == 6;
    Rnd();
    Rnd();
    const auto expectedState = RndState();
    RndInit(saved);
    ray = aimedRay(critical, true, i);
    critical.tick(critical.feet(), &ray);
    critical.tick(critical.feet());
    check(critical.lastHead() && critical.lastCritical() == expected);
    check(critical.lastDamage() == (expected ? 1350u : 162u));
    check(critical.randomState() == expectedState && RndState() == saved);
    critical.tick(critical.feet());
    if (expected) {
      seenCritical = true;
      break;
    }
  }
  check(seenCritical);
}
void occlusion(const re4::SatCollision &world, const re4::SatCollision &bullets,
                const re4::Sat &eat, const EnemySpawn &spawn, const char *drs) {
  EnemyEncounter enemy(world, bullets, spawn, drs);
  const auto shapes = re4::NormalGanadoHitShapes();
  const auto center = hitCenter(enemy, false);
  bool blocked = false;
  for (std::size_t i = 0; i < eat.polygons.size() && !blocked; ++i) {
    if (!re4::SatCollision::accepts(eat.polygons[i].attributes, 0, 0x400000,
                                   re4::SatQueryMode::Effect))
      continue;
    const auto triangle = bullets.triangle(i);
    CollisionVec wallCenter{}, from{}, to{};
    double length2 = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
      wallCenter[axis] = (triangle[0][axis] + triangle[1][axis] +
                          triangle[2][axis]) / 3;
      length2 += (center[axis] - wallCenter[axis]) *
                  (center[axis] - wallCenter[axis]);
    }
    const double length = std::sqrt(length2);
    if (length < 1200 || length > 12000)
      continue;
    for (unsigned axis = 0; axis < 3; ++axis) {
      const double direction = (center[axis] - wallCenter[axis]) / length;
      from[axis] = wallCenter[axis] - direction * 1000;
      to[axis] = center[axis] + direction * 1000;
    }
    if (!re4::HitAuthoredShapes(shapes, enemy.actor().rig().jointMatrices(),
                                enemy.placement(), from, to, 1e9f))
      continue;
    const auto wall = bullets.line(from, to, re4::SatSubset::All, 0, 0x400000);
    if (wall && !enemy.target(from, to)) {
      PlayerShot shot{1, from, to};
      enemy.tick(enemy.feet(), &shot);
      check(enemy.takeShotScenery(2).empty());
      const auto captured=enemy.takeShotScenery(1);
      check(captured.size()==1 && captured[0] && captured[0]->point==wall->point &&
          captured[0]->normal==wall->normal && captured[0]->attributes==wall->attributes);
      check(enemy.takeShotScenery(1).empty());
      enemy.tick(enemy.feet());
      check(enemy.shots() == 1 && enemy.hits() == 0 && enemy.hp() == 1000);
      blocked = true;
    }
  }
  check(blocked); // Actual r100 EAT, not a fabricated test-only wall.
}
void integratedRate(const re4::SatCollision &world,
                     const re4::SatCollision &bullets, const EnemySpawn &spawn,
                     const char *enemyDrs, const char *playerDrs,
                     const char *weaponDrs) {
  auto simulate = [&](unsigned hz) {
    auto enemy = std::make_unique<EnemyEncounter>(world, bullets, spawn, enemyDrs);
    re4::LeonPreview player(playerDrs, weaponDrs);
    re4::WalkProbe probe(world);
    check(probe.place(EnemyEncounter::TestPlayerPosition()));
    const auto feet = probe.position();
    const float heading = float(std::atan2(spawn.position[0] - feet[0],
                                          spawn.position[2] - feet[2]));
    player.setPlacement(feet, heading);
    re4::PlayerActionInput input;
    input.aimHeld = true;
    input.fireHeld = true;
    for (unsigned i = 0; i < hz * 7; ++i) {
      enemy->stepPlayer(player, probe, 0, 0, false, false, 1. / hz, input);
      check(player.shots() + player.magazine() + player.reserve() == 70);
    }
    check(player.shots() == 10 && enemy->shots() == 10);
    check(enemy->hits() > 0 && enemy->hp() < 1000);
    const auto pose = enemy->actor().rig().jointMatrices();
    const auto before = enemy->feet();
    const auto random = enemy->randomState();
    enemy->stepPlayer(player, probe, 0, 0, false, false, 0, input);
    check(enemy->actor().rig().jointMatrices() == pose &&
          enemy->feet() == before && enemy->randomState() == random);
    player.suspendClock();
    for (unsigned i = 0; i < hz; ++i)
      enemy->stepPlayer(player, probe, 0, 0, false, false, 0, input);
    check(enemy->actor().rig().jointMatrices() == pose && enemy->hp() <= 1000);
    finite(*enemy);
    return enemy;
  };
  RndInit(0xabcd);
  const auto hz30 = simulate(30);
  check(RndState() == 0xabcd);
  const auto hz60 = simulate(60);
  check(RndState() == 0xabcd);
  check(hz30->hp() == hz60->hp() && hz30->shots() == hz60->shots() &&
        hz30->hits() == hz60->hits() &&
        hz30->randomState() == hz60->randomState());
  check(hz30->actor().motion() == hz60->actor().motion() &&
        hz30->actor().rig().jointMatrices() ==
            hz60->actor().rig().jointMatrices());
  check(hz30->feet() == hz60->feet() && hz30->heading() == hz60->heading());
}
} // namespace
int main(int argc, char **argv) {
  check(EnemyEncounter::TestSpawn == 0x25);
  check((EnemyEncounter::TestPlayerPosition() ==
          CollisionVec{-82910, 860, -38480}));
  actorContact();
  if (argc == 1) {
    std::cout << "Enemy encounter contract PASS; supply EM12, r100, ESL00, "
                 "PL00 and WEP01 for original-asset integration checks\n";
    return 0;
  }
  check(argc == 6);
  const auto archive = re4::RoomArchive::Load(argv[2]);
  const auto sat = resource(archive, "SAT"), eat = resource(archive, "EAT");
  const re4::SatCollision world(sat), bullets(eat, re4::SatQueryMode::Effect);
  const re4::EnemySpawns spawns(read(argv[3]));
  const auto *spawn = spawns.find(EnemyEncounter::TestSpawn);
  check(spawn && spawn->hp == 1000 && spawn->room == 0x100 &&
        spawn->id == 0x12 && spawn->type == 0 && spawn->flags == 0x24 &&
        !spawn->alive()); // Explicit test activation, not campaign spawning.
  spawnAndMovement(world, bullets, *spawn, argv[1]);
  queuedDamage(world, bullets, *spawn, argv[1]);
  headsAndDeath(world, bullets, *spawn, argv[1]);
  occlusion(world, bullets, eat, *spawn, argv[1]);
  integratedRate(world, bullets, *spawn, argv[1], argv[4], argv[5]);
  std::cout << "Original ESL HP/pose/root, next-tick handgun damage, head "
               "critical RNG, corpse hold, real EAT occlusion, trigger/ammo "
               "conservation and 30/60-Hz integration: "
            << checks << " checks PASS\n";
}
