#include "collision_query.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
namespace re4 {
namespace {
using V = CollisionVec;
V add(V a, V b) {
  for (int k = 0; k < 3; ++k)
    a[k] += b[k];
  return a;
}
V sub(V a, V b) {
  for (int k = 0; k < 3; ++k)
    a[k] -= b[k];
  return a;
}
V mul(V a, double t) {
  for (auto &x : a)
    x *= t;
  return a;
}
double dot(V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(V a, V b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
double length(V a) { return std::sqrt(dot(a, a)); }
V unit(V a) {
  auto n = length(a);
  return n > 1e-12 ? mul(a, 1 / n) : V{};
}
bool finite(V a) {
  return std::all_of(a.begin(), a.end(), [](double v) {
    return std::isfinite(v) && std::abs(v) < 1e12;
  });
}
using FV = std::array<float, 3>;
FV fv(const V &v) { return {float(v[0]), float(v[1]), float(v[2])}; }
FV fsub(FV a, const FV &b) {
  for (unsigned k = 0; k < 3; ++k) a[k] -= b[k];
  return a;
}
FV fadd(FV a, const FV &b) {
  for (unsigned k = 0; k < 3; ++k) a[k] += b[k];
  return a;
}
FV fscale(FV a, float scale) {
  for (auto &v : a) v *= scale;
  return a;
}
float fdot(const FV &a, const FV &b) {
  return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2];
}
FV fcross(const FV &a, const FV &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
bool sourceInside(const std::array<FV, 3> &tri, const FV &n, const FV &p) {
  for (unsigned k = 0; k < 3; ++k)
    if (fdot(fcross(fsub(tri[(k + 1) % 3], tri[k]), fsub(p, tri[k])), n) < 0)
      return false;
  return true;
}
bool sourcePlaneLine(FV &hit, const FV &a, const FV &n, const FV &old,
                     const FV &pos) {
  const float d0 = fdot(n, fsub(old, a)), d1 = fdot(n, fsub(pos, a));
  hit = pos;
  if (d0 * d1 >= 0) return false;
  const auto scale = std::abs(d1) / (std::abs(d0) + std::abs(d1));
  hit = fadd(pos, fscale(fsub(old, pos), scale));
  return true;
}
// Literal selected At_poly_sphere_ck2 geometry (source at_sub.cpp401).
// Filters are applied by the owner; face and V0/V1/V2 edge rules stay here.
bool sourceSphere(const std::array<FV, 3> &tri, const FV &n, unsigned attributes,
                  const FV &old, FV &pos, float radius, bool edges) {
  if (fdot(n, old) - fdot(n, tri[0]) < 0) return false;
  const auto distance = fdot(n, pos) - fdot(n, tri[0]);
  FV hit{};
  if (std::abs(distance) > radius) {
    if (!sourcePlaneLine(hit, tri[0], n, old, pos) || !sourceInside(tri, n, hit))
      return false;
    pos = fadd(hit, fscale(n, radius));
    return true;
  }
  hit = fsub(pos, fscale(n, distance));
  if (sourceInside(tri, n, hit)) {
    pos = fadd(hit, fscale(n, radius));
    return true;
  }
  if (!edges) return false;
  for (unsigned k = 0; k < 3; ++k) {
    if (attributes & (1u << (k + 29))) continue;
    const auto edge = fsub(tri[(k + 1) % 3], tri[k]);
    const auto d = fsub(pos, tri[k]);
    const auto lengthSq = fdot(edge, edge), along = fdot(d, edge);
    if (along < .01f || lengthSq < along) continue;
    hit = fadd(tri[k], fscale(edge, along / lengthSq));
    auto correction = fsub(pos, hit);
    const auto squared = fdot(correction, correction);
    if (squared >= radius * radius) continue;
    if (fdot(correction, n) > 0) {
      correction = fscale(correction, 1.f / std::sqrt(squared));
      pos = fadd(hit, fscale(correction, radius * .99f));
    }
    return true;
  }
  return false;
}
// Barycentric plane intersection, both sides; zero area/parallel returns no
// hit.
std::optional<double> segment(V a, V b, const std::array<V, 3> &tri) {
  auto ab = sub(tri[1], tri[0]), ac = sub(tri[2], tri[0]), dir = sub(b, a),
       n = cross(ab, ac);
  double den = dot(n, dir), size = length(n);
  if (size < 1e-9 || std::abs(den) <= size * length(dir) * 1e-12)
    return {};
  double t = dot(n, sub(tri[0], a)) / den;
  if (t < 0 || t >= 1)
    return {};
  V p = sub(add(a, mul(dir, t)), tri[0]);
  double aa = dot(ab, ab), bb = dot(ab, ac), cc = dot(ac, ac), pa = dot(p, ab),
         pc = dot(p, ac), det = aa * cc - bb * bb;
  if (det <= 1e-12)
    return {};
  double u = (cc * pa - bb * pc) / det, v = (aa * pc - bb * pa) / det;
  if (u < -1e-9 || v < -1e-9 || u + v > 1 + 1e-9)
    return {};
  return t;
}
V closestEdge(V p, V a, V b) {
  V d = sub(b, a);
  double l = dot(d, d);
  return add(
      a, mul(d, l > 1e-12 ? std::clamp(dot(sub(p, a), d) / l, 0., 1.) : 0.));
}
V closestTriangle(V p, const std::array<V, 3> &t) {
  V ab = sub(t[1], t[0]), ac = sub(t[2], t[0]), n = unit(cross(ab, ac));
  if (length(n) > 0) {
    double d = dot(sub(p, t[0]), n);
    V plane = sub(p, mul(n, d));
    // Projection is inside iff the three consistently wound edge signs agree.
    bool inside = true;
    for (int k = 0; k < 3; ++k)
      if (dot(cross(sub(t[(k + 1) % 3], t[k]), sub(plane, t[k])), n) < -1e-7)
        inside = false;
    if (inside)
      return plane;
  }
  V best = t[0];
  double distance = dot(sub(p, best), sub(p, best));
  for (int k = 0; k < 3; ++k) {
    V q = closestEdge(p, t[k], t[(k + 1) % 3]);
    double d = dot(sub(p, q), sub(p, q));
    if (d < distance) {
      distance = d;
      best = q;
    }
  }
  return best;
}
struct Pair {
  V body, surface;
  double distance;
};
Pair pair(V a, V b) { return {a, b, length(sub(a, b))}; }
Pair segmentPair(V p, V q, V a, V b) {
  V d = sub(q, p), e = sub(b, a), r = sub(p, a);
  double dd = dot(d, d), ee = dot(e, e), de = dot(d, e), dr = dot(d, r),
         er = dot(e, r), s = 0, t = 0;
  if (dd <= 1e-12)
    return pair(p, closestEdge(p, a, b));
  if (ee <= 1e-12)
    return pair(closestEdge(a, p, q), a);
  double den = dd * ee - de * de;
  if (den > 1e-12)
    s = std::clamp((de * er - dr * ee) / den, 0., 1.);
  t = (de * s + er) / ee;
  if (t < 0) {
    t = 0;
    s = std::clamp(-dr / dd, 0., 1.);
  } else if (t > 1) {
    t = 1;
    s = std::clamp((de - dr) / dd, 0., 1.);
  }
  return pair(add(p, mul(d, s)), add(a, mul(e, t)));
}
Pair capsulePair(V feet, double radius, double height,
                 const std::array<V, 3> &tri) {
  V a = feet, b = feet;
  a[1] += radius;
  b[1] += height - radius;
  if (auto hit = segment(a, b, tri)) {
    V p = add(a, mul(sub(b, a), *hit));
    return pair(p, p);
  }
  Pair best = pair(a, closestTriangle(a, tri));
  auto end = pair(b, closestTriangle(b, tri));
  if (end.distance < best.distance)
    best = end;
  for (int k = 0; k < 3; ++k) {
    auto edge = segmentPair(a, b, tri[k], tri[(k + 1) % 3]);
    if (edge.distance < best.distance)
      best = edge;
  }
  return best;
}
void validCapsule(V feet, double radius, double height) {
  if (!finite(feet) || !std::isfinite(radius) || !std::isfinite(height) ||
      radius <= 0 || height < 2 * radius || height > 1e8)
    throw std::runtime_error("SAT: invalid test capsule");
}
} // namespace
bool SatCollision::accepts(std::uint32_t a, std::uint32_t f,
                           std::uint32_t mask, SatQueryMode mode) {
  if (mode == SatQueryMode::Effect)
    return !(mask & a) && !((f & 0x400) && (a & 0x4000)) &&
           !((f & 0x800) && (a & 0x400000));
  return !(mask & a) && !((f & 0x1000) && (a & 0x400000)) &&
         !((f & 0x2000) && (a & 0x4000)) && !((f & 0x4000) && (a & 0x40)) &&
         !((f & 0x800) && (a & 0x8000)) && !((f & 0x8000) && (a & 0x800000)) &&
         !((a & 0x400) && !(f & 0x8000));
}
std::array<CollisionVec, 3> SatCollision::triangle(std::size_t p) const {
  if (p >= sat_.polygons.size() && scenario_)
    return scenario_->triangle(p - sat_.polygons.size());
  std::array<V, 3> t{};
  const auto &poly = sat_.polygons.at(p);
  for (int j = 0; j < 3; ++j)
    for (int k = 0; k < 3; ++k)
      t[j][k] = sat_.vertices.at(poly.vertices[j])[k];
  return t;
}
void SatCollision::setScenarioGeometry(Sat geometry) {
  auto mesh=std::make_unique<Sat>(std::move(geometry));
  auto query=std::make_unique<SatCollision>(*mesh,mode_);
  scenario_.swap(query);scenarioMesh_.swap(mesh);
}
SatCollision::SatCollision(const Sat &s, SatQueryMode mode)
    : sat_(s), mode_(mode) {
  wallRank_.resize(s.polygons.size());
  std::iota(wallRank_.begin(),wallRank_.end(),s.polygons.size());
  for(std::size_t k=0;k<s.wallPolygonOrder.size();++k) wallRank_[s.wallPolygonOrder[k]]=k;
  if (mode != SatQueryMode::Sight && mode != SatQueryMode::Effect)
    throw std::runtime_error("SAT/EAT: invalid query mode");
  order_.resize(s.polygons.size());
  std::iota(order_.begin(), order_.end(), 0);
  boxes_.resize(order_.size());
  for (auto p : order_) {
    auto t = triangle(p);
    boxes_[p] = {t[0], t[0]};
    for (auto v : t)
      for (int k = 0; k < 3; ++k) {
        boxes_[p].low[k] = std::min(boxes_[p].low[k], v[k]);
        boxes_[p].high[k] = std::max(boxes_[p].high[k], v[k]);
      }
  }
  if (!order_.empty())
    build(0, order_.size());
}
int SatCollision::build(std::size_t begin, std::size_t end) {
  Node n;
  n.begin = begin;
  n.end = end;
  n.box = boxes_[order_[begin]];
  for (auto i = begin + 1; i < end; ++i)
    for (int k = 0; k < 3; ++k) {
      n.box.low[k] = std::min(n.box.low[k], boxes_[order_[i]].low[k]);
      n.box.high[k] = std::max(n.box.high[k], boxes_[order_[i]].high[k]);
    }
  int index = int(nodes_.size());
  nodes_.push_back(n);
  if (end - begin > 8) {
    int axis = 0;
    for (int k = 1; k < 3; ++k)
      if (n.box.high[k] - n.box.low[k] > n.box.high[axis] - n.box.low[axis])
        axis = k;
    auto middle = begin + (end - begin) / 2;
    std::nth_element(order_.begin() + begin, order_.begin() + middle,
                     order_.begin() + end, [&](auto a, auto b) {
                       auto x = boxes_[a].low[axis] + boxes_[a].high[axis],
                            y = boxes_[b].low[axis] + boxes_[b].high[axis];
                       return x == y ? a < b : x < y;
                     });
    int left = build(begin, middle), right = build(middle, end);
    nodes_[index].left = left;
    nodes_[index].right = right;
  }
  return index;
}
std::vector<std::size_t> SatCollision::candidates(const Box &b,
                                                  SatSubset subset) const {
  std::vector<std::size_t> result;
  if (nodes_.empty())
    return result;
  std::vector<int> todo{0};
  auto overlap = [&](const Box &q) {
    for (int k = 0; k < 3; ++k)
      if (q.high[k] < b.low[k] || q.low[k] > b.high[k])
        return false;
    return true;
  };
  while (!todo.empty()) {
    auto i = todo.back();
    todo.pop_back();
    const auto &n = nodes_[i];
    if (!overlap(n.box))
      continue;
    if (n.left >= 0) {
      todo.push_back(n.left);
      todo.push_back(n.right);
      continue;
    }
    for (auto j = n.begin; j < n.end; ++j) {
      auto p = order_[j];
      bool ground = p < sat_.floor + sat_.slope;
      if ((subset == SatSubset::Ground && !ground) ||
          (subset == SatSubset::Walls && ground) || !overlap(boxes_[p]))
        continue;
      result.push_back(p);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}
std::optional<SatHit> SatCollision::line(const V &from, const V &to,
                                         SatSubset subset, std::uint32_t flags,
                                         std::uint32_t mask) const {
  if (!finite(from) || !finite(to))
    throw std::runtime_error("SAT: invalid line segment");
  Box b{from, from};
  for (int k = 0; k < 3; ++k) {
    b.low[k] = std::min(from[k], to[k]) - 1e-5;
    b.high[k] = std::max(from[k], to[k]) + 1e-5;
  }
  std::optional<SatHit> best;
  for (auto p : candidates(b, subset)) {
    auto &poly = sat_.polygons[p];
    if (!accepts(poly.attributes, flags, mask, mode_))
      continue;
    if (auto t = segment(from, to, triangle(p)))
      if (!best || *t < best->fraction) {
        const auto &n = sat_.normals[poly.normal];
        best =
            SatHit{add(from, mul(sub(to, from), *t)), unit({n[0], n[1], n[2]}),
                   p, poly.attributes | 0x01000000, *t};
      }
  }
  if (scenario_)
    if (auto hit=scenario_->line(from,to,subset,flags,mask))
      if (!best || hit->fraction < best->fraction) {
        hit->polygon+=sat_.polygons.size();best=hit;
      }
  return best;
}
std::optional<SatHit> SatCollision::ground(const V &feet, double above,
                                           double below) const {
  if (!std::isfinite(above) || !std::isfinite(below) || above < 0 || below <= 0)
    throw std::runtime_error("SAT: invalid floor range");
  V a = feet, b = feet;
  a[1] += above;
  b[1] -= below;
  return line(a, b, SatSubset::Ground);
}
bool SatCollision::clearCapsule(const V &feet, double radius,
                                double height, SatSidedness sides) const {
  validCapsule(feet, radius, height);
  if (scenario_ && !scenario_->clearCapsule(feet,radius,height,sides)) return false;
  Box b{feet, feet};
  for (int k : {0, 2}) {
    b.low[k] -= radius;
    b.high[k] += radius;
  }
  b.high[1] += height;
  for (auto p : candidates(b, SatSubset::Walls)) {
    if (sides == SatSidedness::AuthoredFront) {
      const auto n = sat_.normals[sat_.polygons[p].normal];
      auto center = feet; center[1] += radius;
      if (dot({n[0], n[1], n[2]}, sub(center, triangle(p)[0])) < 0)
        continue;
    }
    if (accepts(sat_.polygons[p].attributes, 0x1000, 0, mode_) &&
        capsulePair(feet, radius, height, triangle(p)).distance < radius + .1)
      return false;
  }
  return true;
}
SatSphereResolution SatCollision::resolveScenerySphere(const V &previous,
    const V &current, float radius, std::uint32_t flags) const {
  if (!finite(previous) || !finite(current) || !std::isfinite(radius) ||
      radius <= 0 || radius > 1e6)
    throw std::runtime_error("SAT: invalid original body sphere");
  SatSphereResolution result;
  auto pos = fv(current);
  const auto old = fv(previous);
  const auto center = [&] { return V{pos[0], pos[1], pos[2]}; };
  const auto lineAdjust = [&] {
    if (!(flags & 1)) return;
    if (const auto hit = line(previous, center(), SatSubset::All, flags)) {
      pos = fadd(fv(hit->point), fscale(fv(hit->normal), radius));
      result.normal = hit->normal;
      ++result.contacts;
    }
  };
  lineAdjust();
  // source wallAdjust tests faces with EDGE_CANCEL then full edges. The
  // original ordered block references avoid BVH traversal-order dependence.
  for (bool edges : {false, true}) {
    Box box;
    for (unsigned k = 0; k < 3; ++k) {
      box.low[k] = std::min(previous[k], double(pos[k])) - radius;
      box.high[k] = std::max(previous[k], double(pos[k])) + radius;
    }
    auto order = candidates(box, SatSubset::Walls);
    std::sort(order.begin(), order.end(),
      [&](auto a, auto b) { return wallRank_[a] < wallRank_[b]; });
    for (const auto polygon : order) {
      const auto &p = sat_.polygons[polygon];
      if (!accepts(p.attributes, flags, 0, mode_)) continue;
      const auto t = triangle(polygon);
      const std::array<FV, 3> tri{fv(t[0]), fv(t[1]), fv(t[2])};
      const auto &normal = sat_.normals[p.normal];
      if (sourceSphere(tri, normal, p.attributes, old, pos, radius, edges)) {
        result.normal = {normal[0], normal[1], normal[2]};
        ++result.contacts;
      }
    }
  }
  lineAdjust();
  result.position = center();
  if (scenario_) {
    const auto contact=scenario_->resolveScenerySphere(previous,result.position,radius,flags);
    result.position=contact.position;result.contacts+=contact.contacts;
    if (contact.contacts) result.normal=contact.normal;
  }
  return result;
}
std::optional<SatHit> SatCollision::sweepCapsule(const V &feet, const V &delta,
                                                 double radius, double height,
                                                 SatSubset subset,
                                                 std::uint32_t flags,
                                                 SatSidedness sides) const {
  validCapsule(feet, radius, height);
  if (!finite(delta))
    throw std::runtime_error("SAT: invalid motion");
  double speed = length(delta);
  if (speed < 1e-9)
    return {};
  Box b{feet, feet};
  for (int k = 0; k < 3; ++k) {
    b.low[k] = std::min(feet[k], feet[k] + delta[k]);
    b.high[k] = std::max(feet[k], feet[k] + delta[k]);
  }
  for (int k : {0, 2}) {
    b.low[k] -= radius;
    b.high[k] += radius;
  }
  b.high[1] += height;
  std::optional<SatHit> best;
  for (auto p : candidates(b, subset)) {
    auto &poly = sat_.polygons[p];
    if (!accepts(poly.attributes, flags, 0, mode_))
      continue;
    auto tri = triangle(p);
    if (sides == SatSidedness::AuthoredFront) {
      const auto n = sat_.normals[poly.normal];
      auto center = feet; center[1] += radius;
      if (dot({n[0], n[1], n[2]}, sub(center, tri[0])) < 0)
        continue;
    }
    double t = 0;
    for (int iteration = 0; iteration < 48; ++iteration) {
      auto contact = capsulePair(add(feet, mul(delta, t)), radius, height, tri);
      V normal = unit(sub(contact.body, contact.surface));
      if (length(normal) == 0) {
        auto n = sat_.normals[poly.normal];
        normal = unit({n[0], n[1], n[2]});
        if (dot(normal, delta) > 0)
          normal = mul(normal, -1);
      }
      double gap = contact.distance - radius;
      if (t == 0 && gap >= -.05 && dot(normal, delta) >= -1e-8)
        break;
      if (gap <= .1 || iteration == 47) {
        if (!best || t < best->fraction)
          best = SatHit{contact.surface, normal, p,
                        poly.attributes | 0x01000000, t};
        break;
      }
      t += (gap - .05) / speed;
      if (t > 1 || (best && t > best->fraction))
        break;
    }
  }
  if (scenario_)
    if (auto hit=scenario_->sweepCapsule(feet,delta,radius,height,subset,flags,sides))
      if (!best || hit->fraction < best->fraction) {
        hit->polygon+=sat_.polygons.size();best=hit;
      }
  return best;
}
std::optional<SatHit> SatCollision::spawn(double radius, double height,
                                         SatSidedness sides) const {
  validCapsule({}, radius, height);
  std::vector<std::pair<double, std::size_t>> ranked;
  for (std::size_t p = 0; p < sat_.floor + sat_.slope; ++p) {
    auto t = triangle(p);
    ranked.emplace_back(-length(cross(sub(t[1], t[0]), sub(t[2], t[0]))), p);
  }
  std::sort(ranked.begin(), ranked.end());
  std::optional<SatHit> fallback;
  for (auto [area, p] : ranked) {
    if (area >= -1e-6)
      continue;
    auto t = triangle(p);
    V center = mul(add(add(t[0], t[1]), t[2]), 1. / 3);
    auto g = ground(center, 1, 2);
    if (g && g->normal[1] >= .5 && std::abs(g->point[1] - center[1]) < .1 &&
        clearCapsule(g->point, radius, height, sides)) {
      if (!fallback)
        fallback = g;
      // Prefer room to see the capsule from the initial diagnostic camera.
      V aim = g->point;
      aim[1] += height * .5;
      V eye = aim;
      double distance = height * 3.75;
      eye[0] += std::sin(.6) * std::cos(.45) * distance;
      eye[1] += std::sin(.45) * distance;
      eye[2] += std::cos(.6) * std::cos(.45) * distance;
      auto obstruction = line(aim, eye, SatSubset::Walls, 0x8000);
      if (!obstruction || obstruction->fraction >= .5)
        return g;
    }
  }
  return fallback;
}
} // namespace re4
