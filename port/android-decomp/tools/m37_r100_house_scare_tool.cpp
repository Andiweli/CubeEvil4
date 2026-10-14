#include "enemy_preview.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
void word(unsigned v) {for(int k=3;k>=0;--k)std::cout.put(char(v>>(k*8)));}
void scalar(float v) {unsigned bits;std::memcpy(&bits,&v,4);word(bits);}
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("ESL missing");
 std::vector<std::uint8_t> b(std::size_t(f.tellg()));f.seekg(0);
 if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("ESL truncated");
 return b;
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=5)return 2;
  const EnemySpawns spawns(read(argv[2]));const auto *spawn=spawns.find(0x25);
  if(!spawn)throw std::runtime_error("Source house actor missing");
  const unsigned walk=std::stoul(argv[3]),choice=std::stoul(argv[4]);
  if(walk>1||choice>1)throw std::runtime_error("Source house fixture choice");
  EnemyPreview enemy(argv[1],*spawn,true);enemy.prepareHouseScareMotions(walk);auto &rig=enemy.rig();
  const auto action=choice?EnemyMotion::HouseWalk:EnemyMotion::HouseTurn;
  const auto index=choice?(walk?31u:1u):30u;const auto selected=enemy.motionSource(action);
  word(0x4d333748);word(walk);word(choice);word(selected.motionEntry);word(selected.sequenceEntry);word(selected.blendFrames);word(selected.flags);
  const auto ticks=rig.cycle(index).sequenceFrames+16;word(ticks);
  rig.reset();enemy.start(action);
  for(unsigned tick=0;tick<ticks;++tick) {
   const auto root=enemy.advance();const auto state=rig.status();
   for(double v:root.translation)scalar(float(v));
   scalar(float(root.yaw));
   scalar(state.curveFrame);scalar(state.sequenceFrame);word(state.state);word(rig.sequenceFree());
   for(const auto &matrix:rig.jointMatrices())for(float v:matrix)scalar(v);
  }
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
