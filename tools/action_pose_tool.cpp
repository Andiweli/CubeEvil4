#include "animation.h"
#include "archive.h"
#include <cstring>
#include <iostream>
namespace {
void word(std::uint32_t w) {
  for (int k = 3; k >= 0; --k)
    std::cout.put(static_cast<char>(w >> (k * 8)));
}
void floating(float f) {
  std::uint32_t w;
  std::memcpy(&w, &f, 4);
  word(w);
}
void pose(const re4::AnimationRig &rig) {
  for (const auto &joint : rig.jointMatrices())
    for (float f : joint)
      floating(f);
}
void state(const re4::AnimationRig &rig) {
  auto s = rig.status();
  floating(s.curveFrame);
  floating(s.sequenceFrame);
  word(s.state);
  word(rig.crossedFrame(12));
  word(rig.crossedFrame(31));
  word(rig.crossedFrame(36));
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      return 2;
    auto pl = re4::RoomArchive::LoadDrs(argv[1]);
    auto wp = re4::RoomArchive::LoadDrs(argv[2]);
    re4::SkinnedModel body(pl.copy(0)), weapon(wp.copy(2));
    std::vector<re4::AnimationBytes> clips;
    for (unsigned index : {7u, 13u, 17u, 15u, 11u, 9u, 19u})
      clips.push_back({wp.copy(index), wp.copy(index + 1)});
    for (unsigned index : {30u, 32u, 34u, 35u, 36u, 37u, 38u, 39u, 41u})
      clips.push_back({wp.copy(index), index == 30 || index == 32
                                           ? wp.copy(index + 1)
                                           : std::vector<std::uint8_t>{}});
    re4::AnimationRig rig(body, clips);
    std::vector<re4::AnimationBytes> gunClips;
    for (unsigned index : {48u, 46u, 50u, 53u, 51u, 47u})
      gunClips.push_back({wp.copy(index), {}});
    re4::AnimationRig gun(weapon, gunClips);
    word(0x4d313800); // M18 binary fixture version.
    word(static_cast<std::uint32_t>(rig.jointMatrices().size()));
    word(static_cast<std::uint32_t>(gun.jointMatrices().size()));
    // Explicit single FCV poses, independent of optional SEQ playback.
    word(9 * 6);
    for (unsigned clip = 7; clip < 16; ++clip) {
      float max = rig.maxFrame(clip);
      for (float frame :
           {0.f, max * .125f, max * .25f, max * .5f, max * .875f, max}) {
        rig.reset();
        rig.sample(clip, frame);
        word(clip);
        floating(frame);
        pose(rig);
      }
    }
    // Steady-state cMot3 endpoints and intermediate local quaternion blends.
    for (const auto &triplet : {std::array<unsigned, 3>{9, 10, 11},
                                std::array<unsigned, 3>{12, 13, 14}}) {
      unsigned frames = unsigned(rig.maxFrame(triplet[0])) + 1;
      word(5 * frames);
      for (float rate : {-1.f, -.5f, 0.f, .5f, 1.f})
        for (unsigned frame = 0; frame < frames; ++frame) {
          rig.sampleBlend3(triplet, rate, float(frame));
          floating(rate);
          floating(float(frame));
          pose(rig);
        }
    }
    word(9);
    for (float rate : {-1.f, 0.f, 1.f})
      for (float waist : {-.35f, 0.f, .35f}) {
        rig.setWaistYaw(0);
        rig.sampleBlend3({9, 10, 11}, rate, 12);
        rig.setWaistYaw(waist);
        floating(rate);
        floating(waist);
        pose(rig);
      }
    const std::array<float, 9> crossing{1, 1, 0, -1, -1, 0, .5f, -.5f, 1};
    const std::array<float, 9> hokan{-.5f, -.5f, 0, .5f, .5f,
                                     -.5f, -.5f, 0, .5f};
    for (unsigned blendFrames : {0u, 3u}) {
      rig.reset();
      rig.startBlend3({9, 10, 11}, 0, blendFrames);
      word(9);
      for (float rate : blendFrames ? hokan : crossing) {
        rig.setBlend3Rate(rate);
        rig.advanceAction();
        floating(rate);
        state(rig);
        pose(rig);
      }
    }
    // Original SEQ (draw/holster) and direct FCV (reload) event timing.
    word(3);
    for (unsigned clip : {7u, 8u, 15u}) {
      rig.reset();
      rig.startActionFlags(clip, 3, 0, clip == 15 ? 1 : clip == 8 ? 5 : 4);
      word(clip);
      word(60);
      for (unsigned i = 0; i < 60; ++i) {
        rig.advanceAction();
        state(rig);
        pose(rig);
      }
    }
    word(6 * 6);
    for (unsigned clip = 0; clip < 6; ++clip) {
      float max = gun.maxFrame(clip);
      for (float frame :
           {0.f, max * .125f, max * .25f, max * .5f, max * .875f, max}) {
        gun.reset();
        gun.sample(clip, frame);
        word(clip);
        floating(frame);
        pose(gun);
      }
    }
    std::cerr << "M18 action fixture: " << rig.jointMatrices().size()
              << " body joints, " << gun.jointMatrices().size()
              << " weapon joints, original stationary action/SEQ/cMot3/waist\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
