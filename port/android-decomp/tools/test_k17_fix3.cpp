// Source-backed regression scenarios; requires the user's original disc assets.
#include "archive.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "inventory_case.h"
#include "save_native.h"
#include "enemy_group.h"
#include "r101_scenario.h"
#include "startup_data.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks{};
void check(bool pass,const std::string &message){++checks;if(!pass)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error(path);return {std::istreambuf_iterator<char>(in),{}};}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);check(e!=nullptr,tag);return a.copy(e-a.entries().data());}
unsigned slot(const InventoryCase &bag,unsigned id){for(const auto &i:bag.items())if(i.active && i.id==id)return i.slot;throw std::runtime_error("Missing case slot");}
CollisionVec direction(const PlayerShot &ray){CollisionVec d{};double len=0;for(unsigned k=0;k<3;++k){d[k]=ray.to[k]-ray.from[k];len+=d[k]*d[k];}for(auto &v:d)v/=std::sqrt(len);return d;}
Sat flat(){ScenarioTriangle a,b;a.vertices={{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}};b.vertices={{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}};a.group=b.group=0;return Sat::ScenarioGeometry({a,b});}
}
int main(int argc,char **argv){try {
  if(argc!=2)throw std::runtime_error("Usage: k17_fix3_check FIXTURE_ROOT");
  const auto root=std::filesystem::path(argv[1]),files=root/"files";
  // All original physical weapon-part pairs, both orders; the saved records must
  // keep base ID, stable owner/part slots and the loaded magazine.
  // Combined IDs are derived profiles, never separately packed weapons.
  struct Attachment{unsigned weapon,part,effective;};
  for(const auto &r:std::vector<Attachment>{{0x21,0x3f,0x22},{0x23,0x3f,0x24},{0x25,0x42,0x26},{0x27,0x3f,0x28},
      {0x30,0x43,0x32},{0x2e,0x44,0x6b},{0x2e,0xc5,0x99},
      {0x2f,0x45,0x6c},{0x2f,0xc5,0x51},{0x36,0xaa,0xab}})for(bool reverse:{false,true}) {
    InventoryCase bag(3);check(bag.add(0x7f,1) && bag.add(r.weapon,1) && bag.add(r.part,1),"Weapon/part fit XL case: "+std::to_string(r.weapon)+"/"+std::to_string(r.part));
    const auto w=slot(bag,r.weapon),p=slot(bag,r.part);auto records=bag.items();records[w].loaded=1;bag.restoreSnapshot(3,records,w);
    check(bag.combine(reverse?p:w,reverse?w:p),"Original attachment accepts either order");
    check(bag.equippedId()==r.effective && bag.item(w)->id==r.weapon && bag.item(w)->loaded==1,"Attachment selects effective ID without replacing base magazine");
    check(bag.item(p)->active && bag.item(p)->attachedTo==w && bag.item(p)->inCase(),"Original separate part record remains packed and linked");
    NativeSaveSnapshot save;save.inventory=bag.items();save.caseSize=3;save.equippedSlot=w;
    const auto encoded=EncodeNativeSave(save);AssetReader inv{encoded.inventory};
    check(inv.u16(0)==r.effective && inv.u16(4+p*12+2)==1 && inv.u16(4+p*12+4)==w,"Original save effective ID and lv/bullet link");
    const auto restored=DecodeNativeSave(encoded);InventoryCase loaded;loaded.restoreSnapshot(restored.caseSize,restored.inventory,restored.equippedSlot);
    check(loaded.equippedId()==r.effective && loaded.item(w)->loaded==1 && loaded.item(p)->attachedTo==w,"SAVE/LOAD restores scope and magazine");
    check(loaded.synchronizeWeaponAmmo(r.effective,0,0) && loaded.item(w)->loaded==0,"Firing scoped variant debits base slot");
    check(loaded.detach(p) && loaded.equippedId()==r.weapon && loaded.item(p)->active,"Remove returns base weapon without deleting scope");
  }
  InventoryCase swaps(3);check(swaps.add(0x2e,1) && swaps.add(0x44,1) && swaps.add(0xc5,1),"Replacement scopes fit");
  const auto rifle=slot(swaps,0x2e),scope=slot(swaps,0x44),infra=slot(swaps,0xc5);
  check(swaps.equip(rifle) && swaps.combine(rifle,scope) && swaps.combine(infra,rifle),"Infrared scope replaces rifle scope");
  check(swaps.equippedId()==0x99 && swaps.item(scope)->attachedTo==~0u,"Replaced scope remains unattached");
  check(swaps.combine(scope,rifle) && swaps.equippedId()==0x6b && swaps.item(infra)->attachedTo==~0u,"Rifle scope replaces infrared scope in reverse order");
  check(swaps.combine(rifle,infra) && swaps.equippedId()==0x99,"Infrared scope can be reattached");
  check(!swaps.combine(scope,infra) && swaps.equippedId()==0x99,"Incompatible combine is transactional");
  check(swaps.discard(infra) && swaps.equippedId()==0x2e,"Discard attached scope restores base ID");
  const auto floor=flat();SatCollision sat(floor);WalkProbe probe(sat);check(probe.place({0,0,0}),"Flat floor");
  LeonPreview gun((files/"em/pl00.drs").string(),(files/"em/wep01.drs").string(),(files/"em/em12.drs").string(),true);
  for(float heading:{0.f,.7853982f,1.5707964f,2.3561945f,3.1415927f,-2.3561945f,-1.5707964f,-.7853982f})for(double pitch:{-.7,0.,.7}) {
    gun.reset();check(gun.equipWeapon(0x36),"Mine equips");gun.setPlacement(probe.position(),heading);
    PlayerActionInput input;input.aimHeld=input.aimPressed=true;
    for(unsigned n=0;n<100 && gun.action()!=PlayerAction::Aim;++n){gun.step(probe,0,0,false,false,1./30,input);input.aimPressed=false;}
    check(gun.action()==PlayerAction::Aim && gun.laserReady(),"Normal mine has original laser");
    input.pitch=pitch;for(unsigned n=0;n<30;++n)gun.step(probe,0,0,false,false,1./30,input);
    input.pitch=0;for(unsigned n=0;n<2;++n)gun.step(probe,0,0,false,false,1./30,input);
    const auto aimed=gun.shotRay(false),laser=gun.laserRay();const auto shotDir=direction(aimed),laserDir=direction(laser);
    for(unsigned k=0;k<3;++k)check(std::abs(shotDir[k]-laserDir[k])<.00001,"Mine marker part1 and hand10 share the original forward axis");
    input.pitch=0;input.fireHeld=input.firePressed=true;gun.step(probe,0,0,false,false,1./30,input);
    const auto fired=gun.shotRay(false);const auto firedDir=direction(fired);
    for(unsigned k=0;k<3;++k)check(std::abs(firedDir[k]-shotDir[k])<.01,"Next authored ready pose keeps mine aimed forward");
    input.fireHeld=input.firePressed=false;
    for(unsigned n=0;n<5;++n){gun.step(probe,0,0,false,false,1./30,input);if(gun.action()!=PlayerAction::Fire)break;const auto recoil=gun.shotRay(false);check(recoil.from==fired.from && recoil.to==fired.to,"Mine emission retains saved trajectory throughout fire state");}
  }
  for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u,0xabu}) {
    gun.reset();check(gun.equipWeapon(id),"Scope profile equips");PlayerActionInput input;input.aimHeld=true;
    for(unsigned n=0;n<100 && gun.action()!=PlayerAction::Aim;++n)gun.step(probe,0,0,false,false,1./30,input);
    check(gun.scopeReady() && !gun.laserReady(),"Original rifles, optional scopes and scoped mine use scope");
  }
  const auto title=TitleData::VillageTitle(files.string());const auto layout=title.layoutJson(8000);
  check(layout.find("\"texture\":151")!=std::string::npos && layout.find("\"width\":640")!=std::string::npos,"Original bell title and fade plates load");
  for(unsigned id:{128u,149u,151u})check(title.texture(id)!=nullptr,"Original title texture registered");
  const auto room=RoomArchive::Load((root/"decoded/r101.dat").string());Sat village(entry(room,"SAT"));SatCollision terrain(village),eat(village,SatQueryMode::Effect);
  EnemySpawns spawns(read((files/"etc/emleon00.esl").string()));auto routes=std::make_shared<EnemyRoutes>(entry(room,"RTP"),terrain);
  WalkProbe villageProbe(terrain);check(villageProbe.placeAuthored({-38027,141,11114}),"Source village entrance");
  LeonPreview player((files/"em/pl00.drs").string(),(files/"em/wep01.drs").string(),(files/"em/em12.drs").string(),true);player.setPlacement(villageProbe.position(),1.72f);player.setUnlimitedHealth(true);
  for(unsigned batch=0;batch<3;++batch) {
    EnemyGroup group(terrain,eat,(files/"em/em15.drs").string());group.setRoutes(routes);
    ScenarioState state;R101Scenario scenario(entry(room,"AEV"));auto active=scenario.initialSpawns(spawns,state);
    if(batch==1)active={0x28,0x29,0x2a,0x2b,0x2c};
    if(batch==2)active={0x2e,0x2f,0x30,0x31,0x32};
    for(auto index:active){const auto &s=*spawns.find(index);group.spawn(s);check(std::abs(group.find(index)->feet()[0]-s.position[0])<.01 && std::abs(group.find(index)->feet()[2]-s.position[2])<.01,"Authored spawn XZ unchanged");}
    group.releaseVillageAlarm();PlayerActionInput idle;
    for(unsigned n=0;n<1800;++n)group.stepPlayer(player,villageProbe,0,0,false,false,1./30,idle);
    for(const auto &a:group.actors()) {
      const auto &s=*spawns.find(a->listIndex());const auto moved=std::hypot(a->feet()[0]-s.position[0],a->feet()[2]-s.position[2]);
      const bool keeper=s.setNo==0 && s.position[0]>30000;
      if(!keeper && !(s.flags&0x400))check(moved>20000,"Full wave traverses tower/road terrain: "+std::to_string(a->listIndex()));
      if(keeper)check(moved<500,"Original distant keeper stays within post");
    }
  }
  // Ordinary dash uses the original FCV/SEQ and speed, with no multiplier.
  const auto &spawn=*spawns.find(0x28);EnemyPreview walker((files/"em/em15.drs").string(),spawn);walker.setOriginalWalking(0,true);
  walker.start(EnemyMotion::Walk);double walkTravel=0;for(unsigned n=0;n<120;++n){const auto d=walker.advance();walkTravel+=std::hypot(d.translation[0],d.translation[2]);}
  walker.start(EnemyMotion::Dash);double dashTravel=0;for(unsigned n=0;n<120;++n){const auto d=walker.advance();dashTravel+=std::hypot(d.translation[0],d.translation[2]);}
  check(dashTravel>walkTravel*1.5,"Original run FCV is faster than walk");
  SatCollision flatEat(floor,SatQueryMode::Effect);EnemyGroup chase(sat,flatEat,(files/"em/em15.drs").string());
  auto runnerSpawn=*spawns.find(0x14);runnerSpawn.position={0,0,25000};runnerSpawn.flags&=~0x440u;
  gun.reset();gun.setPlacement(probe.position(),0);gun.setUnlimitedHealth(true);
  auto &runner=chase.spawn(runnerSpawn);chase.setDifficulty(8);chase.releaseVillageAlarm();bool ran=false;
  for(unsigned n=0;n<600 && !ran;++n){chase.stepPlayer(gun,probe,0,0,false,false,1./30,{});ran=runner.actor().motion()==EnemyMotion::Dash;}
  check(ran,"Live Ganado transitions to dash through original timer/rank/discovery gates: character="+std::to_string(runnerSpawn.character)+" "+runner.status()+" frame="+std::to_string(runner.actor().rig().sequenceFrame())+" distance="+std::to_string(std::hypot(runner.feet()[0]-gun.feet()[0],runner.feet()[2]-gun.feet()[2])));
  std::cout<<"K17 Fix3 attachments/SAVE-LOAD,24 mine directions,source scopes/title,three long village waves and original dash PASS: "<<checks<<" checks\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<"K17 Fix3 FAILED: "<<e.what()<<'\n';return 1;}}
