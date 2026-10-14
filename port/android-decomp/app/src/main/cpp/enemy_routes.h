#pragma once
#include "collision_query.h"
#include <cstdint>
#include <vector>
namespace re4 {
struct EnemyRouteStep { CollisionVec destination{}; float maxHeight{}; bool direct{}; };
// Original game/route_ck.cpp RTP nearest-ten / next-hop queries. The immutable
// collision owner must outlive this room route owner.
class EnemyRoutes {
public:
  EnemyRoutes(const std::vector<std::uint8_t> &bytes,const SatCollision &sat);
  EnemyRouteStep step(const CollisionVec &from,const CollisionVec &to,
                      unsigned mode=0,unsigned attributes=0x300) const;
  float distance(const CollisionVec &from,const CollisionVec &to) const;
  int nearest(const CollisionVec &position,unsigned mode=0,unsigned attributes=0) const;
  float pointDistance(unsigned from,unsigned to) const;
  const std::vector<CollisionVec> &points() const { return points_; }
  int next(unsigned from,unsigned to) const;
private:
  const SatCollision &sat_;
  std::vector<CollisionVec> points_;
  std::vector<std::int8_t> next_;
  bool blocked(const CollisionVec &from,const CollisionVec &to,unsigned attributes,unsigned mode) const;
  bool midpointFloor(const CollisionVec &from,const CollisionVec &to) const;
};
} // namespace re4
