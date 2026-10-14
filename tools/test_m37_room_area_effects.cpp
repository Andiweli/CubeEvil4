#include "room_area_effects.h"
#include "archive.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace re4;
namespace {
std::uint64_t checks{},quads{},followers{},weather{};
void require(bool v,const char *why){++checks;if(!v)throw std::runtime_error(why);}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *name){const auto e=a.find(name);require(e,"Actual source archive entry missing");return a.copy(e-a.entries().data());}
float real(const std::array<std::uint8_t,300> &b,unsigned p){const std::uint32_t u=std::uint32_t(b[p])<<24|std::uint32_t(b[p+1])<<16|std::uint32_t(b[p+2])<<8|b[p+3];float x;std::memcpy(&x,&u,4);return x;}
float areaFloat(const RoomEffectArea &a,unsigned p){std::array<std::uint8_t,300>b{};std::copy(a.source.begin(),a.source.end(),b.begin());return real(b,p);}
unsigned half(const std::vector<std::uint8_t>&b,unsigned p){return unsigned(b[p])*256+b[p+1];}
unsigned word(const std::vector<std::uint8_t>&b,unsigned p){return unsigned(b[p])<<24|unsigned(b[p+1])<<16|unsigned(b[p+2])<<8|b[p+3];}
unsigned rnd(std::uint16_t &seed){const auto old=seed;const unsigned n=unsigned(std::uint8_t((old>>1)+(old>>8)))<<8|std::uint8_t(old>>1);seed=std::uint16_t(n==old?n+0x101:n);return seed>>8;}
bool sourceArea(const RoomEffectArea &a,const std::array<float,3> &p){
  if(a.source[1]==3)return false;
  if(p[1]+100<areaFloat(a,4) || p[1]>=areaFloat(a,4)+areaFloat(a,8))return false;
  if(a.source[1]==2){const float x=p[0]-areaFloat(a,16),z=p[2]-areaFloat(a,20);return std::sqrt(x*x+z*z)<areaFloat(a,12);}
  float x[4],z[4];for(unsigned i=0;i<4;++i){x[i]=areaFloat(a,16+8*i);z[i]=areaFloat(a,20+8*i);}
  if((x[3]-x[0])*(p[2]-z[0])>(z[3]-z[0])*(p[0]-x[0]) || (x[1]-x[0])*(p[2]-z[0])<(z[1]-z[0])*(p[0]-x[0]))return false;
  if((x[3]-x[2])*(p[2]-z[2])<(z[3]-z[2])*(p[0]-x[2]) || (x[1]-x[2])*(p[2]-z[2])>(z[1]-z[2])*(p[0]-x[2]))return false;
  return true;
}
void inventory(const RoomArchive &room,const RoomAreaEffectsData &d){
  const auto raw=entry(room,"EFF");const auto ids=word(raw,12),ofs=word(raw,36);unsigned count=0;std::map<unsigned,unsigned> types;
  require(d.banks.size()==17 && d.areas.size()==14 && d.warnings.empty(),"Every actual r100 SST/EAR is active without fallback");
  for(unsigned t=0;t<d.banks.size();++t){const auto head=ofs+word(raw,ofs+4+t*4);const auto &bank=d.banks[t];
    require(bank.id==half(raw,ids+4+t*8) && bank.area==half(raw,ids+6+t*8) && bank.target==t,"Original SST table key/order retained");
    const auto &records=d.effects.sprites.targets[t].records;require(records.size()==half(raw,head),"No actual authored record omitted");
    for(unsigned r=0;r<records.size();++r){require(records[r].enabled && std::equal(records[r].bytes.begin(),records[r].bytes.end(),raw.begin()+head+48+r*300),"Every source record remains byte-identical and enabled");++count;++types[records[r].bytes[1]];}}
  require(count==236 && types==std::map<unsigned,unsigned>{{0,113},{4,1},{6,4},{8,8},{10,3},{11,3},{21,1},{70,1},{72,99},{78,3}},"Exact original r100 source profile inventory");
  for(const auto &a:d.areas){float x=0,z=0;for(unsigned i=0;i<4;++i){x+=areaFloat(a,16+i*8);z+=areaFloat(a,20+i*8);}x/=4;z/=4;
    for(int i=-9;i<=9;++i)for(int j=-9;j<=9;++j)for(float y:{areaFloat(a,4)-100.01f,areaFloat(a,4)-100,areaFloat(a,4),areaFloat(a,4)+areaFloat(a,8)-.01f,areaFloat(a,4)+areaFloat(a,8)}){
      std::array<float,3>p{x+float(i)*750,y,z+float(j)*750};require(OriginalEffectAreaContains(a,p)==sourceArea(a,p),"Original EAR cross-product and height boundary oracle");}}
}
void displayLifecycle(const RoomAreaEffectsData &d){WorldEffectCamera camera;const std::array<float,3>outside{2000000,2000000,2000000};
  for(auto pair:{std::pair<unsigned,unsigned>{17,6},{18,4}}){const auto bit=1u<<pair.first,area=1u<<pair.second;RoomAreaEffects s(d,0,&camera);
    s.tick(outside,camera,0,area);for(unsigned n=0;n<70;++n)s.tick(outside,camera,0,area);require(s.state().sprites.empty(),"Disabled source SST never starts on area entry");
    s.tick(outside,camera,bit,area);for(unsigned n=0;n<70;++n)s.tick(outside,camera,bit,area);require(!s.state().sprites.empty(),"Original SetSstDispFlag-on starts current area's authored bank");
    for(const auto &q:s.state().sprites)require(d.banks[q.target].id==pair.first,"Only exact source display-bit owner activates");
    s.tick(outside,camera,0,area);require(!s.state().sprites.empty(),"Source SetSstDispFlag-off does not delete existing effects");
    s.tick(outside,camera,0);require(s.state().sprites.empty(),"Leaving source area removes its particles and generators");
    s.tick(outside,camera,0,area);for(unsigned n=0;n<70;++n)s.tick(outside,camera,0,area);require(s.state().sprites.empty(),"Reentry respects cleared source display flag");
  }
}
void overlay(const RoomAreaEffectsData &d){WorldEffectCamera camera;RoomAreaEffects s(d,1,&camera);const std::array<float,3>outside{2000000,2000000,2000000};
  for(unsigned n=0;n<400;++n){const auto before=s.state();auto seed=before.random;const float x=float(rnd(seed)%200)-100,y=float(rnd(seed)%220)-110;s.tick(outside,camera,1);
    const auto &state=s.state();const auto sheet=std::find_if(state.sprites.begin(),state.sprites.end(),[&](const auto &q){return d.effects.sprites.targets[q.target].records[q.record].bytes[1]==4;});
    require(sheet!=state.sprites.end() && sheet->position[0]==x && sheet->position[1]==y && state.random==seed,"Esp04 consumes exactly two global Rnd calls and keeps source jitter center");
    unsigned tiles=0;for(const auto &q:s.quads(camera))if(q.quad.type==4){++tiles;++quads;require(q.quad.screen && !q.quad.tiled && !q.quad.flipS && !q.quad.flipT,"Esp04 source direct screen grid with no sprite flips");require(q.quad.position[1][0]-q.quad.position[0][0]==200 && q.quad.position[3][1]-q.quad.position[0][1]==200 && q.quad.position[0][2]==0,"Original Esp04 s16 tile dimensions and depth");require(q.quad.uv==std::array<std::array<float,2>,4>{{{0,0},{1,0},{1,1},{0,1}}},"Esp04 entire original texture on each tile");}require(tiles==16,"Original542/200+2 and448/200+2 grid counts");}
}
float seeded(std::uint32_t &seed,bool centered){seed=seed*0x19660du+0x3c6ef35fu;const std::uint32_t bits=(seed&0x7fffffu)|0x3f800000u;float value;std::memcpy(&value,&bits,4);return centered?value*2-2-1:value-1;}
void pathOracle(const RoomAreaEffectsData &d){
  WorldEffectCamera camera;const std::array<float,3>outside{2000000,2000000,2000000};RoomAreaEffects sim(d,0,&camera);
  const auto target=unsigned(std::find_if(d.banks.begin(),d.banks.end(),[](const auto &b){return b.id==4;})-d.banks.begin());
  const auto &records=d.effects.sprites.targets[target].records;const auto &path=d.effects.sprites.recordPaths[target][0];
  struct Source {std::array<float,3> base{};EnvironmentMatrix matrix{};float distance{},speed{};unsigned segment{},wait{},stop{};};
  std::vector<Source> source;auto random=sim.state().random;for(unsigned k=0;k<3;++k)(void)rnd(random); // EstSet seed
  for(unsigned r=0;r<4;++r){const auto &b=records[r].bytes;std::uint32_t seed=rnd(random);seed|=rnd(random)<<8;seed|=rnd(random)<<16;
    for(unsigned n=0;n<unsigned(b[269])+1;++n){Source s;for(unsigned k=0;k<3;++k)s.base[k]=real(b,12+k*4)+real(b,24+k*4)*seeded(seed,true);
      for(unsigned k=0;k<12;++k)(void)seeded(seed,true);
      (void)seeded(seed,true); // Speed/accel/angle/angularspeed + sharedSize
      const auto integer=[&](unsigned p){return std::int32_t(std::uint32_t(b[p])<<24|std::uint32_t(b[p+1])<<16|std::uint32_t(b[p+2])<<8|b[p+3]);};
      s.speed=float(integer(204))+float(integer(212))*seeded(seed,true);if(s.speed>0)s.speed=-s.speed;
      s.distance=(real(b,240)*.01f+real(b,244)*.01f*seeded(seed,false))*path.back()[3];s.stop=b[252];
      motion64::Mtx rotation,scaling;motion64::Vec angle{real(b,228)*.017453292f,real(b,232)*.017453292f,real(b,236)*.017453292f};
      motion64::RotMatrix(rotation,&angle);motion64::PSMTXScale(scaling,real(b,216)*.1f+1,real(b,220)*.1f+1,real(b,224)*.1f+1);motion64::PSMTXConcat(rotation,scaling,rotation);std::memcpy(s.matrix.data(),rotation,sizeof rotation);source.push_back(s);}}
  require(source.size()==44,"Actual four Esp06 records emit their exact44 source followers");
  for(unsigned frame=0;frame<1500;++frame){const auto before=sim.state();auto visual=before.random;if(!frame){for(unsigned k=0;k<3;++k)(void)rnd(visual);for(const auto &r:records)if(r.bytes[264] && !(r.bytes[267]&2))for(unsigned k=0;k<3;++k)(void)rnd(visual);}for(const auto &em:before.emitters)if(!em.next && records[em.record].bytes[296])(void)rnd(visual);sim.tick(outside,camera,1u<<4,1u<<9);unsigned index=0;
    for(const auto &actual:sim.state().sprites){if(records[actual.record].bytes[1]==11){for(unsigned k=0;k<6;++k)(void)rnd(visual);}if(records[actual.record].bytes[1]==6){auto &s=source.at(index++);const auto &b=records[actual.record].bytes;
      if(!s.wait)s.distance+=s.speed;else --s.wait;
      const bool loop=s.distance<0 || s.distance>=path.back()[3];if(loop){s.distance+=s.speed>0?-path.back()[3]:path.back()[3];s.wait=s.stop;s.stop=(s.stop+rnd(visual)%b[253])&255u;}
      int next=int(s.segment);const int step=s.distance>=path[s.segment][3]?1:-1;do{next+=step;}while(float(step)*path[unsigned(next)][3]<float(step)*s.distance);s.segment=unsigned(next-step);
      const auto &a=path[s.segment],&z=path[unsigned(next)];const float t=(s.distance-a[3])/(z[3]-a[3]);std::array<float,3>point{};for(unsigned k=0;k<3;++k)point[k]=(z[k]-a[k])*t+a[k];
      for(unsigned row=0;row<3;++row){const float expected=s.matrix[row*4]*point[0]+s.matrix[row*4+1]*point[1]+s.matrix[row*4+2]*point[2]+s.matrix[row*4+3]+s.base[row];if(actual.position[row]!=expected)std::cerr<<"path frame="<<frame<<" record="<<actual.record<<" index="<<index<<" axis="<<row<<" actual="<<actual.position[row]<<" expected="<<expected<<" distance="<<s.distance<<" speed="<<s.speed<<" base="<<s.base[row]<<"\n";require(actual.position[row]==expected,"Original Esp06 seed initialization, cached reverse path, matrix and accumulated loop waits agree exactly");}++followers;}}
    require(index==44,"Original finite generator creates persistent44 path followers exactly once");if(visual!=sim.state().random)std::cerr<<"source visual frame="<<frame<<" ref="<<visual<<" actual="<<sim.state().random<<"\n";require(visual==sim.state().random,"Original global RNG advances only through source ordered path waits and Esp0b camera offsets");}
}
void weatherOracle(const RoomAreaEffectsData &d){
  WorldEffectCamera camera;camera.eye={-80000,2500,-32000};camera.forward={.6f,0,-.8f};camera.right={.8f,0,.6f};
  RoomAreaEffects sim(d,1u<<7,&camera);const std::array<float,3>outside{2000000,2000000,2000000};
  require(sim.state().sprites.size()==60,"Original Esp15 burst initializes exactly60 weather particles");
  for(unsigned frame=0;frame<500;++frame){const auto before=sim.state();camera.eye[0]+=53;camera.eye[1]+=3;sim.tick(outside,camera,1u<<7);
    for(const auto &actual:sim.state().sprites){const auto old=std::find_if(before.sprites.begin(),before.sprites.end(),[&](const auto &s){return s.serial==actual.serial;});require(old!=before.sprites.end(),"Original weather particles persist without replacement");const auto &r=d.effects.sprites.targets[actual.target].records[actual.record];auto position=old->position;for(unsigned k=0;k<3;++k)position[k]+=old->speed[k];
      const float range=real(r.bytes,32),half=range*.6f;
      const auto wrap=[&](std::array<float,3>axis,bool depth){float length=0;for(float x:axis)length+=x*x;length=std::sqrt(length);for(float &x:axis)x/=length;float distance=0;for(unsigned k=0;k<3;++k)distance+=(position[k]-camera.eye[k])*axis[k];int n;if(depth)n=distance>=0?int(distance/range):-int(-distance/range)-1;else n=distance>=0?int((distance+half)/(half*2)):int((distance-half)/(half*2));if(n){const float offset=-(depth?range:half*2)*float(n);for(unsigned k=0;k<3;++k)position[k]+=axis[k]*offset;if(depth){distance=0;for(unsigned k=0;k<3;++k)distance+=(position[k]-camera.eye[k])*axis[k];}}return distance;};
      wrap(camera.right,false);wrap(camera.up,false);const float distance=wrap(camera.forward,true);float alpha=r.bytes[159];const float ratio=1-float(std::int8_t(r.bytes[202]))/100;
      if(distance>range*ratio)alpha*=1-(distance-range*ratio)/(range*(1-ratio));
      require(actual.position==position && actual.color[3]==alpha,"Original Esp15 moving-camera wrap and post-wrap depth alpha agree exactly");
      for(unsigned k=0;k<3;++k)require(actual.angle[k]==old->angle[k]+old->angleSpeed[k],"Original Esp15 native DEG2RAD angular update");
      ++weather;}
    require(before.random==sim.state().random,"Original weather move consumes no additional global randomness");}
}
void areaTraversal(const RoomAreaEffectsData &d){WorldEffectCamera camera;camera.eye={-80000,2500,-32000};const std::array<float,3>outside{2000000,2000000,2000000};RoomAreaEffects s(d,0xffffffff,&camera);
  require(s.clothMeshes().size()==3,"All three original r100 cloth sheets settle before first playable frame");
  for(unsigned area=0;area<10;++area){for(unsigned n=0;n<180;++n){camera.eye[0]+=2;s.tick(outside,camera,0xffffffff,1u<<area);const auto &state=s.state();require(state.sprites.size()<=512,"Original shared room sprite pool budget");
      for(const auto &q:state.sprites){const auto &r=d.effects.sprites.targets[q.target].records[q.record];if(r.bytes[1]==6){require(q.age && std::isfinite(q.position[0]) && std::isfinite(q.position[1]) && std::isfinite(q.position[2]),"All four authored Esp06 path followers remain finite across loop waits");++followers;}if(r.bytes[1]==21){require(q.color[3]>=0 && q.color[3]<=r.bytes[159],"Original camera weather alpha envelope");++weather;}if(q.motionTrailGhost)require(q.motionTrailLife==0,"Actual room motion copies preserve source infinite zero-life");}
      for(const auto &q:s.quads(camera)){++quads;require(q.quad.type!=78 && q.quad.type!=70,"Cloth and Filter03 never become fake sprite fallback");if(q.shimmerType)require(q.shimmerType==1 && q.shimmerPower==255 && q.quad.rgbScale==1 && q.quad.alphaScale==1 && q.maskAnimation.has_value(),"Actual Esp08 shimmer stage replacement and original mask");if(q.maskAnimation)require(q.maskImage<d.effects.sprites.animations[*q.maskAnimation].pattern.size(),"Original independent mask animation stays in its own ANM bank");}
      const auto cloth=s.clothMeshes();require(cloth.size()==3,"Room area transitions preserve three global original sheets");for(const auto &m:cloth){require(!m.vertices.empty() && m.lightSize==10000,"Original cloth mesh and authored temporary LightInfo size");for(const auto &v:m.vertices)for(float x:v.position)require(std::isfinite(x),"Original cloth source geometry stays finite");}}
    s.tick(outside,camera,0xffffffff);for(const auto &q:s.state().sprites)require(d.banks[q.target].area==65535,"Area exit preserves globals and deletes every original area owner");}
  require(followers && weather && quads,"Every new authored source path executed");
}
void pond(const RoomArchive &room,const std::vector<std::uint8_t>&core){const Smd smd(entry(room,"SMD"));const Smx smx(entry(room,"SMX"));const auto d=BuildRoomEnvironment(room,smd,smx,0x100,core);
  require(d.surfaces.size()==2 && d.targets.size()==1 && d.targets[0].sequence==0 && d.targets[0].width==64 && d.targets[0].height==64,"Original r100 pond has two source SMD surfaces and64x64 EST0 render target");
  for(unsigned i=0;i<2;++i){const auto &s=d.surfaces[i];require(s.work==i && s.target==0 && s.colorAlpha==240 && s.shader==2 && s.refractPower==30 && s.refractRatio==128 && !s.textureAlpha,"Exact r100::setTexRender pond material parameters");}
}
}
int main(int argc,char**argv){try{if(argc!=3)throw std::runtime_error("Usage: m37_room_area_effects_check R100_DAT CORE_DAS");const auto room=RoomArchive::Load(argv[1]);const auto core=entry(RoomArchive::LoadDrs(argv[2]),"EFF");const auto d=BuildRoomAreaEffects(room,0x100,core);inventory(room,d);displayLifecycle(d);overlay(d);pathOracle(d);weatherOracle(d);areaTraversal(d);pond(room,core);std::cout<<"M37 room area original effects PASS: "<<checks<<" checks;236 records,17 SST,14 EAR; quads="<<quads<<", followers="<<followers<<", weather="<<weather<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
