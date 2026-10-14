#include "room_area_effects.h"
#include "archive.h"
#include "scene.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <map>
namespace re4 {
namespace {
unsigned word(const std::vector<std::uint8_t> &b,std::size_t p) {
  if(p>b.size() || b.size()-p<4)throw std::runtime_error("SST/EAR: Truncated");
  return unsigned(b[p])<<24|unsigned(b[p+1])<<16|unsigned(b[p+2])<<8|b[p+3];
}
unsigned half(const std::vector<std::uint8_t> &b,std::size_t p) {
  if(p>b.size() || b.size()-p<2)throw std::runtime_error("SST/EAR: Truncated");
  return unsigned(b[p])*256+b[p+1];
}
template<std::size_t N>float real(const std::array<std::uint8_t,N> &b,unsigned p) {
  const std::uint32_t u=unsigned(b.at(p))<<24|unsigned(b.at(p+1))<<16|unsigned(b.at(p+2))<<8|b.at(p+3);
  float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))throw std::runtime_error("SST/EAR: NaN/Inf");return f;
}
std::uint64_t hash(std::uint64_t h,const std::vector<std::uint8_t> &b) {for(auto x:b)h=(h^x)*1099511628211ULL;return h;}
unsigned recHalf(const WaterRecord &r,unsigned p){return unsigned(r.bytes[p])*256+r.bytes[p+1];}
int recInt(const WaterRecord &r,unsigned p) {
  const auto &b=r.bytes;return std::int32_t(std::uint32_t(b[p])<<24|std::uint32_t(b[p+1])<<16|std::uint32_t(b[p+2])<<8|b[p+3]);
}
unsigned visualRnd(std::uint16_t &seed) {
  const auto old=seed;const unsigned next=unsigned(std::uint8_t((old>>1)+(old>>8)))<<8|std::uint8_t(old>>1);
  seed=std::uint16_t(next==old?next+0x101:next);return seed>>8;
}
bool colorMove(WaterSprite &s,const WaterRecord &r) {
  const unsigned fade=recHalf(r,176),hold=recHalf(r,178);
  if(fade<s.age) {
    if(fade+hold<=s.age) {
      for(unsigned k=0;k<4;++k)s.color[k]=std::min(255.f,s.color[k]*real(r.bytes,160+k*4));
      if(s.color[3]<4)return false;
    }
  } else if(fade) {
    const float rate=float(s.age)/float(fade);
    if(r.bytes[194]==3)for(unsigned k=0;k<3;++k)s.color[k]=float(r.bytes[156+k])*rate;
    s.color[3]=s.startAlpha*rate;
  }
  return true;
}
bool commonMove(WaterSprite &s,const WaterRecord &r) {
  if(recHalf(r,180)<=s.age)for(unsigned k=0;k<3;++k) {
    s.position[k]+=s.speed[k];s.speed[k]+=s.acceleration[k];s.speed[k]*=real(r.bytes,48);
  }
  if(recHalf(r,182)<=s.age) {
    s.sizeMultiplier+=s.sizeStep;s.sizeStep*=real(r.bytes,152);
    if(s.sizeMultiplier<=0)return false;
  }
  for(unsigned k=0;k<3;++k)s.angle[k]+=s.angleSpeed[k];
  if(!colorMove(s,r) || (recHalf(r,184) && recHalf(r,184)<=s.age))return false;
  s.age=(s.age+1)&65535u;return true;
}
bool animationMove(WaterSprite &s,const WaterRecord &r,const WaterAnimation &a) {
  s.animationCounter=(s.animationCounter+std::uint8_t(int(std::int8_t(r.bytes[189]))+32))&65535u;
  const auto duration=a.duration.at(s.pattern);
  while((s.animationCounter>>5)>duration) {
    s.animationCounter-=duration<<5;++s.pattern;
    if(s.pattern>=a.pattern.size()) {
      if(!a.loop)return false;
      s.pattern=a.loop==1?0u:unsigned(a.pattern.size()-1);
    }
  }
  return true;
}
bool pathSample(const std::vector<std::array<float,4>> &path,float distance,unsigned &point,std::array<float,3> &out) {
  if(distance<0 || distance>=path.back()[3]){out={};return false;}
  const int step=distance>=path.at(point)[3]?1:-1;int next=int(point);
  do{next+=step;}while(float(step)*path.at(unsigned(next))[3]<float(step)*distance);
  point=unsigned(next-step);const auto &a=path.at(point),&b=path.at(unsigned(next));
  distance-=a[3];distance/=b[3]-a[3];
  for(unsigned k=0;k<3;++k)out[k]=(b[k]-a[k])*distance+a[k];
  return true;
}
EnvironmentData cpuData(const EnvironmentData &source) {
  EnvironmentData d;d.room=source.room;d.signature=source.signature;
  d.targets=source.targets;d.recordPaths=source.recordPaths;d.recordEmitMatrices=source.recordEmitMatrices;
  for(const auto &a:source.animations) {
    WaterAnimation m;m.id=a.id;m.width=a.width;m.height=a.height;m.loop=a.loop;
    m.centerX=a.centerX;m.centerY=a.centerY;m.pattern=a.pattern;m.duration=a.duration;
    d.animations.push_back(std::move(m));
  }
  return d;
}
} // namespace
bool OriginalEffectAreaContains(const RoomEffectArea &a,const std::array<float,3> &p) {
  const auto &b=a.source;const unsigned type=b[1];
  if(type==3)return false;
  if(type!=1 && type!=2)throw std::runtime_error("EAR: Unknown original area type");
  const float floor=real(b,4),height=real(b,8);
  if(p[1]+100.f<floor || p[1]>=floor+height)return false;
  if(type==2) {
    const float x=p[0]-real(b,16),z=p[2]-real(b,20);
    return std::sqrt(x*x+z*z)<real(b,12);
  }
  const auto x=[&](unsigned n){return real(b,16+n*8);};
  const auto z=[&](unsigned n){return real(b,20+n*8);};
  if((x(3)-x(0))*(p[2]-z(0))>(z(3)-z(0))*(p[0]-x(0)) ||
      (x(1)-x(0))*(p[2]-z(0))<(z(1)-z(0))*(p[0]-x(0)))return false;
  if((x(3)-x(2))*(p[2]-z(2))<(z(3)-z(2))*(p[0]-x(2)) ||
      (x(1)-x(2))*(p[2]-z(2))>(z(1)-z(2))*(p[0]-x(2)))return false;
  return true;
}
RoomAreaEffectsData BuildRoomAreaEffects(const RoomArchive &room,std::uint16_t id,
                                        const std::vector<std::uint8_t> &core) {
  RoomAreaEffectsData out;if(id!=0x100 && id!=0x103 && id!=0x106 && id!=0x111 && id!=0x113 && id!=0x11a)return out;
  const auto *ee=room.find("EFF"),*ea=room.find("EAR");
  if(!ee || !ea)throw std::runtime_error("r100: Original EFF/EAR is missing");
  const auto eff=room.copy(ee-room.entries().data()),ear=room.copy(ea-room.entries().data());
  auto &d=out.effects.sprites;d.room=id;d.signature=hash(hash(hash(1469598103934665603ULL,eff),ear),core);
  if(word(eff,0)!=11 || word(ear,4) || word(ear,0)>256 ||
      std::size_t(word(ear,0))*152+16>ear.size())throw std::runtime_error("r100: EFF/EAR original format");
  for(unsigned i=0;i<word(ear,0);++i) {
    const auto p=16+i*152;RoomEffectArea a;a.number=ear[p+2];a.flags=word(ear,p+52);
    std::copy_n(ear.begin()+p+4,48,a.source.begin());
    if(a.number>=32 && a.number!=255)throw std::runtime_error("r100: EAR number");
    for(unsigned f=4;f<=44;f+=4)(void)real(a.source,f);
    out.areas.push_back(a);
  }
  const auto ids=word(eff,12),offsets=word(eff,36),count=word(eff,ids);
  if(count>256 || count!=word(eff,offsets))throw std::runtime_error("r100: SST table count");
  for(unsigned t=0;t<count;++t) {
    const auto item=ids+4+t*8;const unsigned sequence=half(eff,item),area=half(eff,item+2);
    if(sequence>31 || (area>31 && area!=65535) || word(eff,item+4))throw std::runtime_error("r100: SST original key");
    const auto head=offsets+word(eff,offsets+4+t*4);const unsigned n=half(eff,head);
    if(!n || n>256 || head+48+std::size_t(n)*300>eff.size() || eff.at(head+10)!=0xfe ||
        eff.at(head+36)!=16 || half(eff,head+8))throw std::runtime_error("r100: SST header");
    for(unsigned p=12;p<=32;p+=4)if(word(eff,head+p))throw std::runtime_error("r100: SST header transform");
    WaterTarget target;target.sequence=sequence;target.records.resize(n);
    std::vector<std::vector<std::array<float,4>>> paths(n);
    std::vector<std::optional<unsigned>> masks(n);unsigned previous=0;
    for(unsigned j=0;j<n;++j) {
      auto &r=target.records[j];std::copy_n(eff.begin()+head+48+j*300,300,r.bytes.begin());const auto &b=r.bytes;
      const unsigned time=unsigned(b[4])*256+b[5];if(time<previous)throw std::runtime_error("r100: Unsorted SST times");previous=time;
      // SST banks use the same EffSetAreaState lifecycle in later rooms.
      // Dedicated filter generators and EST-spawning Esp0c must never be
      // mistaken for ordinary sprites. Preserve explicit diagnostics.
      if(id!=0x100 && (b[264] && b[265]!=0 && b[265]!=2)) {
        r.enabled=false;out.warnings.push_back("SST"+std::to_string(sequence)+": dedicated generator "+std::to_string(b[265]));continue;
      }
      if(b[0]!=1 || b[3] || b[6] || (b[7]!=0xfe && b[7]!=0xfd) || b[194]>5 ||
          b[264]>1 || (b[264] && b[265]!=0 && b[265]!=2))throw std::runtime_error("r100: SST original profile");
      if(b[1]!=0 && b[1]!=4 && b[1]!=6 && b[1]!=8 && b[1]!=10 && b[1]!=11 && b[1]!=21 && b[1]!=70 && b[1]!=72 && b[1]!=78) {
        r.enabled=false;out.warnings.push_back("SST"+std::to_string(sequence)+" Record"+std::to_string(j)+
            ": original Esp"+std::to_string(b[1])+" requires a separate special backend");continue;
      }
      if(b[195] && (b[1]!=8 || b[195]!=1 || b[196]!=255)) {
        if(id==0x100)throw std::runtime_error("r100: unauthorized CommonShimmer profile");
        r.enabled=false;out.warnings.push_back("SST"+std::to_string(sequence)+" Record"+std::to_string(j)+": separate shimmer backend");continue;
      }
      for(unsigned p=12;p<=172;p+=4)if(p!=156 && std::abs(real(b,p))>200000)throw std::runtime_error("r100: SST parameter budget");
      if(b[1]==10 && (b[201] || b[202] || b[203]))throw std::runtime_error("r100: Unknown Esp0a original type");
      if(b[1]==6) {
        if(b[200]>1 || b[202]!=1)throw std::runtime_error("SST: Esp06 PathOwner/Loop profile");
        paths[j]=ReadOriginalEffectPath(b[200]?eff:core,b[201]);
      }
      if(b[264] && b[265]==2) {
        if(b[260] || b[299]>2)throw std::runtime_error("r100: gen02 path owner/orientation");
        paths[j]=ReadOriginalEffectPath(core,b[261]);
      }
      if(b[1]!=70)RegisterOriginalEffectAnimation(d,eff,core,r);
      const auto flags=std::uint32_t(b[8])<<24|std::uint32_t(b[9])<<16|std::uint32_t(b[10])<<8|b[11];
      if(flags&0x4000) {
        WaterRecord mask;mask.bytes[2]=b[197];RegisterOriginalEffectAnimation(d,eff,core,mask);masks[j]=mask.animation;
      }
    }
    out.banks.push_back({sequence,area,t});d.targets.push_back(std::move(target));d.recordPaths.push_back(std::move(paths));
    d.recordEmitMatrices.emplace_back(n);out.maskAnimations.push_back(std::move(masks));out.effects.sequences.push_back(sequence);
  }
  d.warnings=out.warnings;return out;
}
struct RoomAreaEffects::Impl {
  const RoomAreaEffectsData *source;
  struct Aux {
    std::array<float,3> base{};EnvironmentMatrix pathMatrix{1,0,0,0,0,1,0,0,0,0,1,0};
    float distance{},pathSpeed{},pathAcceleration{},baseAlpha{};
    unsigned segment{},wait{},stopFrame{},indoorCount{};
  };
  struct Mask {unsigned pattern{},counter{},age{};bool alive=true;};
  struct Instance {unsigned bank{},generation{};EnvironmentData data;std::unique_ptr<EnvironmentSimulation> simulation;std::map<std::uint64_t,Aux> aux;std::map<std::uint64_t,Mask> masks;std::map<std::uint64_t,std::unique_ptr<RoomEffectCloth>> cloth;};
  std::vector<std::unique_ptr<Instance>> instances;
  std::uint32_t areas{},display{},generation{};std::uint16_t random=0x4e22;
  std::uint64_t frames{};bool indoors{};mutable WorldEffectsState cached;
  WorldEffectCamera view;
  explicit Impl(const RoomAreaEffectsData &d,std::uint32_t mask,const WorldEffectCamera *camera):source(&d),display(mask) {
    if(camera)view=*camera;
    for(unsigned b=0;b<d.banks.size();++b)if(d.banks[b].area==65535 && (display&(1u<<d.banks[b].id)))start(b);
    for(unsigned n=0;n<200;++n)move(camera);
  }
  void start(unsigned bank) {
    auto p=std::make_unique<Instance>();p->bank=bank;p->generation=++generation;p->data=cpuData(source->effects.sprites);
    for(unsigned t=0;t<p->data.targets.size();++t)if(t!=source->banks[bank].target)p->data.targets[t].records.clear();
    p->data.sourceSequenceSeeds=true;p->data.initialRandom=random;
    auto *instance=p.get();
    p->data.spriteInitializer=[this,instance](WaterSprite &s,const WaterRecord &r,std::uint32_t &seed){return init(*instance,s,r,seed);};
    p->data.spriteMover=[this,instance](WaterSprite &s,const WaterRecord &r,EnvironmentState &state){return customMove(*instance,s,r,state);};
    p->simulation=std::make_unique<EnvironmentSimulation>(p->data);random=p->simulation->state().random;
    instances.push_back(std::move(p));
  }
  bool init(Instance &p,WaterSprite &s,const WaterRecord &r,std::uint32_t &seed) {
    if(r.bytes[1]==78){p.cloth.emplace(s.serial,std::make_unique<RoomEffectCloth>(s,r));return true;}
    const unsigned type=r.bytes[1];if(type!=4 && type!=6 && type!=21)return true;
    Aux a;a.base=s.position;a.baseAlpha=s.color[3];
    if(type==4) {
      for(float &size:s.size)size=std::max(.1f,size);
    } else if(type==6) {
      a.stopFrame=r.bytes[252];
      a.pathSpeed=float(recInt(r,204));a.pathAcceleration=float(recInt(r,208))*.1f;
      a.pathSpeed+=float(recInt(r,212))*fRandSeed1_1(&seed);
      if(recInt(r,204)>0 && a.pathSpeed<0)a.pathSpeed=-a.pathSpeed;
      if(recInt(r,204)<0 && a.pathSpeed>0)a.pathSpeed=-a.pathSpeed;
      motion64::Vec angle{real(r.bytes,228)*.017453292f,real(r.bytes,232)*.017453292f,real(r.bytes,236)*.017453292f};
      motion64::Mtx matrix;motion64::RotMatrix(matrix,&angle);
      motion64::Vec scale{1+real(r.bytes,216)*.1f,1+real(r.bytes,220)*.1f,1+real(r.bytes,224)*.1f};
      motion64::Mtx scaling;motion64::PSMTXScale(scaling,scale.x,scale.y,scale.z);motion64::PSMTXConcat(matrix,scaling,matrix);
      std::memcpy(a.pathMatrix.data(),matrix,sizeof matrix);
      const auto &path=p.data.recordPaths.at(s.target).at(s.record);
      if(real(r.bytes,240)!=0 || real(r.bytes,244)!=0) {
        float t=real(r.bytes,240)*.01f+real(r.bytes,244)*.01f*fRandSeed0_1(&seed);
        if(t>1)t-=float(unsigned(t));
        if(t<0)t+=float(unsigned(-t))+1;
        a.distance=t*path.back()[3];
      } else if(a.pathSpeed<0)a.distance=path.back()[3];
    } else {
      const float range=real(r.bytes,32);
      for(unsigned k=0;k<3;++k)s.position[k]+=range*fRandSeed1_1(&seed);
      if(indoors){a.indoorCount=r.bytes[203];s.color[3]=0;}
    }
    p.aux.emplace(s.serial,a);return true;
  }
  std::optional<bool> customMove(Instance &p,WaterSprite &s,const WaterRecord &r,EnvironmentState &state) {
    if(r.bytes[1]==78)return p.cloth.at(s.serial)->move(s,r,state.random);
    const unsigned type=r.bytes[1];if(type!=4 && type!=6 && type!=21)return {};
    auto &a=p.aux.at(s.serial);const auto &animation=p.data.animations.at(r.animation);
    if(type==4) {
      if(!colorMove(s,r) || (recHalf(r,184) && recHalf(r,184)<=s.age))return true;
      s.age=(s.age+1)&65535u;
      if(s.age==recHalf(r,186)+1){a.base=s.position;return false;}
      const auto wrap=[&](unsigned k,float limit,float size) {
        float v=s.position[k];
        if(v>=limit)while(v>=0){v-=size;s.position[k]=v;}
        else if(v<-size && v<limit-s.size[0])while(v<limit-s.size[0]){v+=size;s.position[k]=v;}
      };
      wrap(0,512,s.size[0]);wrap(1,448,s.size[1]);
      for(unsigned k=0;k<2;++k)if(r.bytes[201+k]) {
        const int range=std::int8_t(r.bytes[201+k]);
        s.position[k]=a.base[k]+float(visualRnd(state.random)%unsigned(range*2))-float(range);
        const float limit=k?448.f:512.f;
        if(s.position[k]<-s.size[k])s.position[k]+=s.size[k]+limit;
        if(s.position[k]>s.size[k]+limit)s.position[k]-=s.size[k]+limit;
      }
      const unsigned calls=(s.age-recHalf(r,186))&65535u;
      if(calls>unsigned(r.bytes[203])+1) {
        const auto rate=std::int8_t(r.bytes[207]);
        if(rate>0)s.color[3]=int(s.color[3])+rate>255?255.f:s.color[3]+float(rate);
        else if(rate<0)s.color[3]=int(s.color[3])+rate<0?0.f:s.color[3]+float(rate);
      }
      return !animationMove(s,r,animation);
    }
    if(type==6) {
      s.position=a.base;if(recHalf(r,180)<=s.age)a.pathSpeed+=a.pathAcceleration;
      if(!commonMove(s,r))return true;
      a.base=s.position;
      if(!animationMove(s,r,animation))return true;
      if(!a.wait)a.distance+=a.pathSpeed;else --a.wait;
      const auto &path=p.data.recordPaths.at(s.target).at(s.record);std::array<float,3> point{};
      if(!pathSample(path,a.distance,a.segment,point)) {
        a.distance+=a.pathSpeed>0?-path.back()[3]:path.back()[3];
        if(!pathSample(path,a.distance,a.segment,point))throw std::runtime_error("r100: Esp06 loop outside path bounds");
        a.wait=a.stopFrame;
        // The original adds the random wait to StopFrame, after copying
        // that value into wait; preserve its accumulation and byte wrap.
        if(r.bytes[253])a.stopFrame=(a.stopFrame+visualRnd(state.random)%r.bytes[253])&255u;
      }
      s.position=TransformPoint(a.pathMatrix,point);for(unsigned k=0;k<3;++k)s.position[k]+=a.base[k];return false;
    }
    s.color[3]=a.baseAlpha;
    if(!commonMove(s,r) || !animationMove(s,r,animation))return true;
    const float oldY=s.position[1],minY=real(r.bytes,216);const bool wasAbove=!minY || oldY>=minY;
    a.baseAlpha=s.color[3];const unsigned indoorFrames=r.bytes[203];
    if(indoorFrames) {
      if(indoors)++a.indoorCount;else if(a.indoorCount)--a.indoorCount;
      if(a.indoorCount){a.indoorCount=std::min(a.indoorCount,indoorFrames);s.color[3]*=1-float(a.indoorCount)/float(indoorFrames);}
    }
    const float range=real(r.bytes,32),halfRange=range*.6f;
    const auto wrap=[&](std::array<float,3> axis,float period,bool depth) {
      float length=0;for(float x:axis)length+=x*x;length=std::sqrt(length);if(length)for(float &x:axis)x/=length;
      float d=0;for(unsigned k=0;k<3;++k)d+=(s.position[k]-view.eye[k])*axis[k];
      const int n=depth ? (d>=0 ? int(d/period) : -int(-d/period)-1) :
          (d>=0 ? int((d+period*.5f)/period) : int((d-period*.5f)/period));
      if(n)for(unsigned k=0;k<3;++k)s.position[k]+=axis[k]*-(period*float(n));
      if(depth && n) {
        d=0;for(unsigned k=0;k<3;++k)d+=(s.position[k]-view.eye[k])*axis[k];
      }
      return d;
    };
    wrap(view.right,halfRange*2,false);wrap(view.up,halfRange*2,false);
    const float d=wrap(view.forward,range,true);
    const float ratio=1-std::min(1.f,float(std::int8_t(r.bytes[202]))/100.f);
    if(d>range*ratio)s.color[3]*=1-(d-range*ratio)/(range*(1-ratio));
    if(minY && s.position[1]<minY && wasAbove)s.position[1]=oldY;
    return false;
  }
  bool moveMask(Instance &p,const WaterSprite &s) {
    const auto animation=source->maskAnimations.at(s.target).at(s.record);if(!animation)return true;
    const auto &r=p.data.targets[s.target].records[s.record];const auto &a=p.data.animations.at(*animation);
    auto inserted=p.masks.emplace(s.serial,Mask{0,0,recHalf(r,186),true});auto &m=inserted.first->second;
    unsigned calls=(s.age-m.age)&65535u;m.age=s.age;
    const unsigned rate=std::uint8_t(int(std::int8_t(r.bytes[189]))+32);
    while(calls-- && m.alive) {
      m.counter=(m.counter+rate)&65535u;const auto duration=a.duration.at(m.pattern);
      while((m.counter>>5)>duration) {
        ++m.pattern;m.counter-=duration<<5;
        if(m.pattern>=a.pattern.size()) {
          if(!a.loop){m.alive=false;break;}
          m.pattern=a.loop==1?0u:unsigned(a.pattern.size()-1);
        }
      }
    }
    return m.alive;
  }
  void move(const WorldEffectCamera *camera) {
    if(camera)view=*camera;
    EnvironmentCamera view;if(camera){view.eye=camera->eye;view.up=camera->up;}
    unsigned count=0;for(const auto &p:instances)count+=unsigned(p->simulation->state().sprites.size());
    for(auto &p:instances) {
      auto state=p->simulation->state();state.random=random;
      if(!p->simulation->restore(state))throw std::runtime_error("r100: SST RNG handoff");
      const auto before=unsigned(state.sprites.size());p->simulation->tickGenerators(camera?&view:nullptr,512-(count-before));
      count=count-before+unsigned(p->simulation->state().sprites.size());random=p->simulation->state().random;
    }
    for(auto &p:instances) {
      auto state=p->simulation->state();state.random=random;
      if(!p->simulation->restore(state))throw std::runtime_error("r100: SST EspMove RNG handoff");
      p->simulation->tickSprites(camera?&view:nullptr);state=p->simulation->state();random=state.random;
      state.sprites.erase(std::remove_if(state.sprites.begin(),state.sprites.end(),[&](const auto &s){
        return !moveMask(*p,s);}),state.sprites.end());
      if(!p->simulation->restore(state))throw std::runtime_error("r100: SST mask animation");
      const auto alive=[&](std::uint64_t serial){return std::any_of(state.sprites.begin(),state.sprites.end(),[&](const auto &s){return s.serial==serial;});};
      for(auto i=p->aux.begin();i!=p->aux.end();)if(!alive(i->first))i=p->aux.erase(i);else ++i;
      for(auto i=p->masks.begin();i!=p->masks.end();)if(!alive(i->first))i=p->masks.erase(i);else ++i;
      for(auto i=p->cloth.begin();i!=p->cloth.end();)if(!alive(i->first))i=p->cloth.erase(i);else ++i;
    }
    ++frames;
  }
  const WorldEffectsState &snapshot() const {
    cached={};cached.signature=source->effects.sprites.signature;cached.frames=frames;cached.random=random;
    cached.recordCursor.resize(source->effects.sprites.targets.size());cached.sequenceSeed.resize(source->effects.sprites.targets.size());
    for(const auto &p:instances) {
      const auto &state=p->simulation->state();const auto prefix=std::uint64_t(p->generation)<<32;
      for(auto s:state.sprites){s.serial|=prefix;cached.serial=std::max(cached.serial,s.serial);cached.sprites.push_back(std::move(s));}
      cached.emitters.insert(cached.emitters.end(),state.emitters.begin(),state.emitters.end());
      cached.recordCursor[source->banks[p->bank].target]=state.recordCursor[source->banks[p->bank].target];
      cached.sequenceSeed[source->banks[p->bank].target]=state.sequenceSeed[source->banks[p->bank].target];
    }
    return cached;
  }
};
RoomAreaEffects::RoomAreaEffects(const RoomAreaEffectsData &data,std::uint32_t mask,const WorldEffectCamera *camera):impl_(std::make_unique<Impl>(data,mask,camera)){}
RoomAreaEffects::~RoomAreaEffects()=default;
void RoomAreaEffects::tick(const std::array<float,3> &player,const WorldEffectCamera &camera,std::uint32_t display,
                           std::uint32_t add,bool useCamera) {
  auto point=useCamera?camera.eye:player;if(!useCamera)point[1]+=100.f;
  std::uint32_t areas=add;bool indoor=false;
  for(const auto &a:impl_->source->areas)if(OriginalEffectAreaContains(a,point)) {
    if(a.number<32)areas|=1u<<a.number;
    if(a.flags&1)indoor=true;
  }
  const auto on=areas&~impl_->areas,off=impl_->areas&~areas,newDisplay=display&~impl_->display;
  impl_->instances.erase(std::remove_if(impl_->instances.begin(),impl_->instances.end(),[&](const auto &p){
    const auto area=impl_->source->banks[p->bank].area;return area<32 && (off&(1u<<area));}),impl_->instances.end());
  for(unsigned area=0;area<32;++area) {
    const bool entered=(on&(1u<<area))!=0;
    for(unsigned b=0;b<impl_->source->banks.size();++b) {
      const auto &bank=impl_->source->banks[b];if(bank.area!=area || !(display&(1u<<bank.id)))continue;
      if(entered || ((areas&(1u<<area)) && (newDisplay&(1u<<bank.id))))impl_->start(b);
    }
  }
  impl_->areas=areas;impl_->display=display;impl_->indoors=indoor;impl_->move(&camera);
}
std::uint32_t RoomAreaEffects::areaMask() const{return impl_->areas;}
bool RoomAreaEffects::inRoom() const{return impl_->indoors;}
const WorldEffectsState &RoomAreaEffects::state() const{return impl_->snapshot();}
std::vector<RoomEffectClothMesh> RoomAreaEffects::clothMeshes() const {
  std::vector<RoomEffectClothMesh> out;
  for(const auto &p:impl_->instances)for(const auto &cloth:p->cloth) {
    auto mesh=cloth.second->mesh();mesh.serial|=std::uint64_t(p->generation)<<32;out.push_back(std::move(mesh));
  }
  return out;
}
std::vector<RoomAreaEffectQuad> RoomAreaEffects::quads(const WorldEffectCamera &camera) const {
  const auto &state=impl_->snapshot();std::vector<RoomAreaEffectQuad> out;
  for(const auto &q:WorldEffectQuads(impl_->source->effects,state,camera)) {
    const auto sprite=std::find_if(state.sprites.begin(),state.sprites.end(),[&](const auto &s){return s.serial==q.serial;});
    if(sprite==state.sprites.end())throw std::runtime_error("r100: SST mask owner");
    RoomAreaEffectQuad request;request.quad=q;request.maskAnimation=impl_->source->maskAnimations.at(sprite->target).at(sprite->record);
    const auto &record=impl_->source->effects.sprites.targets[sprite->target].records[sprite->record];
    request.maskStretched=record.bytes[1]==8 && record.bytes[202]==1;
    request.maskAlphaScale=record.bytes[1]!=8 && (q.flags&0x20000) ? 4.f : 1.f;
    request.shimmerType=record.bytes[195];request.shimmerPower=record.bytes[196];
    if(request.shimmerType)request.quad.rgbScale=request.quad.alphaScale=1;
    if(request.maskAnimation) {
      const auto owner=std::find_if(impl_->instances.begin(),impl_->instances.end(),[&](const auto &p){return p->generation==unsigned(sprite->serial>>32);});
      if(owner==impl_->instances.end())throw std::runtime_error("r100: SST mask instance is missing");
      request.maskImage=(*owner)->masks.at(sprite->serial&0xffffffffu).pattern;
    }
    if(record.bytes[1]!=4){out.push_back(request);continue;}
    // Esp04_Trans uses a direct s16 grid, not a centered billboard. Each
    // tile receives the whole texture, independent of the plain-sprite flips.
    const int sx=int(sprite->size[0]),sy=int(sprite->size[1]);
    if(sx<=0 || sy<=0)throw std::runtime_error("r100: Esp04 original tile size");
    const int nx=(record.bytes[200]&1)?542/sx+2:1;
    const int ny=(record.bytes[200]&2)?448/sy+2:1;
    std::int16_t x=std::int16_t(sprite->position[0]),y=std::int16_t(sprite->position[1]);
    if(record.bytes[200]&1)while(x>0)x=std::int16_t(int(x)-sx);
    if(record.bytes[200]&2)while(y>0)y=std::int16_t(int(y)-sy);
    request.quad.tiled=request.quad.flipS=request.quad.flipT=false;
    request.quad.uv={{{0,0},{1,0},{1,1},{0,1}}};
    for(int row=0;row<ny;++row) {
      auto xx=x;
      for(int col=0;col<nx;++col) {
        const auto right=std::int16_t(int(xx)+sx),bottom=std::int16_t(int(y)+sy);
        request.quad.position={{{float(xx),float(y),0},{float(right),float(y),0},
            {float(right),float(bottom),0},{float(xx),float(bottom),0}}};
        out.push_back(request);xx=right;
      }
      y=std::int16_t(int(y)+sy);
    }
  }
  return out;
}
} // namespace re4
