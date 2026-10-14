#include "leon_preview.h"
#include "weapon_preview.h"
#include <algorithm>
#include <array>
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
  std::memcpy(&bits, &value, sizeof(bits));
  word(bits);
}
template <class Values> void floats(const Values &values) {
  for (float value : values)
    floating(value);
}
void weight(const re4::BoneWeight &value) {
  word(value.count);
  for (unsigned i = 0; i < 3; ++i) {
    word(value.joints[i]);
    floating(value.rates[i]);
  }
}
void pixels(const std::vector<std::uint8_t> &rgba) {
  word(static_cast<std::uint32_t>(rgba.size()));
  std::cout.write(reinterpret_cast<const char *>(rgba.data()), rgba.size());
}
std::array<float, 11> frames(float maximum) {
  return {0,
          3,
          6,
          maximum * .125f,
          maximum * .25f,
          maximum * .5f,
          maximum * .875f,
          maximum,
          std::min(maximum, 14.f),
          std::min(maximum, 31.f),
          std::min(maximum, 39.f)};
}
// Export posed corners as well as the matrices. This checks the original
// position and normal indices independently, including inverse bind for the
// position and the whole model's attachment to the raw right-hand joint.
std::array<float, 3> skinned(const re4::WeaponPreview &weapon,
                             std::size_t corner, bool normal) {
  const auto &scene = weapon.mesh();
  const auto &source = normal ? scene.normals : scene.vertices;
  const auto &influences =
      normal ? weapon.skin()[corner].normal : weapon.skin()[corner].position;
  std::array<float, 3> value{}, result{};
  std::copy_n(source.begin() + corner * 3, 3, value.begin());
  for (unsigned i = 0; i < influences.count; ++i) {
    const auto joint = influences.joints[i];
    const auto &matrix = weapon.rig().bones().at(joint);
    const auto &bind = weapon.rig().bindCenters().at(joint);
    for (unsigned row = 0; row < 3; ++row) {
      float sum = normal ? 0 : matrix[row * 4 + 3];
      for (unsigned column = 0; column < 3; ++column)
        sum += matrix[row * 4 + column] *
               (value[column] - (normal ? 0 : bind[column]));
      result[row] += influences.rates[i] * sum;
    }
  }
  return result;
}
std::array<float, 3> direction(const re4::Matrix34 &matrix,
                               const std::array<float, 3> &value) {
  std::array<float, 3> result{};
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned column = 0; column < 3; ++column)
      result[row] += matrix[row * 4 + column] * value[column];
  return result;
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 3)
      return 2;
    re4::WeaponPreview weapon(argv[2]);
    re4::LeonPreview actor(argv[1], argv[2]);
    std::cerr << weapon.describe() << '\n';
    const auto &scene = weapon.mesh();
    const auto corners = weapon.skin().size();
    if (scene.vertices.size() != corners * 3 ||
        scene.normals.size() != corners * 3 || scene.uv.size() != corners * 2)
      throw std::runtime_error("WEP01 exporter: incomplete mesh");
    std::array<std::size_t, 3> representatives{};
    for (unsigned joint = 0; joint < representatives.size(); ++joint) {
      auto found = std::find_if(weapon.skin().begin(), weapon.skin().end(),
                                [joint](const re4::SkinCorner &corner) {
                                  return corner.position.count == 1 &&
                                         corner.position.joints[0] == joint;
                                });
      if (found == weapon.skin().end())
        throw std::runtime_error("WEP01 exporter: missing local joint");
      representatives[joint] = std::size_t(found - weapon.skin().begin());
    }
    word(0x57504631); // WPF1, big endian fixture protocol.
    word(static_cast<std::uint32_t>(weapon.rig().bones().size()));
    word(static_cast<std::uint32_t>(corners));
    word(static_cast<std::uint32_t>(scene.draws.size()));
    word(static_cast<std::uint32_t>(scene.textures.size()));
    for (const auto &bind : weapon.rig().bindCenters())
      floats(bind);
    for (std::size_t i = 0; i < corners; ++i) {
      for (unsigned component = 0; component < 3; ++component)
        floating(scene.vertices[i * 3 + component]);
      for (unsigned component = 0; component < 3; ++component)
        floating(scene.normals[i * 3 + component]);
      floating(scene.uv[i * 2]);
      floating(scene.uv[i * 2 + 1]);
      weight(weapon.skin()[i].position);
      weight(weapon.skin()[i].normal);
      for (unsigned component = 0; component < 4; ++component)
        word(scene.colors[i * 4 + component]);
    }
    for (const auto &draw : scene.draws) {
      for (std::uint32_t value :
           {static_cast<std::uint32_t>(draw.first),
            static_cast<std::uint32_t>(draw.count),
            static_cast<std::uint32_t>(draw.texture), draw.image,
            std::uint32_t(draw.alphaTest),
            static_cast<std::uint32_t>(draw.alphaTexture), draw.alphaImage,
            draw.alphaReference, std::uint32_t(draw.vertexColors)})
        word(value);
    }
    for (const auto &texture : scene.textures) {
      for (unsigned value :
           {texture.width, texture.height, texture.format, texture.wrapS,
            texture.wrapT, texture.minLOD, texture.maxLOD,
            static_cast<unsigned>(texture.mips.size())})
        word(value);
      pixels(texture.rgba);
      for (const auto &mip : texture.mips) {
        word(mip.width);
        word(mip.height);
        pixels(mip.rgba);
      }
    }
    for (auto corner : representatives)
      word(static_cast<std::uint32_t>(corner));

    re4::SmdPlacement placement;
    placement.position = {1300.f, -200.f, 950.f};
    placement.rotation = {0.f, .73f, 0.f};
    placement.scale = {1.f, 1.f, 1.f};
    const auto actorPlacement = re4::PlacementMatrix(placement);
    std::array<re4::Matrix34, 3> attachments{};
    for (unsigned clip = 0; clip < 3; ++clip) {
      const auto frame = actor.rig().maxFrame(clip) * .375f;
      actor.rig().sample(clip, frame);
      const auto &hand = actor.rig().bones().at(10);
      attachments[clip] = re4::WeaponAttachmentMatrix(hand, actorPlacement);
      word(clip);
      floating(frame);
      floats(hand);
      floats(actorPlacement);
      floats(attachments[clip]);
    }
    for (unsigned clip = 0; clip < 6; ++clip) {
      // Some FCVs intentionally omit channels. Match cObjFn57::init's initial
      // idle pose, then isolate the clip from any previous reload/fire history.
      re4::WeaponPreview isolated(argv[2]);
      const auto motion = static_cast<re4::WeaponMotion>(clip % 3);
      const bool empty = clip >= 3;
      const auto maximum = isolated.maxFrame(motion, empty);
      for (float frame : frames(maximum)) {
        isolated.sample(motion, frame, empty);
        word(clip);
        floating(frame);
        floating(maximum);
        for (const auto &bone : isolated.rig().bones())
          floats(bone);
        for (const auto &attachment : attachments)
          for (auto corner : representatives) {
            floats(re4::TransformPoint(attachment,
                                       skinned(isolated, corner, false)));
            floats(direction(attachment, skinned(isolated, corner, true)));
          }
      }
    }
    if (!std::cout.good())
      throw std::runtime_error("WEP01 exporter: write failed");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
