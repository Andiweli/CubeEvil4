#include "walk_probe.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
WalkProbe::WalkProbe(const SatCollision &world, WalkConfig c)
    : world_(world), config_(c) {
  for (auto v : {c.radius, c.height, c.speed, c.stepUp, c.drop})
    if (!std::isfinite(v) || v <= 0 || v > 1e6)
      throw std::runtime_error("Test actor: invalid configuration");
  if (c.radius < .1 || c.height < 2 * c.radius)
    throw std::runtime_error("Test actor: Height is less than diameter");
}
bool WalkProbe::reset() {
  ground_ = world_.spawn(config_.radius, sceneryHeight(), scenerySides());
  wall_.reset();
  edgeBlocked_ = false;
  if (ground_)
    position_ = ground_->point;
  return ready();
}
bool WalkProbe::placeAuthored(const CollisionVec &position) {
  for(auto v:position)if(!std::isfinite(v))return false;
  const auto floor=world_.ground(position,600,100000);
  if(!floor || floor->normal[1]<.5)return false;
  position_=position;position_[1]=floor->point[1];ground_=floor;
  wall_.reset();edgeBlocked_=false;return true;
}
bool WalkProbe::place(const CollisionVec &position, double tolerance) {
  if (!std::isfinite(tolerance) || tolerance < 0 || tolerance > 1e6)
    return false;
  for (auto v : position)
    if (!std::isfinite(v))
      return false;
  auto floor = world_.ground(position, tolerance + .01, tolerance + .01);
  if (!floor || floor->normal[1] < .5 ||
      std::abs(floor->point[1] - position[1]) > tolerance)
    return false;
  auto feet = position;
  feet[1] = floor->point[1];
  if (!world_.clearCapsule(feet, config_.radius, sceneryHeight(), scenerySides()))
    return false;
  if (objectWorld_ &&
      !objectWorld_->clearCapsule(feet, config_.radius, config_.height))
    return false;
  position_ = feet;
  ground_ = floor;
  wall_.reset();
  edgeBlocked_ = false;
  return true;
}
std::optional<CollisionVec> WalkProbe::traversalLanding(const CollisionVec &end) const {
  for (auto value : end) if (!std::isfinite(value)) return {};
  auto position = end;
  // Action root curves can finish within the scenery body sphere. Original
  // SCA restoration resolves that contact before getFloor; do not reject a
  // valid reverse window solely for this residual overlap, and do not search
  // for an unrelated nearby exit or move farther than one body radius.
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    const auto floor = world_.ground(position, 600, 100000);
    if (!floor || floor->normal[1] < .5 ||
        std::abs(floor->point[1] - position[1]) >= 1000) return {};
    position[1] = floor->point[1];
    auto checked = *this;
    if (checked.place(position, 5)) return checked.position();
    const auto hit = world_.sweepCapsule(position, {.001, 0, .001},
        config_.radius, sceneryHeight(), SatSubset::Walls, 0x1000, scenerySides());
    if (!hit || hit->fraction > 0) return {};
    auto center = position;
    center[1] = std::clamp(hit->point[1], position[1] + config_.radius,
                         position[1] + sceneryHeight() - config_.radius);
    double distance = 0;
    for (unsigned k = 0; k < 3; ++k)
      distance += (center[k] - hit->point[k]) * (center[k] - hit->point[k]);
    const auto push = config_.radius + .2 - std::sqrt(distance);
    if (push <= 0) return {};
    double travel = 0;
    for (unsigned k = 0; k < 3; ++k) {
      position[k] += hit->normal[k] * push;
      travel += (position[k] - end[k]) * (position[k] - end[k]);
    }
    // A vertical ground snap is not lateral contact travel.
    travel -= (position[1] - end[1]) * (position[1] - end[1]);
    if (travel > config_.radius * config_.radius) return {};
  }
  return {};
}
void WalkProbe::setScriptPosition(const CollisionVec &position) {
  if (!ready()) throw std::runtime_error("Test actor: Action has no verified starting floor");
  for (auto value : position)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Test actor: invalid action position");
  position_ = position;
  wall_.reset();
  edgeBlocked_ = false;
}
bool WalkProbe::resolvePlayerScenery(const CollisionVec &previousCenter,
                                    const CollisionVec &posedCenter) {
  return resolveScenery(previousCenter,posedCenter,400,0x1001);
}
bool WalkProbe::resolveScenery(const CollisionVec &previousCenter,
                              const CollisionVec &posedCenter,double radius,unsigned flags) {
  if (!ready()) return false;
  const auto contact = world_.resolveScenerySphere(previousCenter, posedCenter,
                                                  radius, flags);
  auto corrected = position_;
  for (unsigned k = 0; k < 3; ++k)
    corrected[k] += contact.position[k] - posedCenter[k];
  const auto floor = world_.ground(corrected, 600, 100000);
  if (floor && std::abs(floor->point[1] - corrected[1]) < 1000) {
    corrected[1] = floor->point[1];
    ground_ = floor;
  }
  const auto moved = corrected != position_;
  position_ = corrected;
  return moved;
}
void WalkProbe::step(double x, double forward, double yaw, bool run,
                     double dt) {
  wall_.reset();
  edgeBlocked_ = false;
  if (!ready())
    return;
  if (!std::isfinite(x) || !std::isfinite(forward) || !std::isfinite(yaw) ||
      !std::isfinite(dt))
    return;
  dt = std::clamp(dt, 0., .05);
  x = std::clamp(x, -1., 1.);
  forward = std::clamp(forward, -1., 1.);
  double magnitude = std::hypot(x, forward);
  if (magnitude > 1) {
    x /= magnitude;
    forward /= magnitude;
  }
  double travel = config_.speed * (run ? 2 : 1) * dt;
  CollisionVec delta{(std::cos(yaw) * x - std::sin(yaw) * forward) * travel, 0,
                     (-std::sin(yaw) * x - std::cos(yaw) * forward) * travel};
  moveDelta(delta);
}
void WalkProbe::moveDelta(const CollisionVec &displacement) {
  wall_.reset();
  edgeBlocked_ = false;
  if (!ready())
    return;
  for (auto v : displacement)
    if (!std::isfinite(v) || std::abs(v) > 1e6)
      throw std::runtime_error("Test actor: invalid root motion");
  resolveObjectOverlaps();
  // These locomotion clips have no vertical root travel. SAT supplies grounded
  // Y, including slopes and steps; ladder actions use setScriptPosition.
  CollisionVec delta{displacement[0], 0, displacement[2]};
  const auto actorContactRate = [&](const CollisionVec &origin,
                                    const CollisionVec &movement) {
    double actorRate = 1;
    for (const auto &blocker : blockers_) {
      if (origin[1] + config_.height <= blocker.feet[1] ||
          origin[1] >= blocker.feet[1] + blocker.height) continue;
      const double x = origin[0] - blocker.feet[0],
                   z = origin[2] - blocker.feet[2],
                   radius = config_.radius + blocker.radius;
      const double a = movement[0] * movement[0] + movement[2] * movement[2],
                   b = x * movement[0] + z * movement[2],
                   c = x * x + z * z - radius * radius;
      if (a > 1e-12 && b < 0) {
        const double discriminant = b * b - a * c;
        if (c <= 0) {
          actorRate = 0;
        } else if (discriminant > 0) {
          const double time = (-b - std::sqrt(discriminant)) / a;
          if (time >= 0 && time <= 1) {
            const double rate = std::max(0., time - 1e-5);
            actorRate = std::min(actorRate, rate);
          }
        }
      }
    }
    return actorRate;
  };
  double length = std::hypot(delta[0], delta[2]);
  if (length < 1e-9)
    return;
  int steps = int(std::ceil(length / (config_.radius * .25)));
  if (steps > 4096)
    throw std::runtime_error("Test actor: Root motion budget");
  for (auto &v : delta)
    v /= steps;
  for (int step = 0; step < steps; ++step) {
    auto old = position_, trial = old, remaining = delta;
    for (int slide = 0; slide < 4; ++slide) {
      const auto actorRate = actorContactRate(trial, remaining);
      remaining[0] *= actorRate;
      remaining[2] *= actorRate;
      auto hit =
          world_.sweepCapsule(trial, remaining, config_.radius, sceneryHeight(),
                              SatSubset::Walls, 0x1000, scenerySides());
      if (objectWorld_)
        if (const auto object = objectWorld_->sweepCapsule(
                trial, remaining, config_.radius, config_.height))
          if (!hit || object->fraction < hit->fraction) hit = object;
      if (!hit) {
        for (int k = 0; k < 3; ++k)
          trial[k] += remaining[k];
        break;
      }
      wall_ = hit;
      double rate = std::max(0., hit->fraction - 1e-5);
      for (int k = 0; k < 3; ++k) {
        trial[k] += remaining[k] * rate;
        remaining[k] *= 1 - rate;
      }
      double n = std::hypot(hit->normal[0], hit->normal[2]);
      if (n < 1e-9)
        break;
      double nx = hit->normal[0] / n, nz = hit->normal[2] / n,
             d = remaining[0] * nx + remaining[2] * nz;
      if (d >= 0)
        break;
      remaining[0] -= d * nx;
      remaining[2] -= d * nz;
      if (std::hypot(remaining[0], remaining[2]) < .01)
        break;
    }
    auto floor = world_.ground(trial, config_.stepUp + .01, config_.drop + .01);
    if (!floor || floor->normal[1] < .5 ||
        floor->point[1] - old[1] > config_.stepUp ||
        old[1] - floor->point[1] > config_.drop) {
      edgeBlocked_ = true;
      break;
    }
    trial[1] = floor->point[1];
    // Vertical snap is swept too: slopes/steps may otherwise lift into a wall.
    CollisionVec snap{0, trial[1] - old[1], 0};
    CollisionVec horizontal = trial;
    horizontal[1] = old[1];
    if (world_.sweepCapsule(horizontal, snap, config_.radius, sceneryHeight(),
                            SatSubset::Walls, 0x1000, scenerySides())) {
      edgeBlocked_ = true;
      break;
    }
    position_ = trial;
    ground_ = floor;
  }
}
void WalkProbe::moveGanadoRoot(const CollisionVec &delta,const CollisionVec &previousPart0,
                               const CollisionVec &posedPart0) {
  if(!ready())return;
  wall_.reset();edgeBlocked_=false;
  const auto old=position_;
  position_[0]+=delta[0];position_[2]+=delta[2];
  auto center=posedPart0;center[0]+=delta[0];center[2]+=delta[2];
  // At_em_sphere_sphere_ck resolves penetration; stopping the complete
  // displacement at contact left mutually overlapping wave actors frozen.
  for(const auto &other:blockers_) {
    if(position_[1]+1600<=other.feet[1] || position_[1]>=other.feet[1]+other.height)continue;
    double x=center[0]-other.feet[0],z=center[2]-other.feet[2],distance=std::hypot(x,z);
    const double radius=250+other.radius;
    if(distance>=radius)continue;
    if(distance<.1){x+=.1;distance=std::hypot(x,z);}
    const double push=(radius-distance)/distance;
    position_[0]+=x*push;position_[2]+=z*push;center[0]+=x*push;center[2]+=z*push;
  }
  auto previous=previousPart0;previous[1]=old[1]+400;center[1]=position_[1]+400;
  const auto contact=world_.resolveScenerySphere(previous,center,400,0x2001);
  for(unsigned k=0;k<3;++k)position_[k]+=contact.position[k]-center[k];
  const auto floor=world_.ground(position_,600,100000);
  if(floor && std::abs(floor->point[1]-position_[1])<1000){position_[1]=floor->point[1];ground_=floor;}
  else {position_=old;edgeBlocked_=true;}
  if(contact.contacts)wall_=SatHit{contact.position,contact.normal,0,0,0};
}
void WalkProbe::setObjectBodies(std::vector<ObjectBodyBox> bodies) {
  if (bodies.size() > 128) throw std::runtime_error("Object body: Budget");
  for (const auto &b : bodies) {
    for (auto v : b.origin) if (!std::isfinite(v)) throw std::runtime_error("Object body: Position");
    for (auto v : {b.yaw, b.centerX, b.halfX, b.halfZ, b.lowY, b.highY})
      if (!std::isfinite(v) || std::abs(v) > 1e7) throw std::runtime_error("Object body: Dimensions");
    if (b.halfX <= 0 || b.halfZ <= 0 || b.lowY >= b.highY)
      throw std::runtime_error("Object body: empty rectangle");
  }
  objectBodies_ = std::move(bodies);
}
bool WalkProbe::resolveObjectOverlaps() {
  if (!ready() || objectBodies_.empty()) return false;
  auto trial = position_;
  bool moved = false;
  for (unsigned pass = 0; pass < 8; ++pass) {
    bool contact = false;
    for (const auto &b : objectBodies_) {
      if (trial[1] >= b.origin[1] + b.highY ||
          trial[1] + config_.height <= b.origin[1] + b.lowY) continue;
      const double c = std::cos(b.yaw), s = std::sin(b.yaw);
      const double dx = trial[0] - b.origin[0], dz = trial[2] - b.origin[2];
      double x = c * dx - s * dz - b.centerX, z = s * dx + c * dz;
      const double qx = std::clamp(x, -b.halfX, b.halfX),
                   qz = std::clamp(z, -b.halfZ, b.halfZ);
      double nx = x - qx, nz = z - qz, distance = std::hypot(nx, nz);
      const double r = config_.radius + .25;
      if (distance >= r) continue;
      if (distance > 1e-9) { nx /= distance; nz /= distance; }
      else {
        // Inside the rectangle: nearest face, as sphereRectCk. Include the
        // interior distance; a surface triangle alone cannot recover this.
        const double sx = b.halfX - std::abs(x), sz = b.halfZ - std::abs(z);
        if (sx < sz) { nx = x < 0 ? -1 : 1; nz = 0; distance = -sx; }
        else { nx = 0; nz = z < 0 ? -1 : 1; distance = -sz; }
      }
      const double push = r - distance;
      trial[0] += (c * nx + s * nz) * push;
      trial[2] += (-s * nx + c * nz) * push;
      moved = contact = true;
    }
    if (!contact) break;
  }
  if (!moved || std::hypot(trial[0] - position_[0], trial[2] - position_[2]) > 2000) return false;
  // No fabricated floor, wall removal or unchecked teleport through a wall.
  return place(trial, config_.stepUp);
}
void WalkProbe::setActorBlocker(const std::optional<ActorBlocker> &blocker) {
  setActorBlockers(blocker ? std::vector<ActorBlocker>{*blocker}
                          : std::vector<ActorBlocker>{});
}
void WalkProbe::setActorBlockers(std::vector<ActorBlocker> blockers) {
  if (blockers.size() > 256)
    throw std::runtime_error("Test actor: Actor contact budget");
  for (const auto &blocker : blockers) {
    for (auto value : blocker.feet)
      if (!std::isfinite(value))
        throw std::runtime_error("Test actor: invalid actor contact");
    if (!std::isfinite(blocker.radius) || blocker.radius <= 0 ||
        blocker.radius > 1e6 || !std::isfinite(blocker.height) ||
        blocker.height < 2 * blocker.radius || blocker.height > 1e6)
      throw std::runtime_error("Test actor: invalid actor dimensions");
  }
  blockers_ = std::move(blockers);
}
} // namespace re4
