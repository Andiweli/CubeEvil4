#include "archive.h"
#include "scene.h"
#include <cstring>
#include <iostream>
void word(std::uint32_t v) {
  for (int i = 3; i >= 0; --i)
    std::cout.put(static_cast<char>(v >> (i * 8)));
}
void number(float f) {
  std::uint32_t v;
  std::memcpy(&v, &f, 4);
  word(v);
}
std::uint32_t hashBytes(const std::uint8_t *begin, std::size_t count) {
  std::uint32_t hash = 2166136261u;
  for (std::size_t i = 0; i < count; ++i)
    hash = (hash ^ begin[i]) * 16777619u;
  return hash;
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto a = re4::RoomArchive::Load(argv[1]);
    for (int occurrence = 0; const auto *e = a.find("SMD", occurrence);
         ++occurrence) {
      auto data = a.copy(e - a.entries().data());
      if (data.at(0) != 64)
        continue;
      re4::Smd s(data);
      auto scene = re4::BuildStaticScene(s);
      re4::AttachSceneTextures(s, scene);
      word(occurrence);
      word(s.models.size());
      for (const auto &m : s.models) {
        word(m.texcoords.size());
        word(m.colored);
        word(m.vertexAmbient);
        word(m.colors.size());
        std::uint32_t colorHash = 2166136261u;
        for (const auto &color : m.colors)
          for (auto byte : color)
            colorHash = (colorHash ^ byte) * 16777619u;
        word(colorHash);
        for (const auto &uv : m.texcoords) {
          number(uv[0]);
          number(uv[1]);
        }
      }
      word(scene.draws.size());
      word(scene.texturedTriangles);
      word(scene.fallbackTriangles);
      for (const auto &d : scene.draws) {
        word(d.first);
        word(d.count);
        word(d.palette);
        word(d.image);
        word(d.repeat);
        word(d.texture >= 0);
        word(d.alphaTest);
        word(d.alphaImage);
        word(d.alphaReference);
        word(d.alphaTest && d.texture >= 0 && d.alphaTexture >= 0);
        word(d.vertexColors);
        word(hashBytes(scene.colors.data() + d.first * 4, d.count * 4));
        for (std::size_t i = d.first * 2; i < (d.first + d.count) * 2; ++i)
          number(scene.uv.at(i));
      }
      word(scene.textures.size());
      for (const auto &t : scene.textures) {
        word(t.width);
        word(t.height);
        word(t.format);
        std::uint32_t hash = 2166136261u;
        for (std::size_t i = 3; i < t.rgba.size(); i += 4)
          hash = (hash ^ t.rgba[i]) * 16777619u;
        word(hash);
        word(t.minLOD);
        word(t.maxLOD);
        word(t.mips.size());
        for (const auto &m : t.mips) {
          word(m.width);
          word(m.height);
          std::uint32_t rgbaHash = 2166136261u;
          for (auto byte : m.rgba)
            rgbaHash = (rgbaHash ^ byte) * 16777619u;
          word(rgbaHash);
        }
      }
    }
    if (!std::cout)
      throw std::runtime_error("export failed");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
