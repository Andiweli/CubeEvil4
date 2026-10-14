#include "leon_preview.h"
#include "archive.h"
#include "walk_probe.h"
#include "grab_rules.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks{};
void require(bool ok,const char *why) { if(!ok) throw std::runtime_error(why); ++checks; }
std::vector<unsigned char> entry(const re4::RoomArchive &arc,unsigned n,const char *tag) {
  require(n<arc.entries().size() && std::equal(tag,tag+3,arc.entries()[n].tag.begin()),"Original archive entry/type");
  return arc.copy(n);
}
std::vector<unsigned char> entry(const re4::RoomArchive &arc,const char *tag) {
  const auto *e=arc.find(tag); require(e,"Original room resource"); return arc.copy(e-arc.entries().data());
}
bool samePose(const re4::AnimationRig &a,const re4::AnimationRig &b) {
  if(a.bones().size()!=b.bones().size()) return false;
  for(unsigned j=0;j<a.bones().size();++j) for(unsigned c=0;c<12;++c)
    if(std::abs(a.bones()[j][c]-b.bones()[j][c])>.004f) return false;
  return true;
}
void equal(const re4::CollisionVec &a,const re4::CollisionVec &b,const char *why) {
  for(unsigned k=0;k<3;++k) require(std::abs(a[k]-b[k])<.03,why);
}
void tick(re4::LeonPreview &p,re4::WalkProbe &w,const re4::PlayerActionInput &i={}) {
  p.step(w,0,0,false,false,1./30,i);
}
void sound(const re4::LeonPreview &p,const re4::PlayerSequenceSound &e) {
  require(e.sub<=7 && e.part==(e.number==0 || e.number==2 || e.number==13?21u:
      e.number==1 || e.number==3 || e.number==14 || e.number==20?25u:0u),"Exact seqSeCtrl part/kind selection");
  const auto &m=p.rig().jointMatrices().at(e.part);
  const auto v=re4::TransformPoint(p.placement(),{m[3],m[7],m[11]});
  equal(e.position,{v[0],v[1],v[2]},"SEQ position uses the solved source part");
  equal(e.rootPosition,p.parts0World(),"Companion SEQ clothing cue uses source root");
}
}
int main(int argc,char **argv) { try {
  if(argc!=8) throw std::runtime_error("Usage: m35_player_actions_check PL00 WEP01 WEP02 WEP07 EM12 R101 R10C");
  const auto pl=re4::RoomArchive::LoadDrs(argv[1]);
  const auto wep=re4::RoomArchive::LoadDrs(argv[2]);
  re4::SkinnedModel body(entry(pl,0,"BIN"));
  const auto village=re4::RoomArchive::Load(argv[6]);
  const re4::Sat sat(entry(village,"SAT")); const re4::SatCollision world(sat);
  // Both real firearm grips and knife start horizontal after any exploration
  // look pitch. Live aiming remains adjustable after the original draw.
  for(unsigned mode=0;mode<4;++mode) for(double oldPitch:{-1.,-.4,.4,1.}) {
    re4::LeonPreview player(argv[1],argv[2],argv[5],true);
    if(mode==1) { player.installHandgun(argv[3]); require(player.equipWeapon(0x23),"Handgun profile equips"); }
    if(mode==2) { player.installShotgun(argv[4]); require(player.equipWeapon(0x2c),"Shotgun profile equips"); }
    re4::WalkProbe probe(world); require(probe.reset(),"Real starting floor"); player.setPlacement(probe.position(),0);
    re4::PlayerActionInput input; input.hasCameraLook=true; input.cameraPitch=oldPitch;
    input.cameraYaw=.2; input.aimHeld=mode<3; input.knifeHeld=mode==3;
    for(unsigned frame=0;frame<35;++frame) { tick(player,probe,input); require(player.aimRate()==0,"New gun/knife uses original zero camera pitch"); }
    input.pitch=-1;
    for(unsigned frame=0;frame<4;++frame) tick(player,probe,input);
    require(player.aimRate()<0,"Held live aim still moves vertically");
  }
  // Compare native SEQ transitions with an independent original motion rig,
  // including two display frames per native tick and pause/zero-dt queries.
  std::vector<re4::AnimationBytes> movement;
  for(unsigned n:{7u,13u,17u,15u,11u,9u,19u}) movement.push_back({entry(wep,n,"FCV"),entry(wep,n+1,"SEQ")});
  re4::AnimationRig reference(body,movement);
  re4::LeonPreview walking(argv[1],argv[2]); re4::WalkProbe walk(world);
  require(walk.reset(),"Walk sound source floor"); walking.setPlacement(walk.position(),0);
  unsigned emitted=0;
  for(unsigned frame=0;frame<180;++frame) {
    walking.step(walk,0,1,false,false,1./60);
    require(walking.drainSequenceSounds().empty(),"Display half tick emits no motion sound");
    walking.step(walk,0,1,false,false,1./60); reference.tick(re4::Locomotion::Walk);
    const auto actual=walking.drainSequenceSounds(); const auto expected=reference.consumeSequenceSound();
    require(actual.size()==(expected?1u:0u),"Every walk SEQ event is neither omitted nor duplicated");
    if(expected) { require(actual[0].number+1==expected->se && actual[0].sub==(expected->free&7),"Exact original SEQ Se/Free"); sound(walking,actual[0]); ++emitted; }
    walking.step(walk,0,1,false,false,0); require(walking.drainSequenceSounds().empty(),"Zero-dt emits no duplicate cue");
  }
  require(emitted>8,"Original walk FCVs contain real footstep events");
  walking.suspendClock(); require(walking.drainSequenceSounds().empty(),"Suspension emits no fabricated step");
  // Every normal axe hit height/front/back follows the original damageNormal
  // clip, root extraction, protected timer and return condition.
  std::vector<re4::AnimationBytes> hurts;
  hurts.push_back(movement[0]);
  for(const auto &n:std::array<std::array<unsigned,2>,5>{{{68,0},{69,0},{70,96},{71,97},{72,73}}})
    hurts.push_back({entry(pl,n[0],"FCV"),n[1]?entry(pl,n[1],"SEQ"):std::vector<unsigned char>{}});
  for(unsigned type=0;type<6;++type) {
    re4::LeonPreview player(argv[1],argv[2]); re4::WalkProbe probe(world);
    require(probe.reset(),"Axe reaction real floor"); player.setPlacement(probe.position(),0);
    const auto random=RndState(); const auto expectedLife=re4::GrabPlayerLifeDown(player.life(),380,false);
    re4::GrabPlayerLifeDown(expectedLife,0,true); const auto expectedRandom=RndState(); RndInit(random);
    require(player.receiveGanadoAxeHit({0,0,100},380,type),"Original normal axe damage accepted");
    require(player.life()==expectedLife && RndState()==expectedRandom,"Source rank5 damage/RNG branch");
    require(!player.receiveGanadoAxeHit({0,0,100},380,type),"Protected hurt does not debit twice");
    re4::AnimationRig expected(body,hurts);
    expected.startActionFlags(type<2?type+1:3+(type&1),5,0,1);
    unsigned frames=0;
    do {
      tick(player,probe); expected.advanceRootAction();
      require(samePose(player.rig(),expected),"Every hurt FCV pose equals independent original replay");
      ++frames; require(frames<180,"Original hurt completes");
    } while(player.hurtActive());
    require(player.action()==re4::PlayerAction::Footwork && player.damageWait()==5,"Original hurt returns control with five protected ticks");
    for(unsigned frame=0;frame<4;++frame) { tick(player,probe); require(!player.receiveGanadoAxeHit({0,0,100},380,type),"Recovery protection lasts the original four remaining ticks"); }
    tick(player,probe);
    require(player.receiveGanadoAxeHit({0,0,100},380,type),"Source normal attack can hit after five recovery ticks");
  }
  for(unsigned type:{0u,1u,2u,5u}) {
    re4::LeonPreview player(argv[1],argv[2]); re4::WalkProbe probe(world);
    require(probe.reset(),"Fatal axe real floor"); player.setPlacement(probe.position(),0);
    player.takeDamage(1130,false); require(player.life()==70,"Mortal low life fixture");
    const auto random=RndState(); re4::GrabPlayerLifeDown(70,380,false);
    if(type<2) re4::GrabPlayerLifeDown(0,0,true);
    const auto expectedRandom=RndState(); RndInit(random);
    require(player.receiveGanadoAxeHit({0,0,100},380,type) && !player.alive(),"Original normal axe can kill mortal Leon");
    require(RndState()==expectedRandom,"Fatal PlSetDamage direct-Die versus setDamage RNG branch");
    re4::AnimationRig expected(body,hurts); expected.startActionFlags(5,5,0,1);
    unsigned frames=0;
    while(player.hurtActive()) {
      tick(player,probe); expected.advanceRootAction();
      require(samePose(player.rig(),expected),"Original PL4c/4d fatal pose on every native tick");
      require(++frames<180,"Original fatal motion completes");
    }
    require(player.action()==re4::PlayerAction::Dead && !player.alive(),"Mortal normal damage reaches source final dead pose");
    require(!player.receiveGanadoAxeHit({0,0,100},380,type),"Dead Leon receives no duplicate attack");
  }
  {
    re4::LeonPreview player(argv[1],argv[2]); re4::WalkProbe probe(world);
    require(probe.reset(),"Test override real floor"); player.setPlacement(probe.position(),0);
    player.setUnlimitedHealth(true);
    require(player.receiveGanadoAxeHit({0,0,100},380,2) && player.life()==re4::LeonPreview::MaxLife && player.hurtActive(),"Optional health override preserves original hurt reaction");
  }
  for(unsigned rank:{2u,5u,10u}) {
    re4::LeonPreview player(argv[1],argv[2]); re4::WalkProbe probe(world);
    require(probe.reset(),"Difficulty damage real floor"); player.setPlacement(probe.position(),0);
    player.setDamageDifficulty(rank);
    const auto random=RndState();
    const auto expectedLife=re4::GrabPlayerLifeDown(1200,380,false,rank);
    re4::GrabPlayerLifeDown(expectedLife,0,true,rank); const auto expectedRandom=RndState(); RndInit(random);
    require(player.receiveGanadoAxeHit({0,0,100},380,2) && player.life()==expectedLife && RndState()==expectedRandom,"Original selected rank HP multiplier and RNG");
    require(player.life()==1200-unsigned(int(380*(float(rank)*.1f+.5f))),"Original float rank multiplier/truncation");
    re4::LeonPreview rebuilt(argv[1],argv[2]); rebuilt.inheritInventory(player);
    require(rebuilt.damageDifficulty()==rank && rebuilt.life()==player.life(),"Room owner retains selected source rank and mortal health");
  }
  // Easy's automatic keep-one above300 precedes, but does not skip, the
  // original random survival roll. Choose a stream whose roll is nonzero.
  for(unsigned rank:{2u,5u,10u}) {
    RndInit(0x2468); const auto saved=RndState(); Rnd();Rnd(); const auto roll=Rnd(); RndInit(saved);
    require((roll&3)!=0,"Fixed source RNG fixture has no random survival");
    const auto remaining=re4::GrabPlayerLifeDown(400,2000,false,rank);
    require(remaining==(rank<=2?1u:0u),"Only original Easy rank<=2 automatically survives fatal hits above300");
  }
  // Real r10c authored bit19 walls offer both facing directions. Dynamic
  // crate subslots remain absent; only SAT0 geometry is used in this test.
  const auto gapRoom=re4::RoomArchive::Load(argv[7]);
  const re4::Sat gapSat(entry(gapRoom,"SAT")); const re4::SatCollision gapWorld(gapSat);
  re4::WalkConfig config; config.originalScenerySphere=true; config.stepUp=600; config.drop=999;
  re4::WalkProbe gapProbe(gapWorld,config); re4::LeonPreview jumping(argv[1],argv[2]);
  std::vector<re4::AnimationBytes> jumpClips{movement[0],{entry(pl,90,"FCV"),entry(pl,92,"SEQ")},{entry(pl,91,"FCV"),entry(pl,93,"SEQ")}};
  unsigned completed=0; bool positive=false,negative=false;
  for(unsigned polygon=0;polygon<gapSat.polygons.size();++polygon) {
    const auto &poly=gapSat.polygons[polygon]; if(!(poly.attributes&0x80000)) continue;
    const auto normal=gapSat.normals[poly.normal]; if(std::abs(normal[1])>.5) continue;
    const auto triangle=gapWorld.triangle(polygon);
    for(double offset:{350.,450.,550.}) {
      re4::CollisionVec approach{};
      for(const auto &p:triangle) for(unsigned k=0;k<3;++k) approach[k]+=p[k]/3;
      for(unsigned k=0;k<3;++k) approach[k]+=normal[k]*offset;
      const auto floor=gapWorld.ground(approach,2000,4000); if(!floor) continue;
      approach[1]=floor->point[1]; if(!gapProbe.place(approach)) continue;
      jumping.setPlacement(gapProbe.position(),float(std::atan2(-normal[0],-normal[2])));
      const auto offer=jumping.jumpFallOffer(gapProbe); if(!offer) continue;
      auto feet=gapProbe.position(); auto heading=offer->heading;
      re4::AnimationRig expected(body,jumpClips); expected.startActionFlags(1,3,0,0x201);
      jumping.beginJumpFall(gapProbe,*offer);
      double oldPelvisY=feet[1]+expected.jointMatrices().at(4)[7]; unsigned work=0,phase=0;
      while(jumping.jumpFallActive()) {
        ++work;
        if(!phase) {
          if(work>=5 && work<=14) feet[1]+=offer->heightAdjustment*.1f;
          if(expected.sequenceFrame()>=7) if(const auto hit=gapWorld.line({feet[0],oldPelvisY,feet[2]},feet,re4::SatSubset::All,0)) {
            feet[1]=hit->point[1]; expected.startActionFlags(2,3,0,0x201); phase=1;
          }
        }
        const auto root=expected.advanceRootAction();
        const auto c=std::cos(heading),s=std::sin(heading);
        feet[0]+=c*root.translation[0]+s*root.translation[2]; feet[1]+=root.translation[1]; feet[2]+=-s*root.translation[0]+c*root.translation[2];
        heading=float(std::remainder(double(heading)+root.yaw,6.283185307179586));
        oldPelvisY=feet[1]+expected.jointMatrices().at(4)[7];
        tick(jumping,gapProbe);
        require(samePose(jumping.rig(),expected),"Every gap jump/landing pose equals actual PL5e/60 and PL5f/61");
        if(jumping.jumpFallActive()) equal(jumping.feet(),feet,"Exact source gap root/height/contact order");
        const auto cue=expected.consumeSequenceSound(); const auto events=jumping.drainSequenceSounds();
        require(events.size()==(cue?1u:0u),"Exact authored gap jump/landing SEQ events");
        if(cue) { require(events[0].number+1==cue->se,"Gap landing original SEQ number"); sound(jumping,events[0]); }
        require(work<180,"Gap traversal terminates");
      }
      require(!jumping.jumpFallState().failed && phase==1,"Actual authored gap lands without rollback");
      equal(jumping.feet(),offer->landing,"Predicted and actual gap landing agree");
      positive|=offer->direction[2]>.5; negative|=offer->direction[2]<-.5; ++completed;
      std::cout<<"r10c gap "<<polygon<<" heading "<<offer->heading<<" start "<<approach[0]<<','<<approach[1]<<','<<approach[2]<<" end "<<offer->landing[0]<<','<<offer->landing[1]<<','<<offer->landing[2]<<'\n';
      break;
    }
  }
  require(completed>=4 && positive && negative,"Real r10c authored gap offers and completes both directions");
  std::cout<<"M35 original Leon actions PASS: "<<checks<<" checks, "<<completed<<" real r10c gaps\n";
  return 0;
} catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; } }
