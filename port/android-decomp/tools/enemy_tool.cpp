#include "enemy_preview.h"
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
void pose(const re4::AnimationRig &rig) {
  for (const auto &joint : rig.jointMatrices())
    for (float value : joint)
      floating(value);
}
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void checkMesh(const re4::EnemyPreview &enemy) {
  const auto &mesh = enemy.mesh();
  require(enemy.rig().jointMatrices().size() == 34, "EM12: joint count");
  require(mesh.included == 4 && mesh.triangles == 3013 &&
              mesh.vertices.size() == 9039 * 3 && mesh.textures.size() == 3,
          "EM12: original type-0 geometry counts");
  require(enemy.skin().size() == 9039, "EM12: skin corner count");
  for (const auto &corner : enemy.skin())
    for (const auto *weight : {&corner.position, &corner.normal}) {
      require(weight->count >= 1 && weight->count <= 3, "EM12: weight count");
      float sum = 0;
      for (unsigned i = 0; i < weight->count; ++i) {
        require(weight->joints[i] < 34 && std::isfinite(weight->rates[i]) &&
                    weight->rates[i] >= 0,
                "EM12: owning joint / weight");
        sum += weight->rates[i];
      }
      require(std::abs(sum - 1) < .00001f, "EM12: normalized weights");
    }
  for (const auto &draw : mesh.draws)
    require(draw.texture >= 0 && unsigned(draw.texture) < mesh.textures.size(),
            "EM12: resolved original texture");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    re4::EnemyPreview enemy(argv[1]);
    checkMesh(enemy);
    auto &rig = enemy.rig();
    word(0x4d313900); // M19 binary fixture version.
    word(34);
    word(enemy.mesh().included);
    word(enemy.mesh().triangles);
    word(enemy.skin().size());
    word(enemy.mesh().textures.size());
    word(5);
    for (unsigned clip = 0; clip < 5; ++clip) {
      const auto motion = static_cast<re4::EnemyMotion>(clip);
      const auto source = re4::EnemyPreview::source(motion);
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
    for (unsigned clip = 0; clip < 5; ++clip) {
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
    word(7);
    for (unsigned group = 0; group < 7; ++group) {
      const auto clip = group < 5 ? group : group - 3;
      const bool mirror = group >= 5;
      const auto motion = static_cast<re4::EnemyMotion>(clip);
      const auto source = re4::EnemyPreview::source(motion);
      const auto frames = rig.cycle(clip).sequenceFrames;
      const auto ticks = clip == 1 ? frames * 3 + 5 : frames + 16;
      rig.reset();
      enemy.start(motion, mirror);
      require(enemy.mirrored() == mirror, "EM12: mirrored source state");
      word(clip);
      word(mirror);
      word(source.flags | (mirror ? 0x40 : 0));
      word(source.blendFrames);
      word(ticks);
      for (unsigned tick = 0; tick < ticks; ++tick) {
        const auto root = enemy.advance(1);
        const auto state = rig.status();
        floating(1); // Original 30-Hz sequence speed.
        for (auto value : root.translation)
          floating(float(value));
        floating(float(root.yaw));
        floating(state.curveFrame);
        floating(state.sequenceFrame);
        word(state.state);
        word(rig.sequenceFree());
        pose(rig);
      }
    }
    std::cerr << enemy.describe()
              << "; R100 ESL index 0x25 -> walk SEQ ARC0b; "
                 "30 poses, 7 source rooted/SEQ/mirror traces\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
