#include "enemy_routes.h"
#include "asset_reader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
CollisionVec above(CollisionVec p) { p[1]=float(p[1])+500.f;return p; }
float squared(const CollisionVec &a,const CollisionVec &b) {
 const float x=float(a[0])-float(b[0]),y=float(a[1])-float(b[1]),z=float(a[2])-float(b[2]);
 return x*x+y*y+z*z;
}
float xz(const CollisionVec &a,const CollisionVec &b) {
 const float x=float(a[0])-float(b[0]),z=float(a[2])-float(b[2]);return std::sqrt(x*x+z*z);
}
}
EnemyRoutes::EnemyRoutes(const std::vector<std::uint8_t> &bytes,const SatCollision &sat):sat_(sat) {
 if(bytes.empty())return;
 const AssetReader r{bytes};r.range(0,24);
 const unsigned n=r.u16(6),lines=r.u16(8),nnext=r.u16(10);
 if(r.u32(0)!=0x32525450 || n>127 || nnext!=n*n || bytes.size()>1024*1024)
  throw std::runtime_error("RTP: Original header/budget");
 const auto pp=r.u32(12),pl=r.u32(16),pn=r.u32(20);
 r.range(pp,n*16);r.range(pl,lines*4);r.range(pn,nnext);
 for(unsigned i=0;i<n;++i) {
  const auto p=pp+i*16;CollisionVec position{r.f32(p),r.f32(p+4),r.f32(p+8)};
  for(double value:position)if(!std::isfinite(value)||std::abs(value)>1e7)throw std::runtime_error("RTP: Position");
  if(unsigned(r.u16(p+12))+r.u16(p+14)>lines)throw std::runtime_error("RTP: Link range");
  points_.push_back(position);
 }
 for(unsigned i=0;i<lines;++i) {
  const int connection=std::int16_t(r.u16(pl+i*4));
  if(connection< -1 || connection>=int(n))throw std::runtime_error("RTP: Link index");
 }
 for(unsigned i=0;i<nnext;++i) {
  const int value=bytes[pn+i]<128?bytes[pn+i]:int(bytes[pn+i])-256;
  if(value< -1 || value>=int(n))throw std::runtime_error("RTP: Follow table");
  next_.push_back(std::int8_t(value));
 }
 // Reject malformed cycles before any live query; valid original tables need
 // at most n hops. The source assumes authored acyclic paths.
 for(unsigned a=0;a<n;++a)for(unsigned b=0;b<n;++b) {
  int p=int(a);unsigned hops=0;
  while(p!=int(b) && (p=next_[std::size_t(p)*n+b])!=-1)
   if(++hops>n)throw std::runtime_error("RTP: Cyclic follow table");
 }
}
bool EnemyRoutes::blocked(const CollisionVec &a,const CollisionVec &b,unsigned attributes,unsigned mode) const {
 unsigned mask=(mode&2)?0x383078:0x383070;
 if(!(mode&4))mask|=0x40000;
 return bool(sat_.line(a,b,SatSubset::All,attributes|0x4000,mask));
}
bool EnemyRoutes::midpointFloor(const CollisionVec &a,const CollisionVec &b) const {
 CollisionVec middle{};for(unsigned k=0;k<3;++k)middle[k]=float(a[k])*.5f+float(b[k])*.5f;
 const auto floor=sat_.ground(middle,600,100000);
 return floor && floor->point[1]>float(middle[1])-2000.f;
}
int EnemyRoutes::nearest(const CollisionVec &position,unsigned mode,unsigned attributes) const {
 const unsigned count=std::min<std::size_t>(10,points_.size());if(!count)return -1;
 std::array<float,10> distances;distances.fill(1e16f);std::array<int,10> indices{};
 for(unsigned i=0;i<points_.size();++i) {
  const float d=squared(position,points_[i]);unsigned j=count;
  while(j>0 && !(d>distances[j-1]))--j;
  if(j<count) {
   for(unsigned k=count-1;k>j;--k){indices[k]=indices[k-1];distances[k]=distances[k-1];}
   indices[j]=int(i);distances[j]=d;
  }
 }
 if(mode)return indices[0];
 const auto p=above(position);
 for(unsigned i=0;i<count;++i)if(!blocked(p,points_[unsigned(indices[i])],attributes,0))return indices[i];
 return -1;
}
int EnemyRoutes::next(unsigned from,unsigned to) const {
 if(from>=points_.size() || to>=points_.size())throw std::runtime_error("RTP: Query index");
 return next_[std::size_t(from)*points_.size()+to];
}
float EnemyRoutes::pointDistance(unsigned from,unsigned to) const {
 if(from>=points_.size() || to>=points_.size())throw std::runtime_error("RTP: Distance index");
 float result=0;while(from!=to) {
  const int hop=next(from,to);
  if(hop==-1)return std::sqrt(squared(points_[from],points_[to]));
  result+=std::sqrt(squared(points_[from],points_[unsigned(hop)]));from=unsigned(hop);
 }return result;
}
EnemyRouteStep EnemyRoutes::step(const CollisionVec &from,const CollisionVec &to,unsigned mode,unsigned attributes) const {
 const auto a=above(from),b=above(to);
 const float height=std::abs(float(a[1])-float(b[1]));
 const EnemyRouteStep direct{to,height,true};
 if(!(mode&1) && (points_.empty() || (!blocked(a,b,attributes,mode) && midpointFloor(a,b))))return direct;
 int p=nearest(from,0,attributes),t=nearest(b,0,attributes);
 if(p==-1 || t==-1)return direct;
 const int hop=next(unsigned(p),unsigned(t));if(hop==-1)return direct;
 if((mode&1) && hop==t && height<2000 && !blocked(a,b,attributes,mode))return direct;
 const float dx=float(from[0])-float(points_[unsigned(p)][0]),dz=float(from[2])-float(points_[unsigned(p)][2]);
 if(dx*dx+dz*dz<62500.f || (hop!=p && !blocked(a,points_[unsigned(hop)],attributes|0x80,mode)))p=hop;
 float maximum=height;int current=p;
 while((current=next(unsigned(current),unsigned(t)))!=-1) {
  maximum=std::max(maximum,std::abs(float(a[1])-float(points_[unsigned(current)][1])));
  if(current==t)break;
 }
 return {points_[unsigned(p)],maximum,false};
}
float EnemyRoutes::distance(const CollisionVec &from,const CollisionVec &to) const {
 if(!points_.empty()) {
  if(!blocked(from,to,0,0) && midpointFloor(above(from),above(to)))return xz(from,to);
  const int p=nearest(from),t=nearest(to);
  if(p!=-1 && t!=-1)return pointDistance(unsigned(p),unsigned(t));
 }
 return xz(from,to);
}
} // namespace re4
