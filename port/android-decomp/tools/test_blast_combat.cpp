#include "archive.h"
#include "enemy_encounter.h"
#include "grab_rules.h"
#include "melee_rules.h"
#include "object_actions.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace re4;
unsigned checks{};
void check(bool value, const char *label) {
  if (!value)
    throw std::runtime_error(std::string("Blast check: ") + label);
  ++checks;
}
std::vector<std::uint8_t> read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() < 0 || file.tellg() > 32 * 1024 * 1024)
    throw std::runtime_error("Blast fixture path / size");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
    throw std::runtime_error("Blast fixture read");
  return bytes;
}
Sat floorData(bool wall = false) {
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
  half(2, 4); half(4, 1); half(6, 1); half(10, 2); half(12, 2);
  std::size_t offset = 20;
  const std::array<float, 15> floorValues{
      -100000.f, 0.f, -100000.f, 100000.f, 0.f, -100000.f,
      100000.f, 0.f, 100000.f, -100000.f, 0.f, 100000.f, 0.f, 1.f, 0.f};
  const std::array<float, 15> wallValues{
      1000.f, -10000.f, -10000.f, 1000.f, 10000.f, -10000.f,
      1000.f, 10000.f, 10000.f, 1000.f, -10000.f, 10000.f, 1.f, 0.f, 0.f};
  for (float value : wall ? wallValues : floorValues) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    word(offset, bits);
    offset += 4;
  }
  offset += 12;
  half(offset + 2, 2); half(offset + 4, 1);
  half(offset + 22, 3); half(offset + 24, 2);
  return Sat(bytes);
}
std::vector<std::uint8_t> entry(const RoomArchive &archive, unsigned index,
                              const char *tag) {
  check(index < archive.entries().size(), "motion index exists");
  check(std::equal(tag, tag + 3, archive.entries()[index].tag.begin()),
        "motion entry has original tag");
  return archive.copy(index);
}
std::unique_ptr<AnimationRig> referencePlayer(const char *playerDrs,
                                           const char *weaponDrs) {
  const auto player = RoomArchive::LoadDrs(playerDrs);
  const auto weapon = RoomArchive::LoadDrs(weaponDrs);
  SkinnedModel body(entry(player, 0, "BIN"));
  std::vector<AnimationBytes> clips(16);
  const unsigned movement[]{7, 13, 17, 15, 11, 9, 19};
  for (unsigned i = 0; i < 7; ++i)
    clips[i] = {entry(weapon, movement[i], "FCV"),
                entry(weapon, movement[i] + 1, "SEQ")};
  clips[7] = {entry(weapon, 30, "FCV"), entry(weapon, 31, "SEQ")};
  clips[8] = {entry(weapon, 32, "FCV"), entry(weapon, 33, "SEQ")};
  for (unsigned i = 0; i < 3; ++i) {
    clips[9 + i] = {entry(weapon, 34 + i, "FCV"), {}};
    clips[12 + i] = {entry(weapon, 37 + i, "FCV"), {}};
  }
  clips[15] = {entry(weapon, 41, "FCV"), {}};
  // Independently selected from original pl_dmg.cpp PL_ARC pointers.
  clips.push_back({entry(player, 0x51 - 4, "FCV"),
                   entry(player, 0x67 - 4, "SEQ")});
  clips.push_back({entry(player, 0x4e - 4, "FCV"),
                   entry(player, 0x66 - 4, "SEQ")});
  clips.push_back({entry(player, 0x4f - 4, "FCV"),
                   entry(player, 0x50 - 4, "SEQ")});
  clips.push_back({entry(player, 0x52 - 4, "FCV"), {}});
  return std::make_unique<AnimationRig>(body, clips);
}
void comparePose(const LeonPreview &player, const AnimationRig &reference) {
  const auto &actual = player.rig().jointMatrices();
  const auto &expected = reference.jointMatrices();
  check(actual.size() == expected.size(), "root and119 parts retained");
  float maximum{};unsigned maximumPart{},maximumAxis{};
  for (std::size_t i = 0; i < actual.size(); ++i)
    for (unsigned axis = 0; axis < 12; ++axis) {
      check(std::isfinite(actual[i][axis]), "finite original pose");
      const auto delta=std::abs(actual[i][axis]-expected[i][axis]);
      if(delta>maximum){maximum=delta;maximumPart=unsigned(i);maximumAxis=axis;}
    }
  if(maximum>=.001f)
    throw std::runtime_error("Blast pose mismatch: action="+std::string(PlayerActionLabel(player.action()))+
        " life="+std::to_string(player.life())+" playerFrame="+std::to_string(player.rig().sequenceFrame())+
        " referenceFrame="+std::to_string(reference.sequenceFrame())+" maxDelta="+std::to_string(maximum)+
        " part="+std::to_string(maximumPart)+" axis="+std::to_string(maximumAxis));
  check(maximum < .001f, "original pl_dmg authored pose matches");
}
void playerMotion(const SatCollision &world, const char *playerDrs,
                  const char *weaponDrs, bool inner, bool fatal) {
  LeonPreview player(playerDrs, weaponDrs);
  // Compare damage FCVs in the source's authored-eye mode. Normal live
  // blinking/gaze runs after MotionMove and deliberately changes these
  // face joints independently of the damage FCV.
  player.setRoomScriptEyeMode(1);
  WalkProbe probe(world);
  check(probe.place({0, 0, 0}), "player floor placement");
  player.setPlacement(probe.position(), .7f);
  auto reference = referencePlayer(playerDrs, weaponDrs);
  reference->reset();
  comparePose(player,*reference);
  if (fatal)
    player.takeDamage(1000, false); // life200 never consumes survival RNG.
  const auto beforeLife = player.life();
  RndInit(0x1974);
  unsigned expectedLife = beforeLife;
  if (inner)
    expectedLife = GrabPlayerLifeDown(expectedLife, 1200, beforeLife >= 501);
  expectedLife = GrabPlayerLifeDown(expectedLife, 0, true);
  const auto expectedRandom = RndState();
  RndInit(0x1974);
  check(player.applyBlast({inner, inner ? 1200u : 0u}, {100, 500, 100}, probe),
        "player original blast accepted");
  check(player.life() == expectedLife && RndState() == expectedRandom,
        "original player LifeDownSet2 and extra arg0 RNG order");
  check(player.heading() == .7f, "PlSetDamage123 keeps player direction");
  check(!player.weaponVisible(), "weapon hidden during damage");
  const auto savedPose = player.rig().jointMatrices();
  player.step(probe, 1, 1, true, true, 0, {});
  check(savedPose == player.rig().jointMatrices(), "zero-time frame has no tick");
  check(!player.applyBlast({true, 1200}, {0, 500, 0}, probe),
        "protected damage excludes repeated blast");
  reference->reset();
  reference->setWaistYaw(0);
  reference->startActionFlags(!player.alive() ? 17 : inner ? 16 : 19, 5, 0, 1);
  comparePose(player, *reference);
  bool completed{};
  for (unsigned tick = 0; tick < 360; ++tick) {
    const auto root = reference->advanceRootAction();
    player.step(probe, 1, 1, true, true, 1. / 30, {});
    if (root.ended && inner && !fatal &&
        player.action() == PlayerAction::BlastRecover) {
      reference->startActionFlags(18, 3, 0, 5);
      if(player.alive())reference->setPartRotation(0x21,reference->partRotation(0x20));
      comparePose(player, *reference);
      continue;
    }
    if(player.alive())reference->setPartRotation(0x21,reference->partRotation(0x20));
    comparePose(player, *reference);
    if (!player.blastActive()) {
      completed = true;
      break;
    }
    check(player.shots() == 0, "damage handler ignores fire/movement keys");
  }
  if (!completed)
    throw std::runtime_error("Blast completion inner=" + std::to_string(inner) +
                             " fatal=" + std::to_string(fatal) +
                             " action=" + PlayerActionLabel(player.action()) +
                             " sequence=" + std::to_string(player.rig().sequenceFrame()) +
                             " state=" + std::to_string(player.rig().status().state));
  check(completed, "original finite damage motion completes");
  check(player.action() == (fatal ? PlayerAction::Dead : PlayerAction::Footwork),
        "original dead hold or return to footwork");
  check(player.damageWait() == (fatal ? 0x80u : 5u),
        "original recovery damage protection");
}
void verticalGeometry() {
  const Matrix34 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  std::vector<AuthoredHitShape> shapes{{{0, 0, 0}, 100, 200, 0, HitOn, 0}};
  check(SphereAuthoredShapes(shapes, {}, identity, {0, 2200, 0},
                            6000, 0x13, 2000).has_value(),
        "original r2 inclusive height boundary");
  check(!SphereAuthoredShapes(shapes, {}, identity, {0, 2201, 0},
                             6000, 0x13, 2000), "original r2 beyond height");
  check(SphereAuthoredShapes(shapes, {}, identity, {0, 2201, 0},
                            6000, 0x13).has_value(), "kick default unchanged");
  auto scaled = identity;
  scaled[0] = scaled[5] = scaled[10] = 2;
  check(!SphereAuthoredShapes(shapes, {}, scaled, {0, 2301, 0},
                             6000, 0x13, 2000),
        "original r2 uses unscaled authored radius");
}
void enemyBlast(const SatCollision &world, const SatCollision &effects,
                const EnemySpawn &source, const char *enemyDrs,
                const char *playerDrs, const char *weaponDrs,
                bool selectHead = false) {
  auto spawn = source;
  spawn.hp = 30000; // Isolate source2500/head multiplier instead of HP clamp.
  EnemyEncounter enemy(world, effects, spawn, enemyDrs, true,
                       EnemyEncounter::TestPlacement{{0, 0, 0}, 0});
  LeonPreview player(playerDrs, weaponDrs, enemyDrs, true);
  WalkProbe playerProbe(world);
  check(playerProbe.place({20000, 0, 20000}), "remote player placed");
  player.setPlacement(playerProbe.position(), 0);
  ObjectBlast blast{0, 39, 1, 0x13, {2000, 750, 0}, 6000};
  if (selectHead) {
    const auto shapes = NormalGanadoHitShapes();
    const auto headShape = std::find_if(shapes.begin(), shapes.end(),
        [](const auto &shape) { return shape.partsNo == 5; });
    check(headShape != shapes.end(), "original head capsule exists");
    auto center = headShape->offset;
    center[1] += headShape->height * .5f;
    center = TransformPoint(enemy.actor().rig().jointMatrices()[4], center);
    center = TransformPoint(enemy.placement(), center);
    blast.position = {center[0], center[1], center[2]};
  }
  const auto hit = SphereAuthoredShapes(NormalGanadoHitShapes(),
      enemy.actor().rig().jointMatrices(), enemy.placement(), blast.position,
      6000, 0x13, 2000);
  check(hit.has_value(), "posed original Ganado blast capsule hit");
  const bool head = NormalGanadoHitShapes()[hit->partIndex].partsNo == 5;
  check(head == selectHead, "selected authored body/head blast branch");
  RndInit(enemy.randomState());
  const bool critical = head && Rnd() % 12 == 6;
  const unsigned expectedDamage = head ? (critical ? 9999 : 3000) : 2500;
  (void)Rnd(); (void)Rnd();
  const auto choice = SelectMeleeBlow(enemy.feet(), enemy.heading(),
                                     blast.position, true);
  const auto expectedRandom = RndState();
  const auto life = player.life();
  check(enemy.applyBlast(blast, player, playerProbe), "enemy blast queued");
  check(enemy.hp() == 30000 && player.life() == life,
        "enemy queue does not apply player damage twice");
  check(!enemy.applyBlast(blast, player, playerProbe), "blast serial once");
  enemy.tick(player.feet());
  check(enemy.hp() == 30000 - expectedDamage &&
        enemy.lastDamage() == expectedDamage &&
        enemy.lastDamageSource() == EnemyEncounter::DamageSource::Blast &&
        enemy.lastHead() == head && enemy.lastCritical() == critical,
        "weapon13 original damage table/head/critical");
  check(enemy.randomState() == expectedRandom,
        "enemy LifeDownSet2 then original Dm_Blow RNG order");
  check(std::abs(enemy.heading() - choice.heading) < .0001,
        "original blast throw direction");
  auto later = blast; ++later.serial;
  check(!enemy.applyBlast(later, player, playerProbe), "source protected timer");
  EnemyEncounter far(world, effects, source, enemyDrs, true,
                     EnemyEncounter::TestPlacement{{0, 0, 0}, 0});
  later.position = {0, 10000, 0};
  check(!far.applyBlast(later, player, playerProbe), "vertical blast excluded");
  const auto wall = floorData(true);
  const SatCollision blockedEffects(wall, SatQueryMode::Effect);
  EnemyEncounter blocked(world, blockedEffects, source, enemyDrs, true,
                         EnemyEncounter::TestPlacement{{0, 0, 0}, 0});
  later.position = {3000, 750, 0};
  check(blockedEffects.line(later.position, {0, 750, 0}, SatSubset::All,
                           0, 0x00400000).has_value(), "EAT wall fixture");
  check(!blocked.applyBlast(later, player, playerProbe),
        "source beyond30percent radius EAT blast occlusion");
  ++later.serial;
  later.position = {1500, 750, 0};
  check(blocked.applyBlast(later, player, playerProbe),
        "source close blast bypasses scenario wall ray");
  unsigned before{}, after{};
  far.stepPlayer(player, playerProbe, 0, 0, false, false, 0, {},
                 [&] { ++before; }, [&](const PlayerShot *) { ++after; });
  check(before == 0 && after == 0, "render frame skips world callbacks");
  for (unsigned frame = 0; frame < 10; ++frame)
    far.stepPlayer(player, playerProbe, 0, 0, false, false, .02, {},
                   [&] { check(before == after, "before original tick order");
                         ++before; },
                   [&](const PlayerShot *) {
                     check(before == after + 1, "after original tick order");
                     ++after;
                   });
  check(before == 6 && after == 6, "callbacks tied to original30Hz");
}
void cappedHeadCritical(const SatCollision &world, const SatCollision &effects,
                        const EnemySpawn &source, const char *enemyDrs,
                        const char *playerDrs, const char *weaponDrs) {
  auto spawn = source;
  spawn.hp = 30000;
  EnemyEncounter enemy(world, effects, spawn, enemyDrs, true,
                       EnemyEncounter::TestPlacement{{0, 0, 0}, 0});
  LeonPreview player(playerDrs, weaponDrs);
  WalkProbe playerProbe(world);
  check(playerProbe.place({20000, 0, 20000}), "critical-test player placed");
  player.setPlacement(playerProbe.position(), 0);
  bool ready{};
  for (unsigned attempt = 0; attempt < 64; ++attempt) {
    const auto previousRandom = RndState();
    RndInit(enemy.randomState());
    ready = Rnd() % 12 == 6;
    RndInit(previousRandom);
    if (ready)
      break;
    // Advance the real source stream through actual ordinary bullet hits;
    // no private seed injection or invented blast damage is used.
    auto center = TransformPoint(enemy.actor().rig().jointMatrices()[1],
                                 {0, 115, 0});
    center = TransformPoint(enemy.placement(), center);
    PlayerShot shot{attempt + 1,
                    {center[0] - 4000, center[1], center[2]},
                    {center[0] + 4000, center[1], center[2]}};
    check(enemy.target(shot.from, shot.to).has_value(),
          "posed body shot advances original stream");
    enemy.tick(enemy.feet(), &shot);
    enemy.tick(enemy.feet());
    enemy.tick(enemy.feet());
  }
  check(ready, "original stream reaches head-critical byte");
  auto center = TransformPoint(enemy.actor().rig().jointMatrices()[4],
                               {0, 55, 0});
  center = TransformPoint(enemy.placement(), center);
  ObjectBlast blast{0, 39, 1, 0x13, {center[0], center[1], center[2]}, 6000};
  const auto previousHp = enemy.hp();
  check(enemy.applyBlast(blast, player, playerProbe), "critical blast queued");
  enemy.tick(enemy.feet());
  check(enemy.lastHead() && enemy.lastCritical() &&
        enemy.lastDamage() == 9999 && enemy.hp() == previousHp - 9999,
        "em10SetDmVal final9999 cap on2500x10 critical");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 5) {
      std::cerr << "Usage: blast_combat_check EM12 ESL00 PL00 WEP01\n";
      return 2;
    }
    const Sat data = floorData();
    const SatCollision world(data), effects(data, SatQueryMode::Effect);
    const EnemySpawns spawns(read(argv[2]));
    const auto *spawn = spawns.find(EnemyEncounter::TestSpawn);
    check(spawn != nullptr, "original ESL source actor");
    verticalGeometry();
    playerMotion(world, argv[3], argv[4], false, false);
    playerMotion(world, argv[3], argv[4], true, false);
    playerMotion(world, argv[3], argv[4], true, true);
    enemyBlast(world, effects, *spawn, argv[1], argv[3], argv[4]);
    enemyBlast(world, effects, *spawn, argv[1], argv[3], argv[4], true);
    cappedHeadCritical(world, effects, *spawn, argv[1], argv[3], argv[4]);
    std::cout << "Original player blast FCV/SEQ poses, LifeDown/RNG, Ganado "
                 "weapon13 damage, r2 height, protection, 30Hz callbacks: "
              << checks << " checks PASS\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
