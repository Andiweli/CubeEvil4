#include "original_enemy_drop.h"
#include "ports/rnd.h"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
unsigned next(std::uint16_t &state) {
 const unsigned r=state;
 const unsigned high=(((r>>1)+(r>>8))&255)<<8;
 const unsigned low=(r>>1)&255;
 const unsigned candidate=high|low;
 state=std::uint16_t(candidate==r?candidate+257:candidate);
 return state>>8;
}
}
int main(){try {
 re4::EnemyDropContext c;c.carriedWeapons={0x23};
 // Independent byte-generator plus source EM23 branch, every16-bit seed.
 // Ensures the wrapper consumes the leading RandomItemCk roll, retains
 // each rare amount's extra draws and never changes another owner's RNG.
 RndInit(0x71ea);
 for(unsigned seed=0;seed<65536;++seed) {
  std::uint16_t random=std::uint16_t(seed);next(random);
  unsigned count=(next(random)%3)*10+20;
  if(!(next(random)&3)) {
   if(next(random)&1)count=(next(random)&1)*10+10;
   else count=(next(random)&1)*10+40;
  }
  re4::EnemyDropState state{std::uint16_t(seed),3,1};
  const auto got=re4::OriginalEnemyRandomDrop(0x23,c,state);
  require(got&&got->id==0x78&&got->count==count,"crow drop choice differs from source byte path");
  require(state.randomState==random&&state.noDropCount==3&&state.noRecoveryDropCount==1,"crow drop RNG/streak differs");
  require(RndState()==0x71ea,"drop wrapper leaked global RNG");
 }
 // Guaranteed authored treasure bypasses the random table and byte stream.
 re4::AuthoredEnemyDrop treasure;treasure.id=0x57;treasure.autoFlag=11;treasure.room=0x100;
 re4::EnemyDropState state{0x1111,2,1};re4::ScenarioState scenario;
 require(re4::ClaimAuthoredEnemyDrop(treasure,scenario),"first authored claim blocked");
 const auto got=re4::ResolveAuthoredEnemyDrop(treasure,c,state);
 require(got&&got->id==0x57&&got->count==0&&state.randomState==0x1111,"authored treasure consumed RNG/replaced item");
 require(scenario.room(0x100).items.test(11)&&!re4::ClaimAuthoredEnemyDrop(treasure,scenario),"claim flag/idempotency");
 treasure.itemFlag=65;treasure.autoFlag=12;
 require(re4::ClaimAuthoredEnemyDrop(treasure,scenario)&&scenario.globalItems.test(65)&&!scenario.room(0x100).items.test(12),"source global-flag precedence");
 // Source Em12 random table honors carried shotgun, and keeps no-drop
 // streak bounded across long normal-play histories.
 c.carriedWeapons={0x23,0x2c};c.handgunAmmo=60;c.shotgunAmmo=0;
 unsigned shotgun=0,money=0,recovery=0,none=0;
 for(unsigned i=0;i<10000;++i) {
  const auto drop=re4::OriginalEnemyRandomDrop(0x12,c,state);
  if(!drop){++none;continue;}
  shotgun+=drop->id==0x18;money+=drop->id==0x78;recovery+=drop->id==6||drop->id==5||drop->id==0x19;
  require(state.noDropCount<=6&&state.noRecoveryDropCount<=2,"source streak budget");
 }
 require(shotgun&&money&&recovery&&none,"source normal-enemy item branches missing");
 std::cout<<"M37 enemy drop contracts: "<<checks<<" checks PASS (all65536crow seeds, authored flags/rewards, carried shotgun).\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
