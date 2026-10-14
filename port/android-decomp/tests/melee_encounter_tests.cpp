#include "enemy_encounter.h"
#include "input.h"
#include "melee_rules.h"
#include "ports/rnd.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace re4;
unsigned checks{};
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
}
bool close(const CollisionVec &a, const CollisionVec &b, double tolerance = .02) {
  for (unsigned k = 0; k < 3; ++k)
    if (std::abs(a[k] - b[k]) > tolerance)
      return false;
  return true;
}
CollisionVec transform(const Matrix34 &m, const CollisionVec &p) {
  CollisionVec result{};
  for (unsigned row = 0; row < 3; ++row) {
    result[row] = m[row * 4 + 3];
    for (unsigned column = 0; column < 3; ++column)
      result[row] += m[row * 4 + column] * p[column];
  }
  return result;
}
// Scene collision is deliberately synthetic. The command line supplies the
// unmodified PL00, WEP01 and EM12 archives; generated archives remain a separate
// synthetic test run. A broad floor isolates the original combat routines.
Sat flatFloor() {
  std::vector<std::uint8_t> bytes(20 + 48 + 12 + 12 + 40);
  auto half = [&](std::size_t offset, unsigned value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
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
  for (float value : {-100000.f, 830.f, -100000.f, 100000.f, 830.f, -100000.f,
                     100000.f, 830.f, 100000.f, -100000.f, 830.f, 100000.f,
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
Sat isolatedScene(bool wall) {
  auto result = flatFloor();
  if (!wall)
    return result;
  // Two tall triangles800 units in front of the frozen ESL spawn. Both
  // actors can stand freely, but a knife line or a kick prompt cannot cross.
  const float z = -32960.f - 800.f;
  result.vertices.insert(result.vertices.end(),
                         {{-100000, 0, z}, {100000, 0, z},
                          {100000, 5000, z}, {-100000, 5000, z}});
  result.normals.push_back({0, 0, -1});
  SatPolygon a, b;
  a.vertices = {4, 6, 5};
  b.vertices = {4, 7, 6};
  a.normal = b.normal = 1;
  result.polygons.push_back(a);
  result.polygons.push_back(b);
  result.wall = 2;
  return result;
}
EnemySpawn authoredSpawn() {
  EnemySpawn result;
  result.listIndex = 0x25;
  result.recordOffset = 0x25 * 32;
  result.id = 0x12;
  result.type = 0;
  result.setNo = 0x1b;
  result.character = 1;
  result.flags = 0x24;
  result.hp = 1000;
  result.room = 0x100;
  result.position = {-83950, 830, -32960};
  result.heading = .0005752427954571154;
  return result;
}
struct Assets {
  const char *player;
  const char *weapon;
  const char *enemy;
};
struct Fixture {
  Sat sat;
  SatCollision world{sat};
  SatCollision effects{sat, SatQueryMode::Effect};
  EnemySpawn spawn = authoredSpawn();
  EnemyEncounter enemy;
  LeonPreview player;
  WalkProbe probe{world};
  explicit Fixture(Assets paths, bool melee = false, bool wall = false)
      : sat(isolatedScene(wall)), enemy(world, effects, spawn, paths.enemy, melee),
        player(paths.player, paths.weapon, paths.enemy, true) {
    place({50000, 830, 50000}, 0);
    check(player.hasMeleeMotions(), "Original Leon knife and EM12 kick clips loaded");
  }
  void place(const CollisionVec &position, float heading) {
    check(probe.place(position), "Player placement on isolated SAT floor");
    player.setPlacement(probe.position(), heading);
  }
  void frame(double seconds, PlayerActionInput input = {}, bool coupled = false) {
    if (coupled)
      enemy.stepPlayer(player, probe, 0, 0, false, false, seconds, input);
    else
      player.step(probe, 0, 0, false, false, seconds, input);
  }
  void tick(PlayerActionInput input = {}, bool coupled = false) {
    frame(1. / 30, input, coupled);
  }
  void readyKnife() {
    PlayerActionInput input;
    input.knifeHeld = true;
    for (unsigned i = 0; i < 500 && player.action() != PlayerAction::KnifeReady; ++i)
      tick(input);
    check(player.action() == PlayerAction::KnifeReady && player.bladeVisible(),
          "Authored knife draw reaches visible ready stance");
    check(!player.weaponVisible(), "Handgun hidden in knife stance");
  }
};
auto eventSnapshot(const LeonPreview &player) {
  PlayerMeleeEvent event;
  const bool present = bool(player.lastMeleeEvent());
  if (present)
    event = *player.lastMeleeEvent();
  return std::make_tuple(present, event.kind, event.serial, event.rays,
                         event.flag, event.centers, event.radius);
}
auto snapshot(const Fixture &fixture) {
  const auto &p = fixture.player;
  const auto &e = fixture.enemy;
  return std::make_tuple(p.action(), p.life(), p.magazine(), p.reserve(), p.shots(),
                         p.feet(), p.heading(), p.damageWait(), p.bladeVisible(),
                         p.weaponVisible(), p.rig().curveFrame(),
                         p.rig().sequenceFrame(), p.rig().status().state,
                         p.rig().jointMatrices(), eventSnapshot(p), e.hp(),
                         e.actor().motion(), e.feet(), e.heading(), e.randomState(),
                         e.actor().rig().curveFrame(), e.actor().rig().sequenceFrame(),
                         e.actor().rig().jointMatrices(), e.actor().rig().status().state,
                         e.deadSettled(), e.shots(), e.hits(), e.knifeHits(), e.kickHits(),
                         e.lastDamage(), e.lastDamageSource(), e.lastHead(),
                         e.grabs(), e.escapes(), e.resistance());
}
void ammoUntouched(const Fixture &fixture) {
  check(fixture.player.magazine() == 10 && fixture.player.reserve() == 60 &&
            fixture.player.shots() == 0,
        "Knife/kick never consumes handgun ammo or adds projectile shots");
}
void finite(const Fixture &fixture) {
  for (const auto *rig : {&fixture.player.rig(), &fixture.enemy.actor().rig()})
    for (const auto &joint : rig->jointMatrices())
      for (float value : joint)
        check(std::isfinite(value), "Finite original melee pose");
  if (fixture.player.lastMeleeEvent()) {
    for (const auto &ray : fixture.player.lastMeleeEvent()->rays)
      for (const auto &point : {ray.first, ray.second})
        for (double value : point)
          check(std::isfinite(value), "Finite posed knife sweep");
  }
}
void knifePoseAndLifecycle(Assets assets) {
  Fixture fixture(assets);
  fixture.readyKnife();
  PlayerActionInput input;
  input.knifeHeld = true;
  input.fireHeld = true;
  input.firePressed = true;
  fixture.tick(input);
  check(fixture.player.action() == PlayerAction::KnifeSlash,
        "Fresh universal fire edge starts knife slash");
  input.firePressed = false;
  input.fireHeld = false;
  unsigned eventCount{}, serial{};
  CollisionVec previousTip{};
  for (unsigned tick = 0; tick < 200; ++tick) {
    fixture.tick(input);
    if (const auto &event = fixture.player.lastMeleeEvent()) {
      const auto frame = fixture.player.rig().sequenceFrame();
      check(event->kind == PlayerMeleeKind::Knife && frame >= 6 && frame <= 10,
            "Knife checks use original SEQ frames6..10 only");
      const bool first = eventCount == 0;
      const unsigned expectedFlag = (frame < 7 || frame > 8 ? 4u : 0u) |
                                    (first ? 8u : 0u);
      check(event->flag == expectedFlag,
            "Original first-frame and weak-frame flags retained");
      check(event->rays.size() == (first ? 3u : 7u),
            "First slash frame has3 rays; later frames have4 swept plus3 current");
      if (first)
        serial = event->serial;
      check(serial == event->serial && serial != 0,
            "One attack serial shared across authored knife frames");
      const auto &joints = fixture.player.rig().jointMatrices();
      const auto chest = transform(fixture.player.placement(),
                                    transform(joints.at(2), {0, 0, 0}));
      const auto tip = transform(fixture.player.placement(),
                                  transform(joints.at(9), {-750, 0, 0}));
      const auto base = event->rays.size() - 3;
      check(close(event->rays[base].first, chest) &&
                close(event->rays[base].second, tip),
            "Knife ray uses current solved chest2 and blade9 matrices");
      auto upperChest = chest, upperTip = tip;
      upperChest[1] += 200;
      upperTip[1] += 400;
      auto lowerChest = chest, lowerTip = tip;
      lowerChest[1] -= 100;
      lowerTip[1] -= 200;
      check(close(event->rays[base + 1].first, upperChest) &&
                close(event->rays[base + 1].second, upperTip) &&
                close(event->rays[base + 2].first, lowerChest) &&
                close(event->rays[base + 2].second, lowerTip),
            "Original upper/lower knife ray offsets");
      if (!first) {
        for (unsigned sweep = 0; sweep < 4; ++sweep) {
          CollisionVec interpolated{};
          for (unsigned k = 0; k < 3; ++k)
            interpolated[k] = previousTip[k] +
                              (tip[k] - previousTip[k]) * (.2 * (sweep + 1));
          check(close(event->rays[sweep].first, chest) &&
                    close(event->rays[sweep].second, interpolated),
                "Old blade tip swept in original fifth-step increments");
        }
      }
      previousTip = tip;
      ++eventCount;
      const auto held = snapshot(fixture);
      const auto rng = RndState();
      for (unsigned paused = 0; paused < 3; ++paused)
        fixture.frame(0, input);
      check(snapshot(fixture) == held && RndState() == rng,
            "Zero-dt redraw cannot repeat a knife check or advance its pose");
    }
    ammoUntouched(fixture);
    if (eventCount && fixture.player.action() == PlayerAction::KnifeReady &&
        !fixture.player.lastMeleeEvent())
      break;
  }
  check(eventCount == 5, "One complete slash has exactly five source hit frames");
  check(fixture.player.action() == PlayerAction::KnifeReady,
        "Finished original slash returns to knife stance");
  fixture.tick();
  check(fixture.player.action() == PlayerAction::KnifeHolster,
        "Releasing knife stance starts authored holster");
  for (unsigned tick = 0; tick < 200 && fixture.player.knifeActive(); ++tick)
    fixture.tick();
  check(!fixture.player.knifeActive() && !fixture.player.bladeVisible() &&
            fixture.player.weaponVisible(),
        "Holster restores handgun and hides BIN9 blade");
  ammoUntouched(fixture);
  finite(fixture);
}
void knifeLatchedFireAndGrab(Assets assets) {
  Fixture fixture(assets);
  fixture.readyKnife();
  PlayerActionInput input;
  input.knifeHeld = true;
  input.firePressed = true;
  fixture.frame(0, input);
  fixture.frame(0);
  check(fixture.player.action() == PlayerAction::KnifeReady,
        "A short fire edge waits for a fixed simulation tick");
  input.firePressed = false;
  fixture.tick(input);
  check(fixture.player.action() == PlayerAction::KnifeSlash,
        "Latched fire edge survives a zero-dt render boundary");
  fixture.player.beginGrab(0, {});
  check(fixture.player.action() == PlayerAction::Grabbed &&
            !fixture.player.bladeVisible() && !fixture.player.weaponVisible() &&
            !fixture.player.lastMeleeEvent(),
        "Original grab interruption clears knife pose, blade and damage event");
  const auto before = snapshot(fixture);
  fixture.player.suspendClock();
  const auto suspended = snapshot(fixture);
  for (unsigned i = 0; i < 60; ++i)
    fixture.frame(0);
  check(snapshot(fixture) == suspended, "Suspended interrupted melee remains frozen");
  check(std::get<0>(before) == PlayerAction::Grabbed,
        "Grab interruption is preserved across suspension");
  ammoUntouched(fixture);

  Fixture fresh(assets);
  fresh.readyKnife();
  input.firePressed = true;
  fresh.frame(0, input);
  fresh.player.suspendClock();
  input.firePressed = false;
  for (unsigned i = 0; i < 10; ++i)
    fresh.tick(input);
  check(fresh.player.action() != PlayerAction::KnifeSlash &&
            !fresh.player.lastMeleeEvent(),
        "Suspension discards a fire edge waiting before knife evaluation");
}
void kickPoseAndLifecycle(Assets assets) {
  Fixture fixture(assets);
  const CollisionVec target{50700, 830, 50800};
  const auto externalRandom = RndState();
  RndInit(0x1234);
  (void)Rnd();
  const auto expectedRandom = RndState();
  RndInit(0x1234);
  fixture.player.beginKick(target);
  check(fixture.player.action() == PlayerAction::Kick &&
            RndState() == expectedRandom,
        "Beginning source kick consumes exactly one camera-variant RNG draw");
  check(!fixture.player.weaponVisible() && !fixture.player.bladeVisible(),
        "Both handgun and blade hidden during kick");
  unsigned events{};
  for (unsigned tick = 1; tick < 200; ++tick) {
    const auto beforeFrame = fixture.player.rig().curveFrame();
    const auto placement = fixture.player.placement();
    fixture.tick();
    if (const auto &event = fixture.player.lastMeleeEvent()) {
      ++events;
      check(event->kind == PlayerMeleeKind::Kick && event->rays.empty() &&
                event->radius == 1200 && event->serial != 0,
            "Kick produces the original radius event, not a projectile ray");
      check(tick == 17, "Kick hit is queued by m_Work0 countdown17");
      check(close(event->centers[0], transform(placement, {0, 1500, 300})) &&
                close(event->centers[1], transform(placement, {0, 1000, 300})),
            "Two kick centers come from source pre-advance placement");
      check(fixture.player.rig().curveFrame() > beforeFrame,
            "Kick pose advances after the source hit event is computed");
      const auto frozen = snapshot(fixture);
      for (unsigned i = 0; i < 30; ++i)
        fixture.frame(0);
      check(snapshot(fixture) == frozen,
            "Frozen kick hit tick neither repeats the event nor advances pose");
    }
    ammoUntouched(fixture);
    if (fixture.player.action() == PlayerAction::Kick)
      check(fixture.player.damageWait() == 30,
            "Source30-tick player damage protection is refreshed during kick");
    if (fixture.player.action() != PlayerAction::Kick)
      break;
  }
  check(events == 1 && fixture.player.action() == PlayerAction::Footwork,
        "One source kick finishes once and restores footwork");
  check(fixture.player.weaponVisible() && fixture.player.damageWait() == 15,
        "Finished kick restores handgun with source15-tick player protection");
  finite(fixture);
  RndInit(externalRandom);
}
using Snapshot = decltype(snapshot(std::declval<const Fixture &>()));
using TimedSnapshot = std::pair<Snapshot, std::uint16_t>;
std::vector<TimedSnapshot> actorTimeline(Assets assets, unsigned rendersPerTick) {
  RndInit(0x5b21);
  Fixture fixture(assets);
  std::vector<TimedSnapshot> result;
  for (unsigned step = 0; step < 500; ++step) {
    for (unsigned render = 0; render < rendersPerTick; ++render) {
      PlayerActionInput input;
      input.knifeHeld = step < 100;
      input.firePressed = step == 60 && render == 0;
      if (render == 0) {
        if (step == 120)
          fixture.player.beginKick({50700, 830, 50800});
        if (step == 240)
          fixture.player.beginGrab(0, {});
        if (step == 280)
          fixture.player.releaseGrab(true);
        if (step == 420 && fixture.player.grabbed())
          fixture.player.finishGrab();
      }
      fixture.frame(1. / (30 * rendersPerTick), input);
    }
    result.emplace_back(snapshot(fixture), RndState());
    ammoUntouched(fixture);
  }
  finite(fixture);
  return result;
}
void actorRates(Assets assets) {
  const auto previous = RndState();
  const auto thirty = actorTimeline(assets, 1);
  const auto sixty = actorTimeline(assets, 2);
  check(thirty.size() == sixty.size(), "Equal source ticks at30 and60 render FPS");
  for (std::size_t i = 0; i < thirty.size(); ++i)
    check(thirty[i] == sixty[i],
          "Knife/kick/grab states, authored pose, HP, ammo and RNG match at30/60FPS");
  RndInit(previous);
}
CollisionVec enemyPartCenter(const Fixture &fixture, unsigned shapeIndex) {
  const auto shape = NormalGanadoHitShapes().at(shapeIndex);
  CollisionVec center{shape.offset[0], shape.offset[1], shape.offset[2]};
  const unsigned axis = shape.flags & HitXAxis ? 0 : shape.flags & HitZAxis ? 2 : 1;
  center[axis] += shape.height * .5;
  if (shape.partsNo)
    center = transform(fixture.enemy.actor().rig().jointMatrices().at(shape.partsNo - 1),
                       center);
  return transform(fixture.enemy.placement(), center);
}
std::pair<CollisionVec, CollisionVec> posedPartRay(const Fixture &fixture,
                                                unsigned shapeIndex) {
  const auto center = enemyPartCenter(fixture, shapeIndex);
  const auto shapes = NormalGanadoHitShapes();
  // Select through the actual loaded, animated capsule, not an assumed
  // world-height. Several directions avoid another original part occluding
  // the chosen capsule in either original or generated test skeletons.
  for (double span : {500., 300., 220., 180.})
    for (unsigned axis : {2u, 0u, 1u})
      for (double direction : {-1., 1.}) {
        auto from = center, to = center;
        from[axis] -= span * direction;
        to[axis] += span * direction;
        const auto hit = HitAuthoredShapes(shapes,
            fixture.enemy.actor().rig().jointMatrices(), fixture.enemy.placement(),
            from, to, float(4 * span * span), 0x10);
        if (hit && hit->partIndex == shapeIndex)
          return {from, to};
      }
  throw std::runtime_error("Cannot isolate a current authored enemy capsule");
}
PlayerMeleeEvent knifeAt(const Fixture &fixture, unsigned part, unsigned serial,
                         unsigned flag = 0) {
  PlayerMeleeEvent event;
  event.kind = PlayerMeleeKind::Knife;
  event.serial = serial;
  event.flag = flag;
  event.rays.push_back(posedPartRay(fixture, part));
  return event;
}
std::uint16_t advancedRandom(std::uint16_t state, unsigned count) {
  const auto previous = RndState();
  RndInit(state);
  for (unsigned i = 0; i < count; ++i)
    (void)Rnd();
  const auto result = RndState();
  RndInit(previous);
  return result;
}
void enemyKnifeDamageAndPending(Assets assets) {
  for (unsigned part : {0u, 1u}) {
    Fixture fixture(assets, true);
    const auto event = knifeAt(fixture, part, 1, 12);
    const auto beforeRandom = fixture.enemy.randomState();
    fixture.enemy.queueMelee(event, fixture.player.feet());
    check(fixture.enemy.hp() == 1000 && fixture.enemy.randomState() == beforeRandom,
          "Solved knife event is queued without applying damage or consuming RNG");
    // The same event can be observed by several render passes before EmMgr.
    fixture.enemy.queueMelee(event, fixture.player.feet());
    fixture.tick({}, true);
    const unsigned damage = part == 0 ? 120 : 100;
    check(fixture.enemy.hp() == 1000 - damage &&
              fixture.enemy.lastDamage() == damage && fixture.enemy.knifeHits() == 1,
          "One pending knife hit applies source120 head/100 body; weak flag is not a damage multiplier");
    check(fixture.enemy.shots() == 0 && fixture.enemy.hits() == 0,
          "Knife hit does not enter handgun projectile counters");
    check(fixture.enemy.lastDamageSource() == EnemyEncounter::DamageSource::Knife,
          "Knife damage status records its source independently of projectile part labels");
    if (part == 0) {
      check(fixture.enemy.randomState() == advancedRandom(beforeRandom, 3),
            "Head knife consumes two LifeDown bytes before its directional head reaction RNG");
      check(fixture.enemy.actor().motion() == EnemyMotion::HitHead ||
                fixture.enemy.actor().motion() == EnemyMotion::HeadLeft ||
                fixture.enemy.actor().motion() == EnemyMotion::HeadRight,
            "Knife head hit selects an original head reaction");
    } else {
      const auto expected = advancedRandom(beforeRandom, 2);
      const auto previous = RndState();
      RndInit(expected);
      const bool flinch = Rnd() % 3 != 0;
      RndInit(previous);
      check(fixture.enemy.randomState() == advancedRandom(beforeRandom, flinch ? 4 : 3),
            "Body knife RNG order is HP, no-flinch, then frontal body selection if needed");
      check(flinch ? (fixture.enemy.actor().motion() == EnemyMotion::HitBody ||
                      fixture.enemy.actor().motion() == EnemyMotion::BodyAlt)
                   : (fixture.enemy.actor().motion() == EnemyMotion::Idle ||
                      fixture.enemy.actor().motion() == EnemyMotion::Walk),
            "Body knife one-in-three no-flinch decision follows original post-HP RNG");
    }
    // em10DmCk applies m_Timer=0x11 after the selected damage handler,
    // including a body handler which elected not to replace the motion.
    for (unsigned i = 0; i < 17; ++i) {
      fixture.enemy.queueMelee(knifeAt(fixture, part, 2 + i), fixture.player.feet());
      fixture.tick({}, true);
      check(fixture.enemy.hp() == 1000 - damage && fixture.enemy.knifeHits() == 1,
            "Source knife damage timer17 rejects every intervening sweep");
    }
    fixture.enemy.queueMelee(knifeAt(fixture, part, 1, 4), fixture.player.feet());
    fixture.tick({}, true);
    check(fixture.enemy.hp() == 1000 - 2 * damage && fixture.enemy.knifeHits() == 2,
          "Timer expiry permits a posed knife event; serial is not an invented swing immunity");
    ammoUntouched(fixture);
    finite(fixture);
  }
}
void enemyMissAndWall(Assets assets) {
  Fixture miss(assets, true);
  PlayerMeleeEvent event;
  event.kind = PlayerMeleeKind::Knife;
  event.serial = 1;
  event.rays.push_back({{50000, 1800, 49000}, {50000, 1800, 51000}});
  const auto before = miss.enemy.randomState();
  miss.enemy.queueMelee(event, miss.player.feet());
  miss.tick({}, true);
  check(miss.enemy.hp() == 1000 && miss.enemy.knifeHits() == 0 &&
            miss.enemy.randomState() == before,
        "Knife miss creates no damage, hit count or damage RNG");

  Fixture wall(assets, true, true);
  const auto center = enemyPartCenter(wall, 0);
  auto from = center, to = center;
  from[2] -= 2000;
  to[2] += 500;
  check(bool(wall.effects.line(from, to, SatSubset::All, 0, 0x00400000)),
        "Knife obstruction fixture has a real EAT crossing");
  check(bool(HitAuthoredShapes(NormalGanadoHitShapes(),
             wall.enemy.actor().rig().jointMatrices(), wall.enemy.placement(),
             from, to, 2500 * 2500, 0x10)),
        "Without the EAT wall, the same ray reaches an authored actor capsule");
  event.rays = {{from, to}};
  wall.enemy.queueMelee(event, wall.player.feet());
  wall.tick({}, true);
  check(wall.enemy.hp() == 1000 && wall.enemy.knifeHits() == 0,
        "An intervening EAT wall prevents the knife hit");

  Fixture scenario(assets, true, true);
  auto sphereCenter = enemyPartCenter(scenario, 0);
  sphereCenter[2] -= 1000;
  const auto sphereHit = SphereAuthoredShapes(NormalGanadoHitShapes(),
      scenario.enemy.actor().rig().jointMatrices(), scenario.enemy.placement(),
      sphereCenter);
  check(sphereHit && MeleeSphereNeedsScenarioCheck(*sphereHit),
        "Kick filter fixture reaches an actor beyond the source30-percent EAT threshold");
  PlayerMeleeEvent kick;
  kick.kind = PlayerMeleeKind::Kick;
  kick.serial = 1;
  kick.flag = 0xA;
  kick.radius = 1200;
  kick.centers = {sphereCenter, sphereCenter};
  scenario.enemy.queueMelee(kick, scenario.player.feet());
  scenario.tick({}, true);
  check(scenario.enemy.hp() == 1000 && scenario.enemy.kickHits() == 0,
        "An ordinary EAT wall blocks a kick sphere scenario ray");
  scenario.sat.polygons.at(2).attributes = 0x00400000;
  scenario.sat.polygons.at(3).attributes = 0x00400000;
  auto markedHead = enemyPartCenter(scenario, 0);
  from = markedHead;
  to = markedHead;
  from[2] -= 2000;
  to[2] += 500;
  event.rays = {{from, to}};
  scenario.enemy.queueMelee(event, scenario.player.feet());
  scenario.tick({}, true);
  check(scenario.enemy.hp() == 1000 && scenario.enemy.knifeHits() == 0,
        "Knife uses source EAT ignore-mask0 and retains a0x400000 wall");
  sphereCenter = enemyPartCenter(scenario, 0);
  sphereCenter[2] -= 1000;
  kick.centers = {sphereCenter, sphereCenter};
  scenario.enemy.queueMelee(kick, scenario.player.feet());
  scenario.tick({}, true);
  check(scenario.enemy.hp() < 1000 && scenario.enemy.kickHits() == 1,
        "Kick uses source EAT ignore-mask0x400000 and skips that marked wall");
}
void enemyKickSpheresAndWake(Assets assets) {
  for (unsigned part : {0u, 1u}) {
    Fixture fixture(assets, true);
    PlayerMeleeEvent event;
    event.kind = PlayerMeleeKind::Kick;
    event.serial = 1;
    event.flag = 0xA;
    event.radius = 1200;
    event.centers = {enemyPartCenter(fixture, part), enemyPartCenter(fixture, part)};
    fixture.enemy.queueMelee(event, fixture.player.feet());
    check(fixture.enemy.hp() == 1000, "Kick damage waits for next enemy tick");
    fixture.tick({}, true);
    const unsigned damage = part == 0 ? 240 : 200;
    check(fixture.enemy.hp() == 1000 - damage &&
              fixture.enemy.lastDamage() == damage && fixture.enemy.kickHits() == 1,
          "Two overlapping source kick spheres apply one120%-head/200-body hit");
    check(fixture.enemy.lastDamageSource() == EnemyEncounter::DamageSource::Kick,
          "Kick source is retained in diagnostic damage provenance");
    bool sawDown = false, sawWake = false;
    for (unsigned i = 0; i < 2000; ++i) {
      fixture.tick({}, true);
      const auto motion = fixture.enemy.actor().motion();
      sawDown |= motion == EnemyMotion::DownIdle;
      sawWake |= motion == EnemyMotion::Wake || motion == EnemyMotion::WakeFaceDown;
      if (sawDown && sawWake &&
          (motion == EnemyMotion::Idle || motion == EnemyMotion::Walk))
        break;
    }
    check(sawDown && sawWake && fixture.enemy.alive() &&
              fixture.enemy.hp() == 1000 - damage,
          "Nonfatal original kick fall reaches down wait, wake and living locomotion");
    ammoUntouched(fixture);
    finite(fixture);
  }
}
void kickPromptGatesAndAction(Assets assets) {
  Fixture fixture(assets, true);
  check(!fixture.enemy.kickAvailable(fixture.player),
        "A healthy walking/idle actor does not offer a kick prompt");
  fixture.enemy.queueMelee(knifeAt(fixture, 0, 1), fixture.player.feet());
  fixture.tick({}, true);
  const auto feet = fixture.enemy.feet();
  auto place = [&](double x, double y, double z, float heading = 0) {
    fixture.player.setPlacement({feet[0] + x, feet[1] + y, feet[2] + z}, heading);
  };
  place(0, 700, -1500);
  check(fixture.enemy.kickAvailable(fixture.player),
        "Kick allows exact1500 XZ range and700 height; diagonal3D length is irrelevant");
  place(0, 700.01, -1500);
  check(!fixture.enemy.kickAvailable(fixture.player), "Kick rejects height above700");
  place(0, 0, -1500.01);
  check(!fixture.enemy.kickAvailable(fixture.player), "Kick rejects XZ range above1500");
  place(0, 0, -1400, 1.047f);
  check(fixture.enemy.kickAvailable(fixture.player), "Kick allows facing just below60 degrees");
  place(0, 0, -1400, 1.048f);
  check(!fixture.enemy.kickAvailable(fixture.player), "Kick rejects facing above60 degrees");
  fixture.place({feet[0], feet[1], feet[2] - 900}, 0);
  PlayerActionInput action;
  action.interactPressed = true;
  fixture.tick(action, true);
  check(fixture.player.action() == PlayerAction::Kick,
        "Fresh universal action edge consumes the valid front-head kick prompt");
  unsigned events{};
  for (unsigned i = 0; i < 200; ++i) {
    fixture.tick({}, true);
    events += bool(fixture.player.lastMeleeEvent());
    if (fixture.player.action() != PlayerAction::Kick)
      break;
  }
  check(events == 1 && fixture.enemy.kickHits() == 1 &&
            fixture.enemy.hp() < 880 && fixture.enemy.hp() >= 640,
        "Real stepPlayer kick queues its posed event once and applies it on the next enemy tick");
  ammoUntouched(fixture);

  Fixture wall(assets, true, true);
  wall.enemy.queueMelee(knifeAt(wall, 0, 1), wall.player.feet());
  wall.tick({}, true);
  const auto enemyFeet = wall.enemy.feet();
  wall.player.setPlacement({enemyFeet[0], enemyFeet[1], enemyFeet[2] - 1400}, 0);
  check(!wall.enemy.kickAvailable(wall.player),
        "Original SAT wall blocks a kick prompt within otherwise valid range");
}
void prepareHeadPrompt(Fixture &fixture) {
  fixture.enemy.queueMelee(knifeAt(fixture, 0, 1), fixture.player.feet());
  fixture.tick({}, true);
  const auto feet = fixture.enemy.feet();
  fixture.place({feet[0], feet[1], feet[2] - 900}, 0);
  check(fixture.enemy.kickAvailable(fixture.player), "Prepared original head kick prompt");
}
void kickLatchedInputAndPause(Assets assets) {
  Fixture action(assets, true);
  prepareHeadPrompt(action);
  PlayerActionInput edge;
  edge.interactPressed = true;
  action.frame(0, edge, true);
  action.frame(0, {}, true);
  check(action.player.action() != PlayerAction::Kick,
        "A contextual action cannot start on a zero-dt render pass");
  action.tick({}, true);
  check(action.player.action() == PlayerAction::Kick,
        "Short contextual action edge waits for the next original tick");
  ammoUntouched(action);

  Fixture paused(assets, true);
  prepareHeadPrompt(paused);
  paused.frame(0, edge, true);
  paused.enemy.suspendInput();
  paused.player.suspendClock();
  const auto before = snapshot(paused);
  for (unsigned i = 0; i < 60; ++i)
    paused.frame(0, {}, true);
  check(snapshot(paused) == before,
        "Pause freezes coupled prompt, HP, pose and RNG");
  paused.tick({}, true);
  check(paused.player.action() != PlayerAction::Kick && paused.enemy.kickHits() == 0,
        "Pause drops a contextual kick waiting at a render boundary");

  Fixture trigger(assets, true);
  prepareHeadPrompt(trigger);
  edge = {};
  edge.aimHeld = true;
  edge.fireHeld = true;
  edge.firePressed = true;
  trigger.frame(0, edge, true);
  edge.firePressed = false;
  trigger.tick(edge, true);
  check(trigger.player.action() == PlayerAction::Kick,
        "Fresh universal RT edge while aiming chooses the contextual kick");
  ammoUntouched(trigger);
}
void realKnifeEventToEnemy(Assets assets) {
  bool connected = false;
  for (double distance : {650., 800., 950.}) {
    for (double angle : {0., .5, 1.5707963267948966, 3.141592653589793,
                         -1.5707963267948966}) {
    Fixture fixture(assets, true);
    fixture.readyKnife();
    fixture.enemy.queueMelee(knifeAt(fixture, 0, 1), fixture.player.feet());
    PlayerActionInput input;
    input.knifeHeld = true;
    fixture.tick(input, true);
    for (unsigned i = 0; i < 17; ++i)
      fixture.tick(input, true);
    const auto feet = fixture.enemy.feet();
    fixture.place({feet[0] + std::sin(angle) * distance, feet[1],
                   feet[2] - std::cos(angle) * distance}, 0);
    // Placement resets actions. Redraw the real knife while the already
    // selected head reaction isolates this test from an initial catch.
    fixture.readyKnife();
    input.firePressed = true;
    fixture.tick(input, true);
    input.firePressed = false;
    const unsigned initialHits = fixture.enemy.knifeHits();
    unsigned previousHp = fixture.enemy.hp();
    bool queued = false, applied = false;
    for (unsigned i = 0; i < 18; ++i) {
      const auto hits = fixture.enemy.knifeHits();
      fixture.tick(input, true);
      if (fixture.enemy.knifeHits() > hits) {
        check(bool(fixture.player.lastMeleeEvent()) &&
                  fixture.player.lastMeleeEvent()->kind == PlayerMeleeKind::Knife &&
                  fixture.enemy.hp() == previousHp,
              "Real solved slash rays enqueue damage after the player pose, not before it");
        queued = true;
      }
      if (fixture.enemy.hp() < previousHp) {
        check(queued && fixture.enemy.lastDamageSource() ==
                           EnemyEncounter::DamageSource::Knife,
              "Queued real knife ray applies on the following enemy phase");
        applied = true;
      }
      previousHp = fixture.enemy.hp();
    }
    if (applied) {
      check(fixture.enemy.knifeHits() == initialHits + 1,
            "Five slash hit frames accept one hit because source damage timer17 is active");
      connected = true;
    }
    ammoUntouched(fixture);
    if (connected)
      break;
    }
    if (connected)
      break;
  }
  check(connected, "A real original knife slash reaches original posed enemy capsules");
}
std::vector<Snapshot> coupledTimeline(Assets assets, unsigned rendersPerTick) {
  Fixture fixture(assets, true);
  prepareHeadPrompt(fixture);
  std::vector<Snapshot> result;
  for (unsigned step = 0; step < 600; ++step) {
    for (unsigned render = 0; render < rendersPerTick; ++render) {
      PlayerActionInput input;
      input.interactPressed = step == 0 && render == 0;
      input.knifeHeld = step >= 100 && step < 180;
      input.firePressed = step == 130 && render == 0;
      if (fixture.player.action() == PlayerAction::Grabbed &&
          step % 3 == 0 && render == 0)
        input.resistPressed = Bottom;
      fixture.frame(1. / (30 * rendersPerTick), input, true);
    }
    result.push_back(snapshot(fixture));
  }
  finite(fixture);
  check(fixture.enemy.kickHits() == 1,
        "Coupled FPS timeline includes an actual kick collision");
  ammoUntouched(fixture);
  return result;
}
void coupledRates(Assets assets) {
  const auto thirty = coupledTimeline(assets, 1);
  const auto sixty = coupledTimeline(assets, 2);
  check(thirty.size() == sixty.size(), "Equal coupled ticks at30 and60 render FPS");
  for (std::size_t i = 0; i < thirty.size(); ++i)
    check(thirty[i] == sixty[i],
          "Coupled knife/kick/fall/wake/grab HP, poses and RNG match at30/60FPS");
}
void protectedHeadDamage(Assets assets) {
  for (unsigned part : {0u, 1u}) {
    Fixture fixture(assets, true);
    const auto ray = posedPartRay(fixture, 0);
    PlayerShot shot;
    shot.serial = 1;
    shot.from = ray.first;
    shot.to = ray.second;
    fixture.enemy.tick(fixture.enemy.feet(), &shot);
    fixture.tick({}, true);
    check(fixture.enemy.hits() == 1 && fixture.enemy.alive() &&
              fixture.enemy.lastDamageSource() == EnemyEncounter::DamageSource::Bullet,
          "Protected-head fixture starts with a real posed bullet head stagger");
    fixture.tick({}, true); // Source bullet damage timer1 expires.
    const auto beforeHp = fixture.enemy.hp();
    const auto beforeRandom = fixture.enemy.randomState();
    const auto beforeMotion = fixture.enemy.actor().motion();
    const auto beforeFrame = fixture.enemy.actor().rig().curveFrame();
    fixture.enemy.queueMelee(knifeAt(fixture, part, 1), fixture.player.feet());
    fixture.tick({}, true);
    check(fixture.enemy.hp() == beforeHp - (part == 0 ? 120 : 100),
          "Head Be_flg8 protection accepts new knife HP damage");
    check(fixture.enemy.randomState() == advancedRandom(beforeRandom, 2),
          "Protected head/body knife only consumes the two LifeDown bytes");
    check(fixture.enemy.actor().motion() == beforeMotion &&
              fixture.enemy.actor().rig().curveFrame() > beforeFrame,
          "Head Be_flg8 avoids a new MotionSet and continues the existing stagger");
    ammoUntouched(fixture);
  }
}
void kickIntoDown(Fixture &fixture) {
  PlayerMeleeEvent kick;
  kick.kind = PlayerMeleeKind::Kick;
  kick.serial = 1;
  kick.flag = 0xA;
  kick.radius = 1200;
  kick.centers = {enemyPartCenter(fixture, 1), enemyPartCenter(fixture, 1)};
  fixture.enemy.queueMelee(kick, fixture.player.feet());
  fixture.tick({}, true);
  check(fixture.enemy.lastDamage() == 200, "Down fixture uses actual body kick damage200");
  for (unsigned i = 0; i < 500 &&
       fixture.enemy.actor().motion() != EnemyMotion::DownIdle; ++i)
    fixture.tick({}, true);
  check(fixture.enemy.actor().motion() == EnemyMotion::DownIdle &&
            fixture.enemy.alive(),
        "Original kick reaches a living down actor");
  fixture.tick({}, true);
  fixture.tick({}, true); // Finish the blow phase's timer2; initialize wait RNG.
}
void protectedRearBodyDamage(Assets assets) {
  Fixture fixture(assets, true);
  const auto frontRay = posedPartRay(fixture, 1);
  PlayerShot shot;
  shot.serial = 1;
  shot.from = frontRay.second;
  shot.to = frontRay.first;
  fixture.enemy.tick(fixture.enemy.feet(), &shot);
  const auto beforeRandom = fixture.enemy.randomState();
  fixture.tick({}, true);
  check(fixture.enemy.actor().motion() == EnemyMotion::BodyRear &&
            fixture.enemy.lastDamage() == 135 &&
            fixture.enemy.randomState() == advancedRandom(beforeRandom, 2),
        "Rear body bullet uses original65/66 with two HP bytes and no frontal selection RNG");
  fixture.tick({}, true);
  const auto beforeHp = fixture.enemy.hp();
  const auto beforeFrame = fixture.enemy.actor().rig().curveFrame();
  const auto guardedRandom = fixture.enemy.randomState();
  fixture.enemy.queueMelee(knifeAt(fixture, 0, 1), fixture.player.feet());
  fixture.tick({}, true);
  check(fixture.enemy.hp() == beforeHp - 120 &&
            fixture.enemy.randomState() == advancedRandom(guardedRandom, 2),
        "Rear body Be_flg8 protection accepts HP without new head reaction RNG");
  check(fixture.enemy.actor().motion() == EnemyMotion::BodyRear &&
            fixture.enemy.actor().rig().curveFrame() > beforeFrame,
        "Protected rear body motion continues instead of restarting as a head stagger");
  ammoUntouched(fixture);
}
void downReactionGuardPriority(Assets assets) {
  Fixture fixture(assets, true);
  kickIntoDown(fixture);
  const auto ray = posedPartRay(fixture, 1);
  PlayerShot shot;
  shot.serial = 1;
  shot.from = ray.first;
  shot.to = ray.second;
  fixture.enemy.tick(fixture.player.feet(), &shot);
  fixture.tick({}, true);
  check(fixture.enemy.actor().motion() == EnemyMotion::DownHitFaceUp ||
            fixture.enemy.actor().motion() == EnemyMotion::DownHitFaceDown,
        "A lying bullet establishes the protected original DownHit handler");
  fixture.tick({}, true); // Bullet timer1 expires while DownHit is still active.
  const auto beforeHp = fixture.enemy.hp();
  const auto beforeRandom = fixture.enemy.randomState();
  const auto beforeMotion = fixture.enemy.actor().motion();
  const auto beforeFrame = fixture.enemy.actor().rig().curveFrame();
  fixture.enemy.queueMelee(knifeAt(fixture, 0, 1), fixture.player.feet());
  fixture.tick({}, true);
  check(fixture.enemy.hp() == beforeHp - 120 &&
            fixture.enemy.randomState() == advancedRandom(beforeRandom, 2),
        "Protected lying head knife accepts HP and only LifeDown RNG");
  check(fixture.enemy.actor().motion() == beforeMotion &&
            fixture.enemy.actor().rig().curveFrame() > beforeFrame,
        "Original Be_flg8 priority precedes down flag0x10 and does not reset DownHit");
  ammoUntouched(fixture);
}
void downKnifeAndDeath(Assets assets) {
  for (unsigned part : {0u, 1u}) {
    Fixture fixture(assets, true);
    kickIntoDown(fixture);
    const auto beforeHp = fixture.enemy.hp();
    const auto beforeRandom = fixture.enemy.randomState();
    const auto beforeMirror = fixture.enemy.actor().mirrored();
    fixture.enemy.queueMelee(knifeAt(fixture, part, 1), fixture.player.feet());
    fixture.tick({}, true);
    check(fixture.enemy.hp() == beforeHp - (part == 0 ? 120 : 100) &&
              fixture.enemy.randomState() == advancedRandom(beforeRandom, 2),
          "Lying knife damage precedes head/body branches and consumes only two HP RNG bytes");
    check((fixture.enemy.actor().motion() == EnemyMotion::DownHitFaceUp ||
           fixture.enemy.actor().motion() == EnemyMotion::DownHitFaceDown) &&
              fixture.enemy.actor().mirrored() == beforeMirror,
          "Lying knife selects authored DownHit and inherits fall mirror");
    bool woke = false;
    for (unsigned i = 0; i < 200; ++i) {
      fixture.tick({}, true);
      const auto motion = fixture.enemy.actor().motion();
      if (motion == EnemyMotion::Wake || motion == EnemyMotion::WakeFaceDown) {
        woke = true;
        break;
      }
      check(motion != EnemyMotion::DownIdle,
            "DownHit end proceeds to wake without another randomized down wait");
    }
    check(woke, "Original DownHit reaches its face-up/down wake");
    ammoUntouched(fixture);
    finite(fixture);
  }

  Fixture fatal(assets, true);
  for (unsigned hit = 0; hit < 7; ++hit) {
    fatal.enemy.queueMelee(knifeAt(fatal, 1, hit + 1), fatal.player.feet());
    fatal.tick({}, true);
    for (unsigned i = 0; i < 17; ++i)
      fatal.tick({}, true);
  }
  check(fatal.enemy.hp() == 300, "Fatal down fixture lowers HP with seven actual body knife hits");
  kickIntoDown(fatal);
  check(fatal.enemy.hp() == 100, "A real body kick leaves100 HP for fatal down knife");
  const auto beforeRandom = fatal.enemy.randomState();
  fatal.enemy.queueMelee(knifeAt(fatal, 0, 8), fatal.player.feet());
  fatal.tick({}, true);
  check(fatal.enemy.hp() == 0 && fatal.enemy.lastDamage() == 100 &&
            fatal.enemy.randomState() == advancedRandom(beforeRandom, 2),
        "Fatal lying head knife clamps120 damage to remaining100 HP with no reaction RNG");
  check((fatal.enemy.actor().motion() == EnemyMotion::DownDeathFaceUp ||
         fatal.enemy.actor().motion() == EnemyMotion::DownDeathFaceDown) &&
            !fatal.enemy.actor().mirrored(),
        "Fatal down knife uses original unmirrored DownDeath");
  for (unsigned i = 0; i < 200 && !fatal.enemy.deadSettled(); ++i)
    fatal.tick({}, true);
  check(fatal.enemy.deadSettled() && !fatal.enemy.alive(),
        "Ended DownDeath remains a settled corpse");
  for (unsigned i = 0; i < 200; ++i)
    fatal.tick({}, true);
  check(fatal.enemy.deadSettled() && fatal.enemy.hp() == 0 &&
            (fatal.enemy.actor().motion() == EnemyMotion::DownDeathFaceUp ||
             fatal.enemy.actor().motion() == EnemyMotion::DownDeathFaceDown),
        "Fatal down actor cannot wake or return to approach");
  ammoUntouched(fatal);
  finite(fatal);
}
void sourceContracts() {
  check(NormalGanadoHitShapes().front().partsNo == 5,
        "Source Ganado head uses part5");
  check(PlayerActionLabel(PlayerAction::KnifeDraw) != nullptr &&
            PlayerActionLabel(PlayerAction::KnifeReady) != nullptr &&
            PlayerActionLabel(PlayerAction::KnifeSlash) != nullptr &&
            PlayerActionLabel(PlayerAction::KnifeHolster) != nullptr &&
            PlayerActionLabel(PlayerAction::Kick) != nullptr,
        "All native melee states have a diagnostic label");
}
} // namespace

int main(int argc, char **argv) {
  try {
    sourceContracts();
    if (argc == 1) {
      std::cout << "Melee source contracts PASS (" << checks
                << " checks); actor runtime requires PL00 WEP01 EM12 paths\n";
      return 0;
    }
    if (argc != 4)
      throw std::runtime_error("Usage: melee_encounter_tests PL00.drs WEP01.drs EM12.drs");
    Assets assets{argv[1], argv[2], argv[3]};
    knifePoseAndLifecycle(assets);
    knifeLatchedFireAndGrab(assets);
    kickPoseAndLifecycle(assets);
    actorRates(assets);
    enemyKnifeDamageAndPending(assets);
    enemyMissAndWall(assets);
    enemyKickSpheresAndWake(assets);
    kickPromptGatesAndAction(assets);
    kickLatchedInputAndPause(assets);
    realKnifeEventToEnemy(assets);
    coupledRates(assets);
    protectedHeadDamage(assets);
    protectedRearBodyDamage(assets);
    downReactionGuardPriority(assets);
    downKnifeAndDeath(assets);
    std::cout << "Melee actor runtime PASS (" << checks << " checks)\n";
  } catch (const std::exception &error) {
    std::cerr << "Melee actor runtime FAIL: " << error.what() << '\n';
    return 1;
  }
}
