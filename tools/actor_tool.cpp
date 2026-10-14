#include "leon_preview.h"
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
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      return 2;
    re4::LeonPreview actor(argv[1], argv[2]);
    std::cerr << actor.describe() << '\n';
    word(static_cast<std::uint32_t>(actor.rig().bones().size()));
    word(static_cast<std::uint32_t>(actor.mesh().vertices.size() / 3));
    for (unsigned clip = 0; clip < 3; ++clip) {
      float max = actor.rig().maxFrame(clip);
      for (float frame :
           {0.f, max * .125f, max * .25f, max * .5f, max * .875f, max}) {
        actor.rig().sample(clip, frame);
        word(clip);
        floating(frame);
        for (const auto &bone : actor.rig().bones())
          for (auto f : bone)
            floating(f);
      }
    }
    // Original SEQ playback, looping and transition blending over 10 seconds.
    actor.reset();
    for (unsigned i = 0; i < 300; ++i) {
      actor.update({0, 0, 0}, i < 90 ? 0 : 100, 0, i >= 210, 1. / 30);
      for (const auto &bone : actor.rig().bones())
        for (auto f : bone)
          floating(f);
    }
    for (std::size_t i = 0; i < actor.skin().size(); ++i) {
      for (auto f :
           {actor.mesh().vertices[i * 3], actor.mesh().vertices[i * 3 + 1],
            actor.mesh().vertices[i * 3 + 2], actor.mesh().normals[i * 3],
            actor.mesh().normals[i * 3 + 1], actor.mesh().normals[i * 3 + 2]})
        floating(f);
      for (const auto &weight :
           {actor.skin()[i].position, actor.skin()[i].normal}) {
        word(weight.count);
        for (unsigned j = 0; j < 3; ++j) {
          word(weight.joints[j]);
          floating(weight.rates[j]);
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
