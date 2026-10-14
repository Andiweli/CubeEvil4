// Independent original-source oracle input: authored seven Ganado families.
#include "enemy_preview.h"
#include "r100_scenario.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
void word(unsigned value) {for(int k=3;k>=0;--k)std::cout.put(char(value>>(k*8)));}
void scalar(float value) {unsigned bits;std::memcpy(&bits,&value,4);word(bits);}
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Original ESL missing");
 std::vector<std::uint8_t> b(std::size_t(file.tellg()));file.seekg(0);
 if(!file.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Original ESL truncated");
 return b;
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=5)return 2;
  const EnemySpawns spawns(read(argv[2]));const auto *profile=spawns.find(unsigned(std::stoul(argv[3])));
  if(!profile)throw std::runtime_error("Original list identity missing");
  EnemyPreview enemy(argv[1],*profile,true);auto &rig=enemy.rig();
  const auto action=EnemyMotion(unsigned(std::stoul(argv[4])));const auto selected=enemy.motionSource(action);
  word(0x4d333641);word(enemy.type());word(enemy.weaponType());word(enemy.weaponParentPart());word(enemy.mirrored());
  word(selected.motionEntry);word(selected.sequenceEntry);word(selected.blendFrames);word(selected.flags|(enemy.mirrored()?0x40:0));
  const auto cycle=rig.cycle(unsigned(action));word(cycle.sequenceFrames);scalar(rig.maxFrame(unsigned(action)));
  for(double value:cycle.translation)scalar(float(value));
  scalar(float(cycle.yaw));
  const unsigned ticks=cycle.sequenceFrames+16;word(ticks);
  rig.reset();enemy.start(action);
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
