#include "archive.h"
#include "sat.h"
#include <cstring>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto a = re4::RoomArchive::Load(argv[1]);
    for (const auto &e : a.entries())
      if (e.tag[0] == 'S' && e.tag[1] == 'A' && e.tag[2] == 'T') {
        re4::Sat s(a.copy(&e - a.entries().data()));
        auto scene = s.scene();
        std::uint32_t hash = 2166136261u;
        auto add = [&](std::uint32_t v) {
          for (int k = 24; k >= 0; k -= 8)
            hash = (hash ^ ((v >> k) & 255)) * 16777619u;
        };
        for (const auto &p : s.polygons) {
          for (auto i : p.vertices)
            add(i);
          add(p.normal);
          for (auto i : p.edges)
            add(i);
          add(p.reserved);
          add(p.attributes);
          for (auto i : p.vertices)
            for (float f : s.vertices[i]) {
              std::uint32_t v;
              std::memcpy(&v, &f, 4);
              add(v);
            }
          for (float f : s.normals[p.normal]) {
            std::uint32_t v;
            std::memcpy(&v, &f, 4);
            add(v);
          }
        }
        std::cout << s.vertices.size() << ' ' << s.normals.size() << ' '
                  << s.edges.size() << ' ' << s.polygons.size() << ' '
                  << s.floor << ' ' << s.slope << ' ' << s.wall << ' '
                  << s.blocks << ' ' << s.blockReferences << ' ' << hash << ' '
                  << scene.triangles << '\n';
      }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
