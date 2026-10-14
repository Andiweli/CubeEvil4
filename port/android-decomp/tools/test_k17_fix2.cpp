// Regression checks using authorized original resources; no retail assets included.
#include "archive.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "enemy_group.h"
#include "event_playback.h"
#include "r101_scenario.h"
#include "room_objects.h"
#include "startup_data.h"
#include "item_rules.h"
#include "inventory_case.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
using namespace re4;
unsigned checks{};
void check(bool pass,const std::string &message){if(!pass)throw std::runtime_error(message);++checks;}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);check(e!=nullptr,tag);return a.copy(e-a.entries().data());}
double travel(const CollisionVec &a,const CollisionVec &b){return std::hypot(a[0]-b[0],a[2]-b[2]);}
void equal(const Matrix34 &a,const Matrix34 &b,const char *label){for(unsigned k=0;k<12;++k)check(std::abs(a[k]-b[k])<.003f,label);}
Sat floorData(){ScenarioTriangle a,b;a.vertices={{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}};b.vertices={{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}};a.group=b.group=0;return Sat::ScenarioGeometry({a,b});}
}
int main(int argc,char **argv){using namespace re4;try {
  if(argc<2 || argc>3)throw std::runtime_error("Usage: k17_fix2_check FIXTURE_ROOT [SCOPE_CSV]");
  const auto root=std::filesystem::path(argv[1]),files=root/"files";
  const auto pl=(files/"em/pl00.drs").string(),wep=(files/"em/wep01.drs").string(),em=(files/"em/em12.drs").string();
  const auto floor=floorData();SatCollision world(floor);WalkProbe probe(world);check(probe.place({0,0,0}),"Flat floor");
  LeonPreview player(pl,wep,em,true);player.setPlacement(probe.position(),0);player.setRoomScriptEyeMode(1);
  for(unsigned id=0;id<256;++id)if(const auto *source=OriginalWeaponCarry(id)) {
    const auto label=std::string(OriginalItemLabel(id));
    check(player.equipWeapon(id),label+" equips");check(player.setWeaponAmmo(id,player.magazineCapacity(),20),label+" ammunition");
    PlayerActionInput input;input.aimHeld=input.aimPressed=true;
    player.step(probe,0,0,false,false,1./30,input);input.aimPressed=false;
    check(player.action()==PlayerAction::Draw,label+" draws on first simulation tick");
    for(unsigned n=0;n<120 && player.action()!=PlayerAction::Aim;++n)player.step(probe,0,0,false,false,1./30,input);
    check(player.action()==PlayerAction::Aim,label+" reaches ready");
    const bool scoped=source->number==9 || source->number==10 || source->number==13 || id==0xab;
    check(player.scopeReady()==scoped,label+" original scope selection");
    if(scoped) {
      check(!player.laserReady(),label+" excludes original laser");
      const auto ray=player.shotRay(false);const auto view=player.scopeCamera();
      check(ray.from==view.eye,label+" camera trajectory starts at eye");
      input.fireHeld=input.firePressed=true;player.step(probe,0,0,false,false,1./30,input);
      const auto fired=player.shotRay(false);check(fired.from==ray.from && fired.to==ray.to,label+" recoil does not redirect the emitted shot");
      input.fireHeld=input.firePressed=false;
    }
    if(source->number==19 || source->number==22 || source->number==23) {
      SmdPlacement offset;offset.position={-95,-30,5};offset.rotation={1.5707964f,0,0};offset.scale={1,1,1};
      equal(player.weapon()->attachment(player.rig().jointMatrices(),PlayerAction::Aim,0),WeaponAttachmentMatrix(PlacementMatrix(offset),player.rig().jointMatrices()[10]),"Original grenade grip offset and rotation");
      check(!player.laserReady(),"Grenades use no laser");
    }
    if(source->number==14) {
      const auto posed=player.weapon()->posedParts(player.rig().jointMatrices(),PlayerAction::Aim,0);
      SmdPlacement dart;dart.scale=player.weapon()->rig().partScale(1);
      equal(posed[1],WeaponAttachmentMatrix(PlacementMatrix(dart),player.rig().jointMatrices()[10]),"Mine dart follows right hand independently of left-held launcher");
      check(player.visiblePart(1300+3+id),"Mine's ready right-hand BIN is visible");
    }
    // Release can enter Footwork after the weapon's authored recoil/cycle;
    // movement must interrupt the separate lowering pose on the next tick.
    input.aimHeld=false;
    for(unsigned n=0;n<400 && player.action()!=PlayerAction::Holster && player.action()!=PlayerAction::Footwork;++n)
      player.step(probe,0,0,false,false,1./30,input);
    const auto before=player.feet();player.step(probe,0,1,false,false,1./30,input);
    check(player.action()==PlayerAction::Footwork,label+" movement immediately interrupts lowering");
    for(unsigned n=0;n<5 && travel(before,player.feet())<=.01;++n)player.step(probe,0,1,false,false,1./30,input);
    check(travel(before,player.feet())>.01,label+" original walking FCV advances after its start/interpolation");
  }
  const auto room=RoomArchive::Load((root/"decoded/r101.dat").string());
  Sat villageFloor(entry(room,"SAT"));SatCollision sat(villageFloor),eat(villageFloor,SatQueryMode::Effect);
  WalkProbe villageProbe(sat);check(villageProbe.placeAuthored({-38027,141,11114}),"Village authored player entry");
  LeonPreview villager(pl,wep,em,true);villager.setPlacement(villageProbe.position(),1.72f);villager.setUnlimitedHealth(true);
  std::ifstream input(files/"etc/emleon00.esl",std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};EnemySpawns spawns(bytes);
  auto routes=std::make_shared<EnemyRoutes>(entry(room,"RTP"),sat);
  for(unsigned batch=0;batch<3;++batch) {
    EnemyGroup group(sat,eat,(files/"em/em15.drs").string());group.setRoutes(routes);
    ScenarioState state;R101Scenario scenario(entry(room,"AEV"));auto active=scenario.initialSpawns(spawns,state);
    if(batch==1)active={0x28,0x29,0x2a,0x2b,0x2c};
    if(batch==2)active={0x2e,0x2f,0x30,0x31,0x32};
    for(auto index:active)group.spawn(*spawns.find(index));
    group.releaseVillageAlarm();
    PlayerActionInput idle;for(unsigned n=0;n<300;++n)group.stepPlayer(villager,villageProbe,0,0,false,false,1./30,idle);
    for(const auto &a:group.actors()) {
      const auto &spawn=*spawns.find(a->listIndex());
      const bool distantKeeper=spawn.setNo==0 && spawn.position[0]>30000;
      check(a->foundPlayer(),"Village alarm marks every active actor as found");
      if(!distantKeeper)check(travel(a->feet(),{spawn.position[0],spawn.position[1],spawn.position[2]})>1000,"Alarm releases ordinary villager/spawn overlap: "+std::to_string(a->listIndex()));
    }
  }
  OriginalEvent bell(files.string(),"r101s30");unsigned doorFrames{};
  for(unsigned n=0;n<bell.totalFrames();++n) {
    for(const auto &actor:bell.actors())if(actor->name()=="scr0000" && actor->visible()) {
      const auto root=actor->rig()->jointMatrices().at(0);
      check(std::hypot(root[3],root[11])>10000,"Bell camera cuts retain the external door's last solved world pose");++doorFrames;
    }
    bell.tick30Hz();
  }
  check(doorFrames>1000,"Complete bell event includes external door across every cut");
  const auto core=RoomArchive::LoadDrs((files/"etc/core.das").string());RoomSaveState state;state.room.set(8);
  const auto objects=BuildRoomObjects(room,entry(core,"EFF"),0x101,&state);std::set<int> slots;
  for(const auto &draw:objects.mesh.draws)if(draw.roomObjectSlot>=5 && draw.roomObjectSlot<=7) {
    slots.insert(draw.roomObjectSlot);check(draw.lightVolume.center[1]>1900,"Standing ladder has raised culling/light centre");
    for(std::size_t v=draw.first;v<draw.first+draw.count;++v) {
      double distance{};for(unsigned k=0;k<3;++k)distance+=std::pow(objects.mesh.vertices[v*3+k]-draw.lightVolume.center[k],2);
      check(distance<=double(draw.lightSize)*draw.lightSize+2,"Every posed ladder vertex lies inside its culling sphere");
    }
  }
  check(slots.size()==3,"All three original house ladders checked");
  if(argc==3) {
    std::ofstream out(argv[2]);check(bool(out),"Scope geometry export");
    for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u,0x35u,0x17u,0x6du,0xabu}) {
      const auto *source=OriginalWeaponCarry(id);char path[20];snprintf(path,sizeof(path),"wep%02u.drs",source->archive);
      const auto a=RoomArchive::LoadDrs((files/"em"/path).string());const auto layout=a.copy(source->number==9 || source->number==10?source->variant+1:1);
      auto textures=DecodeSubscreenIdTextures(a.copy(0));
      for(auto shared:DecodeSubscreenIdTextures(core.copy(0x1d-4)))
        if(std::none_of(textures.begin(),textures.end(),[&](const auto &t){return t.id==shared.id && t.frame==shared.frame;}))textures.push_back(std::move(shared));
      const TitleData art(std::move(textures),layout);check(art.layoutJson(8000).find("\"rotationX\"")!=std::string::npos,"Source ID exports X/Y rotations");
      AssetReader r{layout};const bool v2=layout[0]=='2';const auto stride=v2?0x8cu:0x88u;
      for(unsigned i=0;i<layout[5];++i){const auto p=8+i*stride,rot=p+(v2?0x60:0x5c);
        out<<id<<','<<unsigned(layout[p+5])<<','<<unsigned(layout[p+7])<<','<<unsigned(layout[p+9])<<','<<unsigned(layout[p+4])<<','<<unsigned(layout[p+3]);
        for(unsigned k:{0x14u,0x18u,0x50u,0x54u})out<<','<<r.f32(p+k);
        for(unsigned k=0;k<3;++k)out<<','<<r.f32(rot+k*4);
        out<<','<<unsigned(layout[p+12]&15);
        out<<'\n';
      }
    }
  }
  std::cout<<"K17 Fix2 original weapon response/grips/scopes, all village waves,1350 bell frames and3 ladder bounds PASS: "<<checks<<" checks\n";
}catch(const std::exception &e){std::cerr<<"K17 Fix2 failure: "<<e.what()<<'\n';return 1;}}
