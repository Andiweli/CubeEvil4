// Native paired with test_enemy_blow_data.py's independent original host.
#include "enemy_preview.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
void word(std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    std::cout.put(static_cast<char>(value >> shift));
}
void floating(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, 4);
  word(bits);
}
void require(bool valid, const char *message) {
  if (!valid) throw std::runtime_error(message);
}
void pose(const re4::AnimationRig &rig) {
  for (const auto &joint : rig.jointMatrices())
    for (float value : joint) floating(value);
}
void tick(re4::EnemyPreview &enemy) {
  const auto root = enemy.advance();
  const auto state = enemy.rig().status();
  floating(1);
  for (auto value : root.translation) floating(float(value));
  floating(float(root.yaw));
  floating(state.curveFrame);
  floating(state.sequenceFrame);
  word(state.state);
  word(enemy.rig().sequenceFree());
  pose(enemy.rig());
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 2) return 2;
    re4::EnemyPreview enemy(argv[1], 0x25, true);
    auto &rig = enemy.rig();
    require(enemy.hasMeleeMotions(), "M21 enemy motions missing");
    word(0x4d323142);
    word(rig.jointMatrices().size());
    word(26);
    for (unsigned clip = 0; clip < 26; ++clip) {
      const auto source = re4::EnemyPreview::source(
          static_cast<re4::EnemyMotion>(clip));
      const auto cycle = rig.cycle(clip);
      word(source.motionEntry); word(source.sequenceEntry);
      word(source.blendFrames); word(source.flags);
      floating(rig.maxFrame(clip)); word(cycle.sequenceFrames);
      for (auto value : cycle.translation) floating(float(value));
      floating(float(cycle.yaw));
    }
    word(78);
    for (unsigned clip : {11u,12u,13u,14u,15u,16u,17u,20u,21u,22u,23u,24u,25u}) {
      const auto max = rig.maxFrame(clip);
      for (float frame : {0.f,max*.125f,max*.25f,max*.5f,max*.875f,max}) {
        rig.reset();
        enemy.sample(static_cast<re4::EnemyMotion>(clip), frame);
        word(clip); floating(frame); pose(rig);
      }
    }
    word(24);
    for (unsigned clip : {11u,12u,13u,14u,15u,16u,17u,20u,21u,22u,23u,24u,25u})
      for (bool mirror : {false,true}) {
        if ((clip == 22 || clip == 23) && mirror) continue;
        const auto motion = static_cast<re4::EnemyMotion>(clip);
        const auto source = re4::EnemyPreview::source(motion);
        const unsigned ticks = rig.cycle(clip).sequenceFrames + 16;
        const float firstScale = clip == 15 && !mirror ? 2.f : 1.f;
        rig.reset(); enemy.start(motion, mirror);
        require(enemy.mirrored() == mirror, "Blow source mirror ignored");
        word(clip); word(mirror); word(source.flags | (mirror ? 0x40 : 0));
        word(source.blendFrames); word(ticks); word(clip >= 11 && clip < 17);
        floating(firstScale);
        for (unsigned t = 0; t < ticks; ++t) tick(enemy);
        require(rig.status().ended, "Blow/wake action did not finish");
      }
    word(8);
    for (unsigned clip : {11u,12u,13u,14u,15u,16u,22u,23u}) {
      rig.reset(); enemy.start(static_cast<re4::EnemyMotion>(clip));
      for (unsigned t = 0; t < rig.cycle(clip).sequenceFrames + 16; ++t)
        enemy.advance();
      const auto before = rig.jointMatrices();
      const auto state = rig.status();
      enemy.start(re4::EnemyMotion::DownWakeWait);
      require(rig.jointMatrices() == before && rig.status().state == state.state,
              "Logical blow DownWait changed ended pose");
      word(clip); word(0); word(10);
      for (unsigned t = 0; t < 10; ++t) {
        tick(enemy);
        require(rig.jointMatrices() == before && rig.status().ended,
                "Blow DownWait did not hold ended pose");
      }
    }
    std::cerr << enemy.describe()
              << "; M21 source blow/wake/down/body reactions, 78 poses, "
                 "24 root/mirror/Hokan traces, eight held down poses\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
