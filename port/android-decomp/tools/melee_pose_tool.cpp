#include "leon_preview.h"
#include <cstring>
#include <iostream>
namespace {
void word(std::uint32_t value) {
  for (int k = 3; k >= 0; --k)
    std::cout.put(static_cast<char>(value >> (k * 8)));
}
void floating(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, 4);
  word(bits);
}
void pose(const re4::AnimationRig &rig) {
  for (const auto &joint : rig.jointMatrices())
    for (float value : joint)
      floating(value);
}
void state(const re4::AnimationRig &rig) {
  const auto status = rig.status();
  floating(status.curveFrame);
  floating(status.sequenceFrame);
  word(status.state);
  for (float frame : {12.f, 31.f, 36.f, 4.f, 6.f, 10.f, 17.f})
    word(rig.crossedFrame(frame));
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 4)
      return 2;
    re4::LeonPreview player(argv[1], argv[2], argv[3], true);
    auto &rig = player.rig();
    word(0x4d323100);
    word(rig.jointMatrices().size());
    word(60);
    for (unsigned clip = 20; clip < 30; ++clip)
      for (float fraction : {0.f, .125f, .25f, .5f, .875f, 1.f}) {
        rig.reset();
        const float frame = rig.maxFrame(clip) * fraction;
        rig.sample(clip, frame);
        word(clip);
        floating(frame);
        pose(rig);
      }
    const std::array<std::array<unsigned, 3>, 2> triplets{{{21, 22, 23}, {24, 25, 26}}};
    for (const auto &triplet : triplets) {
      const unsigned frames = unsigned(rig.maxFrame(triplet[0])) + 1;
      word(5 * frames);
      for (float rate : {-1.f, -.5f, 0.f, .5f, 1.f})
        for (unsigned frame = 0; frame < frames; ++frame) {
          rig.reset();
          rig.sampleBlend3(triplet, rate, float(frame));
          floating(rate);
          floating(float(frame));
          pose(rig);
        }
    }
    word(18);
    for (const auto &triplet : triplets)
      for (float rate : {-1.f, 0.f, 1.f})
        for (float waist : {-.35f, 0.f, .35f}) {
          rig.reset();
          rig.sampleBlend3(triplet, rate, 8);
          rig.setWaistYaw(waist);
          word(triplet[0]);
          floating(rate);
          floating(waist);
          pose(rig);
        }
    const std::array<float, 9> crossing{1, 1, 0, -1, -1, 0, .5f, -.5f, 1};
    const std::array<float, 9> hokan{-.5f, -.5f, 0, .5f, .5f, -.5f, -.5f, 0, .5f};
    word(4);
    for (const auto &triplet : triplets)
      for (unsigned blend : {0u, 3u}) {
        rig.reset();
        rig.startBlend3(triplet, 0, blend);
        word(triplet[0]);
        word(blend);
        word(9);
        for (float rate : blend ? hokan : crossing) {
          rig.setBlend3Rate(rate);
          rig.advanceAction();
          floating(rate);
          state(rig);
          pose(rig);
        }
      }
    word(4);
    for (unsigned clip : {20u, 27u, 28u, 29u}) {
      const unsigned flags = clip == 20 ? 4 : clip == 29 ? 1 : 0;
      const unsigned blend = clip == 20 ? 3 : clip == 29 ? 6 : 5;
      rig.reset();
      rig.startActionFlags(clip, blend, 0, flags);
      word(clip);
      word(60);
      for (unsigned tick = 0; tick < 60; ++tick) {
        rig.advanceAction();
        state(rig);
        pose(rig);
      }
    }
    std::cerr << "M21 knife/kick FCV, cMot3, Hokan, waist and event fixture\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
