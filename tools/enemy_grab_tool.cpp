#include "enemy_preview.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void word(std::uint32_t w) {
  for (int k = 3; k >= 0; --k)
    std::cout.put(static_cast<char>(w >> (k * 8)));
}
void floating(float value) {
  std::uint32_t w;
  std::memcpy(&w, &value, 4);
  word(w);
}
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void pose(const re4::AnimationRig &rig) {
  for (const auto &joint : rig.jointMatrices())
    for (float value : joint)
      floating(value);
}
void tick(re4::EnemyPreview &enemy) {
  const auto root = enemy.advance();
  const auto state = enemy.rig().status();
  floating(1);
  for (auto value : root.translation)
    floating(float(value));
  floating(float(root.yaw));
  floating(state.curveFrame);
  floating(state.sequenceFrame);
  word(state.state);
  word(enemy.rig().sequenceFree());
  pose(enemy.rig());
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    re4::EnemyPreview enemy(argv[1]);
    auto &rig = enemy.rig();
    require(enemy.mesh().triangles == 3013 && enemy.skin().size() == 9039,
            "M19 diagnostic geometry changed");
    require(enemy.renderMesh().included == 6 &&
                enemy.renderSkin().size() == enemy.renderMesh().vertices.size() / 3 &&
                enemy.renderSkin().size() > enemy.skin().size() &&
                enemy.renderMesh().textures.size() == 3,
            "Original open-hand render geometry missing");
    bool badDown = false;
    try {
      enemy.start(re4::EnemyMotion::DownIdle);
    } catch (const std::runtime_error &) {
      badDown = true;
    }
    require(badDown, "DownIdle must preserve an ended ThrowOff");

    word(0x4d323000);
    word(34);
    word(10);
    for (unsigned clip = 0; clip < 10; ++clip) {
      const auto source = re4::EnemyPreview::source(
          static_cast<re4::EnemyMotion>(clip));
      const auto cycle = rig.cycle(clip);
      word(source.motionEntry);
      word(source.sequenceEntry);
      word(source.blendFrames);
      word(source.flags);
      floating(rig.maxFrame(clip));
      word(cycle.sequenceFrames);
      for (auto value : cycle.translation)
        floating(float(value));
      floating(float(cycle.yaw));
    }
    word(30);
    for (unsigned clip = 5; clip < 10; ++clip) {
      const auto motion = static_cast<re4::EnemyMotion>(clip);
      const auto max = rig.maxFrame(clip);
      for (float frame :
           {0.f, max * .125f, max * .25f, max * .5f, max * .875f, max}) {
        rig.reset();
        enemy.sample(motion, frame);
        word(clip);
        floating(frame);
        pose(rig);
      }
    }
    word(6);
    for (unsigned group = 0; group < 6; ++group) {
      const auto clip = group < 5 ? group + 5 : 9;
      const bool mirror = group == 5;
      const auto motion = static_cast<re4::EnemyMotion>(clip);
      const auto source = re4::EnemyPreview::source(motion);
      const auto ticks = rig.cycle(clip).sequenceFrames + 16;
      rig.reset();
      enemy.start(motion, mirror);
      require(enemy.mirrored() == mirror, "Source grab / wake mirror state");
      require(enemy.openHands() == (clip >= 6 && clip <= 8),
              "Source em10HandSet pose");
      require(enemy.visiblePart(444) == !enemy.openHands() &&
                  enemy.visiblePart(449) == !enemy.openHands() &&
                  enemy.visiblePart(445) == enemy.openHands() &&
                  enemy.visiblePart(450) == enemy.openHands() &&
                  enemy.visiblePart(440) && enemy.visiblePart(442),
              "Source hand submission selection");
      word(clip);
      word(mirror);
      word(source.flags | (mirror ? 0x40 : 0));
      word(source.blendFrames);
      word(ticks);
      for (unsigned frame = 0; frame < ticks; ++frame)
        tick(enemy);
      require(rig.status().ended, "Grab / wake one-shot did not end");
    }
    rig.reset();
    enemy.start(re4::EnemyMotion::ThrowOff);
    for (unsigned frame = 0; frame < rig.cycle(8).sequenceFrames + 16; ++frame)
      enemy.advance();
    const auto before = rig.jointMatrices();
    const auto beforeState = rig.status();
    enemy.start(re4::EnemyMotion::DownWakeWait);
    require(rig.jointMatrices() == before &&
                rig.status().curveFrame == beforeState.curveFrame &&
                rig.status().sequenceFrame == beforeState.sequenceFrame &&
                rig.status().state == beforeState.state && !enemy.openHands(),
            "DownWait changed the source ended ThrowOff pose");
    word(10);
    for (unsigned frame = 0; frame < 10; ++frame) {
      const auto root = enemy.advance();
      require(root.ended && std::abs(root.yaw) < .00001 &&
                  std::all_of(root.translation.begin(), root.translation.end(),
                              [](double value) { return std::abs(value) < .00001; }),
              "Ended down pose emitted root motion");
      require(rig.jointMatrices() == before, "Ended down pose advanced");
      // Serialize the already validated advance in the same trace format.
      floating(1);
      for (auto value : root.translation)
        floating(float(value));
      floating(float(root.yaw));
      const auto state = rig.status();
      floating(state.curveFrame);
      floating(state.sequenceFrame);
      word(state.state);
      word(rig.sequenceFree());
      pose(rig);
    }
    const auto &render = enemy.renderMesh();
    word(render.included);
    word(render.triangles);
    word(enemy.renderSkin().size());
    word(render.textures.size());
    word(render.draws.size());
    for (const auto &draw : render.draws) {
      word(draw.work);
      word(draw.first);
      word(draw.count);
    }
    std::cerr << enemy.describe()
              << "; M20 original Catch/NeckHang/release/wake; "
                 "30 poses, 6 one-shot/root/SEQ/mirror traces, held down pose\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
