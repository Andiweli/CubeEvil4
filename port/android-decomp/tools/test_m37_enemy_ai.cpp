#include "enemy_awareness.h"
#include "enemy_routes.h"
#include "enemy_encounter.h"
#include "archive.h"
#include "asset_reader.h"
#include "ports/rnd.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace SourceOracle {
using f32=float;using u32=std::uint32_t;using s8=std::int8_t;
struct Vec {float x{},y{},z{};};
struct RTP {u32 ID;std::uint16_t mapped,nPoint,nLine,nNext;u32 pPoint,pLine,pNext;};
struct RTP_POINT {Vec pos;std::uint16_t offLine,nLine;};
struct GLOBAL_WK {void *Rtp{};unsigned debug_mode{};}Global;
GLOBAL_WK *pG=&Global;
struct cEm {Vec pos;unsigned id{};struct{int m_flag=0x300;}atari;int RckMy{},RckTo{},RckNear{},RckStat{};};
re4::CollisionVec vec(const Vec &p){return {p.x,p.y,p.z};}
Vec vec(const re4::CollisionVec &p){return {float(p[0]),float(p[1]),float(p[2])};}
struct SatShim {
 const re4::SatCollision *sat{};
 int hitCheck(Vec *a,Vec *b,void*,void*,unsigned attr,unsigned mask) {
  return bool(sat->line(vec(*a),vec(*b),re4::SatSubset::All,attr,mask));
 }
 float getFloor(Vec *p,void*,float above,float below,int) {
  const auto hit=sat->ground(vec(*p),above,below);return hit?float(hit->point[1]):-1e6f;
 }
}SatMgr;
void Draw_line3d(Vec*,Vec*,unsigned,int){}
void PosToPos(Vec *a,Vec *b,Vec *out,float t){out->x=a->x*(1-t)+b->x*t;out->y=a->y*(1-t)+b->y*t;out->z=a->z*(1-t)+b->z*t;}
void PSVECSubtract(Vec *a,Vec *b,Vec *out){out->x=a->x-b->x;out->y=a->y-b->y;out->z=a->z-b->z;}
float PSVECMag(Vec *p){return std::sqrt(p->x*p->x+p->y*p->y+p->z*p->z);}
float VEC_DISTXZ(Vec *a,Vec *b){const float x=a->x-b->x,z=a->z-b->z;return std::sqrt(x*x+z*z);}
RTP *rtpData(){return static_cast<RTP*>(pG->Rtp);}
RTP_POINT *rtpPoint(RTP *r){return reinterpret_cast<RTP_POINT*>(std::uintptr_t(r)+r->pPoint);}
#include "../tests/source_enemy_routes_oracle.inc"
std::vector<std::uint8_t> nativeRtp(const std::vector<std::uint8_t> &bytes) {
 re4::AssetReader r{bytes};std::vector<std::uint8_t> out(bytes.size());
 RTP header{r.u32(0),r.u16(4),r.u16(6),r.u16(8),r.u16(10),r.u32(12),r.u32(16),r.u32(20)};
 std::memcpy(out.data(),&header,sizeof(header));
 for(unsigned i=0;i<header.nPoint;++i){const auto p=header.pPoint+i*16;RTP_POINT point{{r.f32(p),r.f32(p+4),r.f32(p+8)},r.u16(p+12),r.u16(p+14)};std::memcpy(out.data()+p,&point,16);}
 std::copy_n(bytes.begin()+header.pNext,header.nNext,out.begin()+header.pNext);return out;
}
}
namespace {
unsigned checks;
void require(bool v,const char*s){++checks;if(!v)throw std::runtime_error(s);}
std::vector<std::uint8_t> sub(const re4::RoomArchive&a,const char*tag){const auto*e=a.find(tag);require(e,"authored room subfile");return a.copy(e-a.entries().data());}
std::vector<std::uint8_t> read(const char*p){std::ifstream f(p,std::ios::binary|std::ios::ate);require(bool(f),"authored ESL");std::vector<std::uint8_t>b(std::size_t(f.tellg()));f.seekg(0);require(bool(f.read(reinterpret_cast<char*>(b.data()),b.size())),"read ESL");return b;}
void awareness() {
 using namespace re4;GanadoAwarenessInput a;a.lineVisible=true;a.routeDistance=30000;a.viewAngle=0;
 for(float d:{0.f,3499.f,3500.f,4999.f,5000.f,14999.f,15000.f,20000.f})
  for(float angle:{0.f,1.0471975f,1.0471976f,3.1415927f})for(bool foot:{false,true}) {
   a.distanceSquared=d*d;a.viewAngle=angle;a.playerFootstep=foot;
   require(OriginalGanadoFind(a)==((a.distanceSquared<225000000.f && angle<1.0471976f)||
       (foot&&a.distanceSquared<25000000.f)||a.distanceSquared<12250000.f),"literal sight/near/foot boundaries");
  }
 a={};a.routeDistance=24999;a.playerFired=true;require(OriginalGanadoFind(a),"source gunfire hears through occlusion");
 a.routeDistance=25000;require(!OriginalGanadoFind(a),"source fire distance must be strict");
 a.routeDistance=1;a.flags=0x10;require(!OriginalGanadoFind(a),"source deaf flag ignored");
 a={};a.burst=true;a.lineVisible=true;a.routeDistance=24999;a.burstDistanceSquared=624999936.f;
 require(OriginalGanadoFind(a),"original visible nearby burst");a.lineVisible=false;require(!OriginalGanadoFind(a),"source burst requires sight flag");
 a={};a.somebodyDamaged=true;require(OriginalGanadoFind(a),"source nearby damaged peer");
 a={};a.damaged=true;require(OriginalGanadoFind(a),"source damage awareness");
 a={};a.flags=0x80;require(OriginalGanadoFind(a),"source forced awareness");a.found=true;require(!OriginalGanadoFind(a),"found actor discovery repeats");
 a={};a.lineVisible=true;a.distanceSquared=1;a.heightDelta=2001;
 require(OriginalGanadoFind(a,0)&&!OriginalGanadoFind(a,1),"source mode0 retains vertical sight while mode1 gates2000");
 a.type=10;require(!OriginalGanadoFind(a,2),"source type10 special mode2");
 GanadoPeerState peer;peer.hp=500;peer.routine=2;
 for(float z:{-2999.f,-3000.f,9999.f,10000.f}) {
  peer.position={0,0,z};const auto state=OriginalGanadoPeers({0,0,0},0,{peer});
  require(state.somebodyDamaged==(std::abs(z)<3000 || (z>0 && z<10000)),"source peer damage 3000/10000 strict range");
 }
 peer.position={0,0,1};peer.hp=0;peer.routine=1;require(!OriginalGanadoPeers({0,0,0},0,{peer}).somebodyDamaged,"scripted cliff HP0 fabricated damaged peer");
 peer.routine=3;peer.subroutine=2;require(OriginalGanadoPeers({0,0,0},0,{peer}).somebodyDamaged,"active Die_Normal peer ignored");
 peer.subroutine=3;require(!OriginalGanadoPeers({0,0,0},0,{peer}).somebodyDamaged,"source Die_Lost exclusion ignored");
 peer.hp=500;peer.routine=1;peer.subroutine=0xd;peer.position={100000,0,0};
 require(OriginalGanadoPeers({0,0,0},0,{peer}).otherFinding,"source shared find shout gate distance invented");
 peer.active=false;require(!OriginalGanadoPeers({0,0,0},0,{peer}).otherFinding,"inactive peer blocked source find voice");
}
}
int main(int argc,char**argv){try{using namespace re4;
 require(argc==6,"Usage: m37_enemy_ai_check R100.DAT ESL EM12.DRS PL00.DRS WEP01.DRS");
 awareness();const auto room=RoomArchive::Load(argv[1]);const Sat raw(sub(room,"SAT")),eat(sub(room,"EAT"));
 const SatCollision sat(raw),walls(eat);const auto bytes=sub(room,"RTP");
 const auto routes=std::make_shared<EnemyRoutes>(bytes,sat);require(routes->points().size()==66,"actual original r100 66 route points");
 auto oracle=SourceOracle::nativeRtp(bytes);SourceOracle::Global.Rtp=oracle.data();SourceOracle::SatMgr.sat=&sat;
 for(unsigned i=0;i<routes->points().size();++i) {
  for(const auto offset:std::array<CollisionVec,3>{{{0,0,0},{217,37,-499},{-300,500,117}}}) {
   auto p=routes->points()[i];for(unsigned k=0;k<3;++k)p[k]+=offset[k];auto original=SourceOracle::vec(p);
   for(unsigned mode:{0u,1u})for(unsigned attr:{0u,0x300u})
    require(routes->nearest(p,mode,attr)==SourceOracle::getNearPoint(&original,int(mode),int(attr)),"original nearest-ten tie/visibility differs");
  }
  for(unsigned j=0;j<routes->points().size();++j) {
   require(routes->pointDistance(i,j)==SourceOracle::RouteCkGetDist(int(i),int(j)),"original next-hop 3D distance differs");
   auto a=SourceOracle::vec(routes->points()[i]),b=SourceOracle::vec(routes->points()[j]);
   require(routes->distance(routes->points()[i],routes->points()[j])==SourceOracle::RouteCkPosToPosDis(&a,&b),"original routed/straight distance differs");
   for(unsigned mode:{0u,1u}) {
    SourceOracle::cEm enemy;enemy.pos=a;SourceOracle::Vec dest;float maximum=0;
    const bool direct=SourceOracle::RouteCkToPos(&enemy,&b,&dest,int(mode),&maximum)!=0;
    const auto actual=routes->step(routes->points()[i],routes->points()[j],mode);
    require(actual.direct==direct && actual.destination==SourceOracle::vec(dest) && actual.maxHeight==maximum,"original route step/max height differs");
   }
  }
 }
 EnemyRoutes none({},sat);require(none.nearest({0,0,0})==-1 && none.step({0,0,0},{1,2,3}).direct,"source no-route fallback");
 auto corrupt=bytes;corrupt[0]=0;bool rejected=false;try{EnemyRoutes bad(corrupt,sat);}catch(const std::runtime_error&){rejected=true;}require(rejected,"malformed route accepted");
 const EnemySpawns esl(read(argv[2]));LeonPreview player(argv[4],argv[5],argv[3],true);WalkProbe probe(sat);
 RndInit(0x1391);
 for(unsigned i:{3u,4u,5u,0x12u,0x13u,0x26u,0x27u}) {
  const auto spawn=*esl.find(i);EnemyEncounter enemy(sat,walls,spawn,argv[3],true);enemy.setRoutes(routes);enemy.releaseR100Ambush();
  require(probe.place(enemy.feet()),"actual authored ambush floor");player.reset();player.setPlacement(probe.position(),enemy.heading());
  for(unsigned tick=0;tick<160&&!enemy.foundPlayer();++tick)enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});
  if(!enemy.foundPlayer())std::cerr<<"Unfound ESL "<<std::hex<<i<<std::dec<<" "<<enemy.status()
      <<" enemy "<<enemy.feet()[0]<<","<<enemy.feet()[1]<<","<<enemy.feet()[2]
      <<" Leon "<<player.feet()[0]<<","<<player.feet()[1]<<","<<player.feet()[2]<<'\n';
  require(enemy.foundPlayer(),"actual original ambush never discovers nearby Leon");
  const auto gait=enemy.actor().motionSource(EnemyMotion::Walk);
  require(gait.motionEntry==(enemy.actor().weaponType()==1?327u:212u) &&
      gait.sequenceEntry==(enemy.actor().weaponType()==1?328u:213u)+i%5,"original found armed gait/ESL variant");
  if(enemy.actor().weaponType()==1) {
   require(enemy.actor().motionSource(EnemyMotion::HitBody).motionEntry==341 &&
       enemy.actor().motionSource(EnemyMotion::BodyAlt).sequenceEntry==342 &&
       enemy.actor().motionSource(EnemyMotion::BodyRear).motionEntry==343 &&
       enemy.actor().motionSource(EnemyMotion::BodyRear).sequenceEntry==344,"original held-hoe front/rear flinch ARC159/15B");
  }
  require(RndState()==0x1391,"live detection polluted caller RNG");
 }
 EnemyEncounter keeper(sat,walls,*esl.find(3),argv[3],true);keeper.setRoutes(routes);
 bool placed=false;for(auto it=routes->points().rbegin();it!=routes->points().rend();++it) {
  if(std::hypot((*it)[0]-keeper.feet()[0],(*it)[2]-keeper.feet()[2])<60000)continue;
  if(probe.place(*it)){placed=true;break;}
 }
 require(placed,"actual far r100 keeper fade floor");player.reset();player.setPlacement(probe.position(),0);
 for(unsigned tick=0;tick<20;++tick)keeper.stepPlayer(player,probe,0,0,false,false,1./30,{});
 require(!keeper.foundPlayer() && !keeper.visible() && !keeper.sourceCollisionActive() && !keeper.actor().opacity(),"source far Keeper fade/atari remained active");
 require(probe.place(keeper.feet()),"actual close Keeper restore floor");player.setPlacement(probe.position(),keeper.heading());
 keeper.stepPlayer(player,probe,0,0,false,false,1./30,{});
 require(keeper.foundPlayer() && keeper.visible() && keeper.sourceCollisionActive() && keeper.actor().opacity()==1,"source Keeper failed to restore visibility/collision");
 std::cout<<"M37 original Ganado awareness/r100 RTP/gait: "<<checks<<" checks PASS; unchanged FindCk,66-node actual graph and seven live ambush profiles.\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
