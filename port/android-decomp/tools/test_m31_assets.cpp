// Real, separately supplied disc data. No proprietary assets in this tool.
#include "room_interactions.h"
#include "room_lighting.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "room_blocks.h"
#include "environment.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
unsigned checks;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;}
std::vector<unsigned char> data(const re4::RoomArchive &a,const char *tag) {
  auto *e=a.find(tag); check(e,"Required original subfile exists");
  return a.copy(e-a.entries().data());
}
re4::WalkConfig config(){re4::WalkConfig c;c.originalScenerySphere=true;c.stepUp=600;c.drop=999;return c;}
re4::CollisionVec point(const re4::RoomObjectPlacement &p,float x,float y,float z) {
  re4::SmdPlacement pose;pose.position=p.position;pose.rotation=p.rotation;pose.scale={1,1,1};
  auto v=re4::TransformPoint(re4::PlacementMatrix(pose),{x,y,z});return {v[0],v[1],v[2]};
}
void blendInsertion(const re4::RoomArchive &room,const char *pl,const char *wep) {
  re4::Sat sat(data(room,"SAT"));re4::SatCollision world(sat);
  re4::WalkProbe a(world,config()),b(world,config());check(a.reset()&&b.reset(),"Rig fixture has genuine floor");
  re4::LeonPreview edited(pl,wep),control(pl,wep);
  edited.setPlacement(a.position(),1);control.setPlacement(b.position(),1);
  re4::RoomInteractions interior(0x100,room,re4::ReadRoomObjectPlacements(data(room,"ETS")),pl,{});
  re4::PlayerActionInput aim;aim.aimHeld=true;aim.pitch=.3;aim.yaw=.2;
  edited.step(a,0,0,false,false,1./30,aim);control.step(b,0,0,false,false,1./30,aim);
  const auto count=edited.rig().clipCount();
  edited.installWindowMotions(interior.windowMotions());
  check(edited.rig().clipCount()==count+3,"Exactly three owned event clips appended");
  edited.installWindowMotions(interior.windowMotions());
  check(edited.rig().clipCount()==count+3,"Repeated room/profile use does not append duplicate clips");
  for(unsigned tick=0;tick<60;++tick) {
    check(edited.rig().bones()==control.rig().bones()&&edited.rig().sequenceFrame()==control.rig().sequenceFrame()&&
          edited.action()==control.action()&&edited.feet()==control.feet(),
          "Appending during a live blend preserves future Hermite history, pose and active motion");
    check(edited.laserReady()==(edited.action()==re4::PlayerAction::Aim||
          (edited.action()==re4::PlayerAction::Draw&&edited.rig().sequenceFrame()>=4)),
          "Laser switches at original frame four while draw is still active");
    edited.step(a,0,0,false,false,1./30,aim);control.step(b,0,0,false,false,1./30,aim);
  }
  check(edited.laserReady()&&edited.action()==re4::PlayerAction::Aim,"Sustained aim retains source laser");
}
void vaults(const char *path,unsigned id,const char *pl,const char *wep,std::set<unsigned> &directions) {
  const auto room=re4::RoomArchive::Load(path);const auto placements=re4::ReadRoomObjectPlacements(data(room,"ETS"));
  re4::Sat sat(data(room,"SAT")),eat(data(room,"EAT"));re4::SatCollision world(sat),effect(eat,re4::SatQueryMode::Effect);
  const re4::RoomInteractions reference(id,room,placements,pl,{},&effect);
  unsigned complete=0;
  for(const auto &source:reference.windows())for(bool front:{false,true}) {
    re4::RoomInteractions interior(id,room,placements,pl,{},&effect);
    re4::WalkProbe probe(world,config());probe.setObjectCollision(interior.collision());
    re4::LeonPreview player(pl,wep);std::optional<re4::InteriorWindowOffer> offer;
    for(float x:{-350.f,0.f,350.f})for(float z:{450.f,650.f,850.f}) {
      if(offer)continue;
      const auto feet=point(source.placement,x,-1000,front?z:-z);if(!probe.place(feet))continue;
      player.setPlacement(probe.position(),source.placement.rotation[1]+(front?3.1415927f:0));
      const auto found=interior.windowOffer(player,probe);if(found&&found->slot==source.placement.slot)offer=found;
    }
    if(source.placement.id==7&&source.hp)check(!offer,"Intact source wooden boards cannot be vaulted");
    if(!source.field)check(!offer,"Decorative source window without event field cannot be vaulted");
    if(!offer)continue;
    directions.insert(offer->direction);
    check(interior.room()==id,"Interaction flags are owned by the correct room");
    const auto start=probe.position();const auto heading=player.heading();
    player.setPlacement(start,heading+3.1415927f);
    check(!interior.windowOffer(player,probe),"Facing away rejects the window event");
    player.setPlacement(start,heading);
    check(interior.interact(player,probe)&&player.windowActive(),"Mapped context action starts actual source vault");
    check(!player.weaponVisible(),"Window event hides the weapon attachment");
    const unsigned breakTick=offer->direction==0?15:offer->direction==1?16:13;
    unsigned broken=0,sounds=0,ticks=0;
    while(player.windowActive()&&ticks<180) {
      const auto frame=player.windowTicks();
      re4::PlayerActionInput attack;attack.aimHeld=attack.fireHeld=attack.firePressed=attack.knifeHeld=true;
      player.step(probe,1,1,true,true,1./30,attack,{},[&]{interior.tick(player,probe);});
      for(const auto &event:interior.drainEvents()) {
        if(event.kind==re4::InteriorEvent::WindowBroken) {
          check(frame==breakTick&&event.soundBlock==6&&event.soundNo==0x3b,
                "Glass breaks exactly at the authored event counter with original sound");++broken;
        }
        if(event.kind==re4::InteriorEvent::WindowVaultSound)++sounds;
      }
      check(player.life()==1200&&player.shots()==0,"Vault owns movement/attack and preserves unlimited HP");
      if(ticks==18) {
        const auto feet=player.feet();const auto bones=player.rig().bones();const auto counter=player.windowTicks();
        player.suspendClock();player.step(probe,1,1,true,true,0,attack,{},[&]{interior.tick(player,probe);});
        check(player.feet()==feet&&player.rig().bones()==bones&&player.windowTicks()==counter&&interior.drainEvents().empty(),
              "Pause advances no vault pose, flags, counter or sound events");
      }
      ++ticks;
    }
    check(!player.windowActive()&&ticks<180&&broken==1&&sounds==6,"Original event terminates with one break and six source player sounds");
    check(player.feet()==probe.position()&&world.ground(player.feet(),1,1).has_value()&&
          interior.collision()->clearCapsule(player.feet(),300,1600),"Actual landing is grounded and clear of remaining props");
    check(std::hypot(player.feet()[0]-offer->landing[0],player.feet()[2]-offer->landing[2])<1,
          "Original root displacement finishes at the prevalidated physical landing");
    re4::RoomInteractions restored(id,room,placements,pl,interior.state(),&effect);
    for(const auto &w:restored.windows())if(w.placement.slot==offer->slot)check(w.hp==0,"Broken window survives room reconstruction in its own flags");
    ++complete;
  }
  check(complete>0,"At least one real authored window path completes in each room");
  std::cout<<std::hex<<'r'<<id<<std::dec<<" original window paths completed: "<<complete<<'\n';
}
void catalogAudit(const char *folder,const re4::RoomArchive &core) {
  unsigned rooms=0,unsupported=0,spawn=0,invalidCuts=0,models=0;
  const auto eff=data(core,"EFF");
  for(const auto &file:std::filesystem::directory_iterator(folder)) {
    const auto name=file.path().stem().string();
    if(file.path().extension()!=".dat"||name.size()!=4||name[0]!='r')continue;
    const auto id=unsigned(std::stoul(name.substr(1),nullptr,16));
    try {
      const auto room=re4::RoomArchive::Load(file.path().string());const auto lit=re4::ReadRoomLighting(room);
      check(lit->present[0],"Every catalog room has its actual usable initial LIT cut");invalidCuts+=lit->unavailableCuts.size();
      const auto smdBytes=data(room,"SMD");
      if(id==0x313||id==0x314||id==0x322||id==0x323) {
        check(smdBytes[0]==(id<0x320?0:0x31),"Four unregistered legacy model archives retain their unsupported source format");
        ++rooms;continue;
      }
      const re4::Smd smd(smdBytes);
      auto scene=re4::BuildStaticScene(smd);re4::AttachSceneTextures(smd,scene);
      if(room.find("SMX")){const re4::Smx smx(data(room,"SMX"));re4::ApplySceneSmx(smx,scene);}
      check(scene.triangles>0,"Catalog room has real static geometry");
      if(room.find("SAT")){re4::Sat sat(data(room,"SAT"));re4::SatCollision world(sat);re4::WalkProbe probe(world,config());spawn+=probe.reset();}
      if(room.find("ETS")&&room.find("ETM")) {auto objects=re4::BuildRoomObjects(room,eff,id);unsupported+=objects.unsupported;}
      ++models;++rooms;
    }catch(const std::exception &e){throw std::runtime_error(name+": "+e.what());}
  }
  check(rooms==127,"All 127 unique catalog archives audited, not just the development rooms");
  std::cout<<"Catalog CPU audit: "<<rooms<<" valid initial light archives; "<<models<<" supported model archives; "<<spawn
           <<" automatic SAT spawns; "<<unsupported<<" unsupported object placements; "<<invalidCuts
           <<" explicitly unavailable later source light cuts. This is not full campaign/GX coverage.\n";
}
}
int main(int argc,char **argv){try{
  if(argc!=6&&argc!=7)throw std::runtime_error("Usage: m31_assets_check R100 R101 PL00 WEP01 CORE [ALL_127_DECODED_ROOMS]");
  const auto r100=re4::RoomArchive::Load(argv[1]);blendInsertion(r100,argv[3],argv[4]);
  std::set<unsigned> directions;vaults(argv[1],0x100,argv[3],argv[4],directions);vaults(argv[2],0x101,argv[3],argv[4],directions);
  check(directions==std::set<unsigned>{0,1,2},"All three distinct original vault directions exercised");
  const auto core=re4::RoomArchive::LoadDrs(argv[5]);
  for(const auto *path:{argv[1],argv[2]}){
    const auto room=re4::RoomArchive::Load(path);const auto id=path==argv[1]?0x100:0x101;
    const auto objects=re4::BuildRoomObjects(room,data(core,"EFF"),id);unsigned glass=0;
    for(const auto &draw:objects.mesh.draws)for(const auto &p:objects.placements)
      if(draw.roomObjectSlot==int(p.slot)&&(p.id==0||p.id==0x25)) {
        check(draw.orderType==1&&draw.zMode==1,"WindowData.type2 glass uses sorted transparency without depth writes");++glass;
      }
    check(glass>0,"Actual original glass model draws verified");
  }
  if(argc==7)catalogAudit(argv[6],core);
  std::cout<<"M31 real asset checks PASS: "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
