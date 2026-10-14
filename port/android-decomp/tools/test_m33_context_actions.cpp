// Reference checks on the user's supplied original disc archives.
#include "room_ladders.h"
#include "room_interactions.h"
#include "turn_rules.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks{};
void require(bool yes,const char *message) {
  if (!yes) throw std::runtime_error(message);
  ++checks;
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &a,const char *tag) {
  const auto *e=a.find(tag); require(e,"Original resource exists");
  return a.copy(e-a.entries().data());
}
re4::CollisionVec point(const re4::RoomObjectPlacement &p,double x,double y,double z) {
  const double c=std::cos(p.rotation[1]),s=std::sin(p.rotation[1]);
  return {p.position[0]+c*x+s*z,p.position[1]+y,p.position[2]-s*x+c*z};
}
struct PropReference {
  re4::SkinnedModel model;
  re4::AnimationRig rig;
  re4::CollisionVec offset{};
  double yaw{};
  bool reset{}, ended{}, standingPose{};
  PropReference(const re4::RoomObjectArchive &etm,bool hatch,bool reset)
    :model(etm.copy(hatch?"et0800.bin":"et0600.bin")),
     rig(model,std::vector<re4::AnimationBytes>{{
         etm.copy(reset?"et06000.fcv":hatch?"et06001.fcv":"et06003.fcv"),
         etm.copy(reset?"et060000.seq":hatch?"et060010.seq":"et060030.seq")}}) {
    this->reset=reset;
    rig.startActionFlags(0,0,0,1);
  }
  void tick(const re4::RoomObjectPlacement &p) {
    if(reset&&ended){standingPose=true;return;}
    const auto root=rig.advanceRootAction();
    const auto c=std::cos(p.rotation[1]+yaw),s=std::sin(p.rotation[1]+yaw);
    offset[0]+=c*root.translation[0]+s*root.translation[2];offset[1]+=root.translation[1];
    offset[2]+=-s*root.translation[0]+c*root.translation[2];yaw+=root.yaw;
    ended=root.ended;
  }
  void verify(const re4::RoomObjectPlacement &p,const std::optional<re4::Matrix34> &delta) const {
    re4::SmdPlacement authored;authored.position=p.position;authored.rotation=p.rotation;authored.scale={1,1,1};
    re4::SmdPlacement standing;standing.rotation[0]=-1.9198622f;standing.scale={1,1,1};
    const auto pivot=rig.bindCenters().at(0);auto vertex=model.mesh.positions.at(0);
    for(unsigned k=0;k<3;++k)vertex[k]-=pivot[k];
    auto baked=re4::TransformPoint(re4::PlacementMatrix(standing),vertex);
    for(unsigned k=0;k<3;++k)baked[k]+=pivot[k];
    baked=re4::TransformPoint(re4::PlacementMatrix(authored),baked);
    const auto actual=delta?re4::TransformPoint(*delta,baked):baked;
    for(unsigned k=0;k<3;++k)authored.position[k]+=float(offset[k]);
    authored.rotation[1]+=float(yaw);
    const auto expected=standingPose?baked:re4::TransformPoint(re4::PlacementMatrix(authored),
        re4::TransformPoint(rig.jointMatrices().at(0),vertex));
    for(unsigned k=0;k<3;++k) {
      if(std::abs(actual[k]-expected[k])>=.06)
        std::cerr<<"delta mismatch frame"<<rig.sequenceFrame()<<" axis"<<k<<" actual"<<actual[k]<<" expected"<<expected[k]<<"\n";
      require(std::abs(actual[k]-expected[k])<.06,
        "Ladder draw delta matches independent original FCV root-part pose");
    }
  }
};
void intactLadderPane(const char *path,const char *pl,const char *wep) {
  const auto arc=re4::RoomArchive::Load(path);
  const auto placements=re4::ReadRoomObjectPlacements(entry(arc,"ETS"));
  const re4::RoomFlow flow(0x101,entry(arc,"AEV"));
  const re4::Sat sat(entry(arc,"SAT")),eat(entry(arc,"EAT"));
  const re4::SatCollision world(sat),effect(eat,re4::SatQueryMode::Effect);
  re4::RoomSaveState state;state.room.set(8);
  re4::RoomLadders ladders(flow,arc,placements,state,pl);
  re4::RoomInteractions interior(0x101,arc,placements,pl,state,&effect);
  interior.setLadders(&ladders);
  interior.setSceneryBodies(ladders.objectBodies());
  const auto prop=std::find_if(ladders.objectLadders().begin(),ladders.objectLadders().end(),
      [](const auto &p){return p.slot==7&&p.id==8;});
  const auto pane=std::find_if(interior.windows().begin(),interior.windows().end(),
      [](const auto &w){return w.placement.slot==0;});
  require(prop!=ladders.objectLadders().end()&&pane!=interior.windows().end(),
          "Actual r101 hatch ladder and its upper pane exist");
  require(pane->hp==1000&&!pane->protectedByEvent,"Post-house r101 starts with genuinely intact upper glass");
  const auto top=point(*prop,0,8*533.3329-1000,8*-194.1173);
  const auto approach=point(*prop,0,3200-prop->position[1],8*-194.1173-700);
  re4::WalkConfig config;config.originalScenerySphere=true;config.stepUp=600;config.drop=999;
  re4::WalkProbe probe(world,config);probe.setObjectCollision(interior.collision());
  probe.setObjectBodies(ladders.objectBodies());
  require(probe.place(approach,500),"Reported hatch-tip approach is on original SAT without overlapping intact pane");
  re4::LeonPreview leon(pl,wep);leon.setPlacement(probe.position(),prop->rotation[1]);
  const auto sourceOffer=ladders.offer(leon,probe);
  require(sourceOffer&&sourceOffer->no==7&&sourceOffer->action==re4::LadderAction::Tip,
          "Fixture meets the original ladder local bounds and half-turn facing");
  auto reach=top;reach[1]=probe.position()[1];
  re4::CollisionVec delta{};for(unsigned k=0;k<3;++k)delta[k]=reach[k]-probe.position()[k];
  require(bool(interior.collision()->sweepCapsule(probe.position(),delta,config.radius,config.height)),
          "Actual intact upper pane separates Leon's body from the ladder top");
  ladders.setInteractions(&interior);
  require(!ladders.offer(leon,probe)&&!ladders.interact(leon,probe),
          "An intact pane cannot offer or execute the reach-through ladder tip");
  require(!leon.ladderPropActive()&&pane->hp==1000&&!interior.state().object(0,1),
          "Rejected tip leaves actor, ladder and glass unchanged");
  // Original source glass damage is still two distinct pistol hits; cracking
  // the pane does not turn off its physical reach obstruction.
  const auto hit=interior.target(point(pane->placement,0,700,-500),point(pane->placement,0,700,500));
  require(hit&&hit->kind==re4::InteriorHit::Window&&hit->slot==0,"Original hit-box selects the upper pane");
  require(interior.damage(*hit,1,1)&&pane->hp==500,"First source pistol shot cracks but does not shatter glass");
  require(!ladders.offer(leon,probe),"Half-damaged glass still prevents tipping through it");
  require(interior.damage(*hit,1,2)&&!pane->hp,"Second distinct source pistol shot shatters the pane");
  // Hits are after-world callbacks; the next original world tick safely
  // rebuilds/rebinds the borrowed collision before grounded motion.
  interior.tick(leon,probe);
  const auto brokenOffer=ladders.offer(leon,probe);
  require(brokenOffer&&brokenOffer->no==7&&brokenOffer->action==re4::LadderAction::Tip,
          "Same original approach immediately permits tipping after real glass destruction");
  require(brokenOffer->alignment==sourceOffer->alignment&&brokenOffer->heading==sourceOffer->heading,
          "Physical guard leaves the original tip alignment and heading unchanged");
  require(ladders.interact(leon,probe)&&leon.ladderPropActive(),"Broken-pane tip starts original kick FCV");
  require(ladders.permitsWindowCross({pane->placement.position[0],pane->placement.position[1],pane->placement.position[2]}),
          "Valid falling ladder releases original window action blocker");
  unsigned ticks=0;while(leon.ladderPropActive()&&ticks++<200)
    leon.step(probe,0,0,false,false,1./30,{}, {}, [&]{ladders.tick(probe,&interior);interior.tick(leon,probe);});
  require(!leon.ladderPropActive()&&ticks<200,"Broken-pane ladder tip completes its real original animation");
  unsigned breaks=0;for(const auto &event:interior.drainEvents())breaks+=event.kind==re4::InteriorEvent::WindowBroken;
  require(breaks==1,"The required glass destruction emits one break, without repeating it during tipping");
  // A normal roof ladder has no pane between its top and the actor. The
  // same live owner must preserve the original same-side tip there.
  const auto sameSide=std::find_if(ladders.objectLadders().begin(),ladders.objectLadders().end(),
      [](const auto &p){return p.slot==5;});
  require(sameSide!=ladders.objectLadders().end(),"Actual r101 same-side roof ladder exists");
  bool accepted=false;
  for(double z:{-700.,-900.,-500.,0.,300.})for(double x:{0.,-400.,400.}) {
    if(accepted)continue;
    const auto q=point(*sameSide,x,6*533.3329,6*-194.1173+z);
    if(!probe.place(q,500))continue;
    leon.setPlacement(probe.position(),sameSide->rotation[1]);
    const auto offer=ladders.offer(leon,probe);
    accepted=offer&&offer->no==5&&offer->action==re4::LadderAction::Tip;
  }
  require(accepted,"Live-wall tip guard preserves actual same-side roof-ladder action");
  std::cout<<"r101 intact/cracked/broken pane and same-side ladder tip reach PASS\n";
}
void room(const char *path,unsigned id,const char *pl,const char *wep) {
  const auto arc=re4::RoomArchive::Load(path);
  const auto placements=re4::ReadRoomObjectPlacements(entry(arc,"ETS"));
  const re4::RoomFlow flow(id,entry(arc,"AEV"));
  const re4::RoomObjectArchive etm(entry(arc,"ETM"));
  const re4::Sat sat(entry(arc,"SAT")),eat(entry(arc,"EAT"));
  const re4::SatCollision world(sat),effect(eat,re4::SatQueryMode::Effect);
  re4::RoomSaveState state; if(id==0x101) state.room.set(8);
  re4::RoomLadders ladders(flow,arc,placements,state,pl);
  re4::RoomInteractions interior(id,arc,placements,pl,state,&effect);
  ladders.setInteractions(&interior);
  require(ladders.objectMotions().size()==7,"All original climb/reset/tip FCVs are loaded");
  interior.setSceneryBodies(ladders.objectBodies());
  for(const auto &w:interior.windows()) {
    const auto previous=point(w.placement,0,700,-100),next=point(w.placement,0,700,100);
    if(!w.field)continue;
    require(interior.breakWindowsCrossed(previous,next),"Crossing an intact pane breaks it during entry");
    require(!w.hp && interior.state().object(w.placement.slot,1),"Entry break commits authored ETC state");
    require(!interior.breakWindowsCrossed(previous,next),"Crossing a broken pane has no duplicate shards");
  }
  // Regression from M33: ETS10's incoming FCV ends in slight scenery
  // contact. Rejecting that authored endpoint removed re-entry entirely.
  // Exercise both directions repeatedly after the first pane break, with
  // the same actor, object state, original FCVs and production SAT geometry.
  interior.drainEvents();
  for(unsigned slot:{8u,10u}) {
    const auto found=std::find_if(interior.windows().begin(),interior.windows().end(),
        [&](const auto &w){return w.placement.slot==slot;});
    require(found!=interior.windows().end()&&!found->hp,"Round-trip fixture has a genuinely broken pane");
    re4::WalkConfig config; config.originalScenerySphere=true; config.stepUp=600; config.drop=999;
    re4::WalkProbe probe(world,config);probe.setObjectCollision(interior.collision());
    re4::LeonPreview leon(pl,wep);leon.installWindowMotions(interior.windowMotions());
    const auto originalPlayer=re4::RoomArchive::LoadDrs(pl);
    const auto originalWeapon=re4::RoomArchive::LoadDrs(wep);
    const re4::SkinnedModel originalBody(originalPlayer.copy(0));
    re4::AnimationRig reference(originalBody,std::vector<re4::AnimationBytes>{
        {originalWeapon.copy(7),originalWeapon.copy(8)},
        {originalPlayer.copy(81),originalPlayer.copy(82)}});
    for(unsigned round=0;round<3;++round)for(bool front:{false,true}) {
      std::optional<re4::InteriorWindowOffer> offer;
      for(double z:{300.,500.,700.,900.})for(double x:{-450.,0.,450.}) {
        if(offer||!probe.place(point(found->placement,x,-1000,front?z:-z)))continue;
        leon.setPlacement(probe.position(),float(found->placement.rotation[1]+(front?3.141592653589793:0)));
        const auto candidate=interior.windowOffer(leon,probe);
        if(candidate&&candidate->slot==slot)offer=candidate;
      }
      require(offer.has_value(),"Broken original window accepts every repeated incoming/outgoing action");
      require(offer->direction==(front?1u:0u),"Two-sided source scenario fields select the matching original FCV");
      require(offer->brokenFence&&!offer->brokenFence->wide&&!offer->brokenFence->adjustSides,
              "Broken direction0/1 uses source short fence motion without side adjustment");
      reference.startActionFlags(1,3,0,5);
      auto expected=re4::StartFencePlacement(probe.position(),leon.heading(),*offer->brokenFence);
      unsigned tick=0;leon.beginBrokenWindow(probe,*offer->brokenFence);
      while(leon.fenceActive()&&tick++<180) {
        const auto prior=reference.sequenceFrame();re4::RootMotionStep root;
        if(expected.adjustment){reference.advanceAction();root=reference.rootMotionSpeed();}
        else root=reference.advanceRootAction();
        re4::AdvanceFencePlacement(world,expected,root,prior);
        leon.step(probe,0,0,false,false,1./30,{}, {}, [&]{interior.tick(leon,probe);});
        if(tick>3) {
          double error=0;for(std::size_t j=0;j<reference.bones().size();++j)for(unsigned k=0;k<12;++k)
            error=std::max(error,std::abs(double(reference.bones()[j][k])-leon.rig().bones()[j][k]));
          if(error>.1)std::cerr<<"window pose diagnostic slot"<<slot<<" round"<<round<<" side"<<front<<" tick"<<tick<<" frames "<<reference.sequenceFrame()<<","<<leon.rig().sequenceFrame()<<" error"<<error<<'\n';
          require(error<.1,"Repeated broken-window vault preserves the original PL_ARC55/56 pose");
        }
        if(leon.fenceActive())for(unsigned k=0;k<3;++k)
          require(std::abs(leon.feet()[k]-expected.feet[k])<.03,
                  "Re-entry correction does not alter any original airborne root step");
      }
      require(!leon.fenceActive()&&tick<180,"Repeated original vault terminates on genuine SAT");
      require(probe.position()==offer->landing&&leon.feet()==offer->landing,
              "Landing contact correction is shared by preview and actual action");
      require(std::hypot(expected.feet[0]-leon.feet()[0],expected.feet[2]-leon.feet()[2])<=config.radius,
              "Wall adjustment is bounded to one body radius, without modifying the motion");
      require(world.clearCapsule(leon.feet(),config.radius,2*config.radius,re4::SatSidedness::AuthoredFront),
              "Every final window landing is free of original one-sided scenery");
      require(!found->hp&&interior.state().object(slot,1),"Repeated vault preserves original broken ETC bit");
      unsigned bursts=0;for(const auto &event:interior.drainEvents())bursts+=event.kind==re4::InteriorEvent::WindowBroken;
      require(!bursts,"Re-entering broken panes never repeats glass sound or fragments");
    }
  }
  for(const auto &p:ladders.objectLadders()) {
    const unsigned sourceRungs=p.id==8?8:6;
    const auto sourceTop=point(p,0,sourceRungs*533.3329-(p.id==8?1000:0),sourceRungs*-194.1173);
    require(!ladders.permitsWindowCross(sourceTop),"Original standing ladder top reserves nearby window transition");
    re4::WalkConfig config; config.originalScenerySphere=true; config.stepUp=600; config.drop=999;
    re4::WalkProbe probe(world,config);
    probe.setObjectCollision(interior.collision()); probe.setObjectBodies(ladders.objectBodies());
    re4::LeonPreview leon(pl,wep);
    bool placed=false;
    for(double z:{600.,800.,1000.}) {
      if(!probe.place(point(p,0,0,z)))continue;
      leon.setPlacement(probe.position(),float(p.rotation[1]+3.141592653589793));
      const auto offer=ladders.offer(leon,probe);
      if(offer&&offer->object&&offer->no==p.slot&&offer->action==re4::LadderAction::Climb) {
        placed=true; break;
      }
    }
    require(placed,"Original ladder's climb approach is on real SAT");
    require(ladders.interact(leon,probe),"Original ladder climb starts");
    unsigned ticks=0;
    while(leon.ladderActive()&&ticks++<400)
      leon.step(probe,0,0,false,false,1./30,{}, {}, [&]{ladders.tick(probe,&interior); interior.tick(leon,probe);});
    require(!leon.ladderActive()&&!leon.ladderLandingFailed(),"Source climb returns to the real upper floor");
    leon.setPlacement(leon.feet(),p.rotation[1]);
    auto tip=ladders.offer(leon,probe);
    if(!tip||tip->no!=p.slot||tip->action!=re4::LadderAction::Tip) {
      const unsigned n=p.id==8?8:6;
      const auto top=point(p,0,n*533.3329-(p.id==8?1000:0),n*-194.1173);
      for(double z:{-700.,-900.,0.,300.}) {
        auto q=top;const auto offset=point(p,0,0,z);
        q[0]+=offset[0]-p.position[0];q[2]+=offset[2]-p.position[2];
        if(!probe.place(q,500))continue;
        leon.setPlacement(probe.position(),p.rotation[1]);tip=ladders.offer(leon,probe);
        if(tip&&tip->no==p.slot&&tip->action==re4::LadderAction::Tip)break;
      }
    }
    std::cout<<"r"<<std::hex<<id<<std::dec<<" ladder"<<p.slot<<" top "<<leon.feet()[0]<<","<<leon.feet()[1]<<","<<leon.feet()[2]<<'\n';
    require(tip&&tip->no==p.slot&&tip->action==re4::LadderAction::Tip,"Top action offers tipping instead of jumping down");
    require(ladders.interact(leon,probe)&&leon.ladderPropActive(),"Tipping plays source Leon kick FCV");
    require(ladders.permitsWindowCross(sourceTop),"Falling ladder releases source LadderNearCk window blocker immediately");
    PropReference fall(etm,p.id==8,false);
    ticks=0; bool transformed=false; unsigned crash=0;
    while(leon.ladderPropActive()&&ticks++<200) {
      leon.step(probe,0,0,false,false,1./30,{}, {}, [&]{fall.tick(p); ladders.tick(probe,&interior); interior.tick(leon,probe);});
      transformed|=ladders.delta(p.slot).has_value();
      fall.verify(p,ladders.delta(p.slot));
      for(const auto &sound:ladders.drainSounds())if(sound.block==6&&sound.number==0x3f)++crash;
      for(auto value:leon.feet())require(std::isfinite(value),"Prop action placement is finite");
    }
    for(unsigned i=0;i<80;++i)ladders.tick(probe,&interior);
    for(const auto &sound:ladders.drainSounds())if(sound.block==6&&sound.number==0x3f)++crash;
    require(!leon.ladderPropActive()&&transformed,"Source tipping terminates and moves real ladder model");
    require(crash==1,"Original ladder SE emits once at its authored SEQ marker");
    require(ladders.objectBodies().size()+1==ladders.objectLadders().size(),"Fallen ladder's upright body turns off");
    placed=false;
    for(double x:{-1000.,1000.,0.})for(double z:{600.,1000.,-400.}) {
      if(placed||!probe.place(point(p,x,0,z),500))continue;
      leon.setPlacement(probe.position(),p.rotation[1]);
      const auto reset=ladders.offer(leon,probe);
      if(reset&&reset->no==p.slot&&reset->action==re4::LadderAction::Reset)placed=true;
    }
    for(double radius:{300.,600.,900.,1200.,1450.})for(unsigned angle=0;angle<24;++angle) {
      if(placed)break;
      const auto a=angle*6.283185307179586/24;
      if(!probe.place(point(p,std::cos(a)*radius,0,std::sin(a)*radius),500))continue;
      for(double yaw:{-1.57079632679,1.57079632679,0.}) {
        leon.setPlacement(probe.position(),float(p.rotation[1]+yaw));
        const auto reset=ladders.offer(leon,probe);
        if(reset&&reset->no==p.slot&&reset->action==re4::LadderAction::Reset){placed=true;break;}
      }
    }
    require(placed,"Fallen ladder offers original stand-up action within source1500 radius");
    const auto resetOffer=ladders.offer(leon,probe);
    require(resetOffer&&resetOffer->flip==(re4::GameCubeLimitAngle(leon.heading()-p.rotation[1])>=0),
            "Reset side and mirror use original Muku2 destination-minus-source sign");
    require(ladders.interact(leon,probe)&&leon.ladderPropActive(),"Reset plays source mirrored Leon lifting FCV");
    PropReference lift(etm,p.id==8,true);
    ticks=0;unsigned liftSound=0;
    while(leon.ladderPropActive()&&ticks++<200) {
      leon.step(probe,0,0,false,false,1./30,{}, {}, [&]{lift.tick(p); ladders.tick(probe,&interior); interior.tick(leon,probe);});
      lift.verify(p,ladders.delta(p.slot));
      for(const auto &sound:ladders.drainSounds())if(sound.block==6&&sound.number==0x41)++liftSound;
    }
    for(unsigned i=0;i<80;++i)ladders.tick(probe,&interior);
    for(const auto &sound:ladders.drainSounds())if(sound.block==6&&sound.number==0x41)++liftSound;
    require(!leon.ladderPropActive()&&liftSound==1,"Original lift player/prop motions end with one SEQ sound");
    require(!ladders.delta(p.slot)&&ladders.objectBodies().size()==ladders.objectLadders().size(),"Standing pose and collision restore together");
  }
  // Genuine SAT permissions, not geometry heuristics. Each offered source
  // fence can replay the actual FCV and return to its predicted real floor.
  re4::WalkProbe search(world); re4::LeonPreview walker(pl,wep);
  unsigned fences=0;
  for(std::size_t i=0;i<sat.polygons.size()&&fences<3;++i) {
    if(!(sat.polygons[i].attributes&0x20))continue;
    const auto tri=world.triangle(i);
    const auto &n=sat.normals.at(sat.polygons[i].normal);
    re4::CollisionVec q{};
    for(unsigned k=0;k<3;++k)q[k]=(tri[0][k]+tri[1][k]+tri[2][k])/3+n[k]*600;
    const auto floor=world.ground(q,2000,5000);if(!floor)continue;
    q[1]=floor->point[1];if(!search.place(q,5))continue;
    walker.setPlacement(q,float(std::atan2(-n[0],-n[2])));
    const auto offer=walker.fenceOffer(search);if(!offer)continue;
    walker.beginFence(search,*offer);unsigned t=0;
    while(walker.fenceActive()&&t++<200)walker.step(search,0,0,false,false,1./30);
    require(!walker.fenceActive()&&std::hypot(walker.feet()[0]-offer->landing[0],walker.feet()[2]-offer->landing[2])<1,
            "Original fence FCV lands exactly at source-derived SAT preview");
    ++fences;
  }
  std::cout<<"r"<<std::hex<<id<<std::dec<<" source fence offers exercised: "<<fences<<'\n';
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=5)throw std::runtime_error("Usage: m33_context_actions_check R101 R400 PL00 WEP01");
    intactLadderPane(argv[1],argv[3],argv[4]);
    room(argv[1],0x101,argv[3],argv[4]);room(argv[2],0x400,argv[3],argv[4]);
    std::cout<<"M33 source context actions PASS: "<<checks<<" checks\n";
  }catch(const std::exception &e){std::cerr<<"M33 context actions FAIL: "<<e.what()<<'\n';return 1;}
}
