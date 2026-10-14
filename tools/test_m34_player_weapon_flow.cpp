#include "leon_preview.h"
#include "archive.h"
#include "walk_probe.h"
#include "weapon_preview.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks{};
void require(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
  ++checks;
}
bool samePose(const re4::AnimationRig &a, re4::AnimationRig &b) {
  if (a.bones().size()!=b.bones().size()) return false;
  // Normal live-player eyes are independently checked against the literal
  // blink/wander source by m37_player_eyes_check. This fixture compares the
  // weapon FCV/SEQ body after applying those separately verified parts.
  for(unsigned part:{0x1cu,0x20u,0x21u})b.setPartRotation(part,a.partRotation(part));
  // The actor also rebuilds the source waist matrices every tick. Permit
  // the float rounding from that extra identical zero-yaw multiplication.
  for (unsigned joint=0;joint<a.bones().size();++joint)
    for (unsigned cell=0;cell<12;++cell)
      if (!std::isfinite(a.bones()[joint][cell]) ||
          std::abs(a.bones()[joint][cell]-b.bones()[joint][cell])>.002f)
        return false;
  return true;
}
std::vector<unsigned char> entry(const re4::RoomArchive &archive,
                               unsigned index, const char *tag) {
  require(index < archive.entries().size(), "Original reference entry exists");
  const auto &type = archive.entries()[index].tag;
  require(std::equal(tag, tag + 3, type.begin()), "Original reference type matches");
  return archive.copy(index);
}
void tick(re4::LeonPreview &player, re4::WalkProbe &probe,
          const re4::PlayerActionInput &input) {
  player.step(probe, 0, 0, false, false, 1./30, input);
}
void draw(re4::LeonPreview &player, re4::WalkProbe &probe) {
  re4::PlayerActionInput input; input.aimHeld = true;
  for (unsigned i=0; i<90 && player.action()!=re4::PlayerAction::Aim; ++i)
    tick(player, probe, input);
  require(player.action()==re4::PlayerAction::Aim, "Original draw reaches ready");
}
}
int main(int argc, char **argv) { try {
  if (argc != 7) throw std::runtime_error(
      "Usage: m34_player_weapon_flow_check PL00 WEP01 WEP02 WEP07 CORE R101");
  const auto room = re4::RoomArchive::Load(argv[6]);
  const auto *satEntry = room.find("SAT");
  require(satEntry, "Real village SAT exists");
  re4::Sat sat(room.copy(satEntry-room.entries().data()));
  re4::SatCollision world(sat);
  const auto playerArchive = re4::RoomArchive::LoadDrs(argv[1]);
  const auto shotgunArchive = re4::RoomArchive::LoadDrs(argv[4]);
  re4::SkinnedModel body(entry(playerArchive, 0, "BIN"));
  std::vector<re4::AnimationBytes> referenceClips;
  // Frozen pl_shotgun::fire00 ARC1e/21/23 and wepDown ARC1f.
  for (unsigned clip : {26u,29u,31u,27u})
    referenceClips.push_back({entry(shotgunArchive,clip,"FCV"),{}});
  referenceClips.push_back({entry(shotgunArchive,20,"FCV"),entry(shotgunArchive,21,"SEQ")});
  for (unsigned clip : {22u,28u,30u})
    referenceClips.push_back({entry(shotgunArchive,clip,"FCV"),{}});
  for (unsigned releaseAt : {0u,5u,11u,20u,38u}) {
    re4::LeonPreview player(argv[1],argv[2]);
    player.installShotgun(argv[4]);
    require(player.equipWeapon(0x2c), "Actual owned shotgun equips");
    re4::WalkProbe probe(world);
    require(probe.reset(), "Real floor starts player");
    player.setPlacement(probe.position(), 0);
    draw(player,probe);
    re4::PlayerActionInput input; input.aimHeld=true;
    input.firePressed=input.fireHeld=true; tick(player,probe,input);
    require(player.action()==re4::PlayerAction::Fire && player.shots()==1,
            "One physical press enters one shotgun attack");
    input.firePressed=input.fireHeld=false;
    re4::AnimationRig reference(body,referenceClips);
    reference.startActionFlags(4,3,0,5);
    for (unsigned i=0;i<10;++i) reference.advanceAction();
    reference.startBlend3({5,6,7},0,3); reference.advanceAction();
    reference.startBlend3({0,1,2},player.rig().posedBlend3Rate(),0);
    reference.setWaistYaw(player.waistYaw());reference.advanceAction();
    require(samePose(player.rig(),reference), "First shot uses actual original FCV");
    unsigned elapsed=0;
    while (reference.sequenceFrame()<40) {
      input.aimHeld=elapsed<releaseAt;
      tick(player,probe,input);
      reference.setBlend3Rate(player.rig().posedBlend3Rate());reference.setWaistYaw(player.waistYaw());
      reference.advanceAction(); ++elapsed;
      require(player.action()==re4::PlayerAction::Fire,
              "Released aim cannot interrupt the original shotgun pump");
      require(player.rig().sequenceFrame()==reference.sequenceFrame() &&
              samePose(player.rig(),reference),
              "Every pump pose and source sequence frame matches direct original replay");
      require(player.shots()==1 && player.magazine()==5,
              "Pump/release neither re-fires nor debits another shell");
      require(elapsed<90,"Shotgun source threshold is bounded");
    }
    input={}; tick(player,probe,input);
    // Original set40 advances the SAME recoil; set then calls wepDown and
    // advances the freshly installed down motion in this tick.
    reference.advanceAction(); reference.startActionFlags(3,3,0,5);
    reference.setWaistYaw(player.waistYaw());
    reference.advanceAction();
    require(player.action()==re4::PlayerAction::Holster &&
            samePose(player.rig(),reference),
            "Pump tail goes straight to original down, with no intermediate aim/draw");
    for (unsigned i=0;i<90 && player.action()!=re4::PlayerAction::Footwork;++i) {
      tick(player,probe,input);
      require(player.action()!=re4::PlayerAction::Draw &&
              player.action()!=re4::PlayerAction::Aim && player.shots()==1,
              "One released shotgun shot never re-aims during weapon down");
    }
    require(player.action()==re4::PlayerAction::Footwork,"Original down returns movement");
  }
  // Keeping aim held does not replace the remaining fire motion at frame40.
  // set40 completes that motion before set00 installs the idle next tick.
  {
    re4::LeonPreview player(argv[1],argv[2]); player.installShotgun(argv[4]);
    require(player.equipWeapon(0x2c), "Held-tail shotgun equips");
    re4::WalkProbe probe(world); require(probe.reset(), "Held-tail original floor");
    player.setPlacement(probe.position(),0); draw(player,probe);
    re4::PlayerActionInput held; held.aimHeld=held.firePressed=held.fireHeld=true;
    tick(player,probe,held); held.firePressed=held.fireHeld=false;
    re4::AnimationRig reference(body,referenceClips);
    reference.startActionFlags(4,3,0,5);
    for(unsigned i=0;i<10;++i) reference.advanceAction();
    reference.startBlend3({5,6,7},0,3); reference.advanceAction();
    reference.startBlend3({0,1,2},player.rig().posedBlend3Rate(),0);
    reference.setWaistYaw(player.waistYaw());reference.advanceAction();
    unsigned tailTicks=0;
    for(unsigned ticks=0;;++ticks) {
      require(ticks<90,"Held source recoil completes");
      const auto frame=reference.sequenceFrame();
      tick(player,probe,held);
      reference.setBlend3Rate(player.rig().posedBlend3Rate());reference.setWaistYaw(player.waistYaw());
      const auto status=reference.advanceAction();
      require(player.action()==re4::PlayerAction::Fire &&
              player.rig().sequenceFrame()==reference.sequenceFrame() &&
              samePose(player.rig(),reference) && player.shots()==1,
              "Held aim preserves every source recoil-tail pose without refiring");
      if(frame>=40) ++tailTicks;
      if(status.ended || (status.state&3)) break;
    }
    require(tailTicks>0,"Actual source has a recoil tail after frame40");
    tick(player,probe,held);
    reference.startBlend3({5,6,7},player.rig().posedBlend3Rate(),3);
    reference.setWaistYaw(player.waistYaw());reference.advanceAction();
    require(player.action()==re4::PlayerAction::Aim && samePose(player.rig(),reference),
            "Source set00 installs aim only the tick after completed recoil");
  }
  // PlWepLockCtrl weapon7 scales pitch/body turn by0.8 and clamps waist
  // at +6deg/-4.8deg. Validate with both real grip/weapon profile assets.
  re4::LeonPreview handgun(argv[1],argv[2]), shotgun(argv[1],argv[2]);
  handgun.installHandgun(argv[3]); shotgun.installShotgun(argv[4]);
  require(handgun.equipWeapon(0x23) && shotgun.equipWeapon(0x2c), "Real aim profiles equip");
  re4::WalkProbe pistolProbe(world), shotgunProbe(world);
  require(pistolProbe.reset() && shotgunProbe.reset(),"Aim profiles have real floors");
  handgun.setPlacement(pistolProbe.position(),0); shotgun.setPlacement(shotgunProbe.position(),0);
  draw(handgun,pistolProbe);draw(shotgun,shotgunProbe);
  // Enter the same negative pitch sector before comparing speeds. Idle
  // source sway gives these independently timed draws different centers;
  // positive m3r has an additional source0.8 factor that must not mix sectors.
  re4::PlayerActionInput input;input.aimHeld=true;input.pitch=-1;
  for(unsigned i=0;i<10;++i){tick(handgun,pistolProbe,input);tick(shotgun,shotgunProbe,input);}
  input.pitch=0;tick(handgun,pistolProbe,input);tick(shotgun,shotgunProbe,input);input.pitch=-1;
  const float pistolStart=handgun.aimRate(),shotgunStart=shotgun.aimRate();
  for(unsigned i=0;i<5;++i){tick(handgun,pistolProbe,input);tick(shotgun,shotgunProbe,input);}
  require(std::abs((shotgun.aimRate()-shotgunStart)-(handgun.aimRate()-pistolStart)*.8f)<1e-6f,
          "Shotgun source pitch speed is0.8x the handgun profile");
  input.pitch=0;input.yaw=1;
  for(unsigned i=0;i<40;++i){tick(handgun,pistolProbe,input);tick(shotgun,shotgunProbe,input);}
  require(shotgun.waistYaw()<=.10471976f+1e-6f &&
          handgun.waistYaw()>.10471976f+1e-3f,
          "Shotgun retains its original six-degree waist limit");
  std::cout << "M34 original player weapon flow PASS: " << checks << '\n';
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
