#include "enemy_encounter.h"
#include "grab_rules.h"
#include "input.h"
#include "ports/rnd.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace re4;
unsigned checks{};
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
}

// Collision is synthetic; actor archives are supplied by the caller. Use the
// original PL00/WEP01/EM12 for a game-data run, or clearly identify generated
// fixtures as a synthetic state-machine test. The flat SAT avoids route/camera
// assumptions in an audit of the coupled combat state machine.
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
  for (float value :
       {-100000.f, 830.f, -100000.f, 100000.f, 830.f, -100000.f, 100000.f,
        830.f, 100000.f, -100000.f, 830.f, 100000.f, 0.f, 1.f, 0.f}) {
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
EnemySpawn authoredSpawn() {
  // Frozen original etc/emleon00.esl record0x25. No replacement HP,
  // equipment, hostile flags or invented actor position.
  EnemySpawn spawn;
  spawn.listIndex = 0x25;
  spawn.recordOffset = 0x25 * 32;
  spawn.id = 0x12;
  spawn.type = 0;
  spawn.setNo = 0x1b;
  spawn.character = 1;
  spawn.flags = 0x24;
  spawn.hp = 1000;
  spawn.room = 0x100;
  spawn.position = {-83950, 830, -32960};
  spawn.heading = 0.0005752427954571154;
  return spawn;
}
struct Assets {
  const char *player;
  const char *weapon;
  const char *enemy;
};
struct Fixture {
  Assets assets;
  Sat sat = flatFloor();
  SatCollision world{sat};
  SatCollision effects{sat, SatQueryMode::Effect};
  EnemySpawn spawn = authoredSpawn();
  EnemyEncounter enemy;
  LeonPreview player;
  WalkProbe probe{world};
  explicit Fixture(Assets paths)
      : assets(paths), enemy(world, effects, spawn, paths.enemy),
        player(paths.player, paths.weapon, paths.enemy) {
    // A front-range player setup exercises Catch directly. It deliberately
    // avoids depending on a synthetic or original walk speed before the
    // coupled state machine begins; the enemy's ESL position is unchanged.
    const CollisionVec front{spawn.position[0] + std::sin(spawn.heading) * 800,
                             spawn.position[1],
                             spawn.position[2] + std::cos(spawn.heading) * 800};
    check(probe.place(front), "Test player SAT placement");
    const auto feet = probe.position();
    const float heading = static_cast<float>(
        std::atan2(spawn.position[0] - feet[0], spawn.position[2] - feet[2]));
    player.setPlacement(feet, heading);
    check(player.hasGrabMotions() && player.life() == 1200,
          "Original paired Leon motions and life");
  }
  void frame(double seconds, PlayerActionInput input = {}) {
    enemy.stepPlayer(player, probe, 0, 0, false, false, seconds, input);
  }
  void tick(PlayerActionInput input = {}) { frame(1. / 30, input); }
  void waitForHold() {
    for (unsigned ticks = 0; ticks < 1200 && !enemy.grabs(); ++ticks)
      tick();
    check(enemy.grabs() == 1 &&
              enemy.actor().motion() == EnemyMotion::NeckHang &&
              player.action() == PlayerAction::Grabbed,
          "Original catch SEQ reaches the coupled front hold");
    check(player.life() == 1194, "First original hold tick applies six damage");
    check(!player.weaponVisible(), "Weapon hidden throughout hold");
  }
};
auto snapshot(const Fixture &fixture) {
  const auto &player = fixture.player;
  const auto &enemy = fixture.enemy;
  return std::make_tuple(
      player.life(), player.action(), player.state(), player.damageWait(),
      player.magazine(), player.reserve(), player.shots(), player.feet(),
      player.heading(), player.rig().curveFrame(), player.rig().sequenceFrame(),
      player.rig().status().state, player.rig().jointMatrices(), enemy.hp(),
      enemy.actor().motion(), enemy.deadSettled(), enemy.grabs(),
      enemy.escapes(), enemy.resistance(), enemy.randomState(), enemy.feet(),
      enemy.heading(), enemy.scale(), enemy.actor().rig().curveFrame(),
      enemy.actor().rig().sequenceFrame(), enemy.actor().rig().status().state,
      enemy.actor().rig().jointMatrices(), fixture.probe.position());
}
using Snapshot = decltype(snapshot(std::declval<const Fixture &>()));
void finite(const Fixture &fixture) {
  for (const auto &point : {fixture.player.feet(), fixture.enemy.feet()})
    for (double value : point)
      check(std::isfinite(value), "Finite coupled actor placement");
  for (const auto *rig : {&fixture.player.rig(), &fixture.enemy.actor().rig()})
    for (const auto &joint : rig->jointMatrices())
      for (float value : joint)
        check(std::isfinite(value), "Finite paired original bone pose");
  check(fixture.player.life() <= LeonPreview::MaxLife &&
            fixture.enemy.hp() <= fixture.enemy.maxHp(),
        "Combat life remains bounded");
}
void analogResistance(Assets assets) {
  Fixture fixture(assets);
  fixture.waitForHold();
  fixture.player.suspendClock();
  InputBuffer buffer;
  RawInput raw;
  raw.lt = 1;
  for (float x : {-.4f, .4f, -.4f, .4f, -.4f, .4f}) {
    raw.lx = x;
    buffer.submit(raw);
  }
  PlayerActionInput input;
  input.aimHeld = true;
  input.resistancePresses = buffer.consume().resistancePresses;
  check(input.resistancePresses == 12,
        "Six physical LS flicks reach combat input");
  fixture.frame(0, input);
  fixture.tick();
  check(fixture.enemy.resistance() == 12,
        "Latched analog resistance survives held L2 neutral gate and "
        "fractional frame");
  fixture.tick();
  check(fixture.enemy.resistance() == 12,
        "Analog counter consumed once per combat tick");
}
void freezeAndEdges(Assets assets) {
  Fixture fixture(assets);
  fixture.waitForHold();
  const auto held = snapshot(fixture);
  const auto externalRandom = RndState();
  for (unsigned frame = 0; frame < 120; ++frame)
    fixture.frame(0);
  check(snapshot(fixture) == held && RndState() == externalRandom,
        "Paused/context zero-dt hold preserves both actors, HP, counters and "
        "RNG");

  InputBuffer buffer;
  RawInput raw;
  raw.buttons = Bottom;
  buffer.submit(raw);
  raw.buttons = 0;
  buffer.submit(raw); // Short A press survives the render boundary.
  PlayerActionInput edge;
  edge.resistPressed = buffer.consume().pressed;
  check(GrabPressCount(edge.resistPressed) == 1, "Latched real A edge");
  fixture.frame(0, edge);
  fixture.frame(0); // Edge waits for the next original 30-Hz tick.
  check(fixture.enemy.resistance() == 0,
        "No resistance update without simulation");
  fixture.tick();
  check(fixture.enemy.resistance() == 1, "Queued resistance is counted once");
  fixture.tick();
  check(fixture.enemy.resistance() == 1,
        "Resistance does not repeat across ticks");

  fixture.frame(0, edge);
  fixture.player.suspendClock();
  const auto suspended = snapshot(fixture);
  for (unsigned frame = 0; frame < 60; ++frame)
    fixture.frame(0);
  check(snapshot(fixture) == suspended,
        "Suspend freezes original paired phase");
  fixture.tick();
  check(fixture.enemy.resistance() == 1,
        "Suspend discards stale resistance edges");

  // Contact is removed only while the pair shares the catch priority. A small
  // inward displacement must remain possible at the authored hold spacing.
  const auto before = fixture.probe.position();
  const auto enemy = fixture.enemy.feet();
  const double distance =
      std::hypot(enemy[0] - before[0], enemy[2] - before[2]);
  check(distance > 1, "Distinct paired root positions");
  fixture.probe.moveDelta({(enemy[0] - before[0]) / distance * 25, 0,
                           (enemy[2] - before[2]) / distance * 25});
  check(std::abs(std::hypot(fixture.probe.position()[0] - before[0],
                            fixture.probe.position()[2] - before[2]) -
                 25) < .001,
        "Upright contact cannot block the coupled catch adjustment");
  finite(fixture);
}
void failedRelease(Assets assets) {
  Fixture fixture(assets);
  fixture.waitForHold();
  unsigned ticks{};
  bool seenRelease = false, seenRecovery = false;
  for (; ticks < 500 && fixture.player.action() != PlayerAction::Footwork;
       ++ticks) {
    fixture.tick();
    const auto action = fixture.player.action();
    if (action == PlayerAction::GrabRelease && !seenRelease) {
      seenRelease = true;
      check(fixture.player.life() == 990,
            "Thirty-five original hold damage ticks");
    }
    if (action == PlayerAction::Recover && !seenRecovery) {
      seenRecovery = true;
      check(fixture.player.life() == 810,
            "Failed resistance applies original frame37 damage exactly once");
      check(!fixture.player.weaponVisible(),
            "Weapon hidden during original get-up");
      const auto before = snapshot(fixture);
      fixture.player.suspendClock();
      for (unsigned frame = 0; frame < 60; ++frame)
        fixture.frame(0);
      check(snapshot(fixture) == before,
            "Get-up pose and combat life survive suspension");
    }
  }
  check(ticks < 500 && seenRelease && seenRecovery,
        "Failed catch completes release and looping-SEQ get-up");
  check(fixture.player.life() == 810 && fixture.player.damageWait() == 5 &&
            fixture.player.weaponVisible() && fixture.enemy.escapes() == 0,
        "Original get-up exits to live footwork with five protected ticks");
  check(fixture.player.magazine() == 10 && fixture.player.reserve() == 60 &&
            fixture.player.shots() == 0 && fixture.enemy.hp() == 1000,
        "Failed grip consumes no ammunition or enemy HP");
  const auto enemyFeet = fixture.enemy.feet();
  check(fixture.probe.place({enemyFeet[0] + 2000, enemyFeet[1], enemyFeet[2]}),
        "Post-grip contact placement");
  fixture.player.setPlacement(fixture.probe.position(), 0);
  fixture.frame(0); // Refresh contact without advancing enemy or player motion.
  fixture.probe.moveDelta({-4000, 0, 0});
  check(fixture.probe.position()[0] >= enemyFeet[0] +
                                           fixture.probe.config().radius +
                                           400 * fixture.enemy.scale() - .1,
        "Upright enemy contact is restored after the failed paired grip");
  finite(fixture);
}
void successfulRelease(Assets assets, unsigned presses) {
  Fixture fixture(assets);
  fixture.waitForHold();
  unsigned sent = 0, ticks = 0;
  bool seenThrow = false, seenDown = false, seenWake = false,
       repositioned = false;
  unsigned enemyHpAfterThrow = 0;
  for (; ticks < 700; ++ticks) {
    PlayerActionInput input;
    if (fixture.player.action() == PlayerAction::Grabbed && sent < presses) {
      input.resistPressed =
          Confirm; // A new edge on one distinct simulation tick.
      ++sent;
    }
    fixture.tick(input);
    const auto motion = fixture.enemy.actor().motion();
    if (motion == EnemyMotion::ThrowOff && !seenThrow) {
      seenThrow = true;
      enemyHpAfterThrow = fixture.enemy.hp();
      check(fixture.enemy.resistance() == presses,
            "All distinct resistance edges reach the release decision");
      check(fixture.enemy.escapes() == 1,
            "Original ten-edge resistance threshold");
      check(fixture.player.life() == 990,
            "Escape retains hold damage without frame37 loss");
      // Original rnd.cpp recurrence, seed4e19: three scale draws,35*3
      // unarmoured player damage draws, then two enemy LifeDownSet2 draws.
      // The source's second independent >20 if consumes one MORE draw even
      // when the preceding >30 branch already selected fatal damage.
      check(fixture.enemy.randomState() == (presses > 30 ? 0xfc3cu : 0xc078u),
            "Source escape RNG consumes the high-resistance roll even above30");
    }
    if (fixture.player.action() == PlayerAction::Footwork && !repositioned) {
      check(fixture.probe.place({50000, 830, 50000}),
            "Keep escaped player outside a second catch while auditing wake");
      fixture.player.setPlacement(fixture.probe.position(),
                                  fixture.player.heading());
      repositioned = true;
    }
    seenDown |= motion == EnemyMotion::DownIdle;
    seenWake |= motion == EnemyMotion::Wake;
    if (seenDown &&
        (!fixture.enemy.alive() || (seenWake && motion != EnemyMotion::Wake)))
      break;
  }
  check(ticks < 700 && sent == presses && seenThrow && seenDown && repositioned,
        "Successful catch reaches the original held ground pose");
  check(fixture.player.life() == 990 && fixture.enemy.escapes() == 1,
        "Escaping preserves the live player and one registered escape");
  if (presses == 10) {
    check(enemyHpAfterThrow == 900 && fixture.enemy.hp() == 900 && seenWake,
          "Ordinary escape costs enemy100HP and original wake restores "
          "locomotion");
  } else if (presses > 30) {
    check(enemyHpAfterThrow == 0 && !fixture.enemy.alive() &&
              fixture.enemy.deadSettled(),
          "Over-thirty resistance kills and freezes the thrown-off corpse");
    const auto corpse = snapshot(fixture);
    for (unsigned frame = 0; frame < 30; ++frame)
      fixture.tick();
    check(fixture.enemy.hp() == 0 && fixture.enemy.deadSettled() &&
              fixture.enemy.actor().rig().jointMatrices() ==
                  std::get<26>(corpse),
          "Thrown-off corpse does not wake or change its pose");
  }
  finite(fixture);
}
void fatalRelease(Assets assets) {
  Fixture fixture(assets);
  fixture.player.takeDamage(LeonPreview::MaxLife - 199, true);
  check(fixture.player.life() == 199, "Low-life original threshold fixture");
  // waitForHold's standard1200HP assertion deliberately does not apply here.
  for (unsigned ticks = 0; ticks < 1200 && !fixture.enemy.grabs(); ++ticks)
    fixture.tick();
  check(fixture.player.action() == PlayerAction::Grabbed &&
            fixture.player.life() == 193,
        "Low-life catch starts with source hold damage");
  unsigned ticks = 0, sent = 0;
  bool seenRelease = false;
  for (; ticks < 500 && fixture.player.action() != PlayerAction::Dead;
       ++ticks) {
    PlayerActionInput input;
    if (fixture.player.action() == PlayerAction::Grabbed && sent < 10) {
      input.resistPressed = Confirm;
      ++sent;
    }
    fixture.tick(input);
    seenRelease |= fixture.player.action() == PlayerAction::GrabRelease;
  }
  check(ticks < 500 && sent == 10 && seenRelease &&
            fixture.player.life() == 0 && fixture.enemy.escapes() == 0 &&
            !fixture.player.weaponVisible(),
        "Source low-life override defeats ten presses and reaches death");
  const auto pose = fixture.player.rig().jointMatrices();
  const auto feet = fixture.player.feet();
  for (unsigned frame = 0; frame < 120; ++frame)
    fixture.tick();
  check(fixture.player.action() == PlayerAction::Dead &&
            fixture.player.rig().jointMatrices() == pose &&
            fixture.player.feet() == feet && fixture.enemy.grabs() == 1,
        "Dead player freezes and cannot be caught again");
  finite(fixture);
}
std::vector<Snapshot> simulate(Assets assets, unsigned hz, bool escape) {
  Fixture fixture(assets);
  std::vector<Snapshot> result;
  bool relocated = false;
  unsigned sent = 0;
  for (unsigned frame = 0; frame < hz * 20; ++frame) {
    PlayerActionInput input;
    // Match physical edges to original ticks. At60Hz only the second render
    // of the pair supplies an edge; there is never duplicate per-render mash.
    if (escape && fixture.player.action() == PlayerAction::Grabbed &&
        sent < 10 && (frame + 1) % (hz / 30) == 0) {
      input.resistPressed = Confirm;
      ++sent;
    }
    fixture.frame(1. / hz, input);
    if (fixture.enemy.grabs() &&
        fixture.player.action() == PlayerAction::Footwork && !relocated) {
      check(fixture.probe.place({50000, 830, 50000}),
            "Rate-test return placement");
      fixture.player.setPlacement(fixture.probe.position(),
                                  fixture.player.heading());
      relocated = true;
    }
    if ((frame + 1) % (hz / 30) == 0)
      result.push_back(snapshot(fixture));
  }
  check(fixture.enemy.grabs() == 1 &&
            fixture.enemy.escapes() == unsigned(escape) &&
            fixture.player.life() == (escape ? 990u : 810u) && relocated,
        "Full original grab path finishes at both render rates");
  return result;
}
void rates(Assets assets) {
  for (bool escaped : {false, true}) {
    RndInit(0xabcd);
    const auto thirty = simulate(assets, 30, escaped);
    check(RndState() == 0xabcd, "Encounter does not leak its RNG stream");
    const auto sixty = simulate(assets, 60, escaped);
    check(RndState() == 0xabcd,
          "Sixty-Hz encounter restores external RNG stream");
    check(thirty.size() == 600 && thirty == sixty,
          "Every fail/escape actor pose, life, root, counter and RNG "
          "matches30/60Hz");
  }
}
void inventory(Assets assets) {
  Fixture fixture(assets);
  PlayerActionInput trigger;
  trigger.aimHeld = trigger.fireHeld = true;
  for (unsigned tick = 0; tick < 180 && fixture.player.shots() < 3; ++tick)
    fixture.player.step(fixture.probe, 0, 0, false, false, 1. / 30, trigger);
  check(fixture.player.shots() == 3 && fixture.player.magazine() == 7,
        "Original handgun fixture consumes three rounds");
  fixture.player.setDamageDifficulty(10);
  fixture.player.takeDamage(300, true);
  check(fixture.player.life()==750 && !fixture.player.unlimitedHealth(),
        "Mortal inventory fixture uses selected original rank10 damage");
  LeonPreview upgraded(assets.player, assets.weapon, assets.enemy);
  upgraded.inheritInventory(fixture.player);
  check(
      upgraded.magazine() == 7 && upgraded.reserve() == 60 &&
          upgraded.shots() == 3 && upgraded.life() == fixture.player.life() &&
          upgraded.damageDifficulty()==10 && !upgraded.unlimitedHealth() &&
          upgraded.hasGrabMotions(),
      "Room reconstruction retains ammunition, mortal HP and source rank");
  upgraded.setPlacement(fixture.probe.position(), fixture.player.heading());
  check(upgraded.magazine() == 7 && upgraded.reserve() == 60 &&
            upgraded.shots() == 3 && upgraded.life()==750 &&
            upgraded.damageDifficulty()==10 && !upgraded.unlimitedHealth(),
        "Destination placement retains ammunition, mortal HP and source rank");
  // A development combat transfer explicitly requests restored test life;
  // room reconstruction itself must not silently heal a campaign player.
  upgraded.setUnlimitedHealth(true);
  check(upgraded.life()==LeonPreview::MaxLife && upgraded.unlimitedHealth() &&
            upgraded.damageDifficulty()==10 && upgraded.magazine()==7,
        "Explicit unlimited-health test policy restores HP without losing rank/ammo");
  upgraded.takeDamage(300,false);
  check(upgraded.life()==LeonPreview::MaxLife,
        "Explicit test policy prevents health loss");
}
void contracts() {
  check(LeonPreview::MaxLife == OriginalLeonLife && OriginalLeonLife == 1200,
        "Original Leon health contract");
  check(EnemyPreview::source(EnemyMotion::Catch).motionEntry == 130 &&
            EnemyPreview::source(EnemyMotion::NeckHang).motionEntry == 651 &&
            EnemyPreview::source(EnemyMotion::ReleaseFailed).motionEntry ==
                652 &&
            EnemyPreview::source(EnemyMotion::ThrowOff).motionEntry == 654 &&
            EnemyPreview::source(EnemyMotion::Wake).motionEntry == 105,
        "Selected original catch/hold/release/wake ARC contracts");
  check(authoredSpawn().listIndex == EnemyEncounter::TestSpawn &&
            authoredSpawn().hp == 1000 && authoredSpawn().flags == 0x24,
        "Original selected ESL actor contract");
}
} // namespace
int main(int argc, char **argv) {
  try {
    contracts();
    if (argc == 1) {
      std::cout << "Grab encounter contracts PASS; supply original PL00, WEP01 "
                   "and EM12 "
                   "for coupled combat integration\n";
      return 0;
    }
    check(argc == 4, "Expected PL00 WEP01 EM12 asset paths");
    const Assets assets{argv[1], argv[2], argv[3]};
    analogResistance(assets);
    freezeAndEdges(assets);
    failedRelease(assets);
    successfulRelease(assets, 10);
    successfulRelease(assets, 31);
    fatalRelease(assets);
    rates(assets);
    inventory(assets);
    std::cout
        << "Coupled grab runtime PASS: fail/get-up, escape/down/wake, fatal "
           "low-life override, paused/latched edges, contact, inventory and "
           "per-tick30/60Hz traces; "
        << checks << " checks\n";
  } catch (const std::exception &error) {
    std::cerr << "Grab encounter failure: " << error.what() << '\n';
    return 1;
  }
}
