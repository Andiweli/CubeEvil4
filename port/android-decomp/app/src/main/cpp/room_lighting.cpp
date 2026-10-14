#include "room_lighting.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
struct Reader {
  const std::vector<unsigned char> &b;
  void range(std::size_t p, std::size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("LIT: truncated data range");
  }
  unsigned u16(std::size_t p) const { range(p, 2); return unsigned(b[p]) * 256 + b[p+1]; }
  std::uint32_t u32(std::size_t p) const { return (std::uint32_t(u16(p)) << 16) | u16(p+2); }
  float real(std::size_t p) const {
    const auto u = u32(p); float f;
    std::memcpy(&f, &u, 4);
    if (!std::isfinite(f) || std::abs(f) > 1e9f)
      throw std::runtime_error("LIT: invalid float at byte "+std::to_string(p));
    return f;
  }
  std::array<unsigned char, 4> color(std::size_t p) const {
    range(p, 4); return {b[p], b[p+1], b[p+2], b[p+3]};
  }
  std::array<float, 3> vector(std::size_t p) const { return {real(p), real(p+4), real(p+8)}; }
};
}
RoomLighting::RoomLighting(const std::vector<unsigned char> &bytes) {
  const Reader r{bytes}; r.range(0, 4);
  const auto n = r.u16(0), maxLights = unsigned(bytes[3]);
  sourceVersion = bytes[2];
  // Supplied versions 0x27..0x2c retain the 0x104 / 0x12c layouts.
  // Apply rendered cLit::versionUp fields below without relocating pointers.
  if (sourceVersion < 0x27 || sourceVersion > 0x2c || !n || n > 256 ||
      maxLights > 128 || bytes.size() > 16*1024*1024)
    throw std::runtime_error("LIT: Version or table budget");
  r.range(4, std::size_t(n)*4);
  std::vector<std::size_t> offsets;
  for (unsigned i=0; i<n; ++i) {
    const auto p = r.u32(4+i*4);
    if (!p) { offsets.push_back(0); continue; }
    if (p < 4+n*4 || p%4) throw std::runtime_error("LIT: invalid cut offset");
    r.range(p, 260); offsets.push_back(p);
  }
  for (const auto p : offsets) {
    present.push_back(p != 0);
    if (!p) { cuts.emplace_back(); continue; }
    const auto declared=r.u32(p+4);
    if(declared<=128) maximumLights=std::max(maximumLights,declared);
    const auto parseCut = [&]() {
      RoomLightCut cut;
      cut.screenAmbient=r.color(p); cut.actorAmbient=r.color(p+252); cut.effectAmbient=r.color(p+256);
      cut.fog={r.u32(p+8),r.real(p+12),r.real(p+16),r.color(p+20)};
      cut.mirrorFog.type=r.u32(p+24);cut.mirrorFog.color=r.color(p+36);
      if(cut.mirrorFog.type&7) {cut.mirrorFog.start=r.real(p+28);cut.mirrorFog.end=r.real(p+32);}
      // cLit::versionUp clamps TEV values and clears old-format Hokan.
      cut.tevScale={std::min(bytes[p+64],std::uint8_t(2)),std::min(bytes[p+65],std::uint8_t(2))};
      // Original cLightMgr::setMipmap supplies the cut's sampler settings.
      cut.minimumLod=bytes[p+242]>9?0:bytes[p+242];
      cut.maximumLod=bytes[p+243]>9?5:bytes[p+243];
      cut.maximumLod=std::max(cut.maximumLod,cut.minimumLod);
      cut.anisotropy=bytes[p+244]>2?0:bytes[p+244];
      cut.lodBias=r.real(p+248);
      // far_play_ratio is evaluated only when GX fog is enabled. Some
      // shipped no-fog event cuts retain unused pool bytes in this field.
      cut.farRatio=(cut.fog.type&7)?r.real(p+68):0.f;cut.fogFrames=bytes[p+72];
      if(sourceVersion<=0x28)cut.fogFrames=0;
      cut.blurRate=bytes[p+47];cut.blurType=bytes[p+240];cut.blurPower=std::int8_t(bytes[p+241]);
      cut.focusDepth=std::int32_t(r.u32(p+40));cut.focusLevel=bytes[p+45];cut.focusMode=bytes[p+46];
      if(sourceVersion>0x27)cut.contrast={bytes[p+245],bytes[p+246],bytes[p+247]};
      if(cut.farRatio<0 || cut.farRatio>1)
        throw std::runtime_error("LIT: invalid TEV scale or far range");
      const unsigned fogMode = cut.fog.type & 7;
      if ((cut.fog.type & ~15u) || (fogMode!=0 && fogMode!=2 && fogMode!=4 && fogMode!=5 && fogMode!=6 && fogMode!=7) ||
          (fogMode && cut.fog.end <= cut.fog.start))
        throw std::runtime_error("LIT: invalid fog mode or range");
      const auto lights=r.u32(p+4);
      if (lights>128 || (sourceVersion==0x2c && lights>maxLights))
        throw std::runtime_error("LIT: Light count exceeds header bounds");
      r.range(p+260, std::size_t(lights)*300);
      for (auto q : offsets)
        if (q>p && q<p+260+std::size_t(lights)*300)
          throw std::runtime_error("LIT: overlapping cuts");
      for (unsigned i=0; i<lights; ++i) {
        const auto q=p+260+i*300;
        RoomLight light;
        light.flags=bytes[q]; light.type=bytes[q+1]; light.id=bytes[q+2]; light.enableMask=bytes[q+3];
        const bool active=(light.flags&3)==3;
        // Subscreen LightMgr.create promotes finite stored lights to alive;
        // preserve those parameters. Only unused, invalid pool data may be
        // ignored, since the original room manager never evaluates them.
        if(!active && light.type>7) {cut.lights.push_back(light);continue;}
        const auto value=[&](std::size_t at) {try{return r.real(at);}catch(const std::exception &){if(active)throw;return 0.f;}};
        const auto vector=[&](std::size_t at){return std::array<float,3>{value(at),value(at+4),value(at+8)};};
        if (sourceVersion<=0x29 && (light.enableMask&1)) light.enableMask|=0x40;
        light.position=vector(q+4); light.radius=value(q+16); light.color=r.color(q+20); light.intensity=value(q+24);
        light.parent=bytes[q+28]; light.kind=bytes[q+29]; light.attribute=bytes[q+30];
        light.parentNo=r.u32(q+32);
        // cLight::calcParent's default branch detaches invalid parent kinds.
        // Later cuts in the supplied r120 actually use these values; retain
        // original world positions rather than rejecting the whole room.
        if (light.parent>4) { light.parent=0; light.parentNo=1; ++normalizedParents; }
        if (active && (light.type>7 || light.radius<0 || light.intensity<0))
          throw std::runtime_error("LIT: invalid light type or radius");
        if(light.type>=3 && light.type<=6)light.direction=vector(q+44);
        else light.direction[0]=value(q+44);
        if (light.type==3 || light.type==6) { light.cutoff=value(q+56); light.edge=value(q+60); }
        else if (light.type==5) { light.directionFlags=r.u32(q+56); light.edge=value(q+60); }
        else if (light.type==4) for(unsigned k=0;k<6;++k) light.customAttenuation[k]=value(q+56+k*4);
        else if (light.type!=4) light.edge=light.direction[0];
        if (light.id==4) {
          auto &s=light.shadow;
          s.flags=r.u16(q+108); s.kind=bytes[q+110]; s.texture=bytes[q+111];
          s.angleX=static_cast<std::int16_t>(r.u16(q+112));
          s.angleY=static_cast<std::int16_t>(r.u16(q+114));
          s.fovy=bytes[q+116]; s.selfLevel=bytes[q+117]; s.softLevel=bytes[q+118];
          s.fovySub=bytes[q+132];if(s.flags&2)s.sourcePosition=vector(q+120);
        }
        cut.lights.push_back(light);
      }
      return cut;
    };
    try { cuts.push_back(parseCut()); }
    catch (const std::exception &) {
      // An invalid noninitial source cut must never upload nonfinite data.
      if (cuts.empty()) throw;
      unavailableCuts.push_back(unsigned(cuts.size()));
      present.back()=false; cuts.emplace_back();
    }
  }
  if (!present.front()) throw std::runtime_error("LIT: Initial Cut0 is missing");
}
std::shared_ptr<const RoomLighting> ReadRoomLighting(const RoomArchive &room) {
  const auto *entry=room.find("LIT");
  if (!entry) throw std::runtime_error("LIT: original room lighting is missing");
  auto result=std::make_shared<RoomLighting>(room.copy(entry-room.entries().data()));
  if(const auto *cam=room.find("CAM")) {
    const auto bytes=room.copy(cam-room.entries().data());
    // CORE also contains a CAM for menu cameras, without room cut areas.
    if(bytes.size()>=4 && bytes[0]=='B' && bytes[1]=='4' && bytes[2]=='0')result->readCameraAreas(bytes);
  }
  return result;
}
bool RoomLightTouches(const RoomLightVolume &v,const std::array<float,3> &light,float radius) {
  if(!radius)return true;
  const auto sphere=[&](float height) {
    float squared=0;
    for(unsigned k=0;k<3;++k) {const auto d=light[k]-v.center[k]-height*v.axes[3+k];squared+=d*d;}
    const auto range=v.size[0]+radius;return squared<range*range;
  };
  if(v.type==0)return sphere(-v.size[1]) || sphere(v.size[1]);
  if(v.type==1 || v.type==3)return sphere(0);
  for(unsigned axis=0;axis<3;++axis) {
    float local=0;for(unsigned k=0;k<3;++k)local+=(light[k]-v.center[k])*v.axes[axis*3+k];
    if(local-radius>v.size[axis] || local+radius<-v.size[axis])return false;
  }
  return true;
}
void RoomLighting::readCameraAreas(const std::vector<unsigned char> &bytes) {
  const Reader r{bytes};r.range(0,16);
  if(bytes[0]!='B' || bytes[1]!='4' || bytes[2]!='0' || bytes[3]<'0' || bytes[3]>'4')
    throw std::runtime_error("CAM: Unknown original header");
  const unsigned count=bytes[5];r.range(16,count*16);areas.clear();
  for(unsigned i=0;i<count;++i) {
    const auto p=r.u32(16+i*16+8);if(!p)continue;r.range(p,48);
    RoomLightArea a;a.flags=bytes[p];a.number=bytes[p+1];a.attributes=bytes[p+3];a.character=bytes[p+8];
    // Disabled areas may hold the editor's name in the direction union.
    // CamControl never evaluates those records or unused facing values.
    if(!(a.flags&1))continue;
    if(a.attributes&0x40)a.direction=r.real(p+4);
    a.height=r.real(p+32);a.floor=r.real(p+36);
    const auto vertices=r.u32(p+40),offset=r.u32(p+44);
    if(vertices>256 || a.height<0)throw std::runtime_error("CAM: Face budget");
    r.range(offset,std::size_t(vertices)*12);
    for(unsigned k=0;k<vertices;++k)a.vertices.push_back(r.vector(offset+k*12));
    areas.push_back(std::move(a));
  }
}
bool RoomLightArea::contains(const std::array<double,3> &position,float heading) const {
  if(!(flags&1) || (attributes&4) || vertices.size()<3)return false;
  const float y=float(position[1])+100;
  if(y<floor || y>=floor+height)return false;
  if((attributes&0x40) && std::abs(std::remainder(heading-direction,6.283185307179586f))>2.3561945f)return false;
  const float x=float(position[0]),z=float(position[2]);
  // The original convex 3/4-point check includes boundary edges.
  if(vertices.size()<=4) {
    for(unsigned i=0;i<2;++i) {
      const auto n=vertices.size(),v=(i*2+1)%n;
      const auto &o=vertices[v],&a=vertices[(v+n-1)%n],&b=vertices[(v+1)%n];
      const auto cross=[&](const auto &p){return (p[2]-o[2])*(x-o[0])-(p[0]-o[0])*(z-o[2]);};
      if(cross(a)>0 || cross(b)<0)return false;
    }
    return true;
  }
  // area_hit_pN casts the source diagonal x-z ray, with directed,
  // half-open edge bounds. Preserve its decisions at polygon vertices.
  unsigned intersections=0;const float c=x-z;
  for(std::size_t i=0;i<vertices.size();++i) {
    const auto &p=vertices[i],&q=vertices[(i+1)%vertices.size()];
    const float dx=q[0]-p[0],dz=q[2]-p[2];float xi,zi;
    if(dz!=0) {
      const float a=dx/dz,b=p[0]-a*p[2];if(a==1)continue;
      xi=(b-c*a)/(1-a);zi=(b-c)/(1-a);
    }else {zi=p[2];xi=zi+c;}
    if(!(xi>=x))continue;
    if(dx>0?!(xi>=p[0] && xi<q[0]):!(xi>q[0] && xi<=p[0]))continue;
    if(dz>0?!(zi>=p[2] && zi<q[2]):!(zi>q[2] && zi<=p[2]))continue;
    ++intersections;
  }
  return (intersections&1)!=0;
}
unsigned RoomLighting::selectCut(const std::array<double,3> &position,float heading,
                                  unsigned previous,unsigned attributes) const {
  const auto usable=[&](const RoomLightArea &a,unsigned attr){return (a.attributes&attr) && (a.character&1) &&
      a.number<present.size() && present[a.number] && a.contains(position,heading);};
  for(const auto &a:areas)if(usable(a,0x20))return a.number;
  for(const auto &a:areas)if(a.number==previous && usable(a,attributes))return previous;
  for(const auto &a:areas)if(usable(a,attributes))return a.number;
  return 0;
}
void RoomLightTransition::select(const RoomLighting &lighting,unsigned next,double seconds,bool immediate) {
  if(next>=lighting.cuts.size() || !lighting.present.at(next))throw std::runtime_error("LIT: selected cut is missing");
  const auto &source=lighting.cuts[next];
  if(owner!=&lighting || immediate) {owner=&lighting;cut=next;fog=target=source.fog;remaining=0;clock=0;}
  else if(cut!=next) {cut=next;target=source.fog;remaining=source.fogFrames;fog.type=target.type;if(!remaining)fog=target;}
  clock+=std::clamp(seconds,0.,.05)*30;
  while(clock>=1) {
    --clock;if(!remaining)continue;
    --remaining;const float n=float(remaining),d=n+1;
    fog.start=(fog.start*n+target.start)/d;fog.end=(fog.end*n+target.end)/d;
    for(unsigned k=0;k<4;++k)fog.color[k]=std::uint8_t((fog.color[k]*n+target.color[k])/d);
  }
}
float RoomVisibilityFar(const RoomFog &fog,float farRatio) {
  return fog.type ? fog.end*(1.f-farRatio)+1.f : 1000000.f;
}
bool RoomModelVisible(const std::array<float,3> &center,float radius,
    const std::array<float,3> &eye,const std::array<float,3> &right,
    const std::array<float,3> &up,const std::array<float,3> &look,
    float fovRadians,float aspect,float farPlane,float nearPlane) {
  // geometry.cpp::collision_sphere_hexahedron: reject only beyond an
  // outward unit plane by radius+0.01. A partly visible bound survives.
  float x=0,y=0,z=0;
  for(unsigned k=0;k<3;++k) {const float d=center[k]-eye[k];x+=d*right[k];y+=d*up[k];z+=d*look[k];}
  const float r=radius+.01f,t=std::tan(fovRadians*.5f),tx=t*aspect;
  return nearPlane-z<=r && z-farPlane<=r &&
      (std::abs(x)-z*tx)/std::sqrt(1.f+tx*tx)<=r &&
      (std::abs(y)-z*t)/std::sqrt(1.f+t*t)<=r;
}
float RoomFogAmount(const RoomFog &fog, float cameraDepth) {
  if (!(fog.type&7) || !std::isfinite(cameraDepth)) return 0;
  const float z=std::max(cameraDepth,0.f);
  const float x=std::clamp((z-fog.start)/(fog.end-fog.start),0.f,1.f);
  switch (fog.type&7) {
  case 2: return x;
  case 4: return 1-std::exp2(-8*x);
  case 5: return 1-std::exp2(-8*x*x);
  case 6: return std::exp2(-8*(1-x));
  case 7: return std::exp2(-8*(1-x)*(1-x));
  default: return 0;
  }
}
RoomFogRegisters RoomFogParameters(const RoomFog &fog,float near,float far) {
  RoomFogRegisters r;r.orthographic=(fog.type&8)!=0;
  if(far==near || fog.end==fog.start)return r;
  const auto truncate=[](float value){std::uint32_t bits;std::memcpy(&bits,&value,4);bits&=~0xfffu;std::memcpy(&value,&bits,4);return value;};
  if(r.orthographic) {
    const float inverse=1.f/(fog.end-fog.start);
    r.a=truncate(inverse*(far-near));r.c=truncate(inverse*(fog.start-near));
  } else {
    const float a=(far*near)/((far-near)*(fog.end-fog.start));
    float b=far/(far-near);unsigned exponent=0;
    while(b>1.f){b/=2.f;++exponent;}
    while(b>0 && b<.5f){b*=2.f;--exponent;}
    r.a=truncate(a/float(1u<<(exponent+1)));r.b=std::uint32_t(8.388638e6f*b)&0xffffffu;
    r.shift=(exponent+1)&31;r.c=truncate(fog.start/(fog.end-fog.start));
  }
  return r;
}
float RoomFogRasterAmount(const RoomFog &fog,const RoomFogRegisters &r,std::uint32_t depth) {
  const float z=r.orthographic?r.a*float(depth&0xffffffu)/16777216.f:
      r.a*16777216.f/float(std::int64_t(r.b)-((depth&0xffffffu)>>r.shift));
  const float x=std::clamp(z-r.c,0.f,1.f);float amount=0;
  switch(fog.type&7) {
    case 2:amount=x;break;case 4:amount=1-std::exp2(-8*x);break;case 5:amount=1-std::exp2(-8*x*x);break;
    case 6:amount=std::exp2(-8*(1-x));break;case 7:amount=std::exp2(-8*(1-x)*(1-x));break;
  }
  return std::round(amount*256.f)/256.f;
}
std::string RoomLighting::describe() const {
  const auto &c=cuts.at(0); std::ostringstream s;
  s << "LIT v" << std::hex << sourceVersion << std::dec << " · " << cuts.size() << " Cuts · Cut0 " << c.lights.size() << " lights · Fog "
    << c.fog.type << " [" << c.fog.start << ',' << c.fog.end << "] RGB "
    << unsigned(c.fog.color[0]) << '/' << unsigned(c.fog.color[1]) << '/' << unsigned(c.fog.color[2]);
  if (!unavailableCuts.empty()) s << " · " << unavailableCuts.size() << " additional cuts unusable";
  return s.str();
}
} // namespace re4
