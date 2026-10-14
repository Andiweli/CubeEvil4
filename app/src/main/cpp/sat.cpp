#include "sat.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>
#include <stdexcept>
namespace re4 {
Sat Sat::ScenarioGeometry(const std::vector<ScenarioTriangle> &triangles) {
  if (triangles.size() > 8191)
    throw std::runtime_error("Scenario collision: Triangle budget");
  Sat result;
  for (unsigned group = 0; group < 3; ++group)
    for (const auto &triangle : triangles) {
      if (triangle.group > 2)
        throw std::runtime_error("Scenario collision: Invalid polygon group");
      if (triangle.group != group) continue;
      for (const auto &point : triangle.vertices)
        for (const auto value : point)
          if (!std::isfinite(value) || std::abs(value) > 1e7f)
            throw std::runtime_error("Scenario collision: Coordinate budget");
      std::array<float,3> a{}, b{}, normal{};
      for (unsigned k=0;k<3;++k) {
        a[k]=triangle.vertices[1][k]-triangle.vertices[0][k];
        b[k]=triangle.vertices[2][k]-triangle.vertices[0][k];
      }
      normal={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
      const auto length=std::sqrt(double(normal[0])*normal[0]+double(normal[1])*normal[1]+double(normal[2])*normal[2]);
      if (!std::isfinite(length) || length < .001)
        throw std::runtime_error("Scenario collision: Degenerate polygon");
      for (auto &value : normal) value=float(value/length);
      SatPolygon polygon;polygon.attributes=triangle.attributes;
      polygon.normal=static_cast<std::uint16_t>(result.normals.size());
      for (unsigned k=0;k<3;++k) {
        polygon.vertices[k]=static_cast<std::uint16_t>(result.vertices.size());
        result.vertices.push_back(triangle.vertices[k]);
      }
      result.normals.push_back(normal);
      if (group==2) result.wallPolygonOrder.push_back(result.polygons.size());
      result.polygons.push_back(polygon);
      if (group==0) ++result.floor;else if(group==1) ++result.slope;else ++result.wall;
    }
  return result;
}
Sat Sat::DynamicWalls(
    const std::vector<std::array<std::array<float, 3>, 3>> &triangles) {
  if (triangles.size() > 1024)
    throw std::runtime_error("Object collision: Triangle budget");
  Sat result;
  for (const auto &triangle : triangles) {
    for (const auto &point : triangle)
      for (const auto value : point)
        if (!std::isfinite(value) || std::abs(value) > 1e7f)
          throw std::runtime_error("Object collision: Coordinate budget");
    std::array<float, 3> a{}, b{}, normal{};
    for (unsigned k = 0; k < 3; ++k) {
      a[k] = triangle[1][k] - triangle[0][k];
      b[k] = triangle[2][k] - triangle[0][k];
    }
    normal = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
              a[0] * b[1] - a[1] * b[0]};
    const auto length = std::sqrt(double(normal[0]) * normal[0] +
                                  double(normal[1]) * normal[1] +
                                  double(normal[2]) * normal[2]);
    if (!std::isfinite(length) || length < .001 ||
        std::abs(normal[1] / length) > .001)
      throw std::runtime_error("Object collision: no vertical wall");
    for (auto &value : normal) value = float(value / length);
    SatPolygon polygon;
    polygon.normal = static_cast<std::uint16_t>(result.normals.size());
    for (unsigned k = 0; k < 3; ++k) {
      polygon.vertices[k] = static_cast<std::uint16_t>(result.vertices.size());
      result.vertices.push_back(triangle[k]);
    }
    result.normals.push_back(normal);
    result.wallPolygonOrder.push_back(result.polygons.size());
    result.polygons.push_back(polygon);
  }
  result.wall = result.polygons.size();
  return result;
}
Sat::Sat(const std::vector<std::uint8_t> &b) {
  std::size_t begin = 0, end = b.size();
  auto range = [&](std::size_t p, std::size_t n) {
    if (p < begin || p > end || n > end - p)
      throw std::runtime_error("SAT: Range outside subfile bounds");
  };
  auto h = [&](std::size_t p) {
    range(p, 2);
    return std::uint16_t((unsigned(b[p]) << 8) | b[p + 1]);
  };
  auto u = [&](std::size_t p) {
    return (std::uint32_t(h(p)) << 16) | h(p + 2);
  };
  auto f = [&](std::size_t p) {
    auto bits = u(p);
    float value;
    std::memcpy(&value, &bits, 4);
    if (!std::isfinite(value))
      throw std::runtime_error("SAT: non-finite coordinate");
    return value;
  };
  range(0, 20);
  if (b[0] != 0xff && (b[0] & 0x80)) {
    auto first = u(4);
    if (first < 8 || first % 4 || first > 4 + 256 * 4)
      throw std::runtime_error("SAT: invalid offset table");
    range(4, first - 4);
    offsetSlots = (first - 4) / 4;
    for (std::size_t i = 0; i < offsetSlots; ++i) {
      auto offset = u(4 + i * 4);
      if (offset < first || offset >= b.size() || (offset & 0x80000000u))
        throw std::runtime_error("SAT: invalid subfile reference");
      if (offset > first)
        end = std::min(end, std::size_t(offset));
    }
    begin = first;
  }
  range(begin, 20);
  if (b[begin] != 0x20 && b[begin] != 0xff)
    throw std::runtime_error("SAT: unsupported file version");
  auto nv = h(begin + 2), nn = h(begin + 4), ne = h(begin + 6),
       np = h(begin + 10), nb = h(begin + 18);
  floor = h(begin + 12);
  slope = h(begin + 14);
  wall = h(begin + 16);
  if (np > 0x1fff || floor + slope + wall != np)
    throw std::runtime_error("SAT: Polygon/group bounds");
  std::size_t cursor = begin + 20;
  auto vectors = [&](unsigned count, auto &out) {
    range(cursor, std::size_t(count) * 12);
    out.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
      out.push_back({f(cursor), f(cursor + 4), f(cursor + 8)});
      cursor += 12;
    }
  };
  vectors(nv, vertices);
  vectors(nn, normals);
  vectors(ne, edges);
  range(cursor, std::size_t(np) * 20);
  for (unsigned i = 0; i < np; ++i) {
    SatPolygon p;
    for (unsigned k = 0; k < 3; ++k) {
      p.vertices[k] = h(cursor + k * 2);
      p.edges[k] = h(cursor + 8 + k * 2);
      if (p.vertices[k] >= nv || (p.edges[k] & 0x1fff) >= ne)
        throw std::runtime_error("SAT: Polygon index outside array bounds");
    }
    p.normal = h(cursor + 6);
    p.reserved = h(cursor + 14);
    p.attributes = u(cursor + 16);
    if (p.normal >= nn)
      throw std::runtime_error("SAT: Normal index outside array bounds");
    polygons.push_back(p);
    cursor += 20;
  }
  const auto blockStart = cursor;
  std::vector<std::size_t> pending;
  if (nb)
    pending.push_back(blockStart);
  std::set<std::size_t> visited;
  std::vector<std::pair<std::size_t, std::size_t>> spans;
  while (!pending.empty()) {
    auto p = pending.back();
    pending.pop_back();
    range(p, 36);
    if (p < blockStart || !visited.insert(p).second || visited.size() > nb)
      throw std::runtime_error("SAT: Block cycle or block budget");
    // Y is unused in the original XZ partition; do not interpret its padding.
    (void)f(p);
    (void)f(p + 8);
    (void)f(p + 12);
    (void)f(p + 20);
    auto flag = h(p + 30);
    auto next = u(p + 32);
    if (flag & 1) {
      spans.emplace_back(p, p + 36);
      pending.push_back(p + 36);
    } else {
      std::size_t count = std::size_t(h(p + 24)) + h(p + 26) + h(p + 28);
      range(p + 36, count * 2);
      spans.emplace_back(p, p + 36 + count * 2);
      for (std::size_t j = 0; j < count; ++j)
        if (h(p + 36 + j * 2) >= np)
          throw std::runtime_error("SAT: Block polygon index");
      blockReferences += count;
    }
    if (next) {
      if (next < 36 || next > end - p)
        throw std::runtime_error("SAT: Block reference");
      pending.push_back(p + next);
    }
  }
  std::sort(spans.begin(), spans.end());
  std::vector<bool> wallSeen(np,false);
  for(const auto &span:spans) {
    const auto p=span.first;
    if(h(p+30)&1) continue;
    const auto start=std::size_t(h(p+24))+h(p+26);
    for(std::size_t j=0;j<h(p+28);++j) {
      const auto polygon=h(p+36+(start+j)*2);
      if(!wallSeen[polygon]) { wallPolygonOrder.push_back(polygon); wallSeen[polygon]=true; }
    }
  }
  for (std::size_t i = 1; i < spans.size(); ++i)
    if (spans[i].first < spans[i - 1].second)
      throw std::runtime_error("SAT: overlapping block data");
  blocks = visited.size();
  if (blocks != nb)
    throw std::runtime_error("SAT: declared block count mismatch");
}
StaticScene Sat::scene() const {
  StaticScene s;
  for (std::size_t i = 0; i < polygons.size(); ++i) {
    const auto &p = polygons[i];
    // Diagnostic group colours, not game materials or original debug colours.
    std::array<std::uint8_t, 4> c =
        i < floor ? std::array<std::uint8_t, 4>{60, 210, 90, 255}
                  : (i < floor + slope
                         ? std::array<std::uint8_t, 4>{240, 190, 50, 255}
                         : std::array<std::uint8_t, 4>{70, 130, 240, 255});
    for (auto id : p.vertices) {
      const auto &v = vertices.at(id);
      const auto &n = normals.at(p.normal);
      for (unsigned k = 0; k < 3; ++k) {
        if (s.vertices.empty())
          s.minimum = s.maximum = v;
        else {
          s.minimum[k] = std::min(s.minimum[k], v[k]);
          s.maximum[k] = std::max(s.maximum[k], v[k]);
        }
        s.vertices.push_back(v[k]);
        s.normals.push_back(n[k]);
      }
      s.colors.insert(s.colors.end(), c.begin(), c.end());
      s.uv.insert(s.uv.end(), {0, 0});
      if (n == std::array<float, 3>{})
        ++s.zeroNormals;
    }
  }
  if (!polygons.empty()) {
    SceneDraw d;
    d.count = s.vertices.size() / 3;
    d.vertexColors = true;
    s.draws.push_back(d);
    s.included = 1;
  }
  s.triangles = s.vertexColorTriangles = polygons.size();
  return s;
}
std::string Sat::describe() const {
  std::ostringstream s;
  s << "SAT[0], Variant 0: " << polygons.size() << " triangles · " << floor
    << " floor · " << slope << " slope · " << wall << " wall\n"
    << vertices.size() << " Vertices · " << normals.size() << " normals · "
    << edges.size() << " edges\n"
    << blocks << " blocks · " << blockReferences
    << " polygon references verified\n";
  return s.str();
}
} // namespace re4
