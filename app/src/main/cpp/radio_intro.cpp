#include "radio_intro.h"
#include "ports/motion64/runtime.h"
#include <cstring>
namespace re4 {
RadioIntro::RadioIntro(const std::string &path) {
  const auto arc=RoomArchive::LoadDrs(path);motion_.motion=arc.copy(0x79-4);
  actor_=std::make_unique<GenericEventActor>("scr_radio","PL00_ARC77",arc.copy(0x77-4),arc.copy(0x78-4));
  actor_->prepareStaticPose();actor_->setDisplayOverride(true);actor_->setLightMaskOverride(1);
}
void RadioIntro::install(LeonPreview &player) {
  if(owner_==&player)return;
  action_=player.installRoomScriptMotion(motion_);owner_=&player;
}
void RadioIntro::begin(LeonPreview &player,WalkProbe &probe) {
  install(player);ticks_=0;player.setRoomScriptEyeMode(1);
  player.beginRoomScriptMotion(probe,player.feet(),player.heading(),action_,0,0x201);
  tick30Hz(player,probe);
}
void RadioIntro::tick30Hz(LeonPreview &player,WalkProbe &probe) {
  player.step(probe,0,0,false,false,1./30);++ticks_;
  if(ticks_==22)player.setRoomScriptLeftHand(2);
}
bool RadioIntro::finished(const LeonPreview &player) const {return ticks_>=22 && player.roomScriptMotionEnded();}
void RadioIntro::end(LeonPreview &player) {
  if(visible())player.setRoomScriptLeftHand(0x63);
  player.setRoomScriptEyeMode(0);player.endRoomScriptMotion();ticks_=0;
}
Matrix34 RadioIntro::matrix(const LeonPreview &player) const {
  SmdPlacement p;p.position={111,-22,66};p.rotation={-.48869219f,.31415927f,-.73303829f};p.scale={1,1,1};
  const auto concat=[](const Matrix34 &a,const Matrix34 &b) {
    motion64::Mtx first{},second{},result{};
    std::memcpy(first,a.data(),sizeof first);std::memcpy(second,b.data(),sizeof second);
    motion64::PSMTXConcat(first,second,result);Matrix34 out{};std::memcpy(out.data(),result,sizeof result);return out;
  };
  return concat(concat(player.placement(),player.rig().jointMatrices().at(16)),PlacementMatrix(p));
}
}
