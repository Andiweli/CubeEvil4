#pragma once
#include "re4_collision_types.h"
#include "re4/port/consumers.h"
#include <memory>

namespace re4::port {
struct CollisionHandle { u64 runtime{},epoch{},index{}; };
struct CollisionHit { u32 attribute{}; Vec point{},normal{}; };
struct CollisionSnapshot { u32 vertices{},normals{},edges{},polygons{},blocks{}; bool enabled{}; };
// Owned BE-to-native boundary for the original segment/floor algorithms. This
// does not replace cSatMgr::check, sphere push-out or actor ground resolution.
class CollisionRuntime final {
public:
    explicit CollisionRuntime(NativeGameState &,bool effect=false);
    ~CollisionRuntime();
    void configure(u32 capacity); // Caller supplies original CONS_R_NSAT/NEAT.
    CollisionHandle create(ResourceLease,int type=0,Vec position={},Vec rotation={});
    void destroy(CollisionHandle);
    void clear();
    void enable(CollisionHandle,bool);
    void setCoord(CollisionHandle,Vec,Vec);
    CollisionSnapshot snapshot(CollisionHandle) const;
    CollisionHit hitCheck(Vec from,Vec to,int flags=0,int mask=0);
    f32 getFloor(Vec position,f32 above,f32 below,int mask=0);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
