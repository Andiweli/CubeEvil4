#include "event_data.h"
#include "asset_reader.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace re4 {
namespace {
std::string string(const std::vector<std::uint8_t> &b,std::size_t p,std::size_t n) {
  AssetReader{b}.range(p,n);
  auto end=std::find(b.begin()+p,b.begin()+p+n,0);
  if(end==b.begin()+p+n) throw std::runtime_error("EVD: unterminated name");
  return {b.begin()+p,end};
}
void append16(std::vector<std::uint8_t> &out,unsigned v) {out.push_back(v);out.push_back(v>>8);}
void append32(std::vector<std::uint8_t> &out,std::uint32_t v) {append16(out,v);append16(out,v>>16);}
}
std::string EventPacket::text(std::size_t p,std::size_t n) const {return string(bytes,p,n);}
std::int32_t EventPacket::integer(std::size_t p) const {
  const auto v=AssetReader{bytes}.u32(p);
  return v<=0x7fffffff ? std::int32_t(v) : std::int32_t(std::int64_t(v)-0x100000000ll);
}
EventData EventData::Load(const std::string &path) {
  std::ifstream f(path,std::ios::binary|std::ios::ate);
  if(!f) throw std::runtime_error("EVD: File missing: "+path);
  const auto n=f.tellg();
  if(n<80 || n>64*1024*1024) throw std::runtime_error("EVD: File budget");
  std::vector<std::uint8_t> b(static_cast<std::size_t>(n)); f.seekg(0);
  if(!f.read(reinterpret_cast<char*>(b.data()),n)) throw std::runtime_error("EVD: truncated file");
  return EventData(std::move(b));
}
EventData::EventData(std::vector<std::uint8_t> bytes):bytes_(std::move(bytes)) {
  const AssetReader r{bytes_};r.range(0,80);
  if(bytes_.size()>64*1024*1024 || !std::equal(bytes_.begin(),bytes_.begin()+5,"event"))
    throw std::runtime_error("EVD: Identifier/file budget");
  name_=string(bytes_,32,8)+string(bytes_,40,12);soundFlags_=r.u32(52);
  const auto start=std::size_t(r.u32(64)),size=std::size_t(r.u32(68));
  const auto n=std::size_t(r.u32(72)),table=std::size_t(r.u32(76));
  if(!size || !n || n>4096 || start<80 || table<80 || (start&3) || (table&3) ||
     table<start || size>table-start) throw std::runtime_error("EVD: Table budget/overlap");
  r.range(start,size);r.range(table,n*64);
  std::set<std::size_t> offsets{bytes_.size()};
  for(unsigned i=0;i<n;++i) {
    const auto p=table+i*64;EventAsset a;
    a.name=string(bytes_,p,48);a.offset=r.u32(p+48);
    if(a.name.empty() || a.name.front()=='/' || a.name.find("..")!=std::string::npos ||
       a.offset<table+n*64 || a.offset>=bytes_.size() || (a.offset&3))
      throw std::runtime_error("EVD: Asset offset/name");
    if(!names_.emplace(a.name,assets_.size()).second) throw std::runtime_error("EVD: Duplicate asset name");
    offsets.insert(a.offset);assets_.push_back(a);
  }
  for(auto &a:assets_) a.extent=*offsets.upper_bound(a.offset)-a.offset;
  bool terminal=false;int lastCut=-32768,lastFrame=-32768;
  for(std::size_t p=start;p<start+size;) {
    r.range(p,16);EventPacket packet;packet.type=r.u32(p);packet.flags=r.u32(p+4);
    packet.cut=std::int16_t(r.u16(p+8));packet.frame=std::int16_t(r.u16(p+10));packet.offset=p;
    const auto length=std::int16_t(r.u16(p+12));
    if(packet.type>32 || length<16 || (length&3) || std::size_t(length)>start+size-p ||
       packet.cut<lastCut || (packet.cut==lastCut && packet.frame<lastFrame) || packet.frame<0 || packets_.size()>8192)
      throw std::runtime_error("EVD: Packet type/bounds/order");
    unsigned minimum=16;
    switch(packet.type) {
      case 1:case 3: minimum=124;break; case 4:minimum=136;break;
      case 6:case 14:case 28:case 30:case 31:case 32:minimum=64;break;
      case 9:minimum=68;break;case 11:case 12:minimum=76;break;
      case 8:case 15:case 16:case 17:case 18:case 29:minimum=28;break;
      case 13:minimum=36;break;default:break;
    }
    if(unsigned(length)<minimum) throw std::runtime_error("EVD: Truncated packet parameters");
    packet.bytes.assign(bytes_.begin()+p,bytes_.begin()+p+length);
    if(packet.type==6 || packet.type==8) {
      if(packet.cut!=int(cuts_.size()) || packet.frame!=0) throw std::runtime_error("EVD: Cut order");
      const auto frames=packet.type==6 ? unsigned(AssetReader{asset(packet.text(16,48))}.u16(0))+1 : unsigned(packet.integer(16));
      if(!frames || frames>16384 || total_>1000000-frames) throw std::runtime_error("EVD: Frame budget");
      cuts_.push_back(frames);total_+=frames;
    }
    lastCut=packet.cut;lastFrame=packet.frame;packets_.push_back(std::move(packet));p+=length;
    if(packets_.back().type==27) {
      if(p!=start+size) throw std::runtime_error("EVD: Data after EndPac");
      terminal=true;break;
    }
  }
  if(!terminal || cuts_.empty()) throw std::runtime_error("EVD: EndPac/camera is missing");
  for(const auto &p:packets_)
    if(p.cut>=0 && p.type!=26 && p.type!=27 &&
       (unsigned(p.cut)>=cuts_.size() || unsigned(p.frame)>=cuts_[p.cut]))
      throw std::runtime_error("EVD: Packet outside cut bounds");
}
bool EventData::contains(const std::string &name) const {return names_.count(name)!=0;}
std::vector<std::uint8_t> EventData::asset(const std::string &name) const {
  const auto i=names_.find(name);if(i==names_.end()) throw std::runtime_error("EVD: Asset missing: "+name);
  const auto &a=assets_[i->second];return {bytes_.begin()+a.offset,bytes_.begin()+a.offset+a.extent};
}
EventCurve::EventCurve(const std::vector<std::uint8_t> &bytes) {
  const AssetReader r{bytes};r.range(0,3);maximum_=r.u16(0)&0x3fff;
  const auto n=unsigned(bytes[2]),table=(3+n*3+3)&~3u;
  if(!n || n>248 || maximum_>16383 || bytes.size()>16*1024*1024) throw std::runtime_error("EVD-FCV: Budget");
  r.range(table,4+n*4);const auto declaredSize=r.u32(table);
  // MotionSetCore skips this reserved word before its channel table;
  // CameraControl::HermiteExport writes zero here, as does the actual
  // r100s44 dense camera. A nonzero exporter length still bounds parsing.
  const auto size=declaredSize?std::size_t(declaredSize):bytes.size();
  if(size>bytes.size() || size<table+4+n*4) throw std::runtime_error("EVD-FCV: Size");
  constexpr unsigned formats[16][3]={{4,4,4},{4,2,2},{4,1,1},{},{2,4,4},{2,2,2},{2,1,1},{},{1,4,4},{1,2,2},{1,1,1},{},{},{},{},{4,0,0}};
  for(unsigned i=0;i<n;++i) {
    Channel c;c.kind=r.u16(3+i*2);c.index=bytes[3+n*2+i];const auto format=c.kind>>12;
    if(!formats[format][0]) throw std::runtime_error("EVD-FCV: Format");
    auto p=std::size_t(r.u32(table+4+i*4));
    if(p<table+4+n*4 || p>=size) throw std::runtime_error("EVD-FCV: Channel offset");
    for(unsigned axis=0;axis<3;++axis) {
      const auto count=r.u16(p);p+=2;if(!count || count>16384) throw std::runtime_error("EVD-FCV: Key budget");
      append16(c.keys,count);unsigned previous=0;
      for(unsigned k=0;k<count;++k) {
        const auto f=r.u16(p);p+=2;
        const bool terminalDuplicate=k+1==count && f==previous && f==maximum_;
        if((k && f<=previous && !terminalDuplicate) || f>maximum_) throw std::runtime_error("EVD-FCV: Key order");
        previous=f;append16(c.keys,f);
      }
      for(unsigned k=0;k<count;++k) for(const auto width:formats[format]) {
        if(p>size || width>size-p) throw std::runtime_error("EVD-FCV: Key bounds");
        if(width==4) {(void)r.f32(p);append32(c.keys,r.u32(p));}
        else if(width==2) append16(c.keys,r.u16(p));
        else if(width==1) c.keys.push_back(bytes[p]);
        p+=width;
      }
    }
    if(p>size) throw std::runtime_error("EVD-FCV: Axis limit");
    channels_.push_back(std::move(c));
  }
}
std::array<float,3> EventCurve::sample(unsigned channel,float frame) const {
  if(channel>=channels_.size() || !std::isfinite(frame) || frame<0) throw std::runtime_error("EVD-FCV: Sample");
  const auto &c=channels_[channel];motion64::HERMITE_SET p{};p.Frame=frame;p.Frame_max=float(maximum_+1);p.Attr=2;
  p.Data_fmt=c.kind>>12;p.pData=const_cast<std::uint8_t*>(c.keys.data());motion64::Vec out{};motion64::u16 history[3]{};
  motion64::HermiteInterpolation(&p,&out,history);
  if(!std::isfinite(out.x) || !std::isfinite(out.y) || !std::isfinite(out.z)) throw std::runtime_error("EVD-FCV: non-finite result");
  return {out.x,out.y,out.z};
}
}
