#include "forest_animals.h"
#include "archive.h"
#include "ports/rnd.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
std::vector<std::uint8_t> read(const std::string &path) {
 std::ifstream f(path,std::ios::binary);require(bool(f),"missing ESL");return {(std::istreambuf_iterator<char>(f)),{}};
}
void finite(const re4::ForestAnimals &a) {
 for(const auto &bird:a.animals()) {
  require(bird.rig().bones().size()==24,"crow needs24parts");
  for(auto value:bird.placement())require(std::isfinite(value),"nonfinite placement");
  for(const auto &bone:bird.rig().bones())for(auto value:bone)require(std::isfinite(value),"nonfinite pose");
 }
}
}
int main(int argc,char **argv){try{
 require(argc==4,"Usage: forest_animals_check R100.DAT EMLEON00.ESL EM23.DRS");
 const auto room=re4::RoomArchive::Load(argv[1]);const auto *record=room.find("SAT");require(record,"missing SAT");
 const re4::Sat satData(room.copy(record-room.entries().data()));const re4::SatCollision sat(satData);
 const auto esl=read(argv[2]);RndInit(0x1471);re4::ForestAnimals a(sat,esl,argv[3]),b(sat,esl,argv[3]);
 require(RndState()==0x1471,"crow constructor leaked RNG");
 const std::array<unsigned,5> indices{15,16,17,32,33};
 const std::array<re4::CollisionVec,5> positions{{{-89580,2060,-3790},{-85630,0,-4640},{-89250,0,-6400},{-36110,1740,-17410},{-38430,-180,-18950}}};
 std::set<const void*> owners;
 for(unsigned i=0;i<5;++i) {
  const auto &bird=a.animals()[i];require(bird.source().listIndex==indices[i],"source crow ESLindex");
  require(bird.feet()==positions[i],"source crow placement");
  require(bird.source().raw==re4::EnemySpawns(esl).find(indices[i])->raw,"source crow raw metadata");
  require(bird.hp()==10&&bird.routine()==re4::CrowRoutine::Wait&&bird.wing()==1,"source initial crowstate");
  require(bird.flightHeight()==float(indices[i]%5)*1000+8000,"source crowflightheight");
  require(bird.scale()>=.9f&&bird.scale()<=1.1f,"source random crow scale");
  for(unsigned variant=0;variant<2;++variant)require(owners.insert(bird.gpuOwner(variant)).second,"GPUowner alias");
 }
 for(unsigned wing=0;wing<2;++wing) {
  const auto &mesh=a.mesh(wing);require(mesh.vertices.size()/3==a.skin(wing).size(),"source skin corners");
  require(mesh.triangles==(wing?1000u:1080u)&&mesh.textures.size()==(wing?1u:2u),"source body+wing textured mesh");
  require(!mesh.fallbackTriangles&&!mesh.unresolvedAlphaTriangles,"missing crow texture/mask");
 }
 const re4::CollisionVec far{-100000,0,60000};
 for(unsigned frame=0;frame<300;++frame) {a.step(1./30,far);b.step(1./60,far);b.step(1./60,far);}
 for(unsigned i=0;i<5;++i) {
  require(std::hypot(a.animals()[i].feet()[0]-positions[i][0],a.animals()[i].feet()[2]-positions[i][2])<100,"idle crow escaped authored perch");
  require(a.animals()[i].rig().bones()==b.animals()[i].rig().bones(),"30/60Hz idle difference");
  require(a.animals()[i].routine()==re4::CrowRoutine::Wait,"far crow tookoff");
 }
 // Strict source distance boundaries, XZ only, source noise expands range.
 re4::ForestAnimals threshold(sat,esl,argv[3]);auto close=positions[0];close[0]+=5000;
 threshold.tick(close);require(threshold.animals()[0].routine()==re4::CrowRoutine::Wait,"5000boundary mustnottrigger");
 close[0]-=1;threshold.tick(close);require(threshold.animals()[0].routine()==re4::CrowRoutine::Takeoff,"4999musttrigger");
 re4::ForestAnimals noise(sat,esl,argv[3]);close=positions[0];close[0]+=9000;
 noise.tick(close);require(noise.animals()[0].routine()==re4::CrowRoutine::Wait,"9000withoutfire");
 noise.tick(close,true);require(noise.animals()[0].routine()==re4::CrowRoutine::Takeoff,"9000withfire");
 // Sweep the player past all five authored slots, testing flight/wing/state
 // determinism against split render ticks and isolated game RNG.
 unsigned takeoffs=0,flights=0,wingSwaps=0,soundFlaps=0;
 for(unsigned frame=0;frame<1800;++frame) {
  auto player=positions[(frame/360)%5];player[0]+=1000;
  a.step(1./30,player,frame%121==0);b.step(1./60,player,frame%121==0);b.step(1./60,player,frame%121==0);
  require(a.ticks()==b.ticks()&&RndState()==0x1471,"ticks/globalRNG diverged");
  for(unsigned i=0;i<5;++i) {
   const auto &x=a.animals()[i],&y=b.animals()[i];
   require(x.feet()==y.feet()&&x.heading()==y.heading()&&x.scale()==y.scale(),"flight float placement differs");
   require(x.rig().bones()==y.rig().bones(),"flight poses differ");
   require(x.routine()==y.routine()&&x.wing()==y.wing()&&x.motionClip()==y.motionClip(),"flight source states differ");
   takeoffs+=x.routine()==re4::CrowRoutine::Takeoff;flights+=x.routine()==re4::CrowRoutine::Turn;wingSwaps+=x.wing()==0;
  }
  for(const auto &s:a.takeSounds()) {require(s.block==8&&s.argument==0x23,"source crow soundbank");soundFlaps+=s.number==0xb;}
  b.takeSounds();if(!(frame%60))finite(a);
 }
 require(takeoffs&&flights&&wingSwaps&&soundFlaps,"authored flight/wing/SFXmissing");
 const auto old=a.animals()[0].rig().bones();const auto tick=a.ticks();a.step(1./60,far);a.suspendClock();a.step(1./60,far);
 require(a.ticks()==tick&&a.animals()[0].rig().bones()==old,"pause fractional advance");
 // A real source hit capsule kills an original hp10 crow; fall and death
 // produce exactly one original money pickup. No fabricated item table.
 re4::ForestAnimals killed(sat,esl,argv[3]);auto at=killed.animals()[1].feet();re4::PlayerShot shot;
 shot.serial=1;shot.weaponItemId=0x23;shot.from={at[0]-1000,at[1],at[2]};shot.to={at[0]+1000,at[1],at[2]};
 require(killed.target(shot.from,shot.to,2).has_value(),"source crow capsule target");
 killed.queueShot(shot);killed.queueShot(shot);killed.tick(far);
 require(!killed.animals()[1].alive()&&killed.animals()[1].routine()==re4::CrowRoutine::DamageAir,"crow gunhit deathmissing");
 unsigned drops=0;
 for(unsigned frame=0;frame<600;++frame){killed.tick(far);for(const auto &drop:killed.takeDrops()){
  require(drop.listIndex==16&&drop.id==0x78&&drop.count>=10&&drop.count<=50&&!(drop.count%10),"original crow moneydrop");++drops;
 }}
 require(drops==1&&killed.animals()[1].routine()==re4::CrowRoutine::DieNormal,"crow drop emitted twice/deathblocked");
 require(killed.deadIndices()==std::vector<unsigned>{16},"source deathflags");
 re4::ForestAnimals restored(sat,esl,argv[3]);restored.restoreDead(killed.deadIndices());restored.tick(far);
 require(!restored.animals()[1].visible()&&!restored.animals()[1].alive()&&restored.takeDrops().empty(),"deathsave restored as living/drop rerolled");
 // Actual source ITA overrides two crow drops: Spinel and hand grenade.
 re4::ScenarioState scenario;re4::ForestAnimals authored(sat,esl,argv[3]);authored.setAuthoredDrops(room,scenario);
 const auto spinel=re4::FindAuthoredEnemyDrop(room,16,scenario);
 const auto grenade=re4::FindAuthoredEnemyDrop(room,32,scenario);
 require(spinel&&spinel->id==0x57&&spinel->recordIndex==9&&spinel->autoFlag==11,"source Spinel ITAlink");
 require(grenade&&grenade->id==1&&grenade->recordIndex==10&&grenade->autoFlag==12,"source grenade ITAlink");
 require(!re4::FindAuthoredEnemyDrop(room,15,scenario),"randomcrow received inventedauthoreddrop");
 unsigned authoredDrops=0;
 for(unsigned owner:{1u,3u}) {
  at=authored.animals()[owner].feet();shot.serial++;shot.from={at[0]-1000,at[1],at[2]};shot.to={at[0]+1000,at[1],at[2]};
  authored.queueShot(shot);
  for(unsigned frame=0;frame<600;++frame){authored.tick(far);for(const auto &drop:authored.takeDrops()){
   require(drop.listIndex==(owner==1?16u:32u)&&drop.id==(owner==1?0x57u:1u)&&drop.count==0,"authored crowreward was replaced");
   require(drop.effect==(owner==1?3:5),"authored crow glow colour was replaced");++authoredDrops;
  }}
 }
 require(authoredDrops==2&&scenario.room(0x100).items.test(11)&&scenario.room(0x100).items.test(12),"authored drop flag claimmissing");
 const auto claimed=re4::FindAuthoredEnemyDrop(room,16,scenario);
 require(claimed&&claimed->taken&&!re4::ClaimAuthoredEnemyDrop(*claimed,scenario),"authored drop flags allowed duplicate");
 re4::ForestAnimals blocked(sat,esl,argv[3]);blocked.setAuthoredDrops(room,scenario);at=blocked.animals()[1].feet();
 shot.serial++;shot.from={at[0]-1000,at[1],at[2]};shot.to={at[0]+1000,at[1],at[2]};blocked.queueShot(shot);
 for(unsigned frame=0;frame<600;++frame)blocked.tick(far);
 require(blocked.takeDrops().empty(),"claimed authored reward fellbackto random money");
 // No model substitution for an unavailable corpse/crow type.
 auto malformed=esl;malformed[15*32+3]=1;bool rejected=false;
 try{re4::ForestAnimals bad(sat,malformed,argv[3]);}catch(const std::runtime_error &){rejected=true;}
 require(rejected,"unsupported r20acorpse path silentlysubstituted");
 finite(killed);std::cout<<"M37 forest animals: "<<checks<<" checks PASS, five original24-part EM23 crows; flight,damage,moneydrop,deathsave.\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
