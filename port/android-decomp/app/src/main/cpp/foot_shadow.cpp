// Source: frozen game/foot_shadow.cpp, foot_shadow_tbl.cpp, light.cpp,
// light04.cpp and include/light.h. Disk data remain endian/pointer independent.
#include "foot_shadow.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
struct Reader {
  const std::vector<std::uint8_t> &b;
  void range(std::size_t p,std::size_t n) const {
    if(p>b.size()||n>b.size()-p)throw std::runtime_error("Foot shadow: Truncated CORE range");
  }
  unsigned u16(std::size_t p) const {range(p,2);return unsigned(b[p])*256+b[p+1];}
  unsigned u32(std::size_t p) const {return (u16(p)<<16)|u16(p+2);}
  std::size_t table(unsigned field,unsigned stride) const {
    auto p=u32(field);range(p,4);auto n=u32(p);
    if(p<48||n>4096)throw std::runtime_error("Foot shadow: CORE table limit");
    range(p+4,std::size_t(n)*stride);return p;
  }
  std::size_t entry(std::size_t table,unsigned index,unsigned bytes) const {
    if(index>=u32(table))throw std::runtime_error("Foot shadow: CORE index");
    const auto delta=u32(table+4+index*4);
    if(delta<4+u32(table)*4||delta>b.size()-table)throw std::runtime_error("Foot shadow: CORE offset");
    const auto p=table+delta;range(p,bytes);return p;
  }
  std::size_t end(std::size_t start) const {
    std::size_t limit=b.size();
    for(unsigned field=4;field<=44;field+=4) {
      const auto p=u32(field);if(!p)continue;
      range(p,4);const auto n=u32(p);
      if(p<48||n>4096)throw std::runtime_error("Foot shadow: CORE bounds table");
      if(p>start)limit=std::min(limit,std::size_t(p));
      if(field<24)continue;
      range(p+4,std::size_t(n)*4);
      for(unsigned i=0;i<n;++i) {
        const auto q=entry(p,i,1);if(q>start)limit=std::min(limit,q);
      }
    }
    return limit;
  }
};
struct Joint {unsigned joint,div,flag,color;float size;};
constexpr Joint playerTable[6]{{0x15,2,1,0xff,300},{0x13,1,1,0xc0,450},
    {0x12,1,0,0x80,700},{0x19,2,1,0xff,300},{0x17,1,1,0xc0,450},{0x16,1,0,0x80,700}};
float distanceSquared(const std::array<float,3> &a,const std::array<float,3> &b) {
  float d=0;for(unsigned k=0;k<3;++k){const auto v=a[k]-b[k];d+=v*v;}return d;
}
}
Texture BuildFootShadowTexture(const std::vector<std::uint8_t> &core) {
  const Reader r{core};r.range(0,48);
  if(core.size()>16*1024*1024||r.u32(0)!=11)throw std::runtime_error("Foot shadow: CORE version");
  const auto ids=r.table(4,8),templates=r.table(24,4),animations=r.table(28,4);
  if(r.u32(ids)!=r.u32(templates)||r.u32(ids)!=r.u32(animations))throw std::runtime_error("Foot shadow: CORE count");
  for(unsigned i=0;i<r.u32(ids);++i) {
    if(r.u16(ids+4+i*8)!=0x13)continue;
    const auto p=r.entry(templates,i,12),a=r.entry(animations,i,16);
    if(!r.u16(a+8)||r.u16(a+8)!=r.u32(p+4))throw std::runtime_error("Foot shadow: CORE-TPL/ANM");
    // DecodeTpl validates all texture-local offsets and image budgets.
    auto texture=DecodeTpl({core.begin()+p,core.begin()+r.end(p)});
    if(!texture.width||!texture.height||texture.width>1024||texture.height>1024)
      throw std::runtime_error("Foot shadow: Texture budget");
    texture.wrapS=texture.wrapT=0;return texture;
  }
  throw std::runtime_error("Foot shadow: Original texture 0x13 is missing");
}
std::vector<FootShadowQuad> BuildPlayerFootShadows(const RoomLightCut &cut,
    const std::vector<std::array<float,12>> &parts,const std::array<double,3> &feet,
    float floorY,unsigned shadowColor,unsigned enableMask) {
  std::vector<FootShadowQuad> out;
  if(shadowColor>=255||parts.size()<=0x19||!std::isfinite(floorY))return out;
  const std::array<float,3> position{float(feet[0]),float(feet[1]),float(feet[2])};
  const std::array<float,3> root{parts[0][3],parts[0][7],parts[0][11]};
  for(const auto &l:cut.lights) {
    if((l.flags&3)!=3||l.id!=4||!(l.enableMask&enableMask)||l.shadow.kind!=5||l.type>2)continue;
    auto light=l.position;
    if(l.parent) {
      if(l.parent!=1||(l.parentNo&0xffff)!=0||(l.parentNo>>16)>=parts.size())continue;
      const auto &m=parts[l.parentNo>>16];
      for(unsigned k=0;k<3;++k){light[k]=m[k*4+3];for(unsigned j=0;j<3;++j)light[k]+=m[k*4+j]*l.position[j];}
    }
    float range=l.radius?l.radius:1000000000.f;range*=range;
    if(distanceSquared(position,light)>range)continue;
    std::array<float,3> direction{};
    if(l.type==0) {
      for(unsigned k=0;k<3;++k)direction[k]=root[k]-light[k];
      const auto length=std::sqrt(distanceSquared(root,light));
      if(length<=0)continue;
      for(auto &v:direction)v/=length;
    } else {
      constexpr float degrees=.017453292519943295f;
      const auto x=l.shadow.angleX*degrees,y=l.shadow.angleY*degrees;
      direction={-std::sin(y)*std::sin(x),-std::cos(x),-std::cos(y)*std::sin(x)};
    }
    // Source divides by dir.y. Horizontal/upward authored lights cannot cast
    // onto the floor; reject rather than emit nonfinite vertices on GLES.
    if(direction[1]>=-.00001f)continue;
    if(l.type==2) {
      float dot=0;for(unsigned k=0;k<3;++k)dot+=direction[k]*(root[k]-light[k]);
      auto center=light;for(unsigned k=0;k<3;++k)center[k]+=direction[k]*dot;
      const auto dist=distanceSquared(position,center);
      const auto d=std::sin((l.shadow.fovy?l.shadow.fovy:90)*.017453292519943295f)*dot;
      if(d<=0||dist>d)continue;
      // Match the source's squared-range expression, including its units.
      range*=dist/d;if(range<=0)continue;
    }
    const float rate=l.color[0]/255.f*(255-shadowColor)/255.f;
    const float offset=static_cast<std::int8_t>(l.shadow.texture)*10.f+50.f;
    FootShadowQuad previous{};bool linked=false;unsigned div=0;
    const auto append=[&](FootShadowQuad q) {
      const auto d=distanceSquared(q.position,light)/range;
      const auto alpha=q.alpha*rate*(1-d*d);
      if(!std::isfinite(alpha)||alpha<=0)return;
      // GX stores alpha to one byte before drawing the original black quad.
      q.alpha=std::clamp(std::floor(alpha),0.f,255.f)/255.f;out.push_back(q);
    };
    for(const auto &entry:playerTable) {
      const auto &m=parts[entry.joint];
      FootShadowQuad current{{m[3],m[7],m[11]},entry.size,float(entry.color)};
      const auto scale=-(current.position[1]-floorY)/direction[1];
      for(unsigned k=0;k<3;++k)current.position[k]+=direction[k]*scale;
      current.position[1]+=offset;append(current);
      if(linked&&div)for(unsigned i=0;i<div;++i) {
        const auto t=float(i+1)/float(div+1),s=1-t;
        FootShadowQuad mid;for(unsigned k=0;k<3;++k)mid.position[k]=s*previous.position[k]+t*current.position[k];
        mid.size=s*previous.size+t*current.size;mid.alpha=s*previous.alpha+t*current.alpha;append(mid);
      }
      linked=entry.flag&1;div=entry.div;previous=current;
    }
  }
  return out;
}
} // namespace re4
