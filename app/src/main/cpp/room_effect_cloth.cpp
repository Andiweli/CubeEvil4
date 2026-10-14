#include "room_effect_cloth.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
float real(const WaterRecord &r,unsigned p) {
  const auto &b=r.bytes;std::uint32_t raw=std::uint32_t(b[p])<<24|std::uint32_t(b[p+1])<<16|std::uint32_t(b[p+2])<<8|b[p+3];
  float v;std::memcpy(&v,&raw,4);if(!std::isfinite(v))throw std::runtime_error("Cloth: Original parameters NaN/Inf");return v;
}
int integer(const WaterRecord &r,unsigned p) {
  const auto &b=r.bytes;return std::int32_t(std::uint32_t(b[p])<<24|std::uint32_t(b[p+1])<<16|std::uint32_t(b[p+2])<<8|b[p+3]);
}
unsigned half(const WaterRecord &r,unsigned p) {return unsigned(r.bytes[p])*256+r.bytes[p+1];}
float limit(float x) {
  while(x>=3.1415927f)x-=6.2831855f;
  while(x<-3.1415927f)x+=6.2831855f;
  return x;
}
float randomFloat(std::uint16_t &seed) {
  const auto byte=[&]() {
    const auto old=seed;const unsigned next=unsigned(std::uint8_t((old>>1)+(old>>8)))<<8|std::uint8_t(old>>1);
    seed=std::uint16_t(next==old?next+0x101:next);return unsigned(seed>>8);
  };
  const unsigned a=byte(),b=byte(),c=byte();const std::uint32_t raw=a+(b<<8)+((c&127)<<16)+0x3f800000;
  float value;std::memcpy(&value,&raw,4);return value-1;
}
bool commonMove(WaterSprite &s,const WaterRecord &r) {
  if(half(r,180)<=s.age)for(unsigned k=0;k<3;++k) {
    s.position[k]+=s.speed[k];s.speed[k]+=s.acceleration[k];s.speed[k]*=real(r,48);
  }
  if(half(r,182)<=s.age) {
    s.sizeMultiplier+=s.sizeStep;s.sizeStep*=real(r,152);if(s.sizeMultiplier<=0)return false;
  }
  for(unsigned k=0;k<3;++k)s.angle[k]+=s.angleSpeed[k];
  const auto fade=half(r,176),hold=half(r,178);
  if(fade<s.age) {
    if(fade+hold<=s.age) {
      for(unsigned k=0;k<4;++k)s.color[k]=std::min(255.f,s.color[k]*real(r,160+k*4));
      if(s.color[3]<4)return false;
    }
  }else if(fade) {
    const float rate=float(s.age)/float(fade);
    if(r.bytes[194]==3)for(unsigned k=0;k<3;++k)s.color[k]=float(r.bytes[156+k])*rate;
    s.color[3]=s.startAlpha*rate;
  }
  if(half(r,184) && half(r,184)<=s.age)return false;
  s.age=(s.age+1)&65535u;return true;
}
}
float OriginalClothSin(float angle) {
  const float x=limit(angle),square=x*x;
  constexpr float coefficients[]{1.0000012f,2.9073722e-06f,-0.16666685f,-5.727683e-06f,
    0.008331681f,4.281338e-06f,-0.00019622728f,-6.4099936e-07f,2.363633e-06f,4.0048576e-08f};
  float odd=1,even=1,sumOdd=0,sumEven=0;
  for(unsigned i=0;i<5;++i) {
    odd=x*even;even=square*even;
    sumOdd=std::fma(odd,coefficients[i*2],sumOdd);
    sumEven=std::fma(even,coefficients[i*2+1],sumEven);
  }
  return sumOdd+sumEven;
}
RoomEffectCloth::RoomEffectCloth(WaterSprite &s,const WaterRecord &r):
    target_(s.target),record_(s.record),animation_(r.animation),serial_(s.serial) {
  if(r.bytes[1]!=0x4e || s.parented || r.bytes[7]!=0xfe)
    throw std::runtime_error("Cloth: native backend requires the original world profile");
  state_.columns=unsigned(std::clamp(int(s.size[0]/200.f*36.f),2,100));
  state_.rows=unsigned(std::clamp(int(s.size[1]/200.f*24.f),2,50));
  state_.widthGap=real(r,216)*.1f+1.f;
  const float height=real(r,220)*.1f+1.f;
  if(state_.widthGap==0)state_.widthGap=.001f;
  state_.heightGap=(height==0?.001f:height)*(3000.f/60.857143f/23.f);
  state_.scale=60.857143f;state_.pinFourRows=(integer(r,8)&1)==0;state_.additive=r.bytes[194]!=0;
  motion64::Mtx mat;motion64::Vec angle{s.angle[0],s.angle[1],s.angle[2]},pos{s.position[0],s.position[1],s.position[2]};
  motion64::RotMatrix(mat,&angle);motion64::TransMatrix(mat,&pos);std::memcpy(state_.placement.data(),mat,sizeof mat);
  const auto count=state_.columns*state_.rows;state_.positions.resize(count);state_.normals.resize(count,{0,0,1});state_.speeds.resize(count);
  for(unsigned y=0;y<state_.rows;++y)for(unsigned x=0;x<state_.columns;++x) {
    auto &p=state_.positions[x+state_.columns*y];p={float(x)*state_.widthGap,-float(y)*state_.heightGap,0};
    if(state_.pinFourRows && y<=3)p[2]=std::sin(float(x%6)*6.2831855f/6.f)*-2.f;
  }
  for(unsigned i=0;i<120;++i) {calcSpeed(.9f);integrate();}
  const std::array<float,3> center{state_.widthGap*float(state_.columns)*.5f*state_.scale,0,0};
  for(unsigned k=0;k<3;++k) {
    state_.center[k]=state_.placement[k*4+3];
    for(unsigned j=0;j<3;++j)state_.center[k]+=state_.placement[k*4+j]*center[j];
  }
  timePlus_=std::int8_t(r.bytes[200]);power_=std::int8_t(r.bytes[201]);
  timePlus2_=std::int8_t(r.bytes[202]);power2_=std::int8_t(r.bytes[203]);
  range_=float(std::int32_t(std::uint32_t(integer(r,204))+1))*.5f;
  range2_=float(std::int32_t(std::uint32_t(integer(r,208))+1))*.5f;
  offset_=float(integer(r,212))*.025f;windTimePlus_=float(unsigned(r.bytes[252])+1)*.0025f;
  windRangePower_=float(unsigned(r.bytes[253])+1)*.07f;randomRatio_=float(unsigned(r.bytes[254])+1)*.2f;
}
void RoomEffectCloth::calcSpeed(float damping) {
  for(unsigned y=0;y<state_.rows;++y)for(unsigned x=0;x<state_.columns;++x) {
    if(!y && x && x!=state_.columns-1)continue;
    const unsigned k=x+y*state_.columns;auto &s=state_.speeds[k];
    const auto spring=[&](unsigned neighbour,float gap) {
      std::array<float,3> v{};for(unsigned j=0;j<3;++j)v[j]=state_.positions[neighbour][j]-state_.positions[k][j];
      const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
      if(length>gap) {
        const float factor=7.5f*(length-gap)/length;
        for(unsigned j=0;j<3;++j) {v[j]*=factor;s[j]=v[j]+s[j];}
      }
    };
    if(x)spring(k-1,state_.widthGap);
    if(x!=state_.columns-1)spring(k+1,state_.widthGap);
    if(y)spring(k-state_.columns,state_.heightGap);
    if(y!=state_.rows-1)spring(k+state_.columns,state_.heightGap);
    s[1]-=.5f;for(auto &value:s)value*=damping;
  }
}
void RoomEffectCloth::integrate() {
  for(unsigned y=0;y<state_.rows;++y) {
    if(state_.pinFourRows?y<=3:y==0)continue;
    for(unsigned x=0;x<state_.columns;++x)for(unsigned j=0;j<3;++j) {
      const unsigned k=x+y*state_.columns;const float v=state_.speeds[k][j]*.06f;
      state_.positions[k][j]+=v;
    }
  }
}
void RoomEffectCloth::calcNormal() {
  for(unsigned y=0;y<state_.rows;++y)for(unsigned x=0;x<state_.columns;++x) {
    const auto k=x+y*state_.columns;
    if(y==state_.rows-1)state_.normals[k]=state_.normals[k-state_.columns];
    else if(x==state_.columns-1)state_.normals[k]=state_.normals[k-1];
    else {
      motion64::Vec a{},b{},cross{},normal{};
      auto *av=&a.x,*bv=&b.x;
      for(unsigned j=0;j<3;++j) {av[j]=state_.positions[k+1][j]-state_.positions[k][j];bv[j]=state_.positions[k+state_.columns][j]-state_.positions[k][j];}
      motion64::PSVECCrossProduct(&b,&a,&cross);motion64::VECNormalize(&cross,&normal);
      state_.normals[k]={normal.x,normal.y,normal.z};
    }
  }
}
bool RoomEffectCloth::move(WaterSprite &s,const WaterRecord &r,std::uint16_t &random) {
  if(!state_.alive)return true;
  const auto position=s.position;
  if(!commonMove(s,r)) {state_.alive=false;return true;}
  s.position=position;
  motion64::Mtx rotation,inverse;motion64::Vec angle{s.angle[0],s.angle[1],s.angle[2]},speed{s.speed[0]*.1f,s.speed[1]*.1f,s.speed[2]*.1f};
  motion64::RotMatrix(rotation,&angle);motion64::PSMTXInverse(rotation,inverse);motion64::PSMTXMultVec(inverse,&speed,&speed);
  for(unsigned i=0;i<4;++i)state_.color[i]=std::uint8_t(s.color[i]);
  calcSpeed(.98f);integrate();calcNormal();
  state_.windTime=limit(state_.windTime+windTimePlus_);
  const float sin=OriginalClothSin(state_.windTime);
  const float strength=windRangePower_*sin*OriginalClothSin(state_.windTime*.3f)+1.f;
  state_.time=float(timePlus_)*.01f+state_.time;state_.time2=float(timePlus2_)*.01f+state_.time2;
  const float stepY=range_/float(state_.rows),stepX=range2_/float(state_.columns);
  const float wx=float(power_)*.025f,wy=float(power2_)*.025f;
  speed.x*=strength;speed.y*=strength;speed.z*=strength;
  float ay=state_.time;state_.time=limit(state_.time);state_.time2=limit(state_.time2);
  for(unsigned y=0;y<state_.rows;++y) {
    const float base=OriginalClothSin(ay)*wx;ay+=stepY*randomRatio_*randomFloat(random)+stepY;
    float ax=state_.time2;
    for(unsigned x=0;x<state_.columns;++x) {
      const float v=OriginalClothSin(ax)*wy;ax+=stepX*randomRatio_*randomFloat(random)+stepX;
      const float power=(base+v)*strength+offset_*strength;auto &velocity=state_.speeds[x+y*state_.columns];
      velocity[2]+=power;velocity[1]+=power*.5f;
      velocity[0]+=speed.x;velocity[1]+=speed.y;velocity[2]+=speed.z;
    }
  }
  return false;
}
RoomEffectClothMesh RoomEffectCloth::mesh() const {
  RoomEffectClothMesh mesh;mesh.target=target_;mesh.record=record_;mesh.animation=animation_;mesh.serial=serial_;
  mesh.color=state_.color;mesh.additive=state_.additive;mesh.lightCenter=state_.center;
  if(!state_.alive)return mesh;
  motion64::Mtx placement;std::memcpy(placement,state_.placement.data(),sizeof placement);
  motion64::Mtx scaled,inverse,normalMatrix;
  motion64::PSMTXScale(scaled,state_.scale,state_.scale,state_.scale);
  motion64::PSMTXConcat(placement,scaled,scaled);
  motion64::PSMTXInverse(scaled,inverse);motion64::PSMTXTranspose(inverse,normalMatrix);
  const auto vertex=[&](unsigned x,unsigned y) {
    const auto k=x+y*state_.columns;RoomEffectClothVertex v;
    motion64::Vec local{state_.positions[k][0],state_.positions[k][1],state_.positions[k][2]},world;
    motion64::PSMTXMultVec(scaled,&local,&world);v.position={world.x,world.y,world.z};
    local={state_.normals[k][0],state_.normals[k][1],state_.normals[k][2]};
    motion64::PSMTXMultVecSR(normalMatrix,&local,&world);v.normal={world.x,world.y,world.z};
    v.uv={float(x)/float(state_.columns-1),float(y)/float(state_.rows-1)};return v;
  };
  for(unsigned y=0;y<state_.rows-1;++y)for(unsigned x=0;x<state_.columns-1;++x) {
    const auto a=vertex(x,y),b=vertex(x,y+1),c=vertex(x+1,y),d=vertex(x+1,y+1);
    mesh.vertices.insert(mesh.vertices.end(),{a,b,c,c,b,d});
  }
  return mesh;
}
} // namespace re4
