// Measures the real player's complete CPU resource preparation. Validation
// happens after the timed region and includes every original cycle and pose.
#include "leon_preview.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
std::uint64_t fingerprint=1469598103934665603ull;
unsigned cycleQueryChecks{}, cycleQueryClips{}, liveCameraFrames{};
void require(bool value,const char *why) {
  if(!value) throw std::runtime_error(why);
  ++cycleQueryChecks;
}
void bytes(const void *data,std::size_t count) {
  const auto *p=static_cast<const unsigned char *>(data);
  for(std::size_t i=0;i<count;++i){fingerprint^=p[i];fingerprint*=1099511628211ull;}
}
template<class T>void values(const std::vector<T> &v){bytes(v.data(),v.size()*sizeof(T));}
void model(const re4::LeonPreview &player) {
  values(player.mesh().vertices);values(player.mesh().uv);values(player.rig().bones());
  for(unsigned clip=0;clip<player.rig().clipCount();++clip) {
    const auto cycle=player.rig().cycle(clip);
    bytes(cycle.translation.data(),sizeof(double)*3);bytes(&cycle.yaw,sizeof(double));
    bytes(&cycle.sequenceFrames,sizeof(unsigned));
  }
  for(const auto &image:player.mesh().textures)values(image.rgba);
}
void sameCamera(const re4::AttachedMotionCamera &a,const re4::AttachedMotionCamera &b) {
  require(a.type==b.type&&a.hasEye==b.hasEye&&a.hasTarget==b.hasTarget&&
    a.hasRoll==b.hasRoll&&a.hasFov==b.hasFov&&a.eye==b.eye&&a.target==b.target&&
    a.roll==b.roll&&a.fovRadians==b.fovRadians,"Lazy cycle query changes attached camera/history");
}
void sameLive(const re4::AnimationRig &a,const re4::AnimationRig &b) {
  require(a.jointMatrices()==b.jointMatrices(),"Lazy cycle query changes original solved parts or Hokan history");
  const auto x=a.status(),y=b.status();
  require(x.curveFrame==y.curveFrame&&x.sequenceFrame==y.sequenceFrame&&
    x.state==y.state&&x.ended==y.ended,"Lazy cycle query changes original motion/SEQ state");
  sameCamera(a.attachedCamera(),b.attachedCamera());
}
void cycleHistory(char **argv) {
  // A control obtains every immutable cycle before motion, like the previous
  // eager preparation. The other owner first queries every cycle while its
  // real action is blending, then follows the original root/Hokan/SEQ path.
  // Comparing future poses catches hidden state damage, not just current
  // copied bone matrices. Every installed PL/WEP/EM action is queried.
  for(unsigned mode=0;mode<5;++mode) {
    re4::LeonPreview queried(argv[1],argv[2],argv[5],true),control(argv[1],argv[2],argv[5],true);
    queried.installHandgun(argv[3]);queried.installShotgun(argv[4]);
    control.installHandgun(argv[3]);control.installShotgun(argv[4]);
    auto &a=queried.rig(),&b=control.rig();
    require(a.clipCount()==b.clipCount()&&a.clipCount()>90,"All actual installed original clips in cycle-history fixture");
    for(unsigned clip=0;clip<b.clipCount();++clip) b.cycle(clip);
    for(unsigned frame=0;frame<9;++frame) {
      const auto x=a.tick(re4::Locomotion::Walk),y=b.tick(re4::Locomotion::Walk);
      require(x.translation==y.translation&&x.yaw==y.yaw&&x.ended==y.ended,"Original pre-action gait control");
    }
    a.enableAttachedCamera(true);b.enableAttachedCamera(true);
    if(mode==0) { a.startActionFlags(7,12,0,1);b.startActionFlags(7,12,0,1); }
    if(mode==1) { a.startBlend3({12,13,14},.4f,9,0,false);b.startBlend3({12,13,14},.4f,9,0,false); }
    if(mode==2) { a.startBlend3({24,25,26},-.3f,6,0,false,true);b.startBlend3({24,25,26},-.3f,6,0,false,true); }
    if(mode==3) { a.startActionFlags(29,6,0,1);b.startActionFlags(29,6,0,1); }
    if(mode==4) { a.startActionFlags(1,9,4,4);b.startActionFlags(1,9,4,4); }
    // Add an original clip during a running blend, exercising appendActions'
    // stable addresses and independent new cache entry as well.
    if(mode==2 && a.clipCount()<128) {
      const auto archive=re4::RoomArchive::LoadDrs(argv[2]);
      const std::vector<re4::AnimationBytes> clip{{archive.copy(13),archive.copy(14)}};
      require(a.appendActions(clip)==b.appendActions(clip),"Appended original FCV address/cache ownership");
    }
    for(unsigned frame=0;frame<64;++frame) {
      if(frame==2 || frame==17) {
        const auto saved=a.jointMatrices();const auto status=a.status();const auto camera=a.attachedCamera();
        for(unsigned clip=0;clip<a.clipCount();++clip) {
          const auto x=a.cycle(clip),y=b.cycle(clip);
          require(x.translation==y.translation&&x.yaw==y.yaw&&x.sequenceFrames==y.sequenceFrames,
            "Lazy cycle metadata differs from original eager metadata");
          ++cycleQueryClips;
        }
        require(a.jointMatrices()==saved&&a.status().curveFrame==status.curveFrame&&
          a.status().sequenceFrame==status.sequenceFrame&&a.status().state==status.state,
          "Every first/cache-hit query leaves live pose and sequence unchanged");
        sameCamera(a.attachedCamera(),camera);
      }
      if(mode==1 || mode==2) {
        const float rate=frame%3==0?.37f:frame%3==1?-.42f:.12f;
        a.setBlend3Rate(rate);b.setBlend3Rate(rate);
      }
      a.setWaistYaw(.02f*float(int(frame%7)-3));b.setWaistYaw(.02f*float(int(frame%7)-3));
      const auto x=a.advanceRootAction(),y=b.advanceRootAction();
      require(x.translation==y.translation&&x.yaw==y.yaw&&x.ended==y.ended,
        "Lazy query mutates future original root extraction/history");
      sameLive(a,b);
      const auto soundA=a.consumeSequenceSound(),soundB=b.consumeSequenceSound();
      require(bool(soundA)==bool(soundB)&&(!soundA ||
        (soundA->se==soundB->se&&soundA->free==soundB->free)),"Lazy cycle query changes original SEQ sound");
      if(mode==3 && a.attachedCamera().type) ++liveCameraFrames;
    }
  }
  require(liveCameraFrames>8,"Original EM12 kick attach-camera channels exercised after lazy queries");
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=6)throw std::runtime_error("Usage: m36_loading_check PL00 WEP01 WEP02 WEP07 EM12");
    std::cout<<"{\"milliseconds\":[";
    for(unsigned iteration=0;iteration<6;++iteration) {
      const auto start=std::chrono::steady_clock::now();
      re4::LeonPreview player(argv[1],argv[2],argv[5],true);
      player.installHandgun(argv[3]);player.installShotgun(argv[4]);
      if(!player.equipWeapon(0x23))throw std::runtime_error("Original new-game weapon absent");
      const auto stop=std::chrono::steady_clock::now();
      if(iteration)std::cout<<',';
      std::cout<<std::chrono::duration<double,std::milli>(stop-start).count();
      const auto old=fingerprint;fingerprint=1469598103934665603ull;model(player);
      if(iteration&&old!=fingerprint)throw std::runtime_error("Player preparation is not deterministic");
    }
    cycleHistory(argv);
    std::cout<<"],\"original_data_fingerprint\":"<<fingerprint
      <<",\"cycle_query_checks\":"<<cycleQueryChecks<<",\"live_cycle_queries\":"<<cycleQueryClips
      <<",\"live_camera_frames\":"<<liveCameraFrames<<"}\n";
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
