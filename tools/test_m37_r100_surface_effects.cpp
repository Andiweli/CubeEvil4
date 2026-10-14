#include "archive.h"
#include "r100_room_hit_effects.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace source {
using u32=std::uint32_t;using f32=float;
struct Vec {float x{},y{},z{};};
constexpr unsigned EAT_ATTR_EFF_BIT0=0x800000,EAT_ATTR_EFF_BIT1=0x8000,EAT_ATTR_EFF_BIT2=0x80;
struct Log {void err(int,int,const char *,...){}};Log log;Log *pLog=&log;
struct AtEffInfo {
  unsigned flag{},eff0[2]{},eff13[2]{},eff16[2]{},eff17[2]{},effGun[2]{},eff5[2]{},eff6[2]{},eff0D[2]{};
  int getWepEff(int,u32*,u32*);
};
std::uint16_t random;
unsigned Rnd() {
  const auto old=random;const unsigned next=(unsigned(std::uint8_t((old>>1)+(old>>8)))<<8)|std::uint8_t(old>>1);
  random=std::uint16_t(next==old?next+0x101:next);return random>>8;
}
float fRand0_1() {
  const unsigned a=Rnd(),b=Rnd(),c=Rnd();const std::uint32_t bits=a+(b<<8)+((c&127)<<16)+0x3f800000;
  float f;std::memcpy(&f,&bits,4);return f-1;
}
float sinf(float x){return re4::motion64::sinf(x);}
float cosf(float x){return re4::motion64::cosf(x);}
struct cEspSeqTbl {Vec Vec0,Pos;};
struct ESP05_WK {float Pow{},Spd{},Theta{};};
struct cEsp05 {
  Vec m_Pos;ESP05_WK m_Free;bool dead{};
  bool CommonMove(){return true;}bool AnmMove(){return true;}
  void move();int SetFreeWork(cEspSeqTbl*,u32*);
};
void PushEsp(cEsp05 *p){p->dead=true;}
struct Parent {float mat[3][4]{};};Parent world;Parent *pEffParentWorld=&world;
struct ESP40_WK {Vec Base_Pos;float Ofs_y{},Pos_y{},Speed_y{},Speed_plus_y{};};
struct cEsp40 {
  Parent *parent=pEffParentWorld;unsigned m_Release_time{},m_Life_time{};
  Vec m_Pos,m_Speed,m_Speed_plus;ESP40_WK m_Free;
  void ApplyMatrix(float[3][4]){} // The actual FF profile never enters the parent branch.
  int SetFreeWork(cEspSeqTbl*,u32*);
};
int GetWaterHeight(Vec *,float *height){*height=100;return 1;}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "r100_surface_source.inc"
#pragma GCC diagnostic pop
} // namespace source
using namespace re4;
namespace {
std::uint64_t checks{},flutter{},quads{},water{};
void require(bool v,const char *why){++checks;if(!v)throw std::runtime_error(why);}
unsigned word(const WaterRecord &r,unsigned p){return unsigned(r.bytes[p])<<24|unsigned(r.bytes[p+1])<<16|unsigned(r.bytes[p+2])<<8|r.bytes[p+3];}
float real(const WaterRecord &r,unsigned p){const auto bits=word(r,p);float f;std::memcpy(&f,&bits,4);return f;}
std::vector<std::uint8_t> entry(const RoomArchive &room,const char *name){const auto *e=room.find(name);require(e,"Actual room resource missing");return room.copy(e-room.entries().data());}
void lookup() {
  for(unsigned type:{2u,4u,5u}) {
    const auto actual=OriginalR100AtEffectInfo(type);require(bool(actual),"Source r100 registered table exists");
    const auto oracle=type==2?source::r100_eff_info2:type==4?source::r100_eff_info4:source::r100_eff_info5;
    require(actual->flags==oracle.flag,"Registered flag identical to full original table");
    const unsigned *pairs[]={oracle.eff0,oracle.eff13,oracle.eff16,oracle.eff17,oracle.effGun,oracle.eff5,oracle.eff6,oracle.eff0D};
    for(unsigned k=0;k<8;++k)require(actual->effects[k]==R100SurfaceEffect{pairs[k][0],pairs[k][1]},"All8 source pairs retained, including non-gun pairs");
    for(unsigned weapon=0;weapon<256;++weapon) {
      auto original=oracle;unsigned owner=0xd2,id=0;const bool selected=original.getWepEff(int(weapon),&owner,&id);
      const auto native=OriginalR100WeaponSurfaceEffect(*actual,weapon);
      require(bool(native)==selected,"Weapon selection agrees with verbatim source body for all256 IDs");
      if(native)require(*native==R100SurfaceEffect{owner,id},"Original per-weapon owner/EST pair");
    }
  }
  for(unsigned bits=0;bits<65536;++bits) {
    const unsigned attr=bits*0x10001u;const auto p=OriginalR100SurfaceImpact(attr,{.3f,.4f,.5f},2);
    require(p.effectType==unsigned(source::EatGetEffectType(attr)),"Verbatim EatGetEffectType over65536 attribute words");
  }
  for(unsigned weapon:{1u,2u,7u}) {
    const auto wet=OriginalR100SurfaceImpact(0x8000,{0,1,0},weapon);
    require(wet.effects==std::vector<R100SurfaceEffect>{{1,0x20}} && wet.sound==0xb && wet.rotation==std::array<float,3>{},"Ordinary water selects fifth pair ROOM20 and vertical normal orientation");
    const auto solid=OriginalR100SurfaceImpact(0x80,{0,0,1},weapon);
    require(solid.effects==std::vector<R100SurfaceEffect>{{1,0xe}} && !solid.sound,"Ordinary ROOM0 selects fifth pair ROOM0E, no synthetic hit sound");
    const auto forbidden=OriginalR100SurfaceImpact(0x800080,{0,0,1},weapon);
    require(forbidden.effects.empty() && forbidden.flags==2 && !forbidden.sound,"Original ROOM1 EFF_NONE remains quiet");
  }
  require(OriginalR100SurfaceImpact(0,{0,.9f,0},2,true).effects[0].sequence==0x1f,"Puddle normal boundary is strict >.9");
  const auto puddle=OriginalR100SurfaceImpact(0,{0,1,0},2,true);
  require(puddle.effects[0].sequence==0x11 && puddle.sound==0xc,"Actual FLR puddle switches CORE11/Snd2C");
}
void actual(const WorldEffectsData &data) {
  require(data.sprites.targets.size()==5 && data.triggerBanks.size()==5,"All ordinary R100 source banks");
  const std::array<unsigned,5> expected{9,1,9,7,10};unsigned count=0;
  for(unsigned t=0;t<5;++t) {
    require(data.sprites.targets[t].records.size()==expected[t],"Actual immutable record count");
    for(const auto &r:data.sprites.targets[t].records) {require(r.enabled,"Never omitted original record");++count;}
  }
  require(count==36,"36 original ordinary-impact records, no synthetic layers");
  R100RoomHitEffects impacts(data);WorldEffectCamera camera;camera.eye={100,3000,12000};
  SatHit hit;hit.point={100,200,300};hit.normal={0,0,1};hit.attributes=0x80;
  require(impacts.trigger(hit,2,1),"Actual ROOM0 gun activates");require(!impacts.trigger(hit,2,1),"Duplicate ray serial never restarts");
  const auto initial=impacts.state();source::random=initial.random;
  std::map<std::uint64_t,WaterSprite> previous;
  for(unsigned frame=0;frame<300;++frame) {
    // The initial three source flutter generator bursts call SetFreeWork
    // before any move, in their original record order.
    std::vector<source::ESP05_WK> births;
    if(frame==0)for(const auto &r:data.sprites.targets[4].records)if(r.bytes[264]) {
      if(r.bytes[296])source::Rnd(); // Source next-time randomization precedes each burst.
      if(r.bytes[1]==5)for(unsigned count=0;count<=r.bytes[269];++count) {
        source::cEsp05 original;source::cEspSeqTbl table;table.Vec0={real(r,216),real(r,220),real(r,224)};
        std::uint32_t seed=0;original.SetFreeWork(&table,&seed);births.push_back(original.m_Free);
      }
    }
    impacts.tick(camera);const auto &state=impacts.state();unsigned born=0;
    for(const auto &s:state.sprites) {
      const auto &r=data.sprites.targets[s.target].records[s.record];if(r.bytes[1]!=5)continue;
      source::cEsp05 original;
      if(frame==0) {
        original.m_Free=births.at(born++);
        require(s.sineOffset[0]==original.m_Free.Pow && s.sineOffset[1]==original.m_Free.Spd,"Verbatim Esp05 SetFreeWork amplitude/speed and zero-spread global draws");
        // Initial generated position is already transformed and integrated;
        // this observation checks free work/phase rather than common physics.
        original.m_Pos={s.position[0]-original.m_Free.Pow*source::sinf(original.m_Free.Theta),
            s.position[1]-original.m_Free.Pow*.3f*source::cosf(original.m_Free.Theta*1.9f),
            s.position[2]-original.m_Free.Pow*source::cosf(original.m_Free.Theta*1.7f)};
      } else {
        const auto old=previous.find(s.serial);require(old!=previous.end(),"No unaudited later flutter emitter births");
        original.m_Free={old->second.sineOffset[0],old->second.sineOffset[1],old->second.sinePhase};
        original.m_Pos={old->second.position[0]+old->second.speed[0],old->second.position[1]+old->second.speed[1],old->second.position[2]+old->second.speed[2]};
      }
      original.move();
      if(frame)require(s.position==std::array<float,3>{original.m_Pos.x,original.m_Pos.y,original.m_Pos.z},"Every surviving flutter position agrees with original source move body");
      require(s.sinePhase==original.m_Free.Theta,"Every source flutter phase agrees exactly");++flutter;
      previous[s.serial]=s;
    }
    require(state.random==source::random,"Original global flutter RNG exact after each30Hz tick");
    for(const auto &q:WorldEffectQuads(data,state,camera)) {
      require(q.type==0 || q.type==5,"Authored flutter and plain sprites use shared source geometry");
      for(const auto &v:q.position)for(float f:v)require(std::isfinite(f),"All impact geometry finite");
      ++quads;
    }
  }
  for(unsigned frame=0;frame<100;++frame)impacts.tick(camera);
  require(impacts.state().sprites.empty(),"Actual source gun impact naturally expires, including authored slow mark fade");
  require(impacts.drainSounds().empty() && impacts.warnings().empty(),"Original ROOM0 has no fabricated sound or unsupported branch");
  // Actual detached Esp40 initialization at an authored source water plane.
  EnvironmentData env;env.room=0x100;WaterGrid grid;grid.generator=0x42;grid.width=grid.height=8;
  grid.size=1000;grid.position={0,100,0};grid.heightRate=1;env.grids.push_back(grid);
  hit.point={100,200,300};hit.normal={0,1,0};hit.attributes=0x8000;
  require(impacts.trigger(hit,7,2,&env),"Source water table ordinary shotgun selects ROOM20");
  for(const auto &s:impacts.state().sprites) {
    const auto &r=data.sprites.targets[s.target].records[s.record];source::cEsp40 original;
    original.m_Pos={s.position[0],200,s.position[2]};source::cEspSeqTbl table;
    table.Pos={real(r,12),real(r,16),real(r,20)};std::uint32_t seed=0;
    original.SetFreeWork(&table,&seed);
    require(s.position[1]==original.m_Pos.y,"Verbatim Esp40 detached SetFreeWork snaps actual direct records to source height+Pos.y");
  }
  impacts.tick(camera,&env);
  for(const auto &s:impacts.state().sprites) {
    const auto &r=data.sprites.targets[s.target].records[s.record];require(r.bytes[1]==64,"All actual ROOM20 records are Esp40");
    require(!s.parented,"Actual FF water splash remains detached");++water;
  }
  const auto cues=impacts.drainSounds();require(cues.size()==1 && cues[0].number==0xb,"Actual water impact sound2/B exactly once");
  for(unsigned i=0;i<200;++i)impacts.tick(camera,&env);
  require(impacts.state().sprites.empty(),"Detached Esp40 source ANM/color naturally expires");
  hit.point[1]=100;require(impacts.trigger(hit,2,3,&env),"Suppressed source underwater scenery hit accepted");
  require(impacts.drainSounds().empty() && impacts.state().sprites.empty(),"Original PlSetWepTarget suppresses at/below water plane");
  hit.point[1]=200;hit.attributes=0x80;require(impacts.trigger(hit,0x13,4),"Legitimate unported explosive is routed without crash");
  const auto missing=impacts.drainUnsupported();require(missing.size()==1 && missing[0].effect==R100SurfaceEffect{1,0xf},"Explicit original explosive request retained");
  require(!impacts.warnings().empty(),"Unsupported source profile diagnosed rather than guessed");
}
} // namespace
int main(int argc,char **argv) {
  try {
    if(argc!=3)throw std::runtime_error("usage: r100.dat core.das");
    lookup();
    const auto room=RoomArchive::Load(argv[1]);const auto core=RoomArchive::LoadDrs(argv[2]);
    const auto effects=BuildR100RoomHitEffects(entry(room,"EFF"),entry(core,"EFF"));actual(effects);
    std::cout<<"PASS r100 ordinary surface effects: "<<checks<<" checks, "<<flutter<<" exact flutter observations, "<<quads<<" quads, "<<water<<" detached water sprites\n";
  } catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
