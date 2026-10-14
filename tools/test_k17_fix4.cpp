#include "archive.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "enemy_group.h"
#include "r101_scenario.h"
#include "room_blocks.h"
#include "village_animals.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks{};
void check(bool pass,const std::string &message){++checks;if(!pass)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error(p.string());return {std::istreambuf_iterator<char>(f),{}};}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);check(e!=nullptr,tag);return a.copy(e-a.entries().data());}
CollisionVec direction(const ActionCameraPose &v){CollisionVec d{};double length{};for(unsigned k=0;k<3;++k){d[k]=v.target[k]-v.eye[k];length+=d[k]*d[k];}for(auto &x:d)x/=std::sqrt(length);return d;}
Sat flat(){ScenarioTriangle a,b;a.vertices={{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}};b.vertices={{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}};a.group=b.group=0;return Sat::ScenarioGeometry({a,b});}
Sat roof(bool marked) {
  std::vector<ScenarioTriangle> t;
  for(const auto floor:std::array<std::array<float,2>,2>{{{0,10000},{3200,240}}}) {
    const auto y=floor[0],r=floor[1];ScenarioTriangle a,b;a.group=b.group=0;
    a.vertices={{{-r,y,-r},{r,y,r},{r,y,-r}}};b.vertices={{{-r,y,-r},{-r,y,r},{r,y,r}}};t.push_back(a);t.push_back(b);
  }
  if(marked) {
    ScenarioTriangle a,b;a.attributes=b.attributes=0x10;
    a.vertices={{{-2000,3200,600},{2000,6000,600},{2000,3200,600}}};
    b.vertices={{{-2000,3200,600},{-2000,6000,600},{2000,6000,600}}};t.push_back(a);t.push_back(b);
  }
  return Sat::ScenarioGeometry(t);
}
}
int main(int argc,char **argv){try {
  check(argc==2,"Usage: k17_fix4_check FIXTURE_ROOT");
  const auto root=std::filesystem::path(argv[1]),files=root/"files";
  const auto floor=flat();SatCollision ground(floor);WalkProbe probe(ground);
  LeonPreview gun((files/"em/pl00.drs").string(),(files/"em/wep01.drs").string(),(files/"em/em12.drs").string(),true);
  for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u})for(double pitch:{-.35,.35}) {
    check(probe.placeAuthored({0,0,0}),"Scope flat floor");gun.reset();gun.setPlacement(probe.position(),1.f);
    check(gun.equipWeapon(id),"Rifle equips");gun.setUnlimitedAmmo(true);
    PlayerActionInput in;in.aimHeld=true;
    for(unsigned i=0;i<100 && !gun.scopeReady();++i)gun.step(probe,0,0,false,false,1./30,in);
    check(gun.scopeReady(),"Rifle base and accessory scopes ready");
    in.pitch=pitch;for(unsigned i=0;i<15;++i)gun.step(probe,0,0,false,false,1./30,in);
    in.pitch=0;in.scopeZoom=1;for(unsigned i=0;i<6;++i)gun.step(probe,0,0,false,false,1./30,in);in.scopeZoom=0;
    const auto original=direction(gun.scopeCamera());const auto fov=gun.scopeCamera().fovDegrees;
    for(unsigned shot=0;shot<3;++shot) {
      in.fireHeld=in.firePressed=true;const auto serial=gun.shots();
      for(unsigned i=0;i<150 && gun.shots()==serial;++i){gun.step(probe,0,0,false,false,1./30,in);in.firePressed=false;}
      check(gun.shots()==serial+1,"Scoped rifle fires");in.fireHeld=in.firePressed=false;
      for(unsigned i=0;i<180;++i) {
        if(gun.scopeReady()) {
          const auto now=direction(gun.scopeCamera());double dot{};for(unsigned k=0;k<3;++k)dot+=now[k]*original[k];
          check(dot>.9995,"Scope trajectory preserves focus through fire/bolt with original breathing");
          check(std::abs(gun.scopeCamera().fovDegrees-fov)<.00001,"Scope zoom preserved through fire/bolt cycle");
        }
        if(i && gun.action()==PlayerAction::Aim)break;
        gun.step(probe,0,0,false,false,1./30,in);
      }
      check(gun.action()==PlayerAction::Aim,"Rifle returns to scoped aim");
    }
    std::cout<<"Scope profile "<<id<<" pitch "<<pitch<<" PASS\n";
  }
  for(unsigned id=0;id<256;++id) {
    if(!OriginalWeaponCarry(id))continue;
    gun.reset();check(gun.equipWeapon(id),"Knife return weapon equips");
    gun.setPlacement(probe.position(),0);PlayerActionInput in;in.knifeHeld=true;
    for(unsigned i=0;i<80 && gun.action()!=PlayerAction::KnifeReady;++i)gun.step(probe,0,0,false,false,1./30,in);
    check(gun.action()==PlayerAction::KnifeReady && gun.bladeVisible(),"Knife ready");in.knifeHeld=false;
    bool switchSeen=false,transitionSeen=false;
    for(unsigned i=0;i<100 && gun.action()!=PlayerAction::Footwork;++i) {
      gun.step(probe,0,0,false,false,1./30,in);
      if(gun.action()==PlayerAction::KnifeHolster) {
        transitionSeen=true;
        if(!gun.bladeVisible()){switchSeen=true;check(gun.weaponVisible(),"Weapon visible during knife holster after source frame4");}
      }
    }
    check(transitionSeen && switchSeen && gun.action()==PlayerAction::Footwork,"Knife switches gun before transition ends");
  }
  const auto room=RoomArchive::Load((root/"decoded/r101.dat").string());Sat village(entry(room,"SAT"));SatCollision terrain(village),eat(village,SatQueryMode::Effect);
  EnemySpawns esl(read(files/"etc/emleon00.esl"));
  VillageAnimals animals(terrain,esl,(files/"em").string());
  check(animals.animals().size()==3,"r101 original cow and two chickens instantiated");
  for(const auto &animal:animals.animals()) {
    check(animal->feet()==animal->source().position,"Animal uses exact authored ESL placement");
    check(!animal->model().mesh().vertices.empty() && animal->model().rig()->clipCount()>8,"Animal original skinned mesh and motions loaded");
  }
  for(unsigned i=0;i<300;++i)animals.tick({-50000,0,-50000},false,false,false);
  for(unsigned hen:{1u,2u}) {
    const auto &a=*animals.animals()[hen];std::cout<<"Hen idle "<<hen<<" pos "<<a.feet()[0]<<","<<a.feet()[1]<<","<<a.feet()[2]<<'\n';
    check(a.routine()==VillageAnimalRoutine::Wait,"Pre-alarm hen respects original idle flag");
    check(std::hypot(a.feet()[0]-a.source().position[0],a.feet()[2]-a.source().position[2])<250,"Idle hen remains at authored source location with SCA contact");
  }
  const auto henPosition=animals.animals()[1]->feet();
  animals.tick({henPosition[0]+800,henPosition[1],henPosition[2]+800},false,false,false);
  check(animals.animals()[1]->routine()==VillageAnimalRoutine::Dash || animals.animals()[1]->routine()==VillageAnimalRoutine::Jump,"Hen escapes nearby player");
  for(unsigned i=0;i<90;++i)animals.tick({henPosition[0]+800,henPosition[1],henPosition[2]+800},false,false,false);
  check(std::hypot(animals.animals()[1]->feet()[0]-henPosition[0],animals.animals()[1]->feet()[2]-henPosition[2])>400,"Original hen escape FCV moves across real SAT");
  for(const auto &a:animals.animals()) {
    const auto &matrix=a->model().rig()->jointMatrices().at(a->source().id==0x26?1:2);
    const auto posed=WeaponAttachmentMatrix(matrix,a->model().worldMatrix());
    const auto center=TransformPoint(posed,a->source().id==0x26?std::array<float,3>{0,-150,450}:std::array<float,3>{0,0,-80});
    const auto hit=animals.target({center[0]-2500,center[1],center[2]},{center[0]+2500,center[1],center[2]},2);
    std::cout<<"Animal hit id "<<unsigned(a->source().id)<<" index "<<a->source().listIndex<<" center "<<center[0]<<","<<center[1]<<","<<center[2]<<" hp "<<a->hp()<<" target "<<bool(hit)<<'\n';
    check(bool(hit),"Original posed animal capsule accepts firearm hit");
    animals.applyShot(*hit,100+unsigned(a->source().listIndex),a->source().id==0x26?9:2,a->source().id==0x26?3:0,{center[0]-2500,center[1],center[2]});
    animals.tick({-50000,0,-50000},false,false,false);
    check(!a->alive(),"Original rifle kills cow; handgun kills hen");
  }
  for(unsigned i=0;i<150;++i)animals.tick({-50000,0,-50000},false,true,true);
  const auto dead=animals.deadIndices();check(dead.size()==3,"Animal deaths persist by original ESL slot");
  VillageAnimals restored(terrain,esl,(files/"em").string());restored.restoreDead(dead);
  for(const auto &animal:restored.animals())check(!animal->visible() && !animal->alive(),"Saved dead animal remains absent after re-entry");
  for(unsigned index:R101Scenario::actorIndices()) {
    const auto &spawn=*esl.find(index);EnemyEncounter enemy(terrain,eat,spawn,(files/"em/em15.drs").string(),true);
    check(enemy.feet()[0]==spawn.position[0] && enemy.feet()[2]==spawn.position[2],"Every Ganado uses authored spawn XZ");
    check(std::abs(enemy.feet()[1]-spawn.position[1])<600,"Every Ganado uses matching source floor");
    std::cout<<"Spawn "<<index<<" pos "<<enemy.feet()[0]<<","<<enemy.feet()[1]<<","<<enemy.feet()[2]<<" flags "<<std::hex<<spawn.flags<<std::dec<<'\n';
    if(spawn.flags&0x400) {
      auto from=enemy.feet();from[1]+=500;
      for(const std::array<float,3> local:std::array<std::array<float,3>,4>{{{0,500,800},{800,500,0},{-800,500,0},{0,500,-800}}}) {
        const auto p=TransformPoint(enemy.placement(),local);const auto h=terrain.line(from,{p[0],p[1],p[2]},SatSubset::All,0,0);
        if(h)std::cout<<"  Roof surface "<<std::hex<<h->attributes<<std::dec<<" n="<<h->normal[0]<<","<<h->normal[1]<<","<<h->normal[2]<<'\n';
      }
    }
    WalkProbe victimProbe(terrain);gun.reset();gun.setUnlimitedHealth(true);
    const auto pos=enemy.feet();bool placed=false;
    for(double radius:{900.,700.,500.,1100.,1300.})for(unsigned d=0;d<32 && !placed;++d) {
      const double angle=enemy.heading()+double(d)*3.14159265/16;
      const CollisionVec candidate{pos[0]+std::sin(angle)*radius,pos[1],pos[2]+std::cos(angle)*radius};
      auto from=pos,to=candidate;from[1]+=1500;to[1]+=1500;
      if(!eat.line(from,to,SatSubset::All,0,0x4000))
        if(const auto f=terrain.ground(candidate,500,1000);f && std::abs(f->point[1]-pos[1])<500)
          placed=victimProbe.placeAuthored(candidate);
    }
    check(placed,"Every authored Ganado has a local real-SAT attack target");gun.setPlacement(victimProbe.position(),0);
    enemy.releaseVillageAlarm();
    for(unsigned i=0;i<600 && !enemy.axeAttacks() && !enemy.grabs() && !gun.grabbed() && enemy.actor().motion()!=EnemyMotion::Catch;++i)
      enemy.stepPlayer(gun,victimProbe,0,0,false,false,1./30,{});
    check(enemy.axeAttacks()+enemy.grabs()>0 || gun.grabbed() || enemy.actor().motion()==EnemyMotion::Catch,
        "Every released Ganado enters its original attack/grab routine, index "+std::to_string(index)+" "+enemy.status());
  }
  for(bool marked:{false,true}) {
    const auto structure=roof(marked);SatCollision roofWorld(structure),roofEat(floor,SatQueryMode::Effect);
    auto spawn=*esl.find(0x1e);spawn.position={0,3200,0};spawn.heading=0;spawn.hp=500;
    EnemyEncounter enemy(roofWorld,roofEat,spawn,(files/"em/em15.drs").string(),true,EnemyEncounter::TestPlacement{{0,3200,0},0});
    WalkProbe targetProbe(roofWorld);check(targetProbe.placeAuthored({0,0,-3500}),"Roof fall observer floor");
    gun.reset();gun.setPlacement(targetProbe.position(),0);gun.setUnlimitedHealth(true);
    ObjectBlast blast;blast.serial=1;blast.weaponNo=20;blast.position={0,4000,-300};blast.radius=1200;
    check(enemy.applyBlast(blast,gun,targetProbe),"Source blast hits original roof capsules");
    bool fall=false,land=false;
    for(unsigned i=0;i<300;++i) {
      enemy.stepPlayer(gun,targetProbe,0,0,false,false,1./30,{});
      const auto m=enemy.actor().motion();
      fall=fall || (marked?(m==EnemyMotion::RoofBack || m==EnemyMotion::RoofFront):
          (m==EnemyMotion::BlowAirUp || m==EnemyMotion::BlowAirDown));
      land=land || (marked?m==EnemyMotion::RoofLand:(m==EnemyMotion::BlowLandUp || m==EnemyMotion::BlowLandDown));
      if(land && enemy.deadSettled())break;
    }
    std::cout<<"Roof original mask "<<marked<<" falling "<<fall<<" landing "<<land<<" finalY "<<enemy.feet()[1]<<'\n';
    check(fall && land && enemy.feet()[1]<600,"Source roof/knockback transition crosses ledge and lands below");
  }
  R101Scenario scenario(entry(room,"AEV"));ScenarioState state;state.room(0x101).room.set(7);state.scenario.set(10);
  scenario.tick(state,{-3000,0,-3000},0,2,true);auto cues=scenario.takeCues();
  check(cues.size()==1 && cues[0].kind==VillageCue::CombatStart,"Post-bell encounter starts combat stream once");
  for(unsigned i=0;i<60;++i)scenario.tick(state,{-3000,0,-3000},0,1,true);
  check(scenario.takeCues().empty(),"Living last Ganado retains stream without restarting");
  scenario.tick(state,{-3000,0,-3000},0,0,false);cues=scenario.takeCues();
  check(cues.size()==1 && cues[0].kind==VillageCue::CombatStop,"Last post-bell Ganado stops stream");
  std::cout<<"K17 Fix4 PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"K17 Fix4 FAILED: "<<e.what()<<'\n';return 1;}}
