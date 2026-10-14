// Supplied original disc files only; this tool contains no game assets.
#include "room_ladders.h"
#include "leon_preview.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks;
void require(bool yes,const char *message) {
  if(!yes) throw std::runtime_error(message);
  ++checks;
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &a,const char *tag) {
  const auto *e=a.find(tag); require(e,"Required original resource exists");
  return a.copy(e-a.entries().data());
}
re4::CollisionVec point(const re4::RoomObjectPlacement &p,double x,double y,double z) {
  const double c=std::cos(p.rotation[1]),s=std::sin(p.rotation[1]);
  return {p.position[0]+c*x+s*z,p.position[1]+y,p.position[2]-s*x+c*z};
}
// Independent state-number transcription of original obj13.cpp lines550..670.
// Keep the original case2/4 fall-throughs: this catches setting the next FCV
// too early, using the fixed tower clips, wrong loop counts and wrong Hokan.
struct SourceClimb {
  unsigned rno{}, left{}, type{}, exitCounter{};
  bool complete{};
  re4::RootMotionStep tick(re4::AnimationRig &rig) {
    re4::RootMotionStep root;
    switch(rno) {
    case 0:
      rig.startActionFlags(0,5,0,1); rno=1;
      [[fallthrough]];
    case 1:
      root=rig.advanceRootAction();
      if(root.ended) { left-=4; rno=left?2:4; }
      break;
    case 2:
      rig.startActionFlags(1,5,0,5); rno=3;
      [[fallthrough]];
    case 3:
      root=rig.advanceRootAction();
      if(rig.status().state&3) { left-=2; if(!left)rno=4; }
      break;
    case 4:
      rig.startActionFlags(type?3:2,5,0,1); exitCounter=0; rno=5;
      [[fallthrough]];
    case 5:
      ++exitCounter; root=rig.advanceRootAction();
      if(root.ended)complete=true;
      break;
    default: throw std::runtime_error("Source reference rno invalid");
    }
    root.ended=complete; return root;
  }
};
void sourceMotion(const re4::SkinnedModel &body,
                  const std::vector<re4::AnimationBytes> &motions) {
  for(unsigned type=0;type<2;++type) {
    re4::AnimationRig production(body,motions),reference(body,motions);
    SourceClimb source;source.left=type?8:6;source.type=type;
    constexpr std::array<unsigned,4> clips{0,1,2,3};
    auto state=re4::StartObjectLadderMotion(production,clips,source.left,type);
    std::vector<re4::LadderSound> sounds;
    unsigned ticks=0,contacts=0,feet=0;
    while(!source.complete&&ticks++<400) {
      const auto expected=source.tick(reference);
      const auto actual=re4::AdvanceObjectLadderMotion(production,clips,state,sounds);
      require(expected.translation==actual.translation&&expected.yaw==actual.yaw&&
              expected.ended==actual.ended,"Root step matches original state-number reference");
      require(production.bones()==reference.bones()&&
              production.sequenceFrame()==reference.sequenceFrame(),
              "FCV pose/Hokan follows original switch fall-through timing");
      for(const auto &s:sounds) {
        if(s.block==6&&(s.number==0x42||s.number==0x43))++contacts;
        else if(s.block==5&&(s.number==0x0d||s.number==0x0e))++feet;
        else require(false,"Object ladder emitted a non-original sound");
      }
      sounds.clear();
    }
    require(state.complete&&ticks<400,"Both original ET06/ET08 climb routines terminate");
    require(contacts==(type?7u:5u)&&feet==2,
            "Source rung contacts and dismount footsteps emit once at authored frames");
  }
}
void room(const char *path,unsigned id,const char *pl,const char *wep) {
  const auto archive=re4::RoomArchive::Load(path);
  const auto placements=re4::ReadRoomObjectPlacements(entry(archive,"ETS"));
  const re4::RoomFlow flow(id,entry(archive,"AEV"));
  re4::RoomSaveState before;
  const re4::RoomLadders initial(flow,archive,placements,before,pl);
  require(initial.objectLadders().size()==(id==0x101?0u:4u),
          "Original R101Init hides house ladders until RSF8; r400 preserves four standing ladders");
  auto after=before;if(id==0x101)after.room.set(8);
  re4::RoomLadders ladders(flow,archive,placements,after,pl);
  require(ladders.objectLadders().size()==(id==0x101?3u:4u)&&ladders.collision(),
          "Explicit post-house state enables original ETS ladder model/collision/action owner");
  const auto playerArchive=re4::RoomArchive::LoadDrs(pl);
  sourceMotion(re4::SkinnedModel(playerArchive.copy(0)),ladders.objectMotions());
  const re4::Sat sat(entry(archive,"SAT"));const re4::SatCollision world(sat);
  const auto bodies=ladders.objectBodies();
  require(bodies.size()==ladders.objectLadders().size(),"Every visible checked ladder owns a body");
  unsigned climbed=0;
  for(std::size_t i=0;i<ladders.objectLadders().size();++i) {
    const auto &p=ladders.objectLadders()[i]; const auto &b=bodies[i];
    require(b.halfX==330&&b.halfZ==600&&b.lowY==0&&b.highY==2000&&
            std::hypot(b.origin[0]-p.position[0],b.origin[2]-p.position[2])>699.9,
            "Body dimensions and 700-unit local offset exactly match SetLadder AtariInit");
    const auto start=point(p,0,0,600),finish=point(p,0,0,-1600);
    re4::CollisionVec delta{};for(unsigned k=0;k<3;++k)delta[k]=finish[k]-start[k];
    require(bool(ladders.collision()->sweepCapsule(start,delta,300,1600)),
            "The visible ladder cannot be traversed as an intangible prop");
    re4::WalkConfig config;config.originalScenerySphere=true;config.stepUp=600;config.drop=999;
    re4::WalkProbe probe(world,config);probe.setObjectCollision(ladders.collision());
    probe.setObjectBodies(bodies);
    re4::LeonPreview leon(pl,wep);
    bool placed=false;
    for(double z:{600.,800.,1000.}) {
      if(!probe.place(point(p,0,0,z)))continue;
      leon.setPlacement(probe.position(),float(p.rotation[1]+3.141592653589793));
      const auto offer=ladders.offer(leon,probe);
      if(!offer||!offer->object||offer->no!=p.slot)continue;
      placed=true;break;
    }
    require(placed,"Each original village ladder has a reachable SAT-valid approach and real landing");
    const auto offer=ladders.offer(leon,probe);
    require(offer&&offer->rungs==(p.id==8?8u:6u)&&offer->type==(p.id==8?1u:0u),
            "ET06/ET08 selects its authored rung count and dismount variant");
    require(ladders.interact(leon,probe)&&leon.ladderActive(),"Context action starts the actual object climb");
    const auto ownedClips=leon.rig().clipCount();
    leon.installObjectLadderMotions(ladders.objectMotions());
    require(leon.rig().clipCount()==ownedClips,"Repeated room use does not append duplicate ladder FCVs");
    unsigned ticks=0;
    while(leon.ladderActive()&&ticks++<400) {
      re4::PlayerActionInput attack;attack.aimHeld=attack.fireHeld=attack.firePressed=attack.knifeHeld=true;
      leon.step(probe,1,1,true,true,1./30,attack);
      leon.drainLadderSounds();
      require((!leon.ladderActive()||!leon.weaponVisible())&&leon.shots()==0,
              "Object climb hides weapons and rejects weapon/locomotion inputs");
      if(ticks==20) {
        const auto position=leon.feet();const auto bones=leon.rig().bones();
        const auto before=leon.objectLadderState().ticks;
        leon.suspendClock();leon.step(probe,1,1,true,true,0,attack);
        require(leon.feet()==position&&leon.rig().bones()==bones&&
                leon.objectLadderState().ticks==before,
                "Pause advances neither the object ladder motion nor its authored pose");
      }
    }
    require(!leon.ladderActive()&&!leon.ladderLandingFailed()&&leon.feet()[1]>2000,
            "Original FCVs complete on the genuine roof with input isolated during climb");
    require(std::hypot(leon.feet()[0]-offer->landing[0],leon.feet()[2]-offer->landing[2])<1&&
            std::abs(leon.feet()[1]-offer->landing[1])<1,
            "Actual root playback agrees with independently prepared real-SAT landing");
    ++climbed;
  }
  require(climbed==ladders.objectLadders().size(),"Every checked displayed ladder was climbed");
  for(const auto &p:ladders.objectLadders())after.setObject(p.slot,1);
  const re4::RoomLadders gone(flow,archive,placements,after,pl);
  require(gone.objectLadders().empty()&&!gone.collision(),
          "Original removed ETC flag suppresses geometry body and action together");
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=5)throw std::runtime_error("Usage: m32_ladders_check R101.dat R400.dat PL00.drs WEP01.drs");
    room(argv[1],0x101,argv[3],argv[4]);room(argv[2],0x400,argv[3],argv[4]);
    std::cout<<"M32 original ladder assets PASS: "<<checks<<" checks\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
