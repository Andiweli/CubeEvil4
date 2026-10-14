// Frozen original route oracle plus real r101, SS and WEP09/WEP10 data.
#define main retained_m37_enemy_ai_main
#include "test_m37_enemy_ai.cpp"
#undef main
#include "weapon_preview.h"
#include "subscreen_data.h"
#include "generic_event_actor.h"
#include "enemy_feedback.h"
#include "r101_scenario.h"
#include <filesystem>
int main(int argc,char **argv){try{using namespace re4;
 require(argc==2,"Usage: k17_fix5_check FIXTURE_ROOT");
 const auto root=std::filesystem::path(argv[1]),files=root/"files";
 ScopeSwayState sway;float px=0,py=0,phaseX=0,phaseY=0,gain=0,target=0,speedX=0,speedY=0;unsigned count=0;
 for(unsigned tick=0;tick<1800;++tick) {
  float x=float(int(tick%19)-9)/9,y=float(int(tick%23)-11)/11,g=float(tick%37)/36;
  if(count==0){speedX=.062831856f*(x*.5f+1);speedY=.062831856f*(y*.5f+1);target=g*.0005f;count=151;}
  --count;gain=gain*.95f+target*.05f;px+=std::cos(phaseX)*gain;py+=std::cos(phaseY)*(.0005f-gain);
  phaseX=std::remainder(phaseX+speedX,6.283185307179586f);phaseY=std::remainder(phaseY+speedY,6.283185307179586f);
  OriginalScopeSway(sway,x,y,g);
  require(std::abs(sway.pitch-px)<1e-7f && std::abs(sway.yaw-py)<1e-7f && sway.countdown==count,"Source camera scope breathing recurrence/151-tick RNG cadence");
 }
 require(std::abs(px)+std::abs(py)>.0001f,"Original breathing has visible nonzero angular displacement");
 GanadoPeerState peer;peer.active=true;peer.hp=50;peer.routine=2;peer.position={0,0,-100000};
 require(!OriginalGanadoPeers({0,0,0},0,{peer},0x100).somebodyDamaged,"r100 source keeps remote behind-player damage out of awareness");
 require(OriginalGanadoPeers({0,0,0},0,{peer},0x101).somebodyDamaged,"r101 source forwards distant damage behind walls regardless of facing");
 std::set<std::uint32_t> serials;
 for(unsigned owner:{0u,20u,34u,74u,255u})for(unsigned event:{1u,2u,0xffffffu})
  require(serials.insert(GanadoEffectTriggerSerial(owner,event)).second,"Every blood event has unique valid source32-bit sprite id");
 const auto room=RoomArchive::Load((root/"decoded/r101.dat").string());const Sat raw(sub(room,"SAT")),eat(sub(room,"EAT"));
 const SatCollision sat(raw),walls(eat);const auto bytes=sub(room,"RTP");
 const auto routes=std::make_shared<EnemyRoutes>(bytes,sat);auto oracle=SourceOracle::nativeRtp(bytes);
 SourceOracle::Global.Rtp=oracle.data();SourceOracle::SatMgr.sat=&sat;
 for(unsigned i=0;i<routes->points().size();++i)for(unsigned j=0;j<routes->points().size();++j)for(unsigned mode:{0u,1u}) {
  auto a=SourceOracle::vec(routes->points()[i]),b=SourceOracle::vec(routes->points()[j]);SourceOracle::cEm original;original.pos=a;original.atari.m_flag=0x2301;SourceOracle::Vec dest;float maximum=0;
  bool direct=SourceOracle::RouteCkToPos(&original,&b,&dest,int(mode),&maximum)!=0;
  auto actual=routes->step(routes->points()[i],routes->points()[j],mode,0x2301);
  require(actual.direct==direct && actual.destination==SourceOracle::vec(dest) && actual.maxHeight==maximum,"Original r101 graph with exact Ganado Atari mask and both height modes");
 }
 const EnemySpawns esl(read((files/"etc/emleon00.esl").c_str()));
 LeonPreview player((files/"em/pl00.drs").string(),(files/"em/wep01.drs").string(),(files/"em/em12.drs").string(),true);WalkProbe probe(sat);
 require(probe.placeAuthored({-8000,0,-5000}),"Real r101 village center floor");player.setPlacement(probe.position(),0);player.setUnlimitedHealth(true);
 for(unsigned index:{20u,21u,22u,23u,24u,34u,35u,36u}) {
  require(probe.placeAuthored(index>=34?CollisionVec{-38027,141,11114}:CollisionVec{-8000,0,-5000}),"Village observer on original approach/center floor");player.reset();player.setUnlimitedHealth(true);player.setPlacement(probe.position(),0);
  const auto spawn=*esl.find(index);EnemyEncounter enemy(sat,walls,spawn,(files/"em/em15.drs").string(),true);enemy.setRoutes(routes);
  enemy.setPeerAwareness(true,false);
  enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});
  require(enemy.foundPlayer(),"Original r101 damage alert wakes authored house/corner Ganado without player in front");
  const auto start=enemy.feet();
  for(unsigned tick=0;tick<300;++tick){enemy.setPeerAwareness(true,false);enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});}
  const auto moved=std::hypot(enemy.feet()[0]-start[0],enemy.feet()[2]-start[2]);
  std::cout<<"Village alert slot "<<index<<" character "<<unsigned(spawn.character)<<" moved "<<moved<<" state "<<enemy.status()<<'\n';
  require(moved>50 || enemy.axeAttacks() || enemy.grabs(),"Alerted Ganado starts movement/attack from real house/corner source placement");
 }
 // WEP09/10 reload must finish its own FCV even when the scope was tilted.
 const Sat flat=Sat::ScenarioGeometry({ScenarioTriangle{{{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}},0,0},ScenarioTriangle{{{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}},0,0}});
 SatCollision floor(flat);WalkProbe flatProbe(floor);require(flatProbe.placeAuthored({0,0,0}),"Reload floor");
 for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u})for(float pitch:{-.5f,.5f})for(bool hold:{false,true}) {
  player.reset();require(player.equipWeapon(id),"Rifle profile equips");require(player.setWeaponAmmo(id,player.magazineCapacity(),100),"Rifle reserve rounds available");player.setPlacement(flatProbe.position(),0);PlayerActionInput in;in.aimHeld=true;
  for(unsigned tick=0;tick<80 && !player.scopeReady();++tick)player.step(flatProbe,0,0,false,false,1./30,in);
  require(player.scopeReady(),"Rifle scope initialized");in.pitch=pitch;
  for(unsigned tick=0;tick<12;++tick)player.step(flatProbe,0,0,false,false,1./30,in);
  in.pitch=0;
  in.firePressed=in.fireHeld=true;const auto shots=player.shots();
  for(unsigned tick=0;tick<100 && player.shots()==shots;++tick){player.step(flatProbe,0,0,false,false,1./30,in);in.firePressed=false;}
  in.firePressed=in.fireHeld=false;
  for(unsigned tick=0;tick<120 && player.action()!=PlayerAction::Aim;++tick)player.step(flatProbe,0,0,false,false,1./30,in);
  require(player.shots()==shots+1 && player.action()==PlayerAction::Aim,"Rifle shot consumes ammo and returns to scope");
  in.reloadPressed=true;player.step(flatProbe,0,0,false,false,1./30,in);in.reloadPressed=false;
  require(player.action()==PlayerAction::Reload,"Rifle enters source reload");in.aimHeld=hold;unsigned ticks=0;bool scopeTail=false,downObject=false;
  while(player.action()==PlayerAction::Reload && ticks<250) {
   if(player.scopeReady())scopeTail=true;
   player.step(flatProbe,0,0,false,false,1./30,in);++ticks;
  }
  if(!hold && player.weapon()->number()==9)downObject=player.weapon()->motion()==WeaponMotion::RifleReloadDown;
  std::cout<<"Rifle reload id "<<id<<" pitch "<<pitch<<" hold "<<hold<<" ticks "<<ticks<<" tail "<<scopeTail<<'\n';
  require(ticks>50,"Rifle reload is not prematurely cut short by scope pitch");
  require(player.magazine()==player.magazineCapacity(),"Source reload commit restores magazine");
  require(hold?scopeTail && player.action()==PlayerAction::Aim:player.action()==PlayerAction::Holster,"Rifle reload completes original scope-tail/release path");
  require(hold || player.weapon()->number()!=9 || downObject,"Bolt Rifle reload-release uses original weapon ARC24");
  for(unsigned tick=0;tick<100 && player.action()==PlayerAction::Holster;++tick)player.step(flatProbe,0,0,false,false,1./30,in);
  require(hold || player.action()==PlayerAction::Footwork,"Original Rifle reload-down finishes and permits movement");
 }
 for(unsigned id=0;id<256;++id)if(OriginalWeaponCarry(id)) {
  auto menu=LoadOriginalMenuPortrait(files.string(),id,true);require(menu.actors.size()>=7,"Original SS body/clothing/hands loaded");
  auto *rig=menu.actors.front()->rig();auto before=rig->bones();menu.advance(25);
  require(rig->sequenceFrame()>=24,"Independent original menu loop advances source clock");
  require(rig->bones()!=before,"Original menu motion changes posed parts");
  for(unsigned i=1;i<7;++i)require(menu.actors[i]->rig()==rig,"SS clothing and both hands share Leon's live skeleton");
  menu.advance(1200);require(!rig->status().ended,"Original menu MotionSetCore flag4 loops indefinitely");
 }
 std::cout<<"K17 Fix5 original scope/reload/menu/blood/r101 checks PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"K17 Fix5 FAILED: "<<e.what()<<'\n';return 1;}}
