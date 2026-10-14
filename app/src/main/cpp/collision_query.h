#pragma once
#include "sat.h"
#include <optional>
#include <memory>
namespace re4 {
using CollisionVec = std::array<double, 3>;
enum class SatSubset { All, Ground, Walls };
// Original SatMgr uses SEck=0; bullet/effect EatMgr uses SEck=1.
enum class SatQueryMode { Sight, Effect };
// cAtari::scrAtCheckSphere / At_poly_sphere_ck2 only test the authored
// front face. Camera and diagnostic capsules keep their two-sided policy.
enum class SatSidedness { Both, AuthoredFront };
struct SatHit {
  CollisionVec point{}, normal{};
  std::size_t polygon{};
  std::uint32_t attributes{};
  double fraction{};
};
struct SatSphereResolution {
  CollisionVec position{}, normal{};
  unsigned contacts{};
};
// Immutable SAT[0]/variant-0 queries. Own BVH, borrowed immutable Sat lifetime.
// SAT/EAT exclusion rules follow at_sub.cpp. Sight retains existing SAT mode.
class SatCollision {
public:
  explicit SatCollision(const Sat &sat,
                         SatQueryMode mode = SatQueryMode::Sight);
  // Room AEV SCR_AT pieces share every SAT/EAT query with static scenery.
  // Own both geometry and query; replacing pieces leaves all room borrowers valid.
  void setScenarioGeometry(Sat geometry);
  std::optional<SatHit> line(const CollisionVec &from, const CollisionVec &to,
                             SatSubset subset = SatSubset::All,
                             std::uint32_t flags = 0x1000,
                             std::uint32_t mask = 0) const;
  std::optional<SatHit> ground(const CollisionVec &feet, double above,
                               double below) const;
  // Conservative continuous translation of an upright capsule vs wall subset.
  std::optional<SatHit> sweepCapsule(const CollisionVec &feet,
                                     const CollisionVec &delta, double radius,
                                     double height,
                                     SatSubset subset = SatSubset::Walls,
                                     std::uint32_t flags = 0x1000,
                                     SatSidedness sides = SatSidedness::Both) const;
  bool clearCapsule(const CollisionVec &feet, double radius,
                    double height, SatSidedness sides = SatSidedness::Both) const;
  // Original cSatMgr::wallAdjust / At_poly_sphere_ck2: authored front,
  // surface pass then edge pass, radius-sized plane correction. Unlike the
  // diagnostic capsule sweep, stationary posed bodies are checked too.
  SatSphereResolution resolveScenerySphere(const CollisionVec &previous,
      const CollisionVec &current, float radius,
      std::uint32_t flags = 0x1001) const;
  std::optional<SatHit> spawn(double radius, double height,
                             SatSidedness sides = SatSidedness::Both) const;
  std::array<CollisionVec, 3> triangle(std::size_t polygon) const;
  static bool accepts(std::uint32_t attributes, std::uint32_t flags,
                      std::uint32_t mask = 0,
                      SatQueryMode mode = SatQueryMode::Sight);

private:
  struct Box {
    CollisionVec low{}, high{};
  };
  struct Node {
    Box box;
    int left = -1, right = -1;
    std::size_t begin{}, end{};
  };
  const Sat &sat_;
  SatQueryMode mode_;
  std::unique_ptr<Sat> scenarioMesh_;
  std::unique_ptr<SatCollision> scenario_;
  std::vector<std::size_t> order_;
  std::vector<std::size_t> wallRank_;
  std::vector<Box> boxes_;
  std::vector<Node> nodes_;
  int build(std::size_t begin, std::size_t end);
  std::vector<std::size_t> candidates(const Box &box, SatSubset subset) const;
};
} // namespace re4
