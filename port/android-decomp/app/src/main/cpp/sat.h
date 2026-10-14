#pragma once
#include "scene.h"
namespace re4 {
struct SatPolygon {
  std::array<std::uint16_t, 3> vertices{}, edges{};
  std::uint16_t normal{}, reserved{};
  std::uint32_t attributes{};
};
struct ScenarioTriangle {
  std::array<std::array<float, 3>, 3> vertices{};
  std::uint32_t attributes{};
  // 0=floor, 1=slope (including box top/bottom), 2=wall.
  unsigned group = 2;
};
class Sat {
public:
  explicit Sat(const std::vector<std::uint8_t> &bytes);
  // Checked dynamic wall geometry, built from source object collision boxes.
  // All triangles are wall-group members; this never supplies invented floor.
  static Sat DynamicWalls(
      const std::vector<std::array<std::array<float, 3>, 3>> &triangles);
  static Sat ScenarioGeometry(const std::vector<ScenarioTriangle> &triangles);
  std::vector<std::array<float, 3>> vertices, normals, edges;
  std::vector<SatPolygon> polygons;
  // First encounter order in the original depth-first SAT block chain.
  std::vector<std::size_t> wallPolygonOrder;
  std::size_t floor{}, slope{}, wall{}, blocks{}, blockReferences{},
      offsetSlots{};
  StaticScene scene() const;
  std::string describe() const;
private:
  Sat() = default;
};
} // namespace re4
