#include "enemy_feedback.h"
#include "ports/rnd.h"
#include <iostream>
using namespace re4;
namespace { void word(unsigned value) {for(int k=3;k>=0;--k)std::cout.put(char(value>>(k*8)));} }
int main() {
 word(0x4d333647);word(3*7*4*2*258);
 for(unsigned weapon:{1u,2u,7u})for(unsigned level=0;level<7;++level)
  for(float distance:{0.f,35999996.f,36000000.f,36000004.f})for(bool head:{false,true})
   for(unsigned seedIndex=0;seedIndex<258;++seedIndex) {
    const unsigned seed=seedIndex<256?seedIndex:seedIndex==256?0x4e19:0xffff;
    RndInit(std::uint16_t(seed));const auto hit=GanadoFirearmHitDamage(weapon,level,distance,head);
    word(GanadoFirearmBaseDamage(weapon,level,distance<36000000.f));word(hit.damage);word(hit.critical);word(RndState());
   }
 return 0;
}
