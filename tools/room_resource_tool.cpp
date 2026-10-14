#include "archive.h"
#include "scene.h"
#include <cstring>
#include <iostream>
#include <memory>
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto a = re4::RoomArchive::Load(argv[1]);
    std::unique_ptr<re4::Smx> smx;
    if (const auto *x = a.find("SMX")) {
      smx = std::make_unique<re4::Smx>(a.copy(x - a.entries().data()));
      for (const auto &r : smx->records) {
        std::uint32_t u, v;
        std::memcpy(&u, &r.uvSpeed[0], 4);
        std::memcpy(&v, &r.uvSpeed[1], 4);
        std::uint32_t hash = 2166136261u;
        for (auto b : r.objectWork)
          hash = (hash ^ b) * 16777619u;
        std::cout << "X " << unsigned(r.work) << ' ' << unsigned(r.type) << ' '
                  << unsigned(r.order) << ' ' << unsigned(r.cull) << ' '
                  << r.lightMask << ' ' << r.flags << ' ' << r.material << ' '
                  << r.specular << ' ' << u << ' ' << v << ' ' << hash << '\n';
      }
    }
    const auto *e = a.find("SMD");
    if (!e)
      return 0;
    auto data = a.copy(e - a.entries().data());
    if (data.at(0) != 64)
      return 0;
    re4::Smd s(data);
    std::unique_ptr<re4::Smd> common;
    if (const auto *c = a.find("SMD", 1))
      common = std::make_unique<re4::Smd>(a.copy(c - a.entries().data()),
                                          s.commonRequirements());
    auto scene = re4::BuildStaticScene(s, -1, common.get());
    re4::AttachSceneTextures(s, scene, common.get());
    if (smx)
      re4::ApplySceneSmx(*smx, scene);
    std::cout << "S " << scene.included << ' ' << scene.skipped << ' '
              << scene.triangles << ' ' << scene.texturedTriangles << ' '
              << scene.fallbackTriangles << ' ' << scene.commonTextureTriangles
              << ' ' << scene.commonAlphaTriangles << ' '
              << scene.commonPlacements << '\n';
    auto texture = [&](int id) {
      if (id < 0)
        std::cout << " -1 -1 -1";
      else {
        const auto &t = scene.textures.at(id);
        std::cout << ' ' << t.width << ' ' << t.height << ' '
                  << unsigned(t.format);
      }
    };
    for (const auto &d : scene.draws) {
      std::cout << "D " << d.count << ' ' << d.palette << ' ' << d.image << ' '
                << d.commonModel << ' ' << d.alphaTest << ' ' << d.alphaImage;
      texture(d.texture);
      texture(d.alphaTexture);
      std::cout << ' ' << d.work << ' ' << unsigned(d.materialRGB[0]) << ' '
                << unsigned(d.materialRGB[1]) << ' '
                << unsigned(d.materialRGB[2]) << ' ' << d.blendMode << ' '
                << d.orderType << ' ' << d.alphaReference;
      for (auto speed : d.uvSpeed) {
        std::uint32_t bits;
        std::memcpy(&bits, &speed, 4);
        std::cout << ' ' << bits;
      }
      std::cout << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
