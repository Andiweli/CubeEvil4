#include "room_ladders.h"
#include "leon_preview.h"
#include "room_interactions.h"
#include "turn_rules.h"
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
constexpr double pi = 3.14159265358979323846;
CollisionVec travel(const CollisionVec &start, const CollisionVec &root, double heading) {
  const auto c = std::cos(heading), s = std::sin(heading);
  return {start[0] + c * root[0] + s * root[2], start[1] + root[1],
          start[2] - s * root[0] + c * root[2]};
}
Matrix34 multiply(const Matrix34 &a, const Matrix34 &b) {
  Matrix34 out{};
  for (unsigned r=0;r<3;++r) {
    for (unsigned c=0;c<3;++c)
      for (unsigned k=0;k<3;++k) out[r*4+c]+=a[r*4+k]*b[k*4+c];
    out[r*4+3]=a[r*4+3];
    for (unsigned k=0;k<3;++k) out[r*4+3]+=a[r*4+k]*b[k*4+3];
  }
  return out;
}
Matrix34 inverseRigid(const Matrix34 &m) {
  Matrix34 out{};
  for (unsigned r=0;r<3;++r) {
    for (unsigned c=0;c<3;++c) out[r*4+c]=m[c*4+r];
    for (unsigned k=0;k<3;++k) out[r*4+3]-=out[r*4+k]*m[k*4+3];
  }
  return out;
}
CollisionVec ladderTop(const RoomObjectPlacement &p) {
  const unsigned rungs=p.id==8?8:6;
  auto top=travel({p.position[0],p.position[1],p.position[2]},
                  {0,rungs*533.3329,rungs*-194.1173},p.rotation[1]);
  if(p.id==8) top[1]-=1000;
  return top;
}
}
RoomLadders::RoomLadders(const RoomFlow &flow) {
  for (const auto &ladder : flow.ladders()) {
    const auto &a = ladder.trigger;
    const bool checked = (a.sourceRoom == 0x101 && a.no == 5) ||
                         (a.sourceRoom == 0x400 && a.no == 10);
    if (!checked || ladder.level != 9 || ladder.type || ladder.posSet != 1 ||
        a.enabled != 3 || a.targetType != 1 || a.area.enabled != 1 ||
        a.area.type != 1 || a.area.height <= 0 || a.hitType != 1 ||
        a.triggerType != 8 || (a.priority != 2 && a.priority != 5) || a.country || a.parentPointer ||
        a.functionPointer || a.parameterPointer || a.taskLevel || a.kind ||
        a.waitingType || a.waitingNo)
      continue;
    ladders_.push_back(ladder);
  }
}
RoomLadders::RoomLadders(
    const RoomFlow &flow, const RoomArchive &archive,
    const std::vector<RoomObjectPlacement> &placements,
    const RoomSaveState &state, const std::string &playerDrs) : RoomLadders(flow) {
  // These room initializers preserve the source ET06/08 standing profile.
  // Other campaign rooms can setDowned/reposition ladders in their scripts;
  // do not infer their live state from static ETS geometry alone.
  const auto room = flow.sourceRoom();
  if (room != 0x101 && room != 0x400) return;
  villageObjectsEnabled_=room!=0x101 || state.room.test(8);
  for (const auto &p : placements) {
    if (p.id != 6 && p.id != 8) continue;
    if (state.object(p.slot, 1)) continue; // SetLadder/GetEtcFlgPtr.
    if (std::abs(p.rotation[0]) > .0001 || std::abs(p.rotation[2]) > .0001)
      throw std::runtime_error("Object ladder: unverified ETS tilt");
    objectLadders_.push_back(p);
  }
  if (objectLadders_.empty()) return;
  const auto *entry = archive.find("ETM");
  if (!entry || playerDrs.empty())
    throw std::runtime_error("Object ladder: Original ETM/player motions are missing");
  const RoomObjectArchive etm(archive.copy(entry - archive.entries().data()));
  for (const char *name : {"pl00017.fcv", "pl00018.fcv", "pl00019.fcv", "pl00020.fcv",
                          "pl00021.fcv", "pl00024.fcv", "pl00025.fcv"})
    objectMotions_.push_back({etm.copy(name), {}});
  const std::vector<AnimationBytes> propClips{
    {etm.copy("et06000.fcv"),etm.copy("et060000.seq")},
    {etm.copy("et06001.fcv"),etm.copy("et060010.seq")},
    {etm.copy("et06003.fcv"),etm.copy("et060030.seq")}};
  for (const auto &p:objectLadders_) {
    ObjectRuntime runtime;
    runtime.model=std::make_unique<SkinnedModel>(etm.copy(p.id==8?"et0800.bin":"et0600.bin"));
    // RoomObjectModelScene already bakes the whole rigid ladder at root's
    // standing tilt. Apply the same root transform through a draw delta.
    runtime.motion=std::make_unique<AnimationRig>(*runtime.model,propClips);
    runtime.pivot=runtime.motion->bindCenters().at(0);
    objectStates_.push_back(std::move(runtime));
  }
  const auto player = RoomArchive::LoadDrs(playerDrs);
  const SkinnedModel body(player.copy(0));
  AnimationRig predictor(body, objectMotions_);
  constexpr std::array<unsigned, 4> clips{0, 1, 2, 3};
  for (unsigned type = 0; type < 2; ++type) {
    predictor.reset();
    auto state = StartObjectLadderMotion(predictor, clips, type ? 8 : 6, type);
    std::vector<LadderSound> sounds;
    while (!state.complete) {
      const auto root = AdvanceObjectLadderMotion(predictor, clips, state, sounds);
      sounds.clear();
      objectPaths_[type].push_back({root, state.phase == 2 ? state.exitTicks : 0});
    }
  }
  rebuildCollision();
}
void RoomLadders::setVillageHouse(bool enabled, WalkProbe &probe, RoomInteractions *interactions) {
  if(enabled==villageObjectsEnabled_)return;
  villageObjectsEnabled_=enabled;
  if(interactions)interactions->setSceneryBodies(objectBodies(),&probe);
  else {
    probe.setObjectCollision(nullptr);rebuildCollision();
    probe.setObjectCollision(collision_.get());probe.setObjectBodies(objectBodies());
  }
  bodiesChanged_=false;
}
void RoomLadders::rebuildCollision() {
  std::vector<std::array<std::array<float,3>,3>> walls;
  for (const auto &b : objectBodies()) {
    const auto vertex = [&](double x, double y, double z) {
      const auto v = travel(b.origin, {x, y, z}, b.yaw);
      return std::array<float,3>{float(v[0]),float(v[1]),float(v[2])};
    };
    const auto face = [&](double x0,double z0,double x1,double z1) {
      const auto a=vertex(x0,b.lowY,z0),d=vertex(x1,b.lowY,z1),
                 c=vertex(x1,b.highY,z1),e=vertex(x0,b.highY,z0);
      walls.push_back({a,d,c}); walls.push_back({a,c,e});
    };
    face(-b.halfX,-b.halfZ,b.halfX,-b.halfZ);
    face(b.halfX,b.halfZ,-b.halfX,b.halfZ);
    face(-b.halfX,b.halfZ,-b.halfX,-b.halfZ);
    face(b.halfX,-b.halfZ,b.halfX,b.halfZ);
  }
  collisionMesh_ = std::make_unique<Sat>(Sat::DynamicWalls(walls));
  collision_ = std::make_unique<SatCollision>(*collisionMesh_);
}
std::vector<ObjectBodyBox> RoomLadders::objectBodies() const {
  std::vector<ObjectBodyBox> result;
  for (std::size_t i=0;i<objectLadders_.size();++i) {
    if(!villageObjectsEnabled_)continue;
    const auto &p=objectLadders_[i];
    if (i<objectStates_.size() && objectStates_[i].phase!=ObjectRuntime::Standing) continue;
    // SetLadder::AtariInit(0,1000,-700,330,600,600,1000,...).
    // Translate the source centreZ into the existing rectangle's origin;
    // its low/high bounds are centreY+-halfHeight, not the whole visual pole.
    const auto origin = travel({p.position[0],p.position[1],p.position[2]},
                                {0,0,-700}, p.rotation[1]);
    result.push_back({origin, p.rotation[1], 0, 330, 600, 0, 2000});
  }
  return result;
}
bool RoomLadders::permitsWindowCross(const CollisionVec &window) const {
  for (std::size_t i=0;i<objectLadders_.size();++i) {
    if(!villageObjectsEnabled_)continue;
    if (objectStates_[i].phase!=ObjectRuntime::Standing) continue;
    const auto top=ladderTop(objectLadders_[i]);
    double distance=0;
    for(unsigned k=0;k<3;++k)distance+=(window[k]-top[k])*(window[k]-top[k]);
    if(distance<4000000.) return false;
  }
  return true;
}
std::optional<LadderOffer> RoomLadders::offer(
    const LeonPreview &player, const WalkProbe &probe,
    const std::optional<CollisionVec> &clippedFront) const {
  if (!probe.ready() || !player.alive() || player.action() != PlayerAction::Footwork)
    return {};
  std::optional<LadderOffer> fixedOffer;
  for (const auto &ladder : ladders_) {
    const auto start = travel(ladder.position, {0, 0, 300}, ladder.heading);
    const auto heading = float(std::remainder(ladder.heading + pi, 2 * pi));
    auto landingProbe = probe; // Read-only validation of both real endpoints.
    const auto upEnd = travel(start, player.ladderTravel(true), heading);
    if (!landingProbe.place(upEnd)) continue;
    const auto top = landingProbe.position();
    const auto downHeading = float(std::remainder(double(heading) + pi, 2 * pi));
    // Reciprocal mount uses the source's 300-unit front offset again. The
    // unmodified exit FCV otherwise ends inside this adapter's 300-radius
    // capsule; never fake a floor or start an action with an invalid landing.
    const auto downStart = travel(top, {0, 0, 300}, downHeading);
    const auto downEnd = travel(downStart, player.ladderTravel(false), downHeading);
    if (!landingProbe.place(downEnd)) continue;
    const auto bottom = landingProbe.position();
    if (!fixedOffer && RoomFlow::hit(ladder.trigger, player.feet(), player.heading(), clippedFront))
      fixedOffer=LadderOffer{ladder.trigger.no, false, start, top, heading};
    const auto &feet = player.feet();
    if (!fixedOffer && std::abs(feet[1] - top[1]) <= 250 &&
        std::hypot(feet[0] - top[0], feet[2] - top[2]) <= 800 &&
        std::abs(std::remainder(player.heading() - double(downHeading), 2 * pi)) <= pi / 4)
      fixedOffer=LadderOffer{ladder.trigger.no, true, downStart, bottom, downHeading,
                            false,0,0,LadderAction::Descend};
  }
  for (std::size_t i=0;i<objectLadders_.size();++i) {
    if(!villageObjectsEnabled_)continue;
    const auto &p=objectLadders_[i];
    const auto &runtime=objectStates_[i];
    const auto &feet = player.feet();
    const auto c = std::cos(p.rotation[1]), s = std::sin(p.rotation[1]);
    const auto dx = feet[0] - p.position[0], dz = feet[2] - p.position[2];
    const auto x = c * dx - s * dz, z = s * dx + c * dz;
    const auto type=p.id==8?1u:0u;
    if (runtime.phase==ObjectRuntime::Fallen && !runtime.resetWait &&
        dx*dx+dz*dz<=2250000 && std::abs(feet[1]-p.position[1])<=500) {
      // Muku2(src=objYaw,dst=playerYaw,PI), including its -PI tie.
      const auto angle=GameCubeLimitAngle(player.heading()-p.rotation[1]);
      const bool flip=angle>=0;
      const auto start=travel({p.position[0],p.position[1],p.position[2]},
                              {flip?-890.014:890.014,0,1450.51},p.rotation[1]);
      const auto heading=float(std::remainder(p.rotation[1]+(flip?pi/2:-pi/2),2*pi));
      auto landing=probe;
      if (!landing.place(start,500)) continue;
      return LadderOffer{p.slot,false,start,landing.position(),heading,true,
                         type?8u:6u,type,LadderAction::Reset,flip};
    }
    if (runtime.phase!=ObjectRuntime::Standing) continue;
    const auto top=ladderTop(p);
    const auto tdx=feet[0]-top[0],tdz=feet[2]-top[2];
    const auto tx=c*tdx-s*tdz,tz=s*tdx+c*tdz;
    if (std::abs(tx)<=800 && tz>=-1000 && tz<=500 &&
        std::abs(feet[1]-top[1])<=500 &&
        std::abs(std::remainder(player.heading()-p.rotation[1],2*pi))<=pi/2) {
      // plobjLadderDown changes X/Z only and preserves the real top floor.
      auto start=travel(top,{0,0,-700},p.rotation[1]); start[1]=feet[1];
      if (interactions_) {
        const auto *walls=interactions_->collision();
        const auto &body=probe.config();
        CollisionVec alignment{}, reach{};
        for(unsigned k=0;k<3;++k) {
          alignment[k]=start[k]-feet[k];
          reach[k]=top[k]-feet[k];
        }
        reach[1]=0;
        // The source local box is deliberately broad, and its callback
        // snaps X/Z before turning off collision. Our actor adapter must
        // not use that snap, or reach the ladder top, through a live pane.
        // Query the same live walls as grounded movement (not EAT, which
        // retains unrelated visual/bullet surfaces after a pane breaks).
        if (walls && (!walls->clearCapsule(start,body.radius,body.height) ||
            walls->sweepCapsule(feet,alignment,body.radius,body.height) ||
            walls->sweepCapsule(feet,reach,body.radius,body.height))) continue;
      }
      return LadderOffer{p.slot,false,start,start,float(p.rotation[1]),true,
                         type?8u:6u,type,LadderAction::Tip};
    }
    // objLadderClimbActEvtCk's exact local box and half-turn facing test.
    if (runtime.climbWait || std::abs(x) > 800 || z < -500 || z > 1000 ||
        std::abs(feet[1] - p.position[1]) > 500 ||
        std::abs(std::remainder(player.heading() - p.rotation[1], 2 * pi)) < pi / 2)
      continue;
    const auto start = travel({p.position[0],p.position[1],p.position[2]},
                               {0,0,300}, p.rotation[1]);
    const auto heading = float(std::remainder(p.rotation[1] + pi, 2 * pi));
    auto end = start;
    double yaw = heading;
    // Replay the owned source FCV path. The normal dismount raises feet to
    // a real SAT floor from exit counter24, exactly as plobjLadderClimb.
    for (const auto &step : objectPaths_[type]) {
      if (!type && step.exitTicks > 23) {
        const auto floor = probe.world().ground(end, 600, 100000);
        if (floor && end[1] < floor->point[1])
          end[1] = end[1] * .9 + floor->point[1] * .1;
      }
      end = travel(end, step.root.translation, yaw);
      yaw += step.root.yaw;
    }
    const auto floor = probe.world().ground(end, 600, 100000);
    if (!floor || floor->normal[1] < .5 || std::abs(floor->point[1]-end[1]) > 600)
      continue;
    if (end[1] < floor->point[1]) end[1] = floor->point[1];
    auto destination = probe;
    if (!destination.place(end, 600)) continue;
    return LadderOffer{p.slot, false, start, destination.position(), heading,
                        true, type ? 8u : 6u, type};
  }
  // Object actions use source priority5 and must precede the adapter's
  // reciprocal AEV return action at the same roof/ladder junction.
  return fixedOffer;
}
bool RoomLadders::interact(LeonPreview &player, WalkProbe &probe,
                          const std::optional<CollisionVec> &front) {
  const auto action = offer(player, probe, front);
  if (!action) return false;
  if (action->object) {
    player.installObjectLadderMotions(objectMotions_);
    for (std::size_t i=0;i<objectLadders_.size();++i)
      if (objectLadders_[i].slot==action->no) {
        auto &runtime=objectStates_[i];
        if (action->action==LadderAction::Tip || action->action==LadderAction::Reset) {
          const bool reset=action->action==LadderAction::Reset;
          player.beginLadderProp(probe,action->alignment,action->heading,
                                 reset?4:action->type?5:6,action->flip,reset);
          runtime.phase=reset?ObjectRuntime::Raising:ObjectRuntime::Falling;
          runtime.motionPosition={}; runtime.motionYaw=0;
          runtime.standingPosePending=false;
          runtime.downWait=reset?0:17;
          runtime.climbWait=reset?0:90; runtime.resetWait=reset?60:0;
          runtime.motion->startActionFlags(reset?0:action->type?1:2,0,0,1);
          bodiesChanged_=true;
          return true;
        }
        runtime.climbWait=90;
      }
    player.beginObjectLadder(probe, action->alignment, action->heading,
                            action->rungs, action->type, action->landing);
    return true;
  }
  player.beginLadder(probe, action->alignment, action->heading, action->down ? -9 : 9);
  return true;
}
void RoomLadders::tick(WalkProbe &probe, RoomInteractions *interactions) {
  for (std::size_t i=0;i<objectStates_.size();++i) {
    auto &runtime=objectStates_[i];
    if (runtime.climbWait) --runtime.climbWait;
    if (runtime.resetWait) --runtime.resetWait;
    if (runtime.downWait) --runtime.downWait;
    if (runtime.phase==ObjectRuntime::Standing && runtime.standingPosePending) {
      // R1_Reset switches routine/status on MotionMove's terminal tick;
      // R1_Set reinstalls the authored standing root on the following tick.
      runtime.standingPosePending=false;runtime.motionPosition={};runtime.motionYaw=0;
    }
    if (runtime.phase!=ObjectRuntime::Falling && runtime.phase!=ObjectRuntime::Raising) continue;
    // ObjLadder_R1_Fall/Reset consume Seq_old.Free before MotionMove.
    if (runtime.motion->sequenceFree()&1) {
      auto position=runtime.motionPosition;
      for(unsigned k=0;k<3;++k)position[k]+=objectLadders_[i].position[k];
      sounds_.push_back({6,runtime.phase==ObjectRuntime::Raising?0x41u:0x3fu,position});
      if (runtime.phase==ObjectRuntime::Raising && interactions) {
        auto current=objectLadders_[i];
        for(unsigned k=0;k<3;++k)current.position[k]+=float(runtime.motionPosition[k]);
        current.rotation[1]+=float(runtime.motionYaw);
        interactions->breakWindowsAtLadderTop(ladderTop(current));
      }
    }
    const auto root=runtime.motion->advanceRootAction();
    const auto offset=travel({},root.translation,
        objectLadders_[i].rotation[1]+runtime.motionYaw);
    for(unsigned k=0;k<3;++k)runtime.motionPosition[k]+=offset[k];
    runtime.motionYaw+=root.yaw;
    if (root.ended) {
      runtime.phase=runtime.phase==ObjectRuntime::Raising?ObjectRuntime::Standing:ObjectRuntime::Fallen;
      runtime.standingPosePending=runtime.phase==ObjectRuntime::Standing;
      bodiesChanged_=true;
    }
  }
  if (!bodiesChanged_) return;
  bodiesChanged_=false;
  if (interactions) interactions->setSceneryBodies(objectBodies(),&probe);
  else {
    // Rebind before releasing the old borrowed collision owner.
    probe.setObjectCollision(nullptr);
    rebuildCollision();
    probe.setObjectCollision(collision_.get()); probe.setObjectBodies(objectBodies());
  }
}
std::optional<Matrix34> RoomLadders::delta(int slot) const {
  for (std::size_t i=0;i<objectLadders_.size();++i) {
    if(!villageObjectsEnabled_)continue;
    const auto &p=objectLadders_[i]; const auto &runtime=objectStates_[i];
    if (int(p.slot)!=slot || (runtime.phase==ObjectRuntime::Standing && !runtime.standingPosePending)) continue;
    SmdPlacement pose; pose.position=p.position; pose.rotation=p.rotation; pose.scale={1,1,1};
    for(unsigned k=0;k<3;++k)pose.position[k]+=float(runtime.motionPosition[k]);
    pose.rotation[1]+=float(runtime.motionYaw);
    const auto placement=PlacementMatrix(pose);
    SmdPlacement original; original.position=p.position; original.rotation=p.rotation; original.scale={1,1,1};
    const auto originalPlacement=PlacementMatrix(original);
    SmdPlacement standing; standing.scale={1,1,1}; standing.rotation[0]=-1.9198622f;
    auto source=PlacementMatrix(standing);
    for (unsigned r=0;r<3;++r) {
      source[r*4+3]=runtime.pivot[r];
      for (unsigned c=0;c<3;++c) source[r*4+3]-=source[r*4+c]*runtime.pivot[c];
    }
    auto posed=runtime.motion->jointMatrices().at(0);
    for (unsigned r=0;r<3;++r)
      for (unsigned c=0;c<3;++c) posed[r*4+3]-=posed[r*4+c]*runtime.pivot[c];
    return multiply(multiply(multiply(placement,posed),inverseRigid(source)),inverseRigid(originalPlacement));
  }
  return {};
}
std::vector<LadderPropSound> RoomLadders::drainSounds() {
  std::vector<LadderPropSound> result; result.swap(sounds_); return result;
}
std::string RoomLadders::describe() const {
  std::ostringstream text;
  text << "Fixed AEV ladders: " << ladders_.size()
       << " · ETS object ladders: " << objectLadders_.size()
       << " · Original FCVs · SAT-verified destinations";
  return text.str();
}
} // namespace re4
