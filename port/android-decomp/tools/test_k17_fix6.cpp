// Real source FCVs: bolt-cycle compensation, SS interpolation and pitchfork contact.
#define main retained_fix4_main
#include "test_k17_fix4.cpp"
#undef main
#include "subscreen_data.h"
#include "generic_event_actor.h"
int main(int argc,char **argv){try {
 check(argc==2,"Usage: k17_fix6_check FIXTURE_ROOT");const auto files=std::filesystem::path(argv[1])/"files";
 const auto geometry=flat();SatCollision ground(geometry);WalkProbe probe(ground);
 LeonPreview player((files/"em/pl00.drs").string(),(files/"em/wep01.drs").string(),(files/"em/em12.drs").string(),true);
 for(unsigned id:{0x2eu,0x6bu,0x99u})for(bool held:{true,false}) {
  check(probe.placeAuthored({0,0,0}),"Source bolt-cycle floor");player.reset();player.setPlacement(probe.position(),0);check(player.equipWeapon(id),"Bolt rifle profile");
  player.setUnlimitedAmmo(true);PlayerActionInput in;in.aimHeld=true;
  for(unsigned i=0;i<100 && !player.scopeReady();++i)player.step(probe,0,0,false,false,1./30,in);
  check(player.scopeReady(),"Original scope ready");in.firePressed=in.fireHeld=true;
  const auto shots=player.shots();for(unsigned i=0;i<100 && shots==player.shots();++i){player.step(probe,0,0,false,false,1./30,in);in.firePressed=false;}
  in.firePressed=in.fireHeld=false;in.aimHeld=held;
  const auto archive=RoomArchive::LoadDrs((files/"em/wep09.drs").string());
  const auto carry=OriginalWeaponCarry(id);SkinnedModel gun(archive.copy(carry->model-4));
  AnimationRig expected(gun,std::vector<AnimationBytes>{{archive.copy(0x21-4),{},false}});
  unsigned cycles=0,down=0;double oldError=0;
  for(unsigned i=0;i<200;++i) {
   if(player.weapon()->motion()==WeaponMotion::RifleBoltCycle) {
    ++cycles;const auto frame=player.rig().curveFrame();if(std::abs(frame-player.weapon()->frame())>=.001)std::cout<<"Cycle phase player "<<frame<<" weapon "<<player.weapon()->frame()<<" seq "<<player.rig().sequenceFrame()<<" tick "<<i<<std::endl;check(std::abs(frame-player.weapon()->frame())<.001,"Player ARC1B and weapon ARC21 remain frame-synchronous");
    expected.sample(0,frame);const auto &actual=player.weapon()->rig().jointMatrices();
    for(unsigned part=0;part<actual.size();++part)for(unsigned k=0;k<12;++k)check(std::abs(actual[part][k]-expected.jointMatrices()[part][k])<.002,"Every posed weapon part follows independent original bolt FCV");
    WeaponPreview old((files/"em/wep09.drs").string(),id,(files/"em/pl00.drs").string());
    for(unsigned k=0;k<12;++k)oldError+=std::abs(actual[1][k]-old.rig().jointMatrices()[1][k]);
   }
   if(player.action()==PlayerAction::Holster && player.weapon()->motion()==WeaponMotion::RifleReloadDown)++down;
   if(i>5 && (player.action()==PlayerAction::Aim || player.action()==PlayerAction::Footwork))break;
   player.step(probe,0,0,false,false,1./30,in);
  }
  std::cout<<"Bolt profile "<<id<<" held "<<held<<" compensated ticks "<<cycles<<" down "<<down<<" old idle error "<<oldError<<'\n';
  check(cycles>=24 && oldError>100,"Missing bolt compensation is reproduced by the former idle object pose");check(held || down>0,"Released bolt cycle uses source ARC24 reload-type lowering");
 }
 for(unsigned id=0;id<256;++id)if(OriginalWeaponCarry(id)) {
  auto menu=LoadOriginalMenuPortrait(files.string(),id,true);menu.advance(25);const auto clock=menu.actors.front()->rig()->sequenceFrame();
  for(unsigned owner=0;owner<menu.actors.size();++owner) {
   menu.renderFraction=0;const auto a=menu.renderBones(owner);menu.renderFraction=1;const auto b=menu.renderBones(owner);menu.renderFraction=.5;const auto middle=menu.renderBones(owner);
   for(unsigned part=0;part<a.size();++part)for(unsigned k:{3u,7u,11u})check(std::abs(middle[part][k]-(a[part][k]+b[part][k])*.5)<.001,"SS midpoint smoothly interpolates solved body/clothes/hands/gun translations");
  }
  check(menu.actors.front()->rig()->sequenceFrame()==clock,"Presentation interpolation cannot consume original FCV ticks");
 }
 const EnemySpawns spawns(read(files/"etc/emleon00.esl"));
 for(bool mirror:{false,true})for(double distance:{900.,1200.,1600.,1900.}) {
  auto spawn=*spawns.find(20);spawn.position={0,0,0};spawn.heading=0;spawn.rotation={0,0,0};spawn.flags=0x80000000u|(mirror?0x01000000u:0);spawn.beFlags=1;
  EnemyGroup group(ground,ground,(files/"em/em12.drs").string());auto &enemy=group.spawn(spawn);group.releaseVillageAlarm();
  check(enemy.actor().weaponType()==1,"Original pitchfork equipped");check(probe.placeAuthored({0,0,distance}),"Player test floor");player.reset();player.setPlacement(probe.position(),3.14159265f);player.setUnlimitedHealth(true);
  double closest=100000,behind=0;
  for(unsigned tick=0;tick<360;++tick) {
   group.stepPlayer(player,probe,0,0,false,false,1./30,{});
   closest=std::min(closest,std::hypot(enemy.feet()[0]-player.feet()[0],enemy.feet()[2]-player.feet()[2]));
   if(enemy.feet()[2]>player.feet()[2]+200)++behind;
  }
  std::cout<<"Pitchfork mirror "<<mirror<<" distance "<<distance<<" attacks "<<enemy.axeAttacks()<<" hits "<<enemy.axeHits()<<" closest "<<closest<<" crossed "<<behind<<'\n';
  check(enemy.axeAttacks()>0 && enemy.axeHits()>0,"Stationary Leon is reached and struck by original pitchfork sweep");check(behind==0,"Pitchfork attack root cannot pass through Leon");
 }
 std::cout<<"K17 Fix6 source tests PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
