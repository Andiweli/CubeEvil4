// Independent source-oracle fixture: first r100 Ganado's mirrored axe FCV/SEQ.
#include "enemy_preview.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
void word(unsigned value) {for(int k=3;k>=0;--k)std::cout.put(char(value>>(k*8)));}
void scalar(float value) {unsigned bits;std::memcpy(&bits,&value,4);word(bits);}
}
int main(int argc,char **argv) {
 try {
  if(argc!=2)return 2;
  EnemyPreview enemy(argv[1],0xff,true,true);auto &rig=enemy.rig();
  if(!enemy.holdsAxe() || !enemy.visiblePart(616) || !enemy.visiblePart(452) ||
     enemy.visiblePart(449) || enemy.visiblePart(445) || enemy.visiblePart(450))
    throw std::runtime_error("r100: original left axe/grip visibility");
  const auto selected=EnemyPreview::source(EnemyMotion::AxeAttack,0xff);
  word(0x4d333541);word(34);word(selected.motionEntry);word(selected.sequenceEntry);
  word(selected.blendFrames);word(selected.flags|0x40);
  const auto cycle=rig.cycle(unsigned(EnemyMotion::AxeAttack));
  word(cycle.sequenceFrames);scalar(rig.maxFrame(unsigned(EnemyMotion::AxeAttack)));
  for(double value:cycle.translation)scalar(float(value));
  scalar(float(cycle.yaw));
  const unsigned ticks=cycle.sequenceFrames+16;word(ticks);
  rig.reset();enemy.start(EnemyMotion::AxeAttack);
  if(!enemy.mirrored())throw std::runtime_error("r100: source axe mirror lost");
  for(unsigned tick=0;tick<ticks;++tick) {
    const auto root=enemy.advance();const auto state=rig.status();
    for(double value:root.translation)scalar(float(value));
    scalar(float(root.yaw));
    scalar(state.curveFrame);scalar(state.sequenceFrame);word(state.state);word(rig.sequenceFree());
    for(const auto &matrix:rig.jointMatrices())for(float value:matrix)scalar(value);
    for(float value:enemy.weaponMatrix())scalar(value);
  }
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
