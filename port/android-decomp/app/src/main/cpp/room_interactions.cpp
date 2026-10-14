#include "room_interactions.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "turn_rules.h"
#include "room_ladders.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr float pi = 3.1415927f;
Matrix34 matrix(const RoomObjectPlacement &p, float yaw = 0) {
  SmdPlacement pose;
  pose.position = p.position;
  pose.rotation = p.rotation;
  pose.rotation[1] += yaw;
  pose.scale = {1, 1, 1};
  return PlacementMatrix(pose);
}
CollisionVec local(const RoomObjectPlacement &p, const CollisionVec &world) {
  const auto m = matrix(p);
  CollisionVec v{};
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 3; ++c)
      v[r] += double(m[c * 4 + r]) * (world[c] - p.position[c]);
  return v;
}
CollisionVec world(const RoomObjectPlacement &p, const std::array<float, 3> &v) {
  const auto point = TransformPoint(matrix(p), v);
  return {point[0], point[1], point[2]};
}
double lengthSquared(const CollisionVec &a, const CollisionVec &b) {
  double sum = 0;
  for (unsigned k = 0; k < 3; ++k) sum += (a[k] - b[k]) * (a[k] - b[k]);
  return sum;
}
void valid(const CollisionVec &p) {
  for (auto value : p)
    if (!std::isfinite(value) || std::abs(value) > 1e7)
      throw std::runtime_error("Room interaction: Coordinate budget");
}
using WallTriangle = std::array<std::array<float, 3>, 3>;
float doorTurn(float source, float target) {
  // sub2.cpp::Muku2(source,target,PI), including the exact PI tie branch.
  float d = target - source, add = pi;
  if (d < 0) d += 2 * pi;
  if (d < pi) { if (add > d) add = d; }
  else { d -= 2 * pi; add = add > -d ? d : -add; }
  return add;
}
void quad(std::vector<WallTriangle> &walls, const Matrix34 &m,
          std::array<float, 3> a, std::array<float, 3> b,
          std::array<float, 3> c, std::array<float, 3> d) {
  a = TransformPoint(m, a); b = TransformPoint(m, b);
  c = TransformPoint(m, c); d = TransformPoint(m, d);
  walls.push_back({a, b, c}); walls.push_back({a, c, d});
}
} // namespace
unsigned WindowWeaponHitPoints(unsigned hp, unsigned hpType, unsigned weaponNo) {
  if (OriginalWeaponSize(weaponNo)==1 || weaponNo == 0x10) return 0;
  if (!OriginalWeaponAction(weaponNo))
    throw std::runtime_error("Window: unverified damage weapon");
  if (hpType == 0) return 0;
  if (hpType == 1) return hp-std::min(hp,500u);
  if (hpType == 2) return hp-std::min(hp,250u);
  return hp;
}
const char *InteriorEventLabel(InteriorEvent::Kind kind) {
  switch (kind) {
  case InteriorEvent::Open: return "Door opened";
  case InteriorEvent::Kick: return "Door kicked";
  case InteriorEvent::BlockedKick: return "Door locked · Hit the lock";
  case InteriorEvent::Close: return "Door closed";
  case InteriorEvent::LockBroken: return "Door lock released";
  case InteriorEvent::WindowCrack: return "Window hit";
  case InteriorEvent::WindowBroken: return "Window destroyed";
  case InteriorEvent::ProtectedWindow: return "Window locked by room event";
  case InteriorEvent::PushStart: return "Push furniture";
  case InteriorEvent::PushLoop: return "Furniture push cycle";
  case InteriorEvent::PushStop: return "Furniture blocked";
  case InteriorEvent::WindowVaultSound: return "Window jump: Original sound";
  case InteriorEvent::ScriptDoorVoice: return "Original door event";
  }
  return "Room interaction";
}
void RoomInteractions::syncVillageState(const RoomSaveState &save) {
  if(room_!=0x101)return;
  state_.room=save.room;
  for(auto &window:windows_)if(window.placement.slot==0 || window.placement.slot==0x13)
    window.protectedByEvent=!save.room.test(8) && !save.room.test(7);
  for(auto &door:doors_)if(door.placement.slot==2 && !villageDoorCompleted_) {
    if(!save.room.test(8) && !save.room.test(7)) {
      if(door.phase!=InteriorDoorPhase::ScriptOpen)door.scriptAngle=door.placement.rotation[1]+door.yaw;
      door.phase=InteriorDoorPhase::ScriptOpen;door.scriptLocked=true;door.scriptInitialized=false;
    } else {
      if(!villageDoorCloseTicks_) {
        door.scriptAngle=door.placement.rotation[1]+door.yaw;
        door.phase=InteriorDoorPhase::ScriptClose;door.scriptLocked=true;door.scriptInitialized=false;
      }
      if(++villageDoorCloseTicks_>=30) {door.scriptLocked=false;villageDoorCompleted_=true;}
    }
  }
}
void RoomInteractions::setVillageHouseRack(WalkProbe &probe) {
  if(room_!=0x101)return;
  for(auto &rack:racks_)if(rack.placement.slot==0xf) {rack.placement.position[0]=7201;rack.placement.position[2]=-7280;}
  rebuildCollision(nullptr,&probe);
}
void RoomInteractions::breakVillageWindow(unsigned slot, WalkProbe &probe) {
  if(room_!=0x101)return;
  for(auto &window:windows_)if(window.placement.slot==slot) {window.hp=0;window.protectedByEvent=false;state_.setObject(slot,1);}
  rebuildCollision(nullptr,&probe);
}
unsigned RoomInteractions::randomByte() {
  // Original Rnd recurrence, isolated just like the existing drop adapter.
  // Until the campaign owns one RNG stream this does not claim GC stream order.
  const auto r = randomState_;
  const auto n = (std::uint32_t(std::uint8_t((r >> 1) + (r >> 8))) << 8) |
                 std::uint8_t(r >> 1);
  randomState_ = std::uint16_t(n == r ? n + 0x101 : n);
  return std::uint8_t(randomState_ >> 8);
}
RoomInteractions::RoomInteractions(
    std::uint16_t room, const RoomArchive &archive,
    const std::vector<RoomObjectPlacement> &placements,
    const std::string &playerDrs, const RoomSaveState &state,
    const SatCollision *effect)
    : room_(room), state_(state), effect_(effect) {
  if (playerDrs.empty())
    throw std::runtime_error("Room interaction: Original player motions are missing");
  const auto *models = archive.find("ETM");
  if (!models) throw std::runtime_error("r101: Interaction models are missing");
  const RoomObjectArchive etm(archive.copy(models - archive.entries().data()));
  const auto player = RoomArchive::LoadDrs(playerDrs);
  if (player.entries().size() <= 27 || player.entries()[27].tag[0] != 'F' ||
      player.entries()[27].tag[1] != 'C' || player.entries()[27].tag[2] != 'V')
    throw std::runtime_error("r101: Original door motion is missing");
  const auto profile = VillageProfile(state_);
  for (const auto &p : placements) {
    if (p.id == 3 || p.id == 0x0d) {
      InteriorDoor door;
      door.placement = p;
      door.type = p.id == 3 ? 0 : 1;
      (void)(randomByte() % 5); (void)(randomByte() & 1);
      // R101Init installs side 0, normal strength, only on ETS11.
      if (room == 0x101 && p.slot == 11 && !state_.object(p.slot, 2))
        door.lockHp = (randomByte() & 1) + 3;
      if (state_.object(p.slot, 1)) door.hp = 0;
      const auto name = p.id == 3 ? "et0300.bin" : "et0d00.bin";
      const SkinnedModel body(etm.copy(name));
      door.motion = std::make_unique<AnimationRig>(body,
          std::vector<AnimationBytes>{{player.copy(27), {}}});
      const auto cycle = door.motion->cycle(0);
      if (std::abs(cycle.yaw + pi / 2) > .001 ||
          std::abs(cycle.translation[0]) + std::abs(cycle.translation[1]) +
                  std::abs(cycle.translation[2]) > .001)
        throw std::runtime_error("r101: Door motion does not match the leaf profile");
      std::vector<std::uint16_t> flip(body.mesh.parts);
      std::iota(flip.begin(), flip.end(), std::uint16_t(0));
      door.motion->setFlipTable(flip); // emDoor_xflip_tbl is identity.
      doors_.push_back(std::move(door));
    } else if (p.id == 0 || p.id == 7 || p.id == 0x25) {
      InteriorWindow window;
      window.placement = p;
      window.hpType = p.id == 0 ? 1 : p.id == 7 ? 3 : 0;
      window.width = p.id == 0x25 ? 800 : 1300;
      window.height = p.id == 0x25 ? 1010 : 1400;
      window.lowSat = p.id == 0x25;
      window.field = p.id != 0x25;
      window.protectedByEvent = room == 0x101 && profile.protectedWindows &&
                                (p.slot == 0 || p.slot == 19);
      if (state_.object(p.slot, 1)) window.hp = 0;
      // R111Init starts with both event panes broken, independent of r101.
      if (room == 0x111 && (p.slot == 0 || p.slot == 0x13)) {
        window.hp = 0; state_.setObject(p.slot, 1);
      }
      windows_.push_back(window);
    } else if ((room == 0x101 || room == 0x111 || room == 0x103 || room == 0x113 || room == 0x106) && (p.id == 4 || p.id == 5)) {
      InteriorRack rack;
      rack.placement = p;
      if ((room == 0x101 || room == 0x111) && p.slot == 17) rack.placement.position = {6656, 902, 8925};
      rack.base = rack.placement;
      rack.height = p.id == 5 ? 2000.f : 1000.f;
      if ((room == 0x103 || room == 0x113) && p.slot == 6) rack.range = {0, 3000, 0, 3000};
      else if (room == 0x106 && p.slot == 0) rack.range = {0, 1800, 0, 2200};
      else if ((room == 0x101 || room == 0x111) && p.slot == 13) rack.range = {0, 1000, 0, 2000};
      else if ((room == 0x101 || room == 0x111) && p.slot == 15) rack.range = {2000, 2000, 0, 2600};
      else if ((room == 0x101 || room == 0x111) && p.slot == 17) rack.range = {2000, 800, 0, 4400};
      else throw std::runtime_error("Room: Unverified shelf slot");
      // cPlPush::pushTargetInit: PL_ARC0x59 is entry85, with the same
      // original 30-frame root curve as Leon's PL_ARC0x21 push.
      const SkinnedModel body(etm.copy(p.id == 4 ? "et0400.bin" : "et0500.bin"));
      if (player.entries().size() <= 85 || player.entries()[85].tag[0] != 'F')
        throw std::runtime_error("r101: Original shelf motion is missing");
      rack.motion = std::make_unique<AnimationRig>(body,
          std::vector<AnimationBytes>{{player.copy(85), {}}});
      racks_.push_back(std::move(rack));
    }
  }
  if ((room == 0x101 && (doors_.size() != 4 || windows_.size() != 5)) ||
      (room == 0x100 && (doors_.size() != 1 || windows_.size() != 2)))
    throw std::runtime_error("Door/window profile mismatch");
  if (room == 0x100) {
    if (doors_[0].placement.slot != 2 || windows_[0].placement.slot != 0 ||
        windows_[1].placement.slot != 1)
      throw std::runtime_error("r100: original door/window script identities are missing");
    // R100Init starts the tasks before playable input. Install only their
    // unconditional initial presentation; flags are evaluated on live ticks.
    if (!state_.room.test(10))
      for (auto &window : windows_) window.fenceFlags |= 0x60000000;
    if (!state_.room.test(3)) {
      auto &door = doors_[0];
      door.phase = InteriorDoorPhase::ScriptOpen; door.scriptLocked = true;
      door.scriptAngle = door.placement.rotation[1] + pi / 2;
      door.yaw = door.scriptAngle - door.placement.rotation[1];
      door.timer = 20; door.scriptInitialized = true;
    }
  }
  // The ordinary ET00/ET07/ET25 and ET03/ET0D source model profiles are
  // shared across rooms. Cupboard glass (ET25) and door-only rooms do not
  // carry window-vault FCVs and must not lose their valid runtime for that.
  const std::array<const char*,3> vaultNames{{"pl00537.fcv","pl00538.fcv","pl00536.fcv"}};
  // r11d's ET07 has no intact glass jump clips. Already broken directions
  // 0/1 still use Leon's shared short-fence motions; the owner's other
  // collisions, damage and doors remain valid without optional vault FCVs.
  if (std::any_of(windows_.begin(),windows_.end(),[](const auto &w){return w.field;}) &&
      std::all_of(vaultNames.begin(),vaultNames.end(),[&](auto name){return etm.find(name);})) {
    for (const char *name : {"pl00537.fcv", "pl00538.fcv", "pl00536.fcv"})
      windowMotions_.push_back({etm.copy(name), {}});
    const SkinnedModel playerBody(player.copy(0));
    AnimationRig predictor(playerBody, windowMotions_);
    for (unsigned direction=0; direction<3; ++direction) {
      predictor.startActionFlags(direction, 0, 0, 0x201);
      bool complete=false;
      for (unsigned tick=0; tick<180; ++tick) {
        const auto root=predictor.advanceRootAction();
        windowPaths_[direction].push_back(root);
        if (root.ended) { complete=true; break; }
      }
      if (!complete) throw std::runtime_error("Window: Original motion does not end");
    }
  }
  rebuildCollision(nullptr, nullptr);
}
bool RoomInteractions::windowFenceEnabled(unsigned slot, unsigned kind) const {
  const auto window = std::find_if(windows_.begin(), windows_.end(),
      [slot](const auto &value) { return value.placement.slot == slot; });
  if (window == windows_.end() || (kind != 1 && kind != 2)) return false;
  return !(window->fenceFlags & (kind == 1 ? 0x40000000u : 0x20000000u));
}
bool RoomInteractions::tickR100Scripts(RoomSaveState &save, SourceFlags<4> &flags,
                                      const std::array<bool,3> &foundPlayer) {
  if (room_ != 0x100) return false;
  bool release = false;
  // SceExec creation order: the two-window task precedes the door task.
  if (!r100WindowTaskDone_) {
    if (!r100WindowsEnabled_) {
      if (!save.room.test(10)) {
        for (auto &window : windows_) window.fenceFlags |= 0x60000000;
      } else {
        for (auto &window : windows_) window.fenceFlags &= ~0x60000000u;
        r100WindowsEnabled_ = true;
      }
    }
    if (r100WindowsEnabled_) {
      if (save.room.test(4)) r100WindowTaskDone_ = true;
      else if (!flags.test(64)) {
        save.room.set(4); release = true; r100WindowTaskDone_ = true;
      }
    }
  }
  auto &door = doors_[0];
  const auto openLock = [&] {
    door.phase = InteriorDoorPhase::ScriptOpen;
    door.scriptLocked = true; door.scriptInitialized = false;
  };
  const auto finish = [&] {
    r100DoorMessageEnabled_ = false;
    door.scriptLocked = false; // setNormal only clears Be_flg, never snaps yaw.
    r100DoorTask_ = R100DoorTask::Done;
  };
  if (r100DoorTask_ == R100DoorTask::OpenUntilFirstLook) {
    if (!save.room.test(3)) { openLock(); state_.room = save.room; return release; }
    flags.set(0, false); r100DoorFound_ = false;
    r100DoorTask_ = R100DoorTask::WaitForFound;
  }
  if (r100DoorTask_ == R100DoorTask::WaitForFound) {
    if (save.room.test(4)) finish();
    else {
      if (flags.test(0)) r100DoorFound_ = true;
      if (r100DoorFound_) {
        if (flags.test(64))
          event(InteriorEvent::ScriptDoorVoice, door.placement.slot, 6, 0x28, door.placement.position);
        r100CloseSleeps_ = 0; r100CloseSleepPending_ = false;
        r100DoorTask_ = R100DoorTask::CloseUntilExit;
      } else {
        if (save.room.test(10) && std::any_of(foundPlayer.begin(), foundPlayer.end(),
                                           [](bool found) { return found; }))
          r100DoorFound_ = true;
        // Enemy detection still passes through this sleep before closing.
        openLock(); state_.room = save.room; return release;
      }
    }
  }
  if (r100DoorTask_ == R100DoorTask::CloseUntilExit) {
    // The increment is AFTER the preceding SceSleep, before the while test.
    if (r100CloseSleepPending_ && r100CloseSleeps_++ == 19)
      r100DoorMessageEnabled_ = true;
    if (save.room.test(4)) finish();
    else {
      if (door.phase != InteriorDoorPhase::ScriptOpen && door.phase != InteriorDoorPhase::ScriptClose)
        door.scriptAngle = door.placement.rotation[1] + door.yaw;
      door.phase = InteriorDoorPhase::ScriptClose;
      door.scriptLocked = true; door.scriptInitialized = false;
      r100CloseSleepPending_ = true;
    }
  }
  state_.room = save.room;
  return release;
}
std::optional<InteriorWindowOffer> RoomInteractions::windowOffer(
    const LeonPreview &player, const WalkProbe &probe) const {
  if (!probe.ready() || !player.alive() || player.action()!=PlayerAction::Footwork) return {};
  const auto feet=player.feet();
  auto from=feet; from[1]+=400;
  auto forward=from; forward[0]+=std::sin(player.heading())*1000;
  forward[2]+=std::cos(player.heading())*1000;
  for (std::size_t i=0; i<windows_.size(); ++i) {
    const auto &w=windows_[i];
    if (!w.field || !windowFenceEnabled(w.placement.slot,1) || (w.placement.id==7 && w.hp)) continue;
    if (ladders_ && !ladders_->permitsWindowCross({w.placement.position[0],
            w.placement.position[1],w.placement.position[2]})) continue;
    // pl_class::windowCheck calls ChkWindow with a 1000-unit forward point
    // at +400Y. SceAtCheckFieldInfo tests that endpoint in the authored
    // rectangle, without its facing cone (pModel==nullptr), then ChkWindow
    // requires the two points to cross the pane's front/back halfspaces.
    // The field survives SetAtariOff when the pane has already broken.
    const auto a=local(w.placement,from),b=local(w.placement,forward);
    if (std::abs(b[0])>500 || std::abs(b[2])>1000 || b[1]<-2100 || b[1]>=1000) continue;
    if ((a[2]>=0)==(b[2]>=0)) continue;
    const bool front=a[2]>=0;
    const float facing=float(w.placement.rotation[1]+(front?pi:0));
    bool lower=false;
    for (float z : {-2000.f,2000.f}) {
      const auto p=world(w.placement,{0,0,z});
      const auto floor=probe.world().ground(p,0,320000);
      lower|=!floor || floor->point[1]<w.placement.position[1]-3000;
    }
    const unsigned direction=front?1:lower?2:0;
    const std::array<std::array<float,3>,3> starts{{{90,-1000,-1610},{-210,-1000,1990},{0,-1000,-980}}};
    InteriorWindowOffer offer;
    offer.index=i; offer.slot=w.placement.slot; offer.direction=direction;
    offer.start=world(w.placement,starts[direction]); offer.heading=facing;
    bool rack=false;
    for (const auto &r:racks_) {
      const auto v=local(r.placement,feet);
      rack|=std::abs(v[0])<1000 && std::abs(v[2])<1000 && std::abs(v[1])<r.height;
    }
    if (rack) continue;
    if (!w.hp && direction!=2) {
      // cPlayer::actionSelect: a broken same-height pane uses fanceOn
      // (r3=5, short PL_ARC55/56), with PlFancePos400 before its plane.
      auto alignment=world(w.placement,{0,0,front?400.f:-400.f});
      alignment[1]=feet[1];
      offer.brokenFence=player.brokenWindowOffer(probe,alignment,facing);
      if (!offer.brokenFence) continue;
      offer.start=alignment; offer.landing=offer.brokenFence->landing;
      return offer;
    }
    if (windowPaths_[direction].empty()) continue;
    auto end=offer.start; double yaw=facing;
    for (const auto &root:windowPaths_[direction]) {
      const auto c=std::cos(yaw),s=std::sin(yaw);
      end[0]+=c*root.translation[0]+s*root.translation[2]; end[1]+=root.translation[1];
      end[2]+=-s*root.translation[0]+c*root.translation[2]; yaw+=root.yaw;
    }
    auto checked=probe; checked.setObjectCollision(collision_.get());
    const auto landing=checked.traversalLanding(end);
    if (!landing) continue;
    offer.landing=*landing; return offer;
  }
  return {};
}
void RoomInteractions::tickWindow(LeonPreview &player) {
  if (!player.windowActive()) return;
  const auto feet=player.feet();
  const std::array<float,3> soundPosition{float(feet[0]),float(feet[1]),float(feet[2])};
  for (auto &w:windows_) if (w.placement.slot==player.windowSlot()) {
    const unsigned tick=player.windowTicks(),direction=player.windowDirection();
    if (tick==0) state_.setObject(w.placement.slot,2);
    const unsigned breakTick=direction==0?15:direction==1?16:13;
    if (tick==breakTick && w.hp) {
      w.hp=0; state_.setObject(w.placement.slot,1);
      event(InteriorEvent::WindowBroken,w.placement.slot,6,w.placement.id==7?0x39:0x3b,w.placement.position);
      auto &glass = events_.back();
      // SetBreakAll(..., size=1, breakType=1), literal tblB.
      glass.effectObject = w.placement.id;
      glass.effectSequence = direction == 2 ? 3 : int(direction);
      glass.effectRotation = w.placement.rotation;
    }
    for (const auto &[when,sound] : std::array<std::pair<unsigned,unsigned>,3>{{{3,0x29},{4,0x34},{5,0x43}}})
      if (tick==when) event(InteriorEvent::WindowVaultSound,w.placement.slot,1,sound,soundPosition);
    const unsigned land=direction==0?35:direction==1?45:40;
    const unsigned step1=direction==0?41:direction==1?51:55;
    const unsigned step2=direction==0?44:direction==1?55:57;
    if (tick==land) event(InteriorEvent::WindowVaultSound,w.placement.slot,5,5,soundPosition);
    if (tick==step1) event(InteriorEvent::WindowVaultSound,w.placement.slot,5,0x12,soundPosition);
    if (tick==step2) event(InteriorEvent::WindowVaultSound,w.placement.slot,5,0x13,soundPosition);
    return;
  }
}
bool RoomInteractions::rackBlocks(const InteriorDoor &door) const {
  for (const auto &entry : racks_) {
    const auto &rack = entry.placement;
    if (lengthSquared({rack.position[0], 0, rack.position[2]},
                      {door.placement.position[0], 0, door.placement.position[2]}) > 16000000)
      continue;
    constexpr std::array<std::array<float, 2>, 8> corners{{
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {0, 1}, {0, -1}, {1, 0}, {-1, 0}}};
    for (const auto &corner : corners) {
      // emRack type0/type1: Size_x=700, Size_z=400. R101Init's setRange
      // limits rack movement, and does not resize the collision box.
      const auto v = local(door.placement, world(rack, {corner[0] * 700, 0, corner[1] * 400}));
      if (std::abs(v[0] + 650) < 650 && std::abs(v[2]) < 600 && std::abs(v[1]) < 1000)
        return true;
    }
  }
  return false;
}
std::optional<InteriorDoorOffer> RoomInteractions::doorOffer(
    const CollisionVec &feet, double heading) const {
  valid(feet);
  if (!std::isfinite(heading) || std::abs(heading) > 100)
    throw std::runtime_error("Room interaction: Invalid field of view");
  for (std::size_t i = 0; i < doors_.size(); ++i) {
    const auto &door = doors_[i];
    if (!door.hp || door.scriptLocked || door.phase != InteriorDoorPhase::Closed) continue;
    const auto angle = std::abs(std::remainder(double(door.placement.rotation[1]) - heading, 2 * pi));
    if (angle > pi / 4 && angle < 3 * pi / 4) continue;
    const auto v = local(door.placement, feet);
    const bool front = angle < pi / 2;
    if (std::abs(v[0] + 650) > 650 || std::abs(v[1]) > 500 ||
        (front ? v[2] > 0 || v[2] < -800 : v[2] < 0 || v[2] > 800)) continue;
    const bool rack = rackBlocks(door);
    return InteriorDoorOffer{i, door.placement.slot, front,
                             door.lockHp != 0 || rack, door.lockHp > 1};
  }
  return {};
}
bool RoomInteractions::interact(LeonPreview &player, WalkProbe &probe) {
  if (player.action() == PlayerAction::DoorOpen && activeDoor_ && player.doorTicks() < 45) {
    auto &door = doors_.at(*activeDoor_);
    player.beginDoor(true, door.lockHp > 1, player.feet(), player.heading(), 6);
    door.phase = InteriorDoorPhase::Closed;
    door.hit14 = door.hit17 = false;
    rebuildCollision(&player, &probe);
    return true;
  }
  if (player.action() != PlayerAction::Footwork) return false;
  if (const auto window=windowOffer(player,probe)) {
    if (window->brokenFence) player.beginBrokenWindow(probe,*window->brokenFence);
    else { player.installWindowMotions(windowMotions_); player.beginWindow(probe,*window); }
    return true;
  }
  const auto offer = doorOffer(player.feet(), player.heading());
  if (!offer) return false;
  auto &door = doors_.at(offer->index);
  const auto alignment = world(door.placement, {-638.54f, 0, offer->front ? -440.4f : 440.4f});
  const auto heading = std::remainder(door.placement.rotation[1] + (offer->front ? 0 : pi), 2 * pi);
  player.beginDoor(offer->kick, offer->blocked,
                   offer->kick ? player.feet() : alignment,
                   offer->kick ? player.heading() : heading);
  activeDoor_ = offer->index;
  activeFront_ = offer->front;
  door.hit14 = door.hit17 = false;
  if (!offer->kick) {
    door.phase = InteriorDoorPhase::HandOpening;
    door.mirrored = offer->front;
    door.motion->startActionFlags(0, 0, 0, door.mirrored ? 0x41 : 1);
  }
  rebuildCollision(&player, &probe);
  return true;
}
std::optional<InteriorPushOffer> RoomInteractions::pushOffer(
    const LeonPreview &player, const WalkProbe &probe) const {
  if (player.action() != PlayerAction::Footwork || !player.alive()) return {};
  const auto feet = probe.position();
  const auto heading = player.heading();
  auto ahead = feet;
  ahead[0] += std::sin(heading)*300; ahead[2] += std::cos(heading)*300;
  for (std::size_t i=0;i<racks_.size();++i) {
    const auto &rack=racks_[i]; const auto &p=rack.placement;
    if (std::abs(feet[1]-p.position[1])>500 || state_.object(p.slot,1)) continue;
    const auto v=local(p,ahead);
    const double qx=std::clamp(v[0],-600.,600.), qz=std::clamp(v[2],-300.,300.);
    if (std::hypot(v[0]-qx,v[2]-qz)>probe.config().radius) continue;
    const auto side=local(p,feet);
    unsigned dir=4;
    if (side[2]>-300 && side[2]<300) {
      if(side[0]>600) dir=3; else if(side[0]<-600) dir=1;
    }
    if (side[0]>-600 && side[0]<600) {
      if(side[2]>300) dir=0; else if(side[2]<-300) dir=2;
    }
    if(dir==4) continue;
    const double offset[]={pi,pi/2,0,pi*1.5};
    const auto yaw=float(std::remainder(p.rotation[1]+offset[dir],2*pi));
    // cPlPush catch is a forward contact, never a sideways proximity action.
    if(std::abs(std::remainder(double(yaw)-heading,2*pi))>pi/2) continue;
    auto target=CollisionVec{p.position[0],p.position[1]+300,p.position[2]};
    if(probe.world().line(player.parts0World(),target,SatSubset::All,0x800)) continue;
    return InteriorPushOffer{i,p.slot,dir,yaw};
  }
  return {};
}
bool RoomInteractions::rackMoveClear(std::size_t index,
                                      const RoomObjectPlacement &next,
                                      const WalkProbe &probe) const {
  const auto &rack=racks_.at(index);
  // Source scrHitCheck uses front/side lines at Y+100 and a real floor,
  // independently of the player capsule. Check the whole moving footprint.
  const auto floor=probe.world().ground({next.position[0],next.position[1],next.position[2]},600,999);
  if(!floor || floor->normal[1]<.5 || std::abs(floor->point[1]-next.position[1])>=1000) return false;
  constexpr std::array<std::array<float,3>,4> corners{{{-600,100,-300},{600,100,-300},{600,100,300},{-600,100,300}}};
  for(unsigned k=0;k<4;++k) {
    const auto a=world(rack.placement,corners[k]), b=world(next,corners[k]), c=world(next,corners[(k+1)%4]);
    if(probe.world().line(a,b,SatSubset::Walls,0x800) ||
       probe.world().line(b,c,SatSubset::Walls,0x800)) return false;
  }
  const auto overlap=[&](const RoomObjectPlacement &other,float cx,float hx,float hz,
                          float low,float high) {
    if(next.position[1]>=other.position[1]+high ||
       next.position[1]+rack.height<=other.position[1]+low) return false;
    const auto center=world(other,{cx,0,0});
    const auto a=matrix(next), b=matrix(other);
    const double dx=next.position[0]-center[0], dz=next.position[2]-center[2];
    for (const auto axis : {CollisionVec{a[0],0,a[8]},CollisionVec{a[2],0,a[10]},
                           CollisionVec{b[0],0,b[8]},CollisionVec{b[2],0,b[10]}}) {
      const auto dot2=[&](double x,double z){return axis[0]*x+axis[2]*z;};
      const double radius=600*std::abs(dot2(a[0],a[8]))+300*std::abs(dot2(a[2],a[10]))+
                          hx*std::abs(dot2(b[0],b[8]))+hz*std::abs(dot2(b[2],b[10]));
      if(std::abs(dot2(dx,dz))>=radius-.01) return false;
    }
    return true;
  };
  for(std::size_t i=0;i<racks_.size();++i) if(i!=index) {
    const auto &other=racks_[i]; if(overlap(other.placement,0,600,300,0,other.height)) return false;
  }
  for(const auto &door:doors_) if(door.hp) {
    auto p=door.placement; p.rotation[1]+=door.yaw;
    if(overlap(p,-650,700,150,-50,2350)) return false;
  }
  for(const auto &window:windows_) if(window.hp && window.field) {
    const float low=window.lowSat?-window.height/2-200:-200;
    if(overlap(window.placement,0,window.width/2+200,1,low,low+window.height+400)) return false;
  }
  return true;
}
void RoomInteractions::tickPush(LeonPreview &player, WalkProbe &probe,
                                double forward, bool allowPush) {
  if(!std::isfinite(forward)) throw std::runtime_error("Furniture: invalid input");
  const bool held=GameCubeMappedDirections(0,forward)&MoveForward;
  if(activeRack_ && !player.pushActive()) {
    racks_.at(*activeRack_).pushing=false; activeRack_.reset();
  }
  if(!activeRack_ && held && allowPush) {
    if(const auto offer=pushOffer(player,probe)) {
      activeRack_=offer->index;
      auto &rack=racks_.at(*activeRack_);
      rack.direction=offer->direction; rack.pushing=rack.stopped=false;
      player.beginPush(offer->heading);
    }
  }
  if(!activeRack_) return;
  auto &rack=racks_.at(*activeRack_);
  if(player.action()!=PlayerAction::Push || !held) { rack.pushing=false; return; }
  if(!rack.pushing) {
    rack.motion->startActionFlags(0,0,0,5); rack.pushing=true;
    event(InteriorEvent::PushStart,rack.placement.slot,6,0x55,rack.placement.position);
  }
  const auto root=rack.motion->advanceRootAction();
  const double offsets[]={pi,pi/2,0,pi*1.5};
  const auto yaw=rack.placement.rotation[1]+offsets[rack.direction];
  const auto c=std::cos(yaw),s=std::sin(yaw);
  auto next=rack.placement;
  next.position[0]+=float(c*root.translation[0]+s*root.translation[2]);
  next.position[2]+=float(-s*root.translation[0]+c*root.translation[2]);
  auto v=local(rack.base,{next.position[0],next.position[1],next.position[2]});
  bool limit=false;
  switch(rack.direction) {
    case 0: limit=v[2]<-rack.range[2]; v[2]=std::max(v[2],-double(rack.range[2])); break;
    case 1: limit=v[0]> rack.range[1]; v[0]=std::min(v[0], double(rack.range[1])); break;
    case 2: limit=v[2]> rack.range[0]; v[2]=std::min(v[2], double(rack.range[0])); break;
    case 3: limit=v[0]<-rack.range[3]; v[0]=std::max(v[0],-double(rack.range[3])); break;
  }
  const auto clamped=world(rack.base,{float(v[0]),float(v[1]),float(v[2])});
  for(unsigned k=0;k<3;++k) next.position[k]=float(clamped[k]);
  if(!rackMoveClear(*activeRack_,next,probe)) limit=true;
  else rack.placement=next;
  if(limit) {
    rack.pushing=false; rack.stopped=true; player.stopPush();
    event(InteriorEvent::PushStop,rack.placement.slot,-1,0,rack.placement.position);
  } else if(root.ended || (rack.motion->status().state&3))
    event(InteriorEvent::PushLoop,rack.placement.slot,6,0x55,rack.placement.position);
}

void RoomInteractions::event(InteriorEvent::Kind kind, unsigned slot, int block,
                              unsigned number, const std::array<float, 3> &pos) {
  if (events_.size() >= 256) throw std::runtime_error("Room interaction: Event budget");
  events_.push_back({kind, slot, block, number, {pos[0], pos[1], pos[2]}});
}
void RoomInteractions::breakLock(InteriorDoor &door, bool shot) {
  if (!door.lockHp) return;
  door.lockHp = 0;
  // Literal side-0 emDoorSetDmgLock_R writes bit2 in the damage branch,
  // and bit1 in its common hp<=0 tail. The resulting mask is 6 for shots;
  // an opening kick runs the common tail only (mask2).
  state_.setObject(door.placement.slot, shot ? 6 : 2);
  event(InteriorEvent::LockBroken, door.placement.slot, 6,
        shot ? 0x52 : 0x53, door.placement.position);
  event(InteriorEvent::LockBroken, door.placement.slot, 6,
        shot ? 0x53 : 0x52, door.placement.position);
}
void RoomInteractions::tick(LeonPreview &player, WalkProbe &probe,
                             double forward, bool allowPush) {
  ++ticks_;
  tickWindow(player);
  if (activeDoor_ && !player.doorActive()) activeDoor_.reset();
  if (activeDoor_) {
    auto &door = doors_.at(*activeDoor_);
    const auto frame = player.rig().sequenceFrame();
    if ((player.action() == PlayerAction::DoorKick ||
         player.action() == PlayerAction::DoorBlockedKick) && frame >= 14 && !door.hit14) {
      door.hit14 = true;
      if (player.action() == PlayerAction::DoorBlockedKick) {
        const auto v = local(door.placement, player.feet());
        if (v[2] <= 0 && door.lockHp > 1) {
          --door.lockHp;
          if ((randomByte() & 1) && door.lockHp > 1) --door.lockHp;
        }
        door.hp = std::max(1u, door.hp - std::min(door.hp, rackBlocks(door) ? 25u : 50u));
        door.phase = InteriorDoorPhase::Shock;
        door.timer = 7;
        event(InteriorEvent::BlockedKick, door.placement.slot, 1,
              door.type == 0 ? 0x1d : 0x1f, door.placement.position);
      } else {
        breakLock(door, false);
        door.phase = InteriorDoorPhase::KickOpening;
        door.openTimer = 0x96;
        door.mirrored = activeFront_;
        event(InteriorEvent::Kick, door.placement.slot, 1,
              door.type == 0 ? 0x1e : 0x20, door.placement.position);
      }
    }
    if (player.action() == PlayerAction::DoorBlockedKick && frame >= 17 && !door.hit17)
      door.hit17 = true; // Original second setShock only bends the lock (mode2).
  }
  for (auto &door : doors_) {
    if (door.openTimer) --door.openTimer;
    switch (door.phase) {
    case InteriorDoorPhase::HandOpening: {
      const auto before = door.motion->sequenceFrame();
      const auto root = door.motion->advanceRootAction();
      door.yaw = std::remainder(door.yaw + float(root.yaw), 2 * pi);
      if (before <= 15 && door.motion->sequenceFrame() > 15)
        event(InteriorEvent::Open, door.placement.slot, 1,
              door.type == 0 ? 0x42 : 0x41, door.placement.position);
      if (root.ended) door.phase = InteriorDoorPhase::Open;
      break;
    }
    case InteriorDoorPhase::KickOpening: {
      const auto goal = door.mirrored ? pi / 2 : -pi / 2;
      const auto speed = door.mirrored ? pi / 7 : pi / 6;
      const auto step = std::clamp(std::remainder(goal - door.yaw, 2 * pi), -speed, speed);
      door.yaw += step;
      if (std::abs(step) < pi / 1024) {
        door.yaw = goal;
        door.phase = InteriorDoorPhase::Open;
      }
      break;
    }
    case InteriorDoorPhase::Open: {
      if (door.openTimer) break;
      const auto v = local(door.placement, player.feet());
      if (std::abs(v[0] + 650) > 800 || std::abs(v[1]) > 500 || std::abs(v[2]) > 1500) {
        if (!rackBlocks(door)) {
          door.phase = InteriorDoorPhase::Closing;
          door.timer = 20;
          event(InteriorEvent::Close, door.placement.slot, -1, 0, door.placement.position);
        }
      }
      break;
    }
    case InteriorDoorPhase::Closing:
      door.yaw += std::remainder(-door.yaw, 2 * pi) * .3f;
      if (door.timer) --door.timer;
      else { door.yaw = 0; door.phase = InteriorDoorPhase::Closed; }
      break;
    case InteriorDoorPhase::Shock:
      if (door.timer) {
        --door.timer;
        door.yaw = (float(randomByte()) / 256 + 1) * (pi / 128) * (ticks_ & 1 ? -1 : 1);
      } else { door.yaw = 0; door.phase = InteriorDoorPhase::Closed; }
      break;
    case InteriorDoorPhase::Closed: break;
    case InteriorDoorPhase::ScriptOpen:
      if (!door.scriptInitialized) {
        door.scriptAngle = door.placement.rotation[1] + pi / 2;
        door.yaw = door.scriptAngle - door.placement.rotation[1];
        door.timer = 20; door.scriptInitialized = true;
      }
      if (!door.scriptLocked) door.phase = InteriorDoorPhase::Open;
      break;
    case InteriorDoorPhase::ScriptClose: {
      if (!door.scriptInitialized) { door.timer = 20; door.scriptInitialized = true; }
      // Preserve source float arithmetic on the absolute authored angle.
      door.scriptAngle = GameCubeLimitAngle(door.scriptAngle +
          doorTurn(door.scriptAngle, door.placement.rotation[1]) * .3f);
      door.yaw = door.scriptAngle - door.placement.rotation[1];
      if (door.timer) --door.timer;
      else {
        door.yaw = 0;
        door.scriptAngle = door.placement.rotation[1];
        if (!door.scriptLocked) door.phase = InteriorDoorPhase::Closed;
      }
      break;
    }
    }
  }
  tickPush(player, probe, forward, allowPush);
  rebuildCollision(&player, &probe);
  if (!player.doorActive() && !player.ladderActive() && !player.jumpActive() && !player.windowActive() &&
      !player.blastActive() && !player.grabbed() && player.alive())
    probe.resolveObjectOverlaps();
}
void RoomInteractions::rebuildCollision(const LeonPreview *player, WalkProbe *probe) {
  std::vector<WallTriangle> walls;
  std::vector<ObjectBodyBox> bodies = sceneryBodies_;
  for (std::size_t i = 0; i < doors_.size(); ++i) {
    const auto &door = doors_[i];
    if (!door.hp || (activeDoor_ == i && player && player->doorActive())) continue;
    const auto m = matrix(door.placement, door.yaw);
    bodies.push_back({{door.placement.position[0], door.placement.position[1], door.placement.position[2]},
                      door.placement.rotation[1] + door.yaw, -650, 700, 150, -50, 2350});
    // SetDoor AtariInit: centerX=-650, halfX=700, halfZ=150,
    // centerY=1150, halfY=1200. Four vertical cube faces.
    constexpr float x0 = -1350, x1 = 50, z0 = -150, z1 = 150, y0 = -50, y1 = 2350;
    quad(walls, m, {x0,y0,z0}, {x1,y0,z0}, {x1,y1,z0}, {x0,y1,z0});
    quad(walls, m, {x1,y0,z1}, {x0,y0,z1}, {x0,y1,z1}, {x1,y1,z1});
    quad(walls, m, {x0,y0,z1}, {x0,y0,z0}, {x0,y1,z0}, {x0,y1,z1});
    quad(walls, m, {x1,y0,z0}, {x1,y0,z1}, {x1,y1,z1}, {x1,y1,z0});
  }
  for (const auto &rack : racks_) {
    const auto &p = rack.placement;
    const auto m = matrix(p);
    bodies.push_back({{p.position[0], p.position[1], p.position[2]},
                      p.rotation[1], 0, 600, 300, 0, rack.height});
    // SetRack type0/type1 rectangle: Size_x-100=600, Size_z-100=300.
    quad(walls,m,{-600,0,-300},{600,0,-300},{600,rack.height,-300},{-600,rack.height,-300});
    quad(walls,m,{600,0,300},{-600,0,300},{-600,rack.height,300},{600,rack.height,300});
    quad(walls,m,{-600,0,300},{-600,0,-300},{-600,rack.height,-300},{-600,rack.height,300});
    quad(walls,m,{600,0,-300},{600,0,300},{600,rack.height,300},{600,rack.height,-300});
  }
  for (const auto &window : windows_) {
    if (!window.hp || !window.field) continue;
    const auto x = window.width / 2 + 200;
    const auto y0 = window.lowSat ? -window.height / 2 - 200 : -200;
    const auto y1 = y0 + window.height + 400;
    quad(walls, matrix(window.placement), {-x,y0,0}, {x,y0,0}, {x,y1,0}, {-x,y1,0});
  }
  auto mesh = std::make_unique<Sat>(Sat::DynamicWalls(walls));
  auto collision = std::make_unique<SatCollision>(*mesh);
  // Rebind the borrower before the previous owner is destroyed.
  if (probe) {
    probe->setObjectCollision(collision.get());
    probe->setObjectBodies(std::move(bodies));
  }
  collision_.swap(collision);
  collisionMesh_.swap(mesh);
}
bool RoomInteractions::visible(int slot, int variant, int lockParent) const {
  if (lockParent >= 0) {
    for (const auto &door : doors_)
      if (int(door.placement.slot) == lockParent) return door.hp && door.lockHp;
    return false;
  }
  for (const auto &window : windows_)
    if (int(window.placement.slot) == slot)
      return variant < 0 || variant == (window.hp ? 0 : 1);
  for (const auto &door : doors_)
    if (int(door.placement.slot) == slot) return door.hp != 0;
  return variant != 1;
}
std::optional<Matrix34> RoomInteractions::delta(int slot, int lockParent) const {
  for (const auto &rack : racks_) if (int(rack.placement.slot) == slot) {
    Matrix34 delta{1,0,0,0,0,1,0,0,0,0,1,0};
    for (unsigned k=0;k<3;++k) delta[k*4+3]=rack.placement.position[k]-rack.base.position[k];
    return delta;
  }
  for (const auto &door : doors_)
    if (int(door.placement.slot) == (lockParent >= 0 ? lockParent : slot)) {
      // Original bind geometry is already in world space. Rotate about the
      // authored hinge rather than about the room origin or leaf centre.
      RoomObjectPlacement rotation;
      rotation.rotation[1] = door.yaw;
      auto delta = matrix(rotation);
      for (unsigned r = 0; r < 3; ++r) {
        delta[r * 4 + 3] = door.placement.position[r];
        for (unsigned c = 0; c < 3; ++c)
          delta[r * 4 + 3] -= delta[r * 4 + c] * door.placement.position[c];
      }
      return delta;
    }
  return {};
}
std::optional<InteriorHit> RoomInteractions::target(
    const CollisionVec &from, const CollisionVec &to, unsigned weaponNo) const {
  valid(from); valid(to);
  if (!OriginalWeaponAction(weaponNo) && weaponNo != 0x10)
    throw std::runtime_error("Room interaction: unverified weapon");
  float limit = 1e16f;
  if (effect_)
    if (const auto wall = effect_->line(from, to, SatSubset::All, 0, 0x00400000))
      limit = float(lengthSquared(from, wall->point));
  std::optional<InteriorHit> result;
  const auto test = [&](InteriorHit::Kind kind, std::size_t index, unsigned slot,
                        const AuthoredHitShape &shape, const Matrix34 &m) {
    if (const auto hit = HitAuthoredShapes({shape}, {}, m, from, to, limit, weaponNo))
      if (!result || hit->lenSq < result->hit.lenSq)
        result = InteriorHit{kind, index, slot, *hit};
  };
  for (std::size_t i = 0; i < doors_.size(); ++i) {
    const auto &door = doors_[i];
    if (!door.hp) continue;
    const auto m = matrix(door.placement, door.yaw);
    test(InteriorHit::DoorLeaf, i, door.placement.slot,
         {{-650,0,0},650,2300,55,HitOn | HitCube,0}, m);
    if (door.lockHp)
      test(InteriorHit::DoorLock, i, door.placement.slot,
           {{-1150,800,-60},150,350,100,HitOn | HitCube,0}, m);
  }
  for (std::size_t i = 0; i < windows_.size(); ++i) {
    const auto &window = windows_[i];
    if (!window.hp) continue;
    const float y = window.lowSat ? -window.height / 2 - 200 : -200;
    test(InteriorHit::Window, i, window.placement.slot,
         {{0,y,0},window.width / 2 + 200,window.height + 400,25.5f,
          HitOn | HitCube | HitThrough,0}, matrix(window.placement));
  }
  return result;
}
bool RoomInteractions::damageable(const InteriorHit &hit, unsigned weaponNo) const {
  if (!OriginalWeaponAction(weaponNo) && weaponNo != 0x10) return false;
  if (hit.kind == InteriorHit::DoorLeaf) return false;
  if (hit.kind == InteriorHit::DoorLock)
    return hit.index < doors_.size() && doors_[hit.index].placement.slot == hit.slot &&
           !doors_[hit.index].scriptLocked && doors_[hit.index].lockHp != 0;
  return hit.kind == InteriorHit::Window && hit.index < windows_.size() &&
         windows_[hit.index].placement.slot == hit.slot && windows_[hit.index].hp &&
         !windows_[hit.index].protectedByEvent &&
         (OriginalWeaponSize(weaponNo)==1 || weaponNo == 0x10 || windows_[hit.index].hpType <= 1);
}
bool RoomInteractions::damage(const InteriorHit &hit, unsigned weaponNo, unsigned serial) {
  if (!OriginalWeaponAction(weaponNo) && weaponNo != 0x10)
    throw std::runtime_error("Room interaction: unverified damage weapon");
  if (hit.slot >= 64 || (hit.kind == InteriorHit::Window ? hit.index >= windows_.size()
                                                        : hit.index >= doors_.size()))
    throw std::runtime_error("Room interaction: wrong hit object");
  const auto slot = hit.kind == InteriorHit::Window ? windows_[hit.index].placement.slot
                                                   : doors_[hit.index].placement.slot;
  if (slot != hit.slot) throw std::runtime_error("Room interaction: Hit slot mismatch");
  if (weaponNo != 0x10) {
    // Multiple shotgun rays share one attack serial but can strike distinct
    // windows/locks. Suppress repeats per source object, never globally.
    if (hasShotSerialBySlot_[slot] && shotSerialBySlot_[slot] == serial) return false;
    shotSerialBySlot_[slot] = serial; hasShotSerialBySlot_[slot] = true;
  } else {
    if (hasKnifeSerial_[slot] && knifeSerial_[slot] == serial) return false;
    knifeSerial_[slot] = serial; hasKnifeSerial_[slot] = true;
  }
  if (hit.kind == InteriorHit::DoorLeaf) return false; // Pane destruction has its own parts runtime.
  if (hit.kind == InteriorHit::DoorLock) {
    auto &door = doors_.at(hit.index);
    if (!door.lockHp || door.scriptLocked) return false;
    breakLock(door, true); // Normal-strength lock: group0 sets hp=0, not hp-1.
    return true;
  }
  auto &window = windows_.at(hit.index);
  if (!window.hp) return false;
  if (window.protectedByEvent) {
    event(InteriorEvent::ProtectedWindow, slot, -1, 0, window.placement.position);
    return false;
  }
  // GetWepSizeGroup(0x10) is 2. cEmWindow::DmCk case2 breaks the pane
  // outright with small shard size; the earlier exclusion was incorrect.
  window.hp = WindowWeaponHitPoints(window.hp,window.hpType,weaponNo);
  if (!window.hp) {
    state_.setObject(slot, 1);
    event(InteriorEvent::WindowBroken, slot, 6,
          window.placement.id == 7 ? 0x39 : 0x3b, window.placement.position);
    auto &glass = events_.back();
    glass.effectObject = window.placement.id;
    glass.effectRotation = window.placement.rotation;
    // The hit point's entry face is on the same side as the shot origin.
    // A handgun chooses tblA[direction][size0], namely EST6/EST7.
    const auto entry = local(window.placement,
        {hit.hit.cross[0], hit.hit.cross[1], hit.hit.cross[2]});
    glass.effectSequence = OriginalWeaponSize(weaponNo)==1 ? (entry[2] > 0 ? 1 : 0) : (entry[2] > 0 ? 7 : 6);
    // The next before-world tick rebuilds/rebinds the borrowed body walls
    // before any movement. Do not invalidate WalkProbe during an after-tick
    // hit callback; target/render already use the updated hp/flag state.
  } else if (window.placement.id != 7)
    event(InteriorEvent::WindowCrack, slot, 6, 0x2e, window.placement.position);
  return true;
}
void RoomInteractions::breakWindow(InteriorWindow &window, const CollisionVec &from,
                                   unsigned size, unsigned breakType) {
  if (!window.hp || window.protectedByEvent) return;
  window.hp = 0;
  state_.setObject(window.placement.slot, 1);
  event(InteriorEvent::WindowBroken, window.placement.slot, 6,
        window.placement.id == 7 ? 0x39 : 0x3b, window.placement.position);
  auto &glass = events_.back();
  glass.effectObject = window.placement.id;
  glass.effectRotation = window.placement.rotation;
  const unsigned direction = local(window.placement, from)[2] > 0 ? 1 : 0;
  // SetBreakEsp tblA (weapon/ladder) and tblB (body-vault), respectively.
  glass.effectSequence = breakType ? int(direction) :
                        size ? int(direction) : int(direction + 6);
}
bool RoomInteractions::breakWindowsAtLadderTop(const CollisionVec &top) {
  valid(top);
  bool changed = false;
  for (auto &window : windows_) {
    const CollisionVec p{window.placement.position[0], window.placement.position[1],
                         window.placement.position[2]};
    if (!window.hp || window.protectedByEvent || lengthSquared(p, top) >= 4000000) continue;
    breakWindow(window, top, 0, 0);
    changed = true;
  }
  return changed;
}
bool RoomInteractions::breakWindowsCrossed(const CollisionVec &previous,
                                           const CollisionVec &current) {
  valid(previous); valid(current);
  bool changed = false;
  for (auto &window : windows_) {
    if (!window.hp || !window.field || window.protectedByEvent) continue;
    const auto a = local(window.placement, previous), b = local(window.placement, current);
    if (a[2] * b[2] > 0 || std::abs(a[2] - b[2]) < .001) continue;
    const auto t = a[2] / (a[2] - b[2]);
    const auto x = a[0] + (b[0] - a[0]) * t;
    const auto y = a[1] + (b[1] - a[1]) * t;
    if (std::abs(x) > window.width / 2 || y < 0 || y > window.height) continue;
    breakWindow(window, previous, 1, 1);
    changed = true;
  }
  return changed;
}
std::vector<InteriorEvent> RoomInteractions::drainEvents() {
  std::vector<InteriorEvent> out;
  out.swap(events_);
  return out;
}
std::string RoomInteractions::describe() const {
  std::ostringstream out;
  unsigned broken = 0, locks = 0;
  for (const auto &w : windows_) broken += !w.hp;
  for (const auto &d : doors_) locks += d.lockHp != 0;
  out << 'r' << std::hex << room_ << std::dec << " Interior rooms: " << doors_.size() << " doors, " << windows_.size()
      << " windows (" << broken << " destroyed), " << locks << " locks, " << racks_.size()
      << " movable furniture · RSF 6/7/8=";
  const auto profile = VillageProfile(state_);
  out << profile.fight << '/' << profile.bell << '/' << profile.house;
  return out.str();
}
} // namespace re4
