#include "leon_preview.h"
#include "input.h"
#include "walk_probe.h"
#include "weapon_preview.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using re4::PlayerAction;
using re4::PlayerActionInput;
void half(std::vector<std::uint8_t> &bytes, std::size_t offset,
          unsigned value) {
  bytes.at(offset) = value >> 8;
  bytes.at(offset + 1) = value;
}
void word(std::vector<std::uint8_t> &bytes, std::size_t offset,
          unsigned value) {
  half(bytes, offset, value >> 16);
  half(bytes, offset + 2, value);
}
struct SyntheticAimAssets {
  std::filesystem::path directory;
  std::string player, weapon;
  SyntheticAimAssets(const char *playerSource, const char *weaponSource) {
    directory = std::filesystem::temp_directory_path() /
        ("re4-synthetic-aim-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!std::filesystem::create_directory(directory))
      throw std::runtime_error("Synthetic aim temporary directory");
    try {
      player = copyWithTorso(playerSource, "PL00.drs");
      weapon = copyWithTorso(weaponSource, "WEP01.drs");
    } catch (...) {
      std::error_code error;
      std::filesystem::remove_all(directory, error);
      throw;
    }
  }
  ~SyntheticAimAssets() {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }
  std::string copyWithTorso(const char *source, const char *name) {
    std::ifstream input(source, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0x410 || input.tellg() > 16 * 1024 * 1024)
      throw std::runtime_error("Synthetic aim fixture size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    constexpr char signature[] = "SYNTHETIC M20 TEST DATA ONLY!!";
    if (!input.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) ||
        std::memcmp(bytes.data(), signature, sizeof(signature) - 1) != 0)
      throw std::runtime_error("Synthetic aim requires generated test data");
    auto readWord = [&](std::size_t offset) {
      if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::runtime_error("Synthetic aim fixture range");
      return (std::uint32_t(bytes[offset]) << 24) |
             (std::uint32_t(bytes[offset + 1]) << 16) |
             (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
    };
    const auto count = readWord(0x400);
    if (count > 4096 || 0x410 + std::size_t(count) * 8 > bytes.size())
      throw std::runtime_error("Synthetic aim archive table");
    for (unsigned i = 0; i < count; ++i) {
      const auto tag = 0x410 + std::size_t(count) * 4 + i * 4;
      if (std::memcmp(bytes.data() + tag, "BIN", 3) != 0)
        continue;
      const auto offset = 0x400 + std::size_t(readWord(0x410 + i * 4));
      if (offset > bytes.size() || bytes.size() - offset < 26)
        throw std::runtime_error("Synthetic aim model header");
      if (bytes[offset + 25] != 119)
        continue;
      // The generic M20 fixture has a flat hierarchy. Add an explicit test
      // torso: pelvis1 -> torso2, neck3 and hand10 below torso2. Both source
      // waist half-rotations now reach the real raw-hand attachment solver.
      // These are generated synthetic bones, not claims about original poses.
      if (bytes.size() - offset < 64 + 119 * 16)
        throw std::runtime_error("Synthetic aim joint table");
      bytes[offset + 64 + 1 * 16 + 1] = 0;
      bytes[offset + 64 + 2 * 16 + 1] = 1;
      bytes[offset + 64 + 3 * 16 + 1] = 2;
      bytes[offset + 64 + 10 * 16 + 1] = 2;
    }
    const auto result = directory / name;
    std::ofstream output(result, std::ios::binary);
    if (!output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()))
      throw std::runtime_error("Synthetic aim fixture write");
    return result.string();
  }
};
// A broad SAT floor isolates action timing from edges, stairs and wall sliding.
re4::Sat fixture() {
  std::vector<re4::CollisionVec> vertices{{-100000, 0, -100000},
                                          {100000, 0, -100000},
                                          {100000, 0, 100000},
                                          {-100000, 0, 100000}};
  std::vector<std::uint8_t> bytes(20 + vertices.size() * 12 + 12 + 12 + 40);
  bytes[0] = 32;
  half(bytes, 2, vertices.size());
  half(bytes, 4, 1);
  half(bytes, 6, 1);
  half(bytes, 10, 2);
  half(bytes, 12, 2);
  std::size_t offset = 20;
  for (auto point : vertices)
    for (auto value : point) {
      float f = float(value);
      unsigned bits;
      std::memcpy(&bits, &f, 4);
      word(bytes, offset, bits);
      offset += 4;
    }
  for (float value : {0.f, 1.f, 0.f}) {
    unsigned bits;
    std::memcpy(&bits, &value, 4);
    word(bytes, offset, bits);
    offset += 4;
  }
  offset += 12; // Zero edge-vector slot; triangle edge indices remain zero.
  unsigned ids[6]{0, 2, 1, 0, 3, 2};
  for (unsigned triangle = 0; triangle < 2; ++triangle) {
    for (unsigned corner = 0; corner < 3; ++corner)
      half(bytes, offset + corner * 2, ids[triangle * 3 + corner]);
    offset += 20;
  }
  return re4::Sat(bytes);
}
void finite(const re4::LeonPreview &actor) {
  for (const auto &bone : actor.rig().bones())
    for (float value : bone)
      assert(std::isfinite(value));
  for (const auto &joint : actor.rig().jointMatrices())
    for (float value : joint)
      assert(std::isfinite(value));
  assert(std::isfinite(actor.heading()) && std::isfinite(actor.aimRate()) &&
         std::isfinite(actor.waistYaw()));
  assert(actor.waistYaw() >= -.1675516f - .00001f &&
         actor.waistYaw() <= .20943952f + .00001f);
  assert(actor.weapon());
  for (const auto &bone : actor.weapon()->rig().bones())
    for (float value : bone)
      assert(std::isfinite(value));
  for (auto coordinate : actor.muzzle())
    assert(std::isfinite(coordinate));
  auto direction = actor.aimDirection();
  double length2 = 0;
  for (auto coordinate : direction) {
    assert(std::isfinite(coordinate));
    length2 += coordinate * coordinate;
  }
  assert(std::abs(length2 - 1) < .000001);
  assert(actor.magazine() <= 10 && actor.reserve() <= 60);
}
void tick(re4::LeonPreview &actor, re4::WalkProbe &probe,
          PlayerActionInput input = {}, double seconds = 1. / 30) {
  actor.step(probe, 0, 0, false, false, seconds, input);
  finite(actor);
}
void aim(re4::LeonPreview &actor, re4::WalkProbe &probe) {
  PlayerActionInput input;
  input.aimHeld = true;
  for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Aim; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Aim);
}
void mappedHorizontalAim(const char *player, const char *weapon,
                         const re4::SatCollision &world) {
  for (float heading : {0.f, 1.2f, 3.1f, -3.1f}) {
    for (float physicalX : {1.f, -1.f}) {
      re4::LeonPreview actor(player, weapon);
      re4::WalkProbe probe(world);
      assert(probe.place({10000, 0, 0}));
      actor.setPlacement(probe.position(), heading);
      aim(actor, probe);
      const auto feet = probe.position();
      const auto initial = actor.aimDirection();
      // Actual scene_gl view row: camera yaw=atan2(dir.x,dir.z)+PI.
      // Derive its right vector geometrically, rather than treating positive
      // world atan2 as screen-right (the M20 test's incorrect assumption).
      const double horizontal = std::hypot(initial[0], initial[2]);
      assert(horizontal > .1);
      const std::array<double, 3> right{-initial[2] / horizontal, 0,
                                       initial[0] / horizontal};
      re4::InputMapper mapper;
      re4::RawInput raw;
      raw.rx = physicalX;
      raw.lt = 1;
      const auto mapped = mapper.poll(raw);
      assert(mapped.rx == -physicalX && mapped.ry == 0);
      PlayerActionInput input;
      input.aimHeld = (mapped.held & re4::Aim) != 0;
      input.yaw = mapped.rx;
      input.pitch = mapped.ry;
      for (unsigned i = 0; i < 4; ++i)
        tick(actor, probe, input);
      const auto waistDirection = actor.aimDirection();
      const double waistRight = (waistDirection[0] - initial[0]) * right[0] +
                                 (waistDirection[2] - initial[2]) * right[2];
      assert(waistRight * physicalX > .005 && actor.heading() == heading);
      for (unsigned i = 4; i < 30; ++i)
        tick(actor, probe, input);
      const auto direction = actor.aimDirection();
      const double movedRight = (direction[0] - initial[0]) * right[0] +
                                 (direction[2] - initial[2]) * right[2];
      const double bodyYaw = std::remainder(actor.heading() - heading,
                                            6.28318530717958647692);
      assert(movedRight * physicalX > .1 && bodyYaw * physicalX < -.1);
      assert(probe.position() == feet && actor.magazine() == 10 &&
             actor.shots() == 0);
    }
  }
}
void fireOnce(re4::LeonPreview &actor, re4::WalkProbe &probe) {
  aim(actor, probe);
  auto shots = actor.shots(), magazine = actor.magazine();
  PlayerActionInput input;
  input.aimHeld = true;
  input.firePressed = true;
  tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Fire && actor.shots() == shots + 1 &&
         actor.magazine() == magazine - 1);
  input.firePressed = false;
  for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Aim; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Aim && actor.shots() == shots + 1);
}
void stationaryAndAim(const char *player, const char *weapon,
                      const re4::SatCollision &world) {
  re4::LeonPreview actor(player, weapon);
  re4::WalkProbe probe(world);
  assert(probe.place({10000, 0, 0}));
  actor.setPlacement(probe.position(), 0);
  assert(actor.action() == PlayerAction::Footwork && actor.magazine() == 10 &&
         actor.reserve() == 60 && actor.shots() == 0);
  assert(actor.weapon()->rig().bones().size() == 4);
  PlayerActionInput input;
  input.aimHeld = true;
  auto feet = probe.position();
  for (unsigned i = 0; i < 45; ++i) {
    actor.step(probe, 1, 1, true, false, 1. / 30, input);
    assert(probe.position() == feet && actor.feet() == feet);
    finite(actor);
  }
  assert(actor.action() == PlayerAction::Aim && actor.magazine() == 10);
  auto zeroPose = actor.rig().bones();
  auto levelDirection = actor.aimDirection();
  input.pitch = 1;
  for (unsigned i = 0; i < 30; ++i)
    tick(actor, probe, input);
  assert(actor.aimRate() > 0 && actor.rig().bones() != zeroPose);
  assert(actor.aimDirection()[1] > levelDirection[1] + .1);
  auto upPose = actor.rig().bones();
  input.pitch = -1;
  for (unsigned i = 0; i < 60; ++i)
    tick(actor, probe, input);
  assert(actor.aimRate() < 0 && actor.rig().bones() != upPose);
  assert(actor.aimDirection()[1] < levelDirection[1] - .1);
  input.pitch = 0;
  input.yaw = 1;
  for (unsigned i = 0; i < 30; ++i)
    tick(actor, probe, input);
  assert(actor.heading() + actor.waistYaw() > 0);
  assert(std::abs(actor.waistYaw()) <= .3926991f + .0001f);
  assert(probe.position() == feet);
  input.yaw = std::numeric_limits<double>::quiet_NaN();
  input.pitch = std::numeric_limits<double>::infinity();
  auto pose = actor.rig().bones();
  auto heading = actor.heading();
  bool rejected = false;
  try {
    tick(actor, probe, input);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected && actor.rig().bones() == pose &&
         actor.heading() == heading && actor.magazine() == 10);
  finite(actor);
  input = {};
  for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Footwork; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Footwork);
  assert(actor.magazine() == 10 && actor.reserve() == 60 && actor.shots() == 0);
}
void cadenceAndEdges(const char *player, const char *weapon,
                     const re4::SatCollision &world) {
  re4::LeonPreview hz30(player, weapon), hz60(player, weapon);
  re4::WalkProbe p30(world), p60(world);
  assert(p30.place({10000, 0, 0}) && p60.place({10000, 0, 0}));
  hz30.setPlacement(p30.position(), 0);
  hz60.setPlacement(p60.position(), 0);
  aim(hz30, p30);
  aim(hz60, p60);
  PlayerActionInput input;
  input.aimHeld = true;
  input.fireHeld = true;
  input.firePressed = true;
  // The edge arrives before either runtime owns a full simulation tick.
  tick(hz30, p30, input, 0);
  tick(hz60, p60, input, 0);
  assert(hz30.shots() == 0 && hz60.shots() == 0);
  input.firePressed = false;
  unsigned lastShotTick = 0;
  bool fired = false;
  for (unsigned i = 0; i < 80; ++i) {
    auto shots = hz30.shots();
    tick(hz30, p30, input);
    if (hz30.shots() != shots) {
      if (fired)
        assert(i - lastShotTick == 14);
      lastShotTick = i;
      fired = true;
    }
  }
  for (unsigned i = 0; i < 160; ++i)
    tick(hz60, p60, input, 1. / 60);
  assert(hz30.shots() > 1 && hz30.shots() <= 10);
  assert(hz30.shots() == hz60.shots() && hz30.magazine() == hz60.magazine() &&
         hz30.reserve() == hz60.reserve());
  assert(hz30.action() == hz60.action() &&
         hz30.rig().bones() == hz60.rig().bones() &&
         hz30.weapon()->rig().bones() == hz60.weapon()->rig().bones());
  assert(p30.position() == p60.position());
  // Continuous RT cannot consume more rounds than are in the magazine. Empty
  // magazine auto-reload is tested through the separate verified refill event.
  input.fireHeld = false;
  for (unsigned i = 0; i < 90 && hz30.action() != PlayerAction::Aim; ++i)
    tick(hz30, p30, input);
  auto before = hz30.shots();
  input.firePressed = true;
  input.fireHeld = false;
  tick(hz30, p30, input, 0);
  input.firePressed = false;
  tick(hz30, p30, input);
  assert(hz30.shots() == before + 1);
  input.fireHeld = true;
  auto pose = hz30.rig().bones();
  auto ammo = hz30.magazine();
  auto shots = hz30.shots();
  hz30.suspendClock();
  assert(hz30.rig().bones() == pose && hz30.magazine() == ammo &&
         hz30.shots() == shots);
  for (unsigned i = 0; i < 90; ++i)
    tick(hz30, p30, input);
  assert(hz30.magazine() == ammo && hz30.shots() == shots);
  // Neutralising both triggers rearms the input; a fresh aim/shot is accepted.
  tick(hz30, p30);
  aim(hz30, p30);
  fireOnce(hz30, p30);
}
void recoilRelease(const char *player, const char *weapon,
                   const re4::SatCollision &world) {
  re4::LeonPreview actor(player, weapon);
  re4::WalkProbe probe(world);
  assert(probe.place({10000, 0, 0}));
  actor.setPlacement(probe.position(), 0);
  aim(actor, probe);
  PlayerActionInput input;
  input.aimHeld = input.firePressed = true;
  tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Fire && actor.shots() == 1);
  // Original Fire10 ignores aim release until the recoil crossed frame12.
  // Releasing/repressing LT must not bypass the authored fire interval.
  input = {};
  for (unsigned i = 0; i < 11; ++i) {
    tick(actor, probe, input);
    assert(actor.action() == PlayerAction::Fire && actor.shots() == 1);
  }
  for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Footwork; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Footwork && actor.magazine() == 9);
}
void reloadAndPlacement(const char *player, const char *weapon,
                        const re4::SatCollision &world) {
  re4::LeonPreview actor(player, weapon);
  re4::WalkProbe probe(world);
  assert(probe.place({10000, 0, 0}));
  actor.setPlacement(probe.position(), 0);
  aim(actor, probe);
  fireOnce(actor, probe);
  fireOnce(actor, probe);
  fireOnce(actor, probe);
  assert(actor.magazine() == 7 && actor.reserve() == 60 && actor.shots() == 3);
  PlayerActionInput input;
  input.aimHeld = true;
  input.reloadPressed = true;
  tick(actor, probe, input, 0);
  assert(actor.action() == PlayerAction::Aim && actor.magazine() == 7);
  input.reloadPressed = false;
  tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Reload);
  auto feet = probe.position();
  bool committed = false;
  unsigned commits = 0;
  for (unsigned i = 0; i < 90 && actor.action() == PlayerAction::Reload; ++i) {
    auto magazine = actor.magazine();
    auto frame = actor.weapon()->frame();
    actor.step(probe, 1, -1, true, false, 1. / 30, input);
    finite(actor);
    assert(probe.position() == feet && actor.feet() == feet);
    if (actor.magazine() != magazine) {
      ++commits;
      committed = true;
      // cObjFn57::moveReload checks the previously evaluated FCV frame31,
      // then the object motion advances to visible frame32 in the same tick.
      assert(frame == 31 && actor.weapon()->frame() == 32);
      assert(actor.magazine() == 10 && actor.reserve() == 57);
    } else if (!committed) {
      assert(actor.magazine() == 7 && actor.reserve() == 60);
      assert(actor.weapon()->frame() <= 31);
    }
  }
  assert(actor.action() == PlayerAction::Aim && commits == 1 && committed);
  assert(actor.magazine() == 10 && actor.reserve() == 57 && actor.shots() == 3);
  // A full-magazine reload must neither enter a reload nor spend reserve.
  input.reloadPressed = true;
  tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Aim && actor.reserve() == 57);
  input = {};
  for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Footwork; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Footwork);
  assert(probe.place({20000, 0, 20000}));
  actor.setPlacement(probe.position(), -1.94f);
  assert(actor.magazine() == 10 && actor.reserve() == 57 && actor.shots() == 3);
  assert(actor.feet() == probe.position() &&
         actor.action() == PlayerAction::Footwork);
  actor.reset();
  assert(actor.magazine() == 10 && actor.reserve() == 60 && actor.shots() == 0);
}
void emptyAndConservation(const char *player, const char *weapon,
                          const re4::SatCollision &world) {
  re4::LeonPreview actor(player, weapon);
  re4::WalkProbe probe(world);
  assert(probe.place({10000, 0, 0}));
  actor.setPlacement(probe.position(), 0);
  aim(actor, probe);
  PlayerActionInput input;
  input.aimHeld = true;
  for (unsigned magazine = 0; magazine < 7; ++magazine) {
    for (unsigned round = 0; round < 10; ++round) {
      fireOnce(actor, probe);
      assert(actor.shots() + actor.magazine() + actor.reserve() == 70);
    }
    assert(actor.magazine() == 0 && actor.weapon()->empty());
    if (magazine == 6)
      break;
    // An empty magazine plus a new fire edge invokes original auto-reload;
    // holding an already consumed trigger cannot invent rounds or shots.
    auto shots = actor.shots();
    input.firePressed = true;
    tick(actor, probe, input);
    assert(actor.action() == PlayerAction::Reload && actor.shots() == shots &&
           actor.weapon()->empty());
    input.firePressed = false;
    for (unsigned i = 0; i < 90 && actor.action() != PlayerAction::Aim; ++i) {
      tick(actor, probe, input);
      assert(actor.shots() + actor.magazine() + actor.reserve() == 70);
    }
    assert(actor.action() == PlayerAction::Aim && actor.magazine() == 10 &&
           actor.reserve() == 50 - magazine * 10);
  }
  assert(actor.shots() == 70 && actor.magazine() == 0 && actor.reserve() == 0);
  input.firePressed = input.fireHeld = input.reloadPressed = true;
  for (unsigned i = 0; i < 90; ++i)
    tick(actor, probe, input);
  assert(actor.action() == PlayerAction::Aim && actor.shots() == 70 &&
         actor.magazine() == 0 && actor.reserve() == 0);
}
} // namespace
int main(int argc, char **argv) {
  PlayerActionInput neutral;
  assert(!neutral.aimHeld && !neutral.fireHeld && !neutral.firePressed &&
         !neutral.reloadPressed && neutral.yaw == 0 && neutral.pitch == 0);
  auto sat = fixture();
  re4::SatCollision world(sat);
  re4::WalkProbe probe(world);
  assert(probe.place({10000, 0, 0}));
  if (argc == 4 && std::string(argv[3]) == "--synthetic-aim") {
    const SyntheticAimAssets assets(argv[1], argv[2]);
    mappedHorizontalAim(assets.player.c_str(), assets.weapon.c_str(), world);
    std::cout << "Synthetic production Draw/Aim chain: physical RS left/right, "
                 "torso/hand hierarchy, camera-right projection, waist/body "
                 "stages and wrapped headings PASS; no original pose claim\n";
  } else if (argc == 4 && std::string(argv[3]) == "--aim-only") {
    mappedHorizontalAim(argv[1], argv[2], world);
    std::cout << "Production Aim chain: mapped physical RS left/right moves "
                 "the posed muzzle along actual camera-right, waist/body "
                 "stages and wrapped headings PASS\n";
  } else if (argc == 3) {
    mappedHorizontalAim(argv[1], argv[2], world);
    stationaryAndAim(argv[1], argv[2], world);
    cadenceAndEdges(argv[1], argv[2], world);
    recoilRelease(argv[1], argv[2], world);
    reloadAndPlacement(argv[1], argv[2], world);
    emptyAndConservation(argv[1], argv[2], world);
    std::cout << "Original handgun actions PASS: stationary draw/aim/holster, "
                 "mapped physical RS left/right, pitch/waist, finite malformed input, 30/60-Hz shot cadence, "
                 "zero-dt edges, trigger neutral gate, one frame-31 reload "
                 "commit, full-magazine rejection, empty auto-reload, 70-round "
                 "conservation and room placement ammo\n";
  } else {
    assert(argc == 1);
    std::cout << "Handgun action contracts PASS; supply PL00 and WEP01 paths "
                 "to verify original action data\n";
  }
}
