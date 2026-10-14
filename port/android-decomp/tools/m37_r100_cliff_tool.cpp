// Independent original MotionMove oracle fixture for r100 room-owned FCVs.
#include "archive.h"
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
  const EnemySpawns spawns(read(argv[2]));const auto index=unsigned(std::stoul(argv[4]));
  const auto *spawn=spawns.find(index);if(!spawn || index<6 || index>8)throw std::runtime_error("Cliff source profile missing");
  const auto room=RoomArchive::Load(argv[3]);EnemyPreview actor(argv[1],*spawn,true);
  actor.installRoomEventMotion(room.copy(index+36));auto &rig=actor.rig();
  word(0x4d333743);word(index);word(actor.type());word(index+36);word(34);word(5);
  scalar(rig.maxFrame(*actor.roomEventAction()));
  const unsigned ticks=unsigned(rig.maxFrame(*actor.roomEventAction()))+16;word(ticks);
  rig.reset();actor.start(EnemyMotion::RoomEvent);
  for(unsigned tick=0;tick<ticks;++tick) {
   const auto root=actor.advance();const auto state=rig.status();
   for(double value:root.translation)scalar(float(value));
   scalar(float(root.yaw));
   scalar(state.curveFrame);scalar(state.sequenceFrame);word(state.state);word(rig.sequenceFree());
   for(const auto &matrix:rig.jointMatrices())for(float value:matrix)scalar(value);
  }
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
