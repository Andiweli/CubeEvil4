#pragma once
#include "collision_query.h"
namespace re4 {
// Diagnostic dimensions/speeds, NOT the original cPlayer runtime.
struct WalkConfig {
  double radius = 300, height = 1600, speed = 2600, stepUp = 250, drop = 600;
  bool originalScenerySphere = false;
};
// Source At_em_sphere_rect_ck body rectangles, separate from scenery SCA.
struct ObjectBodyBox {
  CollisionVec origin{};
  double yaw{}, centerX{}, halfX{}, halfZ{}, lowY{}, highY{};
};
struct ActorBlocker {
  CollisionVec feet{};
  double radius{}, height{};
};
class WalkProbe {
public:
  explicit WalkProbe(const SatCollision &world, WalkConfig config = {});
  bool reset();
  // Source getFloor for authored ESL and door positions before SCA contact.
  bool placeAuthored(const CollisionVec &position);
  // Validate an authored spawn without changing the current position on
  // failure.
  bool place(const CollisionVec &position, double tolerance = 250);
  // Original root displacement shares the diagnostic capsule's SAT resolver.
  void moveDelta(const CollisionVec &delta);
  // cAtariInfo::getSpeedVector + scrAtCheckSphere for an ordinary Ganado.
  // Part0 supplies XZ; Y is feet+400. Character radius2 is a separate250.
  void moveGanadoRoot(const CollisionVec &delta,const CollisionVec &previousPart0,
                      const CollisionVec &posedPart0);
  // Original PlSetLadder disables atari while the authored 3D root moves.
  // Retains the last real floor reference for diagnostics; place() must
  // validate the actual floor/capsule before restoring grounded movement.
  void setScriptPosition(const CollisionVec &position);
  // Bounded encounter adapter: upright actors, in addition to static SAT.
  // This contact stop does not replace the original cAtari manager.
  // The optional setter replaces the complete collection with zero/one actor.
  void setActorBlocker(const std::optional<ActorBlocker> &blocker);
  void setActorBlockers(std::vector<ActorBlocker> blockers);
  // Borrowed dynamic ETS walls. The owner rebinds after each object update;
  // the immutable room SAT remains the only source of ground placement.
  void setObjectCollision(const SatCollision *world) { objectWorld_ = world; }
  void setObjectBodies(std::vector<ObjectBodyBox> bodies);
  // A swinging/re-enabled leaf may overlap the body before the next move.
  // Resolve the original circle/rectangle contact onto a checked real floor.
  bool resolveObjectOverlaps();
  // Restore grounded collision after an authored action that suspended SCA.
  // Resolve only local sphere contact, then require a real clear floor.
  std::optional<CollisionVec> traversalLanding(const CollisionVec &end) const;
  // Original cPlayer scenery body: posed part0 XZ, actorY+400 and radius400.
  // Applies the returned collision delta to actor feet, then real source floor.
  bool resolvePlayerScenery(const CollisionVec &previousCenter,
                            const CollisionVec &posedCenter);
  bool resolveScenery(const CollisionVec &previousCenter,
                      const CollisionVec &posedCenter,double radius,unsigned flags);
  void step(double x, double forward, double cameraYaw, bool run,
            double seconds);
  bool ready() const { return ground_.has_value(); }
  CollisionVec position() const { return position_; }
  const std::optional<SatHit> &floorHit() const { return ground_; }
  const std::optional<SatHit> &wallHit() const { return wall_; }
  bool edgeBlocked() const { return edgeBlocked_; }
  const WalkConfig &config() const { return config_; }
  const SatCollision &world() const { return world_; }

private:
  const SatCollision &world_;
  WalkConfig config_;
  CollisionVec position_{};
  std::optional<SatHit> ground_, wall_;
  bool edgeBlocked_{};
  std::vector<ActorBlocker> blockers_;
  const SatCollision *objectWorld_{};
  std::vector<ObjectBodyBox> objectBodies_;
  double sceneryHeight() const { return config_.originalScenerySphere ? 2 * config_.radius : config_.height; }
  SatSidedness scenerySides() const { return config_.originalScenerySphere ? SatSidedness::AuthoredFront : SatSidedness::Both; }
};
} // namespace re4
