#pragma once
#include "re4_collision_types.h"
#include "re4/port/consumers.h"
#include <memory>

namespace re4::port {
struct CollisionHandle { u64 runtime{},epoch{},index{}; };
struct CollisionHit { u32 attribute{}; Vec point{},normal{}; };
struct CollisionSnapshot { u32 vertices{},normals{},edges{},polygons{},blocks{}; bool enabled{}; };
// Owned BE-to-native boundary for the original segment/floor algorithms. This
// includes the original model-vs-scenario actor check. Actor-vs-actor EmAtCheck
// still requires the complete live EmMgr actor list.
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
    bool pushSphere(Vec previous,Vec &position,f32 radius,Vec &normal,int flags=0,int mask=0);
    // Borrowers must own every raw linked model/info before the original body
    // follows it. No cEm cast, fixed work stride or unverified dynamic pointer.
    int checkActor(std::shared_ptr<ModelCpu> actor,int mask=0,bool airborne=false,
                   const std::vector<std::shared_ptr<ModelCpu>> &linked={},
                   const std::vector<std::shared_ptr<ActorCollisionInfo>> &infos={});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
