#include "archive.h"
#include "scene.h"
#include <cstring>
#include <iomanip>
#include <iostream>
void word(std::uint32_t v) {
  for (int i = 3; i >= 0; --i)
    std::cout.put(static_cast<char>(v >> (i * 8)));
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto a = re4::RoomArchive::Load(argv[1]);
    for (int occurrence = 0; const auto *e = a.find("SMD", occurrence);
         ++occurrence) {
      auto bytes = a.copy(static_cast<std::size_t>(e - a.entries().data()));
      if (bytes.size() < 16)
        throw std::runtime_error("Short SMD");
      if (bytes[0] != 64)
        continue;
      re4::Smd s(bytes);
      auto scene = re4::BuildStaticScene(s);
      std::vector<float> floats;
      for (const auto &m : s.models) {
        auto rest = re4::RestJointCenters(m);
        for (std::size_t i = 0; i < m.joints.size(); ++i)
          for (auto f : rest[i])
            floats.push_back(f);
        for (const auto &n : m.normals)
          for (auto f : n)
            floats.push_back(f);
      }
      for (const auto &w : s.placements)
        for (auto f : re4::PlacementMatrix(w))
          floats.push_back(f);
      floats.insert(floats.end(), scene.vertices.begin(), scene.vertices.end());
      floats.insert(floats.end(), scene.normals.begin(), scene.normals.end());
      word(occurrence);
      word(scene.included);
      word(scene.skipped);
      word(scene.triangles);
      word(floats.size());
      word(scene.zeroNormals);
      word(scene.singularNormalPlacements);
      for (auto f : floats) {
        std::uint32_t v;
        std::memcpy(&v, &f, 4);
        word(v);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
