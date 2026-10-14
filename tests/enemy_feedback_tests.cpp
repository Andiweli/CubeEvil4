#include "enemy_feedback.h"
#include "ports/rnd.h"
#include "world_effects.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
void point(const HitVector &actual, const HitVector &expected) {
  for (unsigned i = 0; i < 3; ++i)
    require(std::abs(actual[i] - expected[i]) < .0002f,
              "Source capsule surface/axis projection");
}
template<class Function> void rejects(Function function) {
  bool rejected = false;
  try { function(); } catch (const std::runtime_error &) { rejected = true; }
  require(rejected, "Invalid source input rejected");
}
void put(WaterRecord &r,unsigned p,unsigned value) {
  for(unsigned k=0;k<4;++k) r.bytes.at(p+k)=std::uint8_t(value>>(24-k*8));
}
void real(WaterRecord &r,unsigned p,float value) {
  unsigned bits;std::memcpy(&bits,&value,4);put(r,p,bits);
}
void stripGeometry() {
  WorldEffectsData data;data.sprites.signature=33;
  WaterAnimation a;a.width=a.height=4;a.loop=2;a.pattern={0};a.duration={1};
  data.sprites.animations={a};data.sequences={0x29};data.targetPrerun={0};
  WaterRecord r;r.bytes[0]=1;r.bytes[1]=1;r.bytes[7]=0xff;r.bytes[200]=12;
  real(r,16,1500);real(r,36,1);real(r,48,1);real(r,136,7);real(r,140,1);real(r,152,1);
  for(unsigned k=0;k<4;++k){r.bytes[156+k]=255;real(r,160+k*4,1);}
  WaterTarget target;target.sequence=0x29;target.records={r};data.sprites.targets={target};
  WorldEffectsSimulation sim(data);WorldEffectCamera camera;camera.eye={0,1500,5000};
  sim.tick(&camera);sim.tick(&camera);
  const auto state=sim.state();
  require(state.sprites.size()==1,"Original strip owner survives");
  point(state.sprites[0].position,{0,1500,0});
  point(state.sprites[0].speed,{1,0,0});
  require(state.sprites[0].age==2,"Esp01 lifetime advances without integrating owner position");
  const auto quads=WorldEffectQuads(data,state,camera);
  require(quads.size()==2,"Source replay emits two nonzero segments at life2");
  point(quads[0].position[0],{2,1493,0});point(quads[0].position[1],{2,1507,0});
  point(quads[1].position[0],quads[0].position[3]);
  point(quads[1].position[1],quads[0].position[2]);
  require(std::abs(quads[0].uv[2][1]-1.f/3)<.00001f &&
              std::abs(quads[1].uv[0][1]-1.f/3)<.00001f,
            "EspStrip_draw_poly authored texture slice continuity");
  require(sim.restore(state),"Strip replay checkpoint retains immutable source state");
}
}
int main() {
  try {
    const Matrix34 identity{1,0,0,0,0,1,0,0,0,0,1,0};
    const AuthoredHitShape y{{0,0,0},10,100,0,HitOn,1};
    point(GanadoDamagePose(y,{},identity,{30,50,40}).position,{6,50,8});
    point(GanadoDamagePose(y,{},identity,{30,-50,40}).position,{6,0,8});
    point(GanadoDamagePose(y,{},identity,{30,150,40}).position,{6,100,8});
    point(GanadoDamagePose(y,{},identity,{0,50,0}).position,{10,50,0});
    auto x = y; x.flags |= HitXAxis;
    point(GanadoDamagePose(x,{},identity,{50,30,40}).position,{50,6,8});
    point(GanadoDamagePose(x,{},identity,{150,30,40}).position,{100,6,8});
    auto z = y; z.flags |= HitZAxis;
    point(GanadoDamagePose(z,{},identity,{30,40,50}).position,{6,8,50});
    point(GanadoDamagePose(z,{},identity,{30,40,-50}).position,{6,8,0});
    auto both = x; both.flags |= HitZAxis;
    point(GanadoDamagePose(both,{},identity,{50,30,40}).position,{50,6,8});
    SmdPlacement placement;
    placement.scale = {1,1,1}; placement.position = {10,20,30};
    placement.rotation = {0,1.57079632679f,0};
    point(GanadoDamagePose(y,{},PlacementMatrix(placement),{50,70,0}).position,
            {18,70,24});
    auto dm = y; dm.flags |= HitDamagePosition;
    const auto marked = GanadoDamagePose(dm,{7,8,9},identity,{17,8,9});
    point(marked.position,{7,8,9});
    require(std::abs(marked.rotation[1] - 1.57079632679f) < .00001f,
              "DMPOS uses registered cross and original XZ yaw");
    auto offset = y; offset.offset = {10,20,30};
    point(GanadoDamagePose(offset,{},identity,{40,70,70}).position,{16,70,38});
    const auto random = RndState();
    require(GanadoBloodSequence(2,false,false,0) == 1,
              "Original unarmoured handgun EST1");
    require(GanadoBloodSequence(0x10,false,false,0) == 0x29,
              "Original knife EST29");
    require(!GanadoBloodSequence(0,true,false,0), "Kick does not emit gun blood");
    require(GanadoBloodSequence(7,false,false,0) == 1,
              "Distant shotgun uses small bullet blood");
    require(GanadoBloodSequence(7,true,false,15999999) == 0x36,
              "Near shot / near camera / BIGEFF not cooling");
    require(GanadoBloodSequence(7,true,false,16000000) == 2,
              "Original strict4000-unit camera boundary");
    require(GanadoBloodSequence(7,true,true,15999999) == 0x37,
              "CTRL12 BIGEFF cooldown selects EST37");
    require(GanadoBloodSequence(7,true,true,16000000) == 0x35,
              "Cooldown far camera selects EST35");
    require(GanadoBloodSequence(0x13,false,false,0) == 0x36,
              "Explosion follows original large blood branch");
    require(GanadoVoiceCue(0) == 0x16 && GanadoVoiceCue(2) == 0x18 &&
                GanadoVoiceCue(5) == 0x1b && GanadoVoiceCue(7) == 0x25 &&
                GanadoVoiceCue(8) == 0x47 && GanadoVoiceCue(11) == 0x4a &&
                GanadoVoiceCue(13) == 0x4c,
              "Original EM12 type0 death/head/catch/breath/body/wake/release cues");
    require(RndState() == random, "Feedback pure functions never consume combat RNG");
    rejects([&]{ GanadoVoiceCue(19); });
    for(unsigned slot=0;slot<19;++slot)
      require(GanadoVoiceCue(slot,11)==GanadoVoiceCue(slot,1),
          "Original EM15 model type11 selects the EM12 type1 voice table");
    rejects([&]{GanadoVoiceCue(19,11);});
    rejects([&]{GanadoVoiceCue(0,12);});
    rejects([&]{ GanadoBloodSequence(2,false,false,-1); });
    rejects([&]{ GanadoBloodSequence(2,false,false,std::numeric_limits<float>::infinity()); });
    auto invalid = y; invalid.radius = -1;
    rejects([&]{ GanadoDamagePose(invalid,{},identity,{0,0,0}); });
    rejects([&]{ GanadoDamagePose(y,{},identity,{NAN,0,0}); });
    stripGeometry();
    std::cout << "Ganado original feedback checks: " << checks << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
