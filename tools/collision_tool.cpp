#include "archive.h"
#include "walk_probe.h"
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto a = re4::RoomArchive::Load(argv[1]);
    auto e = a.find("SAT");
    if (!e)
      return 0;
    re4::Sat sat(a.copy(e - a.entries().data()));
    re4::SatCollision world(sat);
    std::cout << std::setprecision(17);
    for (std::size_t p = 0; p < sat.polygons.size();
         p += std::max(std::size_t(1), sat.polygons.size() / 32)) {
      auto t = world.triangle(p);
      re4::CollisionVec center{}, from{}, to{};
      for (int k = 0; k < 3; ++k) {
        center[k] = (t[0][k] + t[1][k] + t[2][k]) / 3;
        auto n = sat.normals[sat.polygons[p].normal][k];
        from[k] = center[k] + n * 500;
        to[k] = center[k] - n * 500;
      }
      auto hit = world.line(from, to);
      std::cout << "Q " << p;
      if (hit)
        std::cout << ' ' << hit->polygon << ' ' << hit->attributes << ' '
                  << hit->fraction << ' ' << hit->point[0] << ' '
                  << hit->point[1] << ' ' << hit->point[2];
      else
        std::cout << " -1";
      std::cout << '\n';
    }
    re4::WalkProbe probe(world);
    bool ready = probe.reset();
    std::cout << "W " << ready;
    if (ready) {
      for (int i = 0; i < 240; ++i)
        probe.step(.6, 1, i * .01, false, 1. / 60);
      auto pos = probe.position();
      std::cout << ' ' << pos[0] << ' ' << pos[1] << ' ' << pos[2] << ' '
                << probe.floorHit()->polygon;
    }
    std::cout << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
