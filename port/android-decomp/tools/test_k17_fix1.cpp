// Physical turn direction and radio hand restoration with original assets.
#include "leon_preview.h"
#include "inventory_case.h"
#include "radio_intro.h"
#include "turn_rules.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace re4;
unsigned checks{};
void check(bool value, const std::string &label) {
  if (!value) throw std::runtime_error(label);
  ++checks;
}
Sat floorData() {
  ScenarioTriangle a, b;
  a.vertices = {{{-100000, 0, -100000}, {100000, 0, 100000}, {100000, 0, -100000}}};
  b.vertices = {{{-100000, 0, -100000}, {-100000, 0, 100000}, {100000, 0, 100000}}};
  a.group = b.group = 0;
  return Sat::ScenarioGeometry({a, b});
}
std::vector<unsigned> hands(const LeonPreview &player) {
  std::vector<unsigned> result;
  for (const auto &draw : player.renderMesh().draws)
    if (draw.work >= 600 && player.visiblePart(draw.work)) result.push_back(draw.work);
  return result;
}
void turns(LeonPreview &player, WalkProbe &probe, const std::string &label, bool healthyOnly) {
  for (unsigned hp : {1200u, 700u}) {
    if (healthyOnly && hp != 1200) continue;
    check(player.restoreLife(hp), label + " turn HP fixture");
    for (double heading : {0., 1.2, -2.8})
      for (double horizontal : {-1., 1.})
        for (double forward : {-1., 0., 1.})
          for (bool run : {false, true}) {
            check(probe.place({0, 0, 0}), "Turn fixture floor");
            player.setPlacement(probe.position(), float(heading));
            const auto initial = player.placement();
            const auto camera = PoseFollowCamera({}, nullptr, player.feet(), player.heading());
            for (unsigned tick = 0; tick < 10; ++tick)
              player.step(probe, horizontal, forward, run, false, 1. / 30);
            const auto matrix = player.placement();
            // Project the change in facing onto the real shoulder camera's
            // right axis: looking toward +Z, screen-right is world -X.
            // This tests visible direction, independently of key names/yaw sign.
            double right = 0;
            for (unsigned axis = 0; axis < 3; ++axis)
              right += (matrix[axis * 4 + 2] - initial[axis * 4 + 2]) * camera.right[axis];
            check(horizontal * right > .02, label + " turns toward physical LS direction, HP " + std::to_string(hp));
            if (forward == 0)
              check(player.state() == (horizontal > 0 ? Locomotion::TurnRight : Locomotion::TurnLeft),
                    label + " in-place state matches physical direction");
          }
  }
  check(player.restoreLife(1200), "Restore healthy radio fixture");
}
void radio(LeonPreview &player, WalkProbe &probe, RadioIntro &intro, const std::string &label) {
  player.setPlacement(probe.position(), 0);
  const auto previous = player.leftHandType();
  const auto visible = hands(player);
  for (unsigned round = 0; round < 2; ++round) {
    intro.begin(player, probe);
    for (unsigned tick = 0; tick < 400 && !intro.finished(player); ++tick) intro.tick30Hz(player, probe);
    check(intro.finished(player) && intro.visible() && player.leftHandType() == 2,
          label + " original radio take-out finishes with radio hand");
    intro.end(player);
    check(player.leftHandType() == previous && hands(player) == visible,
          label + " radio restores exact previous visible weapon hands");
    check(!intro.visible() && !player.roomScriptActive() && player.action() == PlayerAction::Footwork,
          label + " radio returns gameplay ownership");
    player.step(probe, 0, 1, false, false, 1. / 30);
    check(player.state() == Locomotion::Walk, label + " movement works after radio");
  }
  intro.begin(player, probe);
  intro.end(player); // START before frame22 must preserve the untouched hands.
  check(player.leftHandType() == previous && hands(player) == visible && !player.roomScriptActive(),
        label + " early radio cancel preserves previous hands");
}
} // namespace
int main(int argc, char **argv) {
  using namespace re4;
  try {
    if (argc != 2 && argc != 3) throw std::runtime_error("Usage: k17_fix1_check FIXTURE_DISC_ROOT [--radio-only|--healthy-only]");
    const bool radioOnly = argc == 3 && std::string(argv[2]) == "--radio-only";
    const bool healthyOnly = argc == 3 && std::string(argv[2]) == "--healthy-only";
    if (argc == 3 && !radioOnly && !healthyOnly) throw std::runtime_error("Unknown regression test option");
    const auto files = std::filesystem::path(argv[1]) / "files";
    const auto pl = (files / "em/pl00.drs").string();
    const auto floor = floorData();
    SatCollision world(floor);
    WalkProbe probe(world);
    check(probe.place({0, 0, 0}), "Floor supports original Leon capsule");
    LeonPreview player(pl, (files / "em/wep01.drs").string(), (files / "em/em12.drs").string(), true);
    RadioIntro intro(pl);
    if (!radioOnly) turns(player, probe, "Default handgun", healthyOnly);
    if (!healthyOnly) radio(player, probe, intro, "Default handgun");
    unsigned profiles{};
    for (unsigned id = 0; id < 256; ++id) {
      if (!OriginalWeaponCarry(id)) continue;
      const auto label = std::string(OriginalItemLabel(id)) + " (" + std::to_string(id) + ")";
      check(player.equipWeapon(id), label + " original weapon loads");
      if (!radioOnly) turns(player, probe, label, healthyOnly);
      if (!healthyOnly) radio(player, probe, intro, label);
      ++profiles;
    }
    check(profiles == 38, "Every original carry profile tested");
    std::cout << "K17 Fix1 original physical LS/radio hand checks PASS: " << checks
              << " checks, default handgun plus " << profiles << " weapon profiles\n";
  } catch (const std::exception &e) {
    std::cerr << "K17 Fix1 regression: " << e.what() << '\n';
    return 1;
  }
}
