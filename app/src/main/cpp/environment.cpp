// Source: frozen upstream game/eff_sys.cpp, esp_sub.cpp, esp08.cpp,
// esp47.cpp, espgen00.cpp, obj02.cpp and room setTexRender functions.
#include "environment.h"
#include "archive.h"
#include "collision_query.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include "scene.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
struct Reader {
  const std::vector<std::uint8_t> &b;
  void range(std::size_t p, std::size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Environment: Truncated subfile");
  }
  unsigned u16(std::size_t p) const {
    range(p, 2);
    return unsigned(b[p]) * 256 + b[p + 1];
  }
  std::uint32_t u32(std::size_t p) const {
    return (std::uint32_t(u16(p)) << 16) | u16(p + 2);
  }
  std::size_t table(unsigned field, unsigned stride) const {
    auto p = u32(field);
    range(p, 4);
    auto n = u32(p);
    if (p < 48 || n > 4096)
      throw std::runtime_error("Environment: Table limit");
    range(p + 4, std::size_t(n) * stride);
    return p;
  }
  std::size_t end(std::size_t start) const {
    std::size_t limit = b.size();
    for (unsigned field = 4; field <= 44; field += 4) {
      auto t = u32(field);
      if (!t)
        continue;
      range(t, 4);
      if (t < 48 || u32(t) > 4096)
        throw std::runtime_error("Environment: Table limit");
      if (t > start)
        limit = std::min(limit, std::size_t(t));
      if (field < 24)
        continue;
      range(t + 4, std::size_t(u32(t)) * 4);
      for (unsigned i = 0; i < u32(t); ++i) {
        auto p = entry(t, i, 1);
        if (p > start)
          limit = std::min(limit, p);
      }
    }
    return limit;
  }
  std::size_t entry(std::size_t table, unsigned i, unsigned minimum) const {
    if (i >= u32(table))
      throw std::runtime_error("Environment: Table index");
    auto delta = u32(table + 4 + std::size_t(i) * 4);
    if (delta < 4 + u32(table) * 4 || delta > b.size() - table)
      throw std::runtime_error("Environment: invalid relative offset");
    auto p = table + delta;
    range(p, minimum);
    return p;
  }
};
unsigned word(const WaterRecord &r, unsigned p) {
  return (unsigned(r.bytes[p]) << 24) | (unsigned(r.bytes[p + 1]) << 16) |
         (unsigned(r.bytes[p + 2]) << 8) | r.bytes[p + 3];
}
unsigned half(const WaterRecord &r, unsigned p) {
  return unsigned(r.bytes[p]) * 256 + r.bytes[p + 1];
}
float real(const WaterRecord &r, unsigned p) {
  auto u = word(r, p);
  float f;
  std::memcpy(&f, &u, 4);
  if (!std::isfinite(f))
    throw std::runtime_error("Environment: NaN/Inf in effect");
  return f;
}
// cObj04::move's authored Efm04 subset used by the r120 road copies:
// Parts FE, Tool_flg40 (lighting), no bounce or model motion.
bool moveEventModel(WaterSprite &s,const WaterRecord &r) {
  if(half(r,180)<=s.age) for(unsigned k=0;k<3;++k) {
    s.position[k]+=s.speed[k];s.speed[k]+=s.acceleration[k];s.speed[k]*=real(r,48);
  }
  if(half(r,182)<=s.age) {
    s.sizeMultiplier+=s.sizeStep;s.sizeStep*=real(r,152);
    if(s.sizeMultiplier<=0)return false;
  }
  for(unsigned k=0;k<3;++k)s.angle[k]+=s.angleSpeed[k];
  const unsigned fade=half(r,176),hold=half(r,178);
  if(fade<s.age) {
    if(fade+hold<=s.age) {
      for(unsigned k=0;k<4;++k)s.color[k]=std::min(255.f,s.color[k]*real(r,160+k*4));
      if(s.color[3]<4)return false;
    }
  } else if(fade)s.color[3]=s.startAlpha*float(s.age)/float(fade);
  if(half(r,184) && half(r,184)<=s.age)return false;
  ++s.age;return true;
}
std::uint64_t hash(std::uint64_t h, const std::vector<std::uint8_t> &b) {
  for (auto x : b)
    h = (h ^ x) * 1099511628211ULL;
  return h;
}
void validateEff(const Reader &r) {
  r.range(0, 48);
  if (r.b.size() > 16 * 1024 * 1024 || r.u32(0) != 11)
    throw std::runtime_error("Environment: EFF version/size");
}
std::vector<WaterRecord> records(const std::vector<std::uint8_t> &b,
                                 unsigned id) {
  Reader r{b};
  validateEff(r);
  auto ids = r.table(8, 8), offsets = r.table(32, 4);
  if (r.u32(ids) != r.u32(offsets))
    throw std::runtime_error("Environment: EST table count");
  for (unsigned i = 0; i < r.u32(ids); ++i) {
    if (r.u16(ids + 4 + i * 8) != id)
      continue;
    auto p = r.entry(offsets, i, 48);
    unsigned n = r.u16(p);
    if (!n || n > 256 || b[p + 36] != 16)
      throw std::runtime_error("Environment: EST header");
    r.range(p + 48, std::size_t(n) * 300);
    if (p + 48 + std::size_t(n) * 300 > r.end(p) || r.u16(p + 8) ||
        r.u32(p + 12) || r.u32(p + 16) || r.u32(p + 20) || r.u32(p + 24) ||
        r.u32(p + 28) || r.u32(p + 32))
      throw std::runtime_error("Environment: EST range/transform");
    std::vector<WaterRecord> out(n);
    for (unsigned k = 0; k < n; ++k)
      std::copy_n(b.begin() + p + 48 + k * 300, 300, out[k].bytes.begin());
    return out;
  }
  throw std::runtime_error("Environment: Water EST is missing");
}
WaterAnimation animation(const std::vector<std::uint8_t> &b, unsigned id) {
  Reader r{b};
  validateEff(r);
  auto ids = r.table(4, 8);
  for (unsigned i = 0; i < r.u32(ids); ++i) {
    if (r.u16(ids + 4 + i * 8) != id)
      continue;
    auto tpl = r.entry(r.table(24, 4), i, 12);
    auto anm = r.entry(r.table(28, 4), i, 16);
    unsigned n = r.u16(anm + 8);
    if (!n || n > 128 || r.u32(tpl + 4) != n || !r.u16(anm) || !r.u16(anm + 2))
      throw std::runtime_error("Environment: TPL/ANM count");
    unsigned bytes = b[anm + 12] ? n * 2 : 0;
    r.range(anm + 16, bytes);
    if (anm + 16 + bytes > r.end(anm))
      throw std::runtime_error("Environment: ANM range");
    WaterAnimation out;
    out.id = id;
    out.width = r.u16(anm);
    out.height = r.u16(anm + 2);
    out.centerX = static_cast<std::int16_t>(r.u16(anm + 4));
    out.centerY = static_cast<std::int16_t>(r.u16(anm + 6));
    out.loop = b[anm + 11] & 3;
    if (out.loop == 3)
      throw std::runtime_error("Environment: ANM loop mode");
    // TPL file offsets remain relative to its own header. Its parser checks all
    // subranges; a bounded resource slice avoids relocation on ARM64.
    std::vector<std::uint8_t> palette(b.begin() + tpl, b.begin() + r.end(tpl));
    std::size_t budget = 0;
    Reader tplView{palette};
    for (unsigned k = 0; k < n; ++k) {
      auto table = tplView.u32(8);
      tplView.range(table, std::size_t(n) * 8);
      auto header = tplView.u32(table + k * 8);
      auto width = tplView.u16(header + 2), height = tplView.u16(header);
      if (width > 1024 || height > 1024 ||
          std::size_t(width) * height * 4 > 8 * 1024 * 1024 - budget)
        throw std::runtime_error("Environment: Effect texture budget");
      // EspTexSet binds Ptn_no directly. The first Frames bytes are not an
      // image-index lookup (the authored data can contain 0xff here).
      out.pattern.push_back(k);
      unsigned time = b[anm + 12] ? b[anm + 16 + n + k] : 1;
      if (!time)
        throw std::runtime_error("Environment: Zero ANM duration");
      out.duration.push_back(time);
      auto t = DecodeTpl(palette, k);
      t.wrapS = t.wrapT = 0;
      if (t.width > 1024 || t.height > 1024 ||
          t.rgba.size() > 8 * 1024 * 1024 - budget)
        throw std::runtime_error("Environment: Effect texture budget");
      budget += t.rgba.size();
      out.images.push_back(std::move(t));
    }
    return out;
  }
  throw std::runtime_error("Environment: Effect texture missing: " +
                           std::to_string(id));
}
void checkRecord(const WaterRecord &r) {
  const auto &b = r.bytes;
  if (b[0] != 1 || (b[1] != 0 && b[1] != 8 && b[1] != 71) ||
      !(word(r, 8) & 0x10000) || b[6] || b[7] < 248 || b[7] > 253 || b[195] ||
      (word(r, 8) & 0x6000) || b[194] > 5 || b[3] || (word(r, 8) & ~0x3809eu))
    throw std::runtime_error(
        "Environment: EST is not a supported water image pass");
  if (b[264] > 1 || (b[264] && (b[265] != 0 || (b[267] & ~2u) || b[269] > 15 ||
                                half(r, 272) || b[296] > 16 || b[292] ||
                                b[293] || b[294] || b[295])))
    throw std::runtime_error("Environment: unsupported water emitter");
  if (b[1] == 8 && (b[264] || b[202] || b[203]))
    throw std::runtime_error("Environment: unsupported Esp08 mode");
  for (unsigned p : {88u, 92u, 100u, 104u, 112u, 116u, 124u, 128u})
    if (real(r, p) != 0)
      throw std::runtime_error("Environment: Image pass rotates outside its plane");
  for (unsigned p = 12; p <= 152; p += 4)
    if (std::abs(real(r, p)) > 10000)
      throw std::runtime_error("Environment: Effect parameter budget");
  for (unsigned p = 160; p <= 172; p += 4)
    if (real(r, p) < 0 || real(r, p) > 4)
      throw std::runtime_error("Environment: Effect color budget");
  for (unsigned p : {204u, 208u})
    if (std::abs(std::int64_t(static_cast<std::int32_t>(word(r, p)))) > 1000)
      throw std::runtime_error("Environment: Scroll budget");
  if (real(r, 136) <= 0 || real(r, 140) <= 0 || real(r, 136) > 10000 ||
      real(r, 140) > 10000 || real(r, 48) < 0 || real(r, 48) > 1 ||
      real(r, 152) < 0 || real(r, 152) > 1)
    throw std::runtime_error("Environment: Effect scale exceeds budget");
}
std::uint8_t visualRnd(std::uint16_t &v) {
  auto old = v;
  unsigned n = (unsigned(std::uint8_t((old >> 1) + (old >> 8))) << 8) |
               std::uint8_t(old >> 1);
  v = std::uint16_t(n == old ? n + 0x101 : n);
  return std::uint8_t(v >> 8);
}
EnvironmentMatrix multiply(const EnvironmentMatrix &a,
                           const EnvironmentMatrix &b) {
  EnvironmentMatrix out{};
  for (unsigned r = 0; r < 3; ++r) {
    for (unsigned c = 0; c < 3; ++c)
      for (unsigned k = 0; k < 3; ++k)
        out[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
    out[r * 4 + 3] = a[r * 4 + 3];
    for (unsigned k = 0; k < 3; ++k)
      out[r * 4 + 3] += a[r * 4 + k] * b[k * 4 + 3];
  }
  return out;
}
std::vector<WaterRecord> gridRecords(const std::vector<std::uint8_t> &bytes,
                                    unsigned sequence, bool est) {
  const Reader r{bytes}; validateEff(r);
  const auto ids=r.table(est?8:12,8), offsets=r.table(est?32:36,4);
  if(r.u32(ids)!=r.u32(offsets))throw std::runtime_error("Water: Sequence count");
  for(unsigned i=0;i<r.u32(ids);++i) {
    const auto list=ids+4+i*8;
    if(r.u16(list)!=sequence || r.u16(list+2)!=(est?0:0xffff) || r.u32(list+4))continue;
    const auto p=r.entry(offsets,i,48);const auto count=r.u16(p);
    if(!count || count>256 || bytes[p+36]!=16 || bytes[p+10]!=0xfe || r.u16(p+8))
      throw std::runtime_error("Water: Sequence header");
    r.range(p+48,std::size_t(count)*300);
    if(p+48+std::size_t(count)*300>r.end(p))throw std::runtime_error("Water: Sequence range");
    std::vector<WaterRecord> out(count);
    for(unsigned j=0;j<count;++j)std::copy_n(bytes.begin()+p+48+j*300,300,out[j].bytes.begin());
    const bool water=std::any_of(out.begin(),out.end(),[](const auto &v) {
      return v.bytes[264]==1&&(v.bytes[265]==0x42||v.bytes[265]==0x45);
    });
    if(water)for(unsigned q=12;q<=32;q+=4)if(r.u32(p+q))throw std::runtime_error("Water: Sequence transform");
    return out;
  }
  return {};
}
std::vector<std::vector<std::uint8_t>> waterNoise(const std::vector<std::uint8_t> &bytes) {
  const Reader r{bytes};validateEff(r);const auto ids=r.table(4,8), offsets=r.table(24,4);
  for(unsigned i=0;i<r.u32(ids);++i) {
    if(r.u16(ids+4+i*8)!=0xfe)continue;
    const auto p=r.entry(offsets,i,12); const auto end=r.end(p);
    std::vector<std::uint8_t> tpl(bytes.begin()+p,bytes.begin()+end);const Reader v{tpl};
    if(v.u32(0)!=0x20af30 || v.u32(4)!=60)throw std::runtime_error("Water: Original noise requires 60 frames");
    const auto table=v.u32(8);v.range(table,60*8);std::vector<std::vector<std::uint8_t>> out;
    for(unsigned j=0;j<60;++j) {
      const auto header=v.u32(table+j*8);v.range(header,36);
      if(v.u16(header)!=64 || v.u16(header+2)!=64 || v.u32(header+4)!=1)
        throw std::runtime_error("Water: Original noise is not I8 64x64");
      const auto data=v.u32(header+8);v.range(data,4096);
      out.emplace_back(tpl.begin()+data,tpl.begin()+data+4096);
    }
    return out;
  }
  throw std::runtime_error("Water: 0xfe noise is missing");
}
void buildGrids(EnvironmentData &data,const std::vector<std::uint8_t> &eff,
                const std::vector<std::uint8_t> &core) {
  const Reader reader{eff};validateEff(reader);const auto ids=reader.table(12,8);
  std::vector<std::pair<unsigned,bool>> sequences;
  for(unsigned i=0;i<reader.u32(ids);++i)if(reader.u16(ids+6+i*8)==0xffff && !reader.u32(ids+8+i*8))
    sequences.push_back({reader.u16(ids+4+i*8),false});
  // R10bInit explicitly creates EST2, then EST5 is its Esp4c controller while
  // Leon is ashore. EST3 belongs to the later boat/boss event and stays off.
  if(data.room==0x10b)sequences.push_back({2,true});
  if(data.room==0x31b)sequences.push_back({3,true}); // unconditional R31bInit
  for(const auto &sequence:sequences) {
    const auto entries=gridRecords(eff,sequence.first,sequence.second);
    for(const auto &r:entries) {
      const auto &b=r.bytes;
      if(b[264]!=1 || (b[265]!=0x42 && b[265]!=0x45))continue;
      if(!b[0] || half(r,4) || b[7]!=0xfe || (word(r,8)&~0xc003u))
        throw std::runtime_error("Water: unsupported generator lifecycle");
      WaterGrid g;g.generator=b[265];g.sequence=sequence.first;
      g.prerun=sequence.second?0:200;
      auto dimension=[](unsigned n) {n=n?std::min(n,184u):64u;return n&~7u;};
      g.width=dimension(b[252]);g.height=dimension(b[253]);
      if(!g.width || !g.height)throw std::runtime_error("Water: Zero-sized grid");
      g.originalSize=g.size=real(r,136);g.heightRate=1-float(b[254])/255;
      if(!g.heightRate)g.heightRate=.0001f;
      g.type=b[200];g.reflection=b[203];
      if(g.size<=0 || g.size>10000 || g.type>2 || g.reflection>3)
        throw std::runtime_error("Water: Generator parameters");
      if(g.type==1 && g.width<g.height)
        throw std::runtime_error("Water: Type 1 neighbor index outside grid bounds");
      if(g.type==2) {g.damping=std::clamp(.5f-float(static_cast<std::int8_t>(b[201]))*.005f,0.f,.5f);g.spread=.99f-float(b[202])*.001f;}
      for(unsigned k=0;k<3;++k) {g.position[k]=real(r,12+k*4);g.angle[k]=real(r,88+k*4)*6.28f/360;}
      if(g.generator==0x45)g.angle={}; // source uses angles only from Esp4c's parameter override
      for(unsigned k=0;k<4;++k) {g.color[k]=b[156+k];g.ambient[k]=std::uint8_t(std::clamp(real(r,160+k*4)*255,0.f,255.f));}
      g.shimmerS=float(static_cast<std::int16_t>(half(r,206)))*.001f+.01f;
      g.shimmerT=float(static_cast<std::int16_t>(half(r,210)))*.007f+.07f;
      g.bounded=(word(r,8)&1)!=0;g.masked=(word(r,8)&0x4000)!=0;if(g.masked)g.bounded=true;
      auto tex=[&](unsigned id) {try{return animation(core,id).images.at(0);}catch(const std::exception &){return animation(eff,id).images.at(0);}};
      g.specular=tex(b[2]);if(g.masked)g.mask=tex(b[197]);
      auto controllers=entries;
      if(data.room==0x10b) {const auto controller=gridRecords(eff,5,true);controllers.insert(controllers.end(),controller.begin(),controller.end());}
      for(const auto &c:controllers)if(!c.bytes[264] && c.bytes[1]==0x4c && !half(c,4)) {
        g.followCamera=false;g.position[0]=real(c,12);g.position[2]=real(c,20);g.size=real(c,136);
        if(word(c,8)&2)g.position[1]=real(c,16);
        const bool overwrite=(word(c,8)&1)!=0;
        for(unsigned k=0;k<4;++k) {
          g.color[k]=overwrite?c.bytes[156+k]:std::uint8_t(unsigned(g.color[k])*c.bytes[156+k]/255);
          g.ambient[k]=std::uint8_t(std::clamp((overwrite?255.f:float(g.ambient[k]))*real(c,160+k*4),0.f,255.f));
        }
        if(overwrite) {
          g.heightRate=1-float(c.bytes[254])/255;if(!g.heightRate)g.heightRate=.0001f;
          g.type=c.bytes[200];if(g.type>2)throw std::runtime_error("Water: Esp4c wave type");
          if(g.type==2) {g.damping=std::clamp(.5f-float(static_cast<std::int8_t>(c.bytes[201]))*.005f,0.f,.5f);g.spread=.99f-float(c.bytes[202])*.001f;}
          for(unsigned k=0;k<3;++k)g.angle[k]=real(c,88+k*4)*(3.14f/180);
          g.shimmerS=float(static_cast<std::int16_t>(half(c,206)))*.001f+.01f;
          g.shimmerT=float(static_cast<std::int16_t>(half(c,210)))*.007f+.07f;
          g.masked=(word(c,8)&0x4000)!=0;if(g.masked)g.mask=tex(c.bytes[197]);
        }
      }
      if(data.grids.size()>=2)throw std::runtime_error("Water: Original pool budget");
      data.grids.push_back(std::move(g));
    }
  }
  if(!data.grids.empty())data.gridNoise=waterNoise(eff);
}
} // namespace
EnvironmentData BuildRoomEnvironment(const RoomArchive &arc, const Smd &smd,
                                     const Smx &smx, std::uint16_t id,
                                     const std::vector<std::uint8_t> &core) {
  EnvironmentData data;
  data.room = id;
  data.signature = 1469598103934665603ULL ^ id;
  // Verified dome textures/geometries on supplied G4BE08 revision 0. These
  // are an explicit bounded room profile, not a heuristic for all scrolls.
  std::vector<unsigned> sky;
  if (id == 0x100)
    sky = {2};
  else if (id == 0x101)
    sky = {0}; // Supplied r101: BIN0, authored cloud TPL1, scroll from SMX0.
  else if (id == 0x300)
    sky = {11, 30};
  else if (id == 0x301)
    sky = {3, 20};
  for (auto work : sky) {
    auto p = std::find_if(smd.placements.begin(), smd.placements.end(),
                          [&](const auto &v) { return v.work == work; });
    if (p == smd.placements.end() || p->motion != 255 || (p->flags & 16) ||
        p->bin >= smd.models.size())
      throw std::runtime_error("Environment: expected sky object is missing");
    auto &m = smd.models[p->bin];
    const auto *r = smx.find(work);
    if (m.joints.size() != 1 || !r || r->type > 1)
      throw std::runtime_error("Environment: Unsupported sky motion");
    SkyObject o;
    o.work = work;
    o.placement = *p;
    o.joint = m.joints[0].center;
    if (r->type == 1) {
      WaterRecord temp;
      std::copy(r->objectWork.begin(), r->objectWork.end(), temp.bytes.begin());
      for (unsigned k = 0; k < 3; ++k)
        o.speed[k] = real(temp, k * 4);
      o.rotateJoint = (r->objectWork[12] & 1) != 0;
      if (r->objectWork[12] > 1)
        throw std::runtime_error("Environment: Rotation flag");
    }
    data.sky.push_back(o);
  }
  if (const auto *entry = arc.find("SMX"))
    data.signature =
        hash(data.signature, arc.copy(entry - arc.entries().data()));
  if (const auto *entry = arc.find("SMD"))
    data.signature =
        hash(data.signature, arc.copy(entry - arc.entries().data()));
  if (id == 0x100)
    data.surfaces = {{0, -1, 240, 2, 30, 128, false},
                     {1, -1, 240, 2, 30, 128, false}};
  else if (id == 0x300)
    data.surfaces = {{12, -1, 255, 0, 0, 255, false},
                     {14, -1, 255, 0, 0, 255, true}};
  else if (id == 0x301)
    data.surfaces = {{21, -1, 255, 0, 0, 255, false}};
  const auto *entry = arc.find("EFF");
  if (!entry) {
    if(!data.surfaces.empty())data.warnings.push_back("EFF missing; water uses base texture");
    return data;
  }
  auto eff = arc.copy(entry - arc.entries().data());
  data.signature = hash(hash(data.signature, eff), core);
  try {buildGrids(data,eff,core);} catch(const std::exception &e) {
    data.grids.clear();data.gridNoise.clear();data.warnings.push_back(e.what());
  }
  if(data.surfaces.empty())return data;
  std::vector<unsigned> sequences =
      id == 0x300 ? std::vector<unsigned>{0, 4}
                  : std::vector<unsigned>{id == 0x301 ? 3u : 0u};
  for (unsigned sequence : sequences) {
    auto animationCount = data.animations.size();
    try {
      WaterTarget t;
      t.sequence = sequence;
      if (id == 0x100)
        t.width = t.height = 64;
      else if (id == 0x300 && sequence == 4)
        t.width = t.height = 32;
      t.records = records(eff, sequence);
      // TexRender's OT receives only Tool_flg 0x10000. EST4 also contains
      // world effects; those are outside this texture target in the source.
      t.records.erase(
          std::remove_if(t.records.begin(), t.records.end(),
                         [](const auto &r) { return !(word(r, 8) & 0x10000); }),
          t.records.end());
      if (t.records.empty())
        throw std::runtime_error("Environment: EST without an image pass");
      unsigned lastTime = 0;
      for (auto &r : t.records) {
        if (half(r, 4) < lastTime)
          throw std::runtime_error("Environment: Unsorted EST times");
        lastTime = half(r, 4);
        checkRecord(r);
        unsigned tex = r.bytes[2];
        auto found =
            std::find_if(data.animations.begin(), data.animations.end(),
                         [&](const auto &a) { return a.id == tex; });
        if (found == data.animations.end()) {
          // Registration is core first; room textures cannot overwrite an id.
          WaterAnimation a;
          try {
            a = animation(core, tex);
          } catch (const std::exception &) {
            a = animation(eff, tex);
          }
          data.animations.push_back(std::move(a));
          r.animation = unsigned(data.animations.size() - 1);
        } else
          r.animation = unsigned(found - data.animations.begin());
        if (r.bytes[188] >= data.animations[r.animation].pattern.size())
          throw std::runtime_error("Environment: Starting pattern outside ANM bounds");
      }
      unsigned target = unsigned(data.targets.size());
      data.targets.push_back(std::move(t));
      for (auto &s : data.surfaces) {
        if ((id == 0x300 && ((sequence == 0 && s.work == 12) ||
                             (sequence == 4 && s.work == 14))) ||
            id == 0x100 || id == 0x301)
          s.target = int(target);
      }
    } catch (const std::exception &e) {
      data.animations.resize(animationCount);
      data.warnings.push_back("EST " + std::to_string(sequence) + ": " +
                              e.what() + "; base texture retained");
    }
  }
  return data;
}
void AttachSceneEnvironment(const EnvironmentData &data, StaticScene &scene) {
  scene.environment = data;
  for (auto &draw : scene.draws) {
    for (unsigned i = 0; i < data.sky.size(); ++i)
      if (draw.work == data.sky[i].work)
        draw.sky = int(i);
    for (unsigned i = 0; i < data.surfaces.size(); ++i)
      if (draw.work == data.surfaces[i].work)
        draw.water = int(i);
  }
}
EnvironmentMatrix SkyDeltaMatrix(const SkyObject &o,
                                 const std::array<float, 3> &a) {
  auto base = PlacementMatrix(o.placement), inverse = base;
  // Inverse affine matrix via inverse-transpose helper, transposed back.
  auto n = NormalMatrix(base);
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 3; ++c)
      inverse[r * 4 + c] = n[c * 3 + r];
  for (unsigned r = 0; r < 3; ++r) {
    inverse[r * 4 + 3] = 0;
    for (unsigned k = 0; k < 3; ++k)
      inverse[r * 4 + 3] -= inverse[r * 4 + k] * base[k * 4 + 3];
  }
  if (o.rotateJoint) {
    SmdPlacement rotation;
    rotation.scale = {1, 1, 1};
    rotation.rotation = a;
    auto around = PlacementMatrix(rotation);
    for (unsigned r = 0; r < 3; ++r) {
      around[r * 4 + 3] = o.joint[r];
      for (unsigned k = 0; k < 3; ++k)
        around[r * 4 + 3] -= around[r * 4 + k] * o.joint[k];
    }
    return multiply(multiply(base, around), inverse);
  }
  auto moved = o.placement;
  for (unsigned k = 0; k < 3; ++k)
    moved.rotation[k] += a[k];
  return multiply(PlacementMatrix(moved), inverse);
}
void ApplyEnvironmentSpriteMatrix(WaterSprite &s,const EnvironmentMatrix &matrix) {
    s.position = TransformPoint(matrix, s.position);
    for (auto *vector : {&s.speed, &s.acceleration}) {
      auto local = *vector;
      for (unsigned row = 0; row < 3; ++row) {
        (*vector)[row] = 0;
        for (unsigned col = 0; col < 3; ++col)
          (*vector)[row] += matrix[row * 4 + col] * local[col];
      }
    }
    if (s.flags & 1) {
      // cEsp::ApplyMatrix / math_sub::Matrix2AxisAngle. Matrix composition
      // happens once at spawn; the original angular velocity remains local.
      using namespace motion64;
      constexpr float worldRadians = 0.017453292f / (3.14f / 180.f);
      Vec angle{s.angle[0] * worldRadians, s.angle[1] * worldRadians,
                s.angle[2] * worldRadians};
      Mtx parent, rotation, combined, transposed, residual, inverse;
      std::memcpy(parent, matrix.data(), sizeof(parent));
      RotMatrix(rotation, &angle);
      PSMTXConcat(parent, rotation, combined);
      PSMTXTranspose(combined, transposed);
      angle.x = -std::atan2(-transposed[1][2], transposed[2][2]);
      angle.y = -std::asin(std::clamp(transposed[0][2], -1.f, 1.f));
      angle.z = 0;
      RotMatrix(rotation, &angle);
      if (!PSMTXInverse(rotation, inverse)) PSMTXIdentity(inverse);
      PSMTXConcat(combined, inverse, residual);
      angle.z = std::atan2(residual[1][0], residual[0][0]);
      s.angle = {angle.x / worldRadians, angle.y / worldRadians,
                 angle.z / worldRadians};
    }
}
EnvironmentSimulation::EnvironmentSimulation(const EnvironmentData &data,
                                              const SatCollision *floor)
    : data_(&data), floor_(floor) {
  state_.signature = data.signature;
  state_.random = data.initialRandom;
  state_.skyAngles.resize(data.sky.size());
  state_.recordCursor.resize(data.targets.size());
  state_.sequenceSeed.resize(data.targets.size(), 0x12345678);
  if (data.sourceSequenceSeeds)
    for (unsigned t = 0; t < data.targets.size(); ++t)
      if (!data.targets[t].records.empty()) {
        state_.sequenceSeed[t] = visualRnd(state_.random);
        state_.sequenceSeed[t] |= unsigned(visualRnd(state_.random)) << 8;
        state_.sequenceSeed[t] |= unsigned(visualRnd(state_.random)) << 16;
      }
  state_.grids.resize(data.grids.size());
  for(unsigned i=0;i<data.grids.size();++i) {
    const auto &g=data.grids[i];auto &s=state_.grids[i];
    const auto n=(g.width+1)*(g.height+1);
    s.position.resize(n);s.speed.resize(n);s.height.resize(n);s.normals.resize(n,{0,1,0});
    for(unsigned y=0;y<=g.height;++y)for(unsigned x=0;x<=g.width;++x) {
      unsigned bits=visualRnd(state_.random);bits|=unsigned(visualRnd(state_.random))<<8;
      bits|=(unsigned(visualRnd(state_.random))&0x7f)<<16;bits+=0x3f800000;
      float random;std::memcpy(&random,&bits,4);
      s.height[y*(g.width+1)+x]=(x && y && x<g.width && y<g.height)?(random*2-3)*.2f:0;
      if(g.generator==0x45)s.normals[y*(g.width+1)+x]={(float(x)-float(g.width/2))*(1.f/float(g.width)),.25f,(float(y)-float(g.height/2))*(1.f/float(g.height))};
    }
    for(unsigned frame=0;frame<=g.prerun;++frame)tickGrid(i);
  }
  activate();
  floor_ = nullptr;
}
void EnvironmentSimulation::tickGrid(unsigned index) {
  const auto &g=data_->grids.at(index);auto &s=state_.grids.at(index);
  if(data_->gridNoise.size()!=60)throw std::runtime_error("Water: Noise frames are missing");
  // EspGenLoopMove runs its 200 startup moves in one game frame. Noise and
  // pingpong parity follow Frame_cnt, rather than the number of move calls.
  const auto &noise=data_->gridNoise[state_.frames%60];const auto nx=g.width,ny=g.height;
  auto &current=(state_.frames&1)?s.position:s.speed;
  auto &next=(state_.frames&1)?s.speed:s.position;
  for(unsigned y=1;y<ny;++y) {
    // Source's cheap Type1 starts k at the row boundary; preserve its original
    // loop/index order, including nx/ny neighbour stride.
    unsigned k=y*(nx+1)+(g.type==1?0:1);
    for(unsigned x=1;x<nx;++x,++k) {
      const auto ni=((y<<6)&0xb00)+((x<<2)&0xa0)+((y&3)<<3)+(x&7);
      const float n=float(noise.at(ni))-80;
      if(g.type!=1) {
        const float sum=current[k-1]+current[k+1]+current[k-nx-1]+current[k+nx+1];
        float h=g.damping*sum+(2-g.damping*4)*current[k];h-=next[k];
        h=n*(g.generator==0x45?.001f:.0002f)+h;h*=g.spread;s.height[k]=next[k]=h;
      } else {
        const float sum=s.position[k-1]+s.position[k+1]+s.position[k-ny-1]+s.position[k+ny+1];
        s.speed[k]+=sum-s.position[k]*4;
        s.position[k]+=n*.0001f+s.speed[k]*.04f;s.speed[k]*=.92f;
        s.height[k]=n*.0018f+s.position[k];
      }
      std::array<float,3> normal{s.height[k-1]-s.height[k+1],2,
                               s.height[k-nx]-s.height[k+nx]};
      if(g.generator==0x45)for(auto &v:normal)v*=1.f/2.3f;
      else {const float length=std::sqrt(normal[0]*normal[0]+4+normal[2]*normal[2]);for(auto &v:normal)v/=length;}
      normal[0]+=(float(x)-float(nx/2))*(1.f/float(nx));
      normal[2]+=(float(y)-float(ny/2))*(1.f/float(ny));normal[1]*=.25f;
      s.normals[k]=normal;
    }
  }
  ++s.frames;
}
void EnvironmentSimulation::activate() {
  for (unsigned t = 0; t < data_->targets.size(); ++t) {
    const auto &records = data_->targets[t].records;
    auto &cursor = state_.recordCursor[t];
    while (cursor < records.size() &&
           half(records[cursor], 4) <= state_.frames) {
      unsigned index = cursor;
      const auto &rec = records[index];
      if (!rec.enabled) {
        ++cursor;
        continue;
      }
      if (!rec.bytes[264]) {
        if (state_.sprites.size() >= spriteBudget_)
          break;
        ++cursor;
        spawn(t, index, state_.sequenceSeed[t]);
        continue;
      }
      ++cursor;
      unsigned interval = rec.bytes[268], range = rec.bytes[296];
      if (range)
        interval = (interval + visualRnd(state_.random) % range) & 255;
      std::uint32_t seed;
      if (rec.bytes[267] & 2)
        seed = 0x12345678 + rec.bytes[270];
      else {
        seed = visualRnd(state_.random);
        seed |= unsigned(visualRnd(state_.random)) << 8;
        seed |= unsigned(visualRnd(state_.random)) << 16;
      }
      state_.emitters.push_back({t, index, 0, interval, seed});
    }
  }
}
bool EnvironmentSimulation::spawn(unsigned target, unsigned record,
                                  std::uint32_t &seed, bool spread,
                                  float spreadAngle) {
  const auto &r = data_->targets.at(target).records.at(record);
  if(r.externalSeedDraws) {
    state_.externalSpawns.push_back({target,record,seed,++state_.externalSerial});
    for(unsigned i=0;i<r.externalSeedDraws;++i)(void)fRandSeed1_1(&seed);
    return false; // Authored geometry is emitted by the cObj05 model backend.
  }
  if (state_.sprites.size() >= 512)
    throw std::runtime_error("Environment: Effect pool budget");
  std::optional<EnvironmentMatrix> pathMatrix;
  if(r.bytes[264]==1 && r.bytes[265]==2) {
    const auto &path=data_->recordPaths.at(target).at(record);
    if(path.size()<2) throw std::runtime_error("EVD EST: Path is missing");
    float t=(float(r.bytes[262])+fRandSeed0_1(&seed)*float(r.bytes[263]))*.01f;
    while(t>1) t-=1;
    while(t<0) t+=1;
    const float distance=std::clamp(path.back()[3]*t,0.f,path.back()[3]-1.01f);
    unsigned segment=1;
    while(segment+1<path.size() && path[segment][3]<distance) ++segment;
    const auto &a=path[segment-1],&b=path[segment];
    const float rate=(distance-a[3])/(b[3]-a[3]);
    std::array<float,3> point{},direction{};
    for(unsigned axis=0;axis<3;++axis)
      point[axis]=(a[axis]+(b[axis]-a[axis])*rate)*(1.f+real(r,280+axis*4)*.1f);
    motion64::Mtx rm;
    motion64::Vec rotation{float(r.bytes[297])*3.1415927f*2.f*.00390625f,
                          float(r.bytes[298])*3.1415927f*2.f*.00390625f,0};
    motion64::RotMatrix(rm,&rotation);
    EnvironmentMatrix rot;std::memcpy(rot.data(),rm,sizeof rm);
    point=TransformPoint(rot,point);
    EnvironmentMatrix matrix{1,0,0,point[0],0,1,0,point[1],0,0,1,point[2]};
    if(r.bytes[299]&3) {
      unsigned nextSegment=segment;
      while(nextSegment+1<path.size() && path[nextSegment][3]<distance+1)++nextSegment;
      const auto &nextA=path[nextSegment-1],&nextB=path[nextSegment];
      const float nextRate=(distance+1-nextA[3])/(nextB[3]-nextA[3]);
      float square=0;
      for(unsigned axis=0;axis<3;++axis) {
        direction[axis]=nextA[axis]+(nextB[axis]-nextA[axis])*nextRate-
            (a[axis]+(b[axis]-a[axis])*rate);square+=direction[axis]*direction[axis];
      }
      const float length=std::sqrt(square);
      if(length)for(float &value:direction)value/=length;
      // Source mode1: cross(dir,+Y), mode2: cross(-Y,dir).
      const std::array<float,3> right{-direction[2],0,direction[0]};
      std::array<float,3> up{right[1]*direction[2]-right[2]*direction[1],
          right[2]*direction[0]-right[0]*direction[2],
          right[0]*direction[1]-right[1]*direction[0]};
      if(!(r.bytes[299]&1))for(float &value:up)value=-value;
      const EnvironmentMatrix tangent{right[0],right[1],right[2],0,
          up[0],up[1],up[2],0,direction[0],direction[1],direction[2],0};
      matrix=multiply(rot,tangent);
      const std::array<float,3> pivot{real(r,12),real(r,16),real(r,20)};
      const auto moved=TransformPoint(matrix,pivot);
      for(unsigned axis=0;axis<3;++axis)matrix[axis*4+3]=point[axis]+pivot[axis]-moved[axis];
    }
    pathMatrix=matrix;
  }
  WaterSprite s;
  s.target = target;
  s.record = record;
  s.serial = ++state_.serial;
  s.flags = word(r, 8);
  if (s.flags & 8) {
    if (fRandSeed1_1(&seed) > 0)
      s.flags |= 2;
    else
      s.flags &= ~2u;
  }
  if (s.flags & 16) {
    if (fRandSeed1_1(&seed) > 0)
      s.flags |= 4;
    else
      s.flags &= ~4u;
  }
  auto v = [&](auto &out, unsigned p, unsigned random) {
    for (unsigned k = 0; k < 3; ++k)
      out[k] =
          real(r, p + k * 4) + real(r, random + k * 4) * fRandSeed1_1(&seed);
  };
  v(s.position, 12, 24);
  v(s.speed, 36, 52);
  if (spread) {
    const float c = std::cos(spreadAngle), sine = std::sin(spreadAngle);
    const float x = s.speed[0], z = s.speed[2];
    s.speed[0] = c * x + sine * z;
    s.speed[2] = -sine * x + c * z;
  }
  v(s.acceleration, 64, 76);
  v(s.angle, 88, 100);
  v(s.angleSpeed, 112, 124);
  for (unsigned k = 0; k < 3; ++k) {
    const float radians=(r.bytes[1]==0xfc || r.bytes[1]==0x48 || r.bytes[1]==0x4e ||
        r.bytes[1]==6 || r.bytes[1]==21) ? .017453292f : 3.14f / 180.f;
    s.angle[k] *= radians;
    s.angleSpeed[k] *= radians;
  }
  auto randomSize = real(r, 144) * fRandSeed1_1(&seed);
  if(r.bytes[1]==0xfc) {
    randomSize*=.005f;
    s.size={real(r,136)*.005f+randomSize,real(r,140)*.005f+randomSize};
  } else s.size = {real(r, 136) + randomSize, real(r, 140) + randomSize};
  s.sizeStep = real(r, 148);
  for (unsigned k = 0; k < 4; ++k)
    s.color[k] = r.bytes[156 + k];
  s.startAlpha = r.bytes[159];
  s.age = half(r, 186);
  s.pattern = r.bytes[188];
  s.animationCounter = half(r, 190);
  if(r.bytes[1]==0x48) s.sinePhase=fRandSeed1_1(&seed)*2.f*3.1415927f;
  if (data_->transformSpawn && r.bytes[7] == 0xff)
    ApplyEnvironmentSpriteMatrix(s,data_->spawnMatrix);
  if (target<data_->recordParents.size() && record<data_->recordParents[target].size() &&
      data_->recordParents[target][record] && r.bytes[7]<0xf8)
    s.parented=true;
  if(s.parented && (s.flags&0x20)) {
    // EspSeqSet: Tool20 uses owner rotation, joint translation and a second
    // independent R_pos draw in world coordinates. The first draws above are
    // still consumed, exactly as in the original generator.
    auto matrix=data_->spawnMatrix;
    matrix[3]=matrix[7]=matrix[11]=0;
    const auto offset=TransformPoint(matrix,{real(r,12),real(r,16),real(r,20)});
    const auto &parent=*data_->recordParents[target][record];
    for(unsigned k=0;k<3;++k)matrix[k*4+3]=parent[k*4+3]+offset[k];
    s.position={};ApplyEnvironmentSpriteMatrix(s,matrix);s.parented=false;
    for(unsigned k=0;k<3;++k)s.position[k]+=real(r,24+k*4)*fRandSeed1_1(&seed);
  }
  if (target<data_->recordEmitMatrices.size() && record<data_->recordEmitMatrices[target].size() &&
      data_->recordEmitMatrices[target][record])
    ApplyEnvironmentSpriteMatrix(s,*data_->recordEmitMatrices[target][record]);
  if(pathMatrix) ApplyEnvironmentSpriteMatrix(s,*pathMatrix);
  if(data_->spriteInitializer && !data_->spriteInitializer(s,r,seed))return false;
  if (r.bytes[1]==18) {
    auto world=s.position;
    if(s.parented) world=TransformPoint(*data_->recordParents[target][record],world);
    s.history.resize(unsigned(r.bytes[200])+2,world);
  }
  if(r.bytes[1]==3)s.history.resize(4);
  if(r.bytes[1]==9)s.history.resize(unsigned(std::clamp(4-int(r.bytes[200]),2,6)));
  if (r.bytes[1] == 0x10) {
    // Esp10::SetFreeWork, getFloor_attr: source SAT ray and +65, not a
    // guessed blood plane at enemy feet. Never retain the collision pointer.
    if(s.parented) {
      ApplyEnvironmentSpriteMatrix(s,*data_->recordParents[target][record]);
      s.parented=false;
    }
    if (!floor_) return false;
    const CollisionVec top{s.position[0], s.position[1] + 600,
                            s.position[2]};
    const CollisionVec bottom{s.position[0], s.position[1] - 100000,
                               s.position[2]};
    const auto hit = floor_->line(top, bottom, SatSubset::All, 0x40, 0);
    s.position[1] = float(hit ? hit->point[1] : -100000) + 65 + real(r, 220);
  }
  if(r.bytes[1]==0xfc && !moveEventModel(s,r)) return false;
  if(r.bytes[1]==10) {
    // Esp0a::SetFreeWork Type0 copies its source path up to fifty times.
    // Every copy is an id0 sprite with frozen physics/colour and Work8[0]
    // lifetime; CommonMove advances only the temporary predictor.
    auto base=s;
    for(unsigned i=0;i<50 && state_.sprites.size()<512;++i) {
      auto ghost=base;ghost.serial=++state_.serial;ghost.motionTrailGhost=true;
      ghost.motionTrailLife=r.bytes[200];ghost.age=0;ghost.speed={};
      ghost.sizeStep=0;state_.sprites.push_back(std::move(ghost));
      for(unsigned k=0;k<3;++k) {
        if(half(r,180)<=base.age) {
          base.position[k]+=base.speed[k];base.speed[k]+=base.acceleration[k];base.speed[k]*=real(r,48);
        }
        base.angle[k]+=base.angleSpeed[k];
      }
      if(half(r,182)<=base.age) {
        base.sizeMultiplier+=base.sizeStep;base.sizeStep*=real(r,152);
        if(base.sizeMultiplier<=0)break;
      }
      const unsigned fade=half(r,176),hold=half(r,178);
      if(fade<base.age) {
        if(fade+hold<=base.age) {
          for(unsigned k=0;k<4;++k)base.color[k]=std::min(255.f,base.color[k]*real(r,160+k*4));
          if(base.color[3]<4)break;
        }
      } else if(fade)base.color[3]=base.startAlpha*float(base.age)/float(fade);
      if(half(r,184) && half(r,184)<=base.age)break;
      ++base.age;
    }
    return true;
  }
  state_.sprites.push_back(s);
  return true;
}
void EnvironmentSimulation::tick(const EnvironmentCamera *camera,
                                 unsigned spriteBudget,
                                 const SatCollision *floor) {
  tickGenerators(camera,spriteBudget,floor);
  tickSprites(camera,floor);
}
void EnvironmentSimulation::tickGenerators(const EnvironmentCamera *camera,
                                           unsigned spriteBudget,
                                           const SatCollision *floor) {
  struct FloorScope {
    const SatCollision *&slot;
    ~FloorScope() { slot = nullptr; }
  } floorScope{floor_};
  floor_ = floor;
  spriteBudget_ = std::min(spriteBudget, 512u);
  if (camera)
    for (const auto *v : {&camera->eye, &camera->up})
      for (float x : *v)
        if (!std::isfinite(x))
          throw std::runtime_error("Environment: invalid effect camera");
  for (unsigned i = 0; i < data_->sky.size(); ++i)
    for (unsigned k = 0; k < 3; ++k)
      state_.skyAngles[i][k] += data_->sky[i].speed[k];
  activate();
  for (auto &e : state_.emitters) {
    const auto &r = data_->targets[e.target].records[e.record];
    const unsigned life = half(r, 272);
    const float rate = life ? float(e.age) / life : 0;
    auto curve = [&](unsigned p) {
      const int d = static_cast<std::int8_t>(r.bytes[p]);
      return d >= 0 ? 1.f - rate * float(d) * (1.f / 128.f)
                    : 1.f + rate * float(-d) * (10.f / 128.f);
    };
    if (!e.next) {
      int next = int(e.interval);
      if (life && r.bytes[295])
        next += int(rate * static_cast<std::int8_t>(r.bytes[295]));
      if (r.bytes[296])
        next +=
            int(visualRnd(state_.random) % (r.bytes[296] * 2)) - r.bytes[296];
      e.next = unsigned(std::clamp(next, 0, 255));
      unsigned count = unsigned(r.bytes[269]) + 1;
      // The original emitter waits if the entire burst cannot fit its pool.
      if (state_.sprites.size() > spriteBudget_ ||
          count > spriteBudget_ - state_.sprites.size())
        continue; // Original pool wait also holds Time_cnt on this return.
      for (unsigned i = 0; i < count; ++i) {
        if (!spawn(e.target, e.record, e.seed, (r.bytes[267] & 1) != 0,
                    6.28f / float(count) * float(i)))
          continue;
        auto &sprite = state_.sprites.back();
        if (life && r.bytes[292])
          for (auto &size : sprite.size)
            size *= curve(292);
        if (life && r.bytes[293])
          for (auto &speed : sprite.speed)
            speed *= curve(293);
        if (life && r.bytes[294]) {
          sprite.startAlpha = float(std::uint8_t(
              std::clamp(sprite.startAlpha * curve(294), 0.f, 255.f)));
          sprite.color[3] *= curve(294);
        }
      }
    } else
      --e.next;
    e.age = (e.age + 1) & 65535;
  }
  state_.emitters.erase(
      std::remove_if(state_.emitters.begin(), state_.emitters.end(),
                     [&](const auto &e) {
                       auto life = half(
                           data_->targets[e.target].records[e.record], 272);
                       return life && e.age >= life;
                     }),
      state_.emitters.end());
}
void EnvironmentSimulation::tickSprites(const EnvironmentCamera *camera,
                                        const SatCollision *floor) {
  struct FloorScope {
    const SatCollision *&slot;
    ~FloorScope(){slot=nullptr;}
  } floorScope{floor_};
  floor_=floor;
  auto dead = [&](WaterSprite &s) {
    const auto &r = data_->targets[s.target].records[s.record];
    if(data_->spriteMover) {
      const auto dead=data_->spriteMover(s,r,state_);
      if(dead)return *dead;
    }
    if(r.bytes[1]==0xfc)return !moveEventModel(s,r);
    if(s.motionTrailGhost) {
      if(s.motionTrailLife && s.age>=s.motionTrailLife)return true;
      s.age=(s.age+1)&65535u;
      const auto &a=data_->animations[r.animation];
      s.animationCounter=(s.animationCounter+unsigned(std::uint8_t(
          int(static_cast<std::int8_t>(r.bytes[189]))+32)))&65535;
      unsigned duration=a.duration.at(s.pattern);
      while((s.animationCounter>>5)>duration) {
        ++s.pattern;s.animationCounter-=duration<<5;
        if(s.pattern>=a.pattern.size()) {
          if(!a.loop)return true;
          s.pattern=a.loop==1 ? 0u : unsigned(a.pattern.size()-1);
        }
      }
      return false;
    }
    if(r.bytes[1]==72) for(unsigned k=0;k<3;++k)s.angle[k]-=s.sineOffset[k];
    if(s.parented && !(r.bytes[1]==14 && r.bytes[192]==0) &&
        r.bytes[192]!=255 && r.bytes[192]<=s.age) {
      ApplyEnvironmentSpriteMatrix(s,*data_->recordParents.at(s.target).at(s.record));
      s.parented=false;
    }
    if (r.bytes[1] == 11)
      for (unsigned k = 0; k < 3; ++k)
        s.position[k] -= s.cameraOffset[k];
    if (r.bytes[1] == 70)
      s.sizeMultiplier = 1e22f; // cEsp46::move
    if (r.bytes[1] != 1 && r.bytes[1]!=3 && (!half(r, 180) || half(r, 180) <= s.age))
      for (unsigned k = 0; k < 3; ++k) {
        s.position[k] += s.speed[k];
        s.speed[k] += s.acceleration[k];
        s.speed[k] *= real(r, 48);
      }
    if (!half(r, 182) || half(r, 182) <= s.age) {
      s.sizeMultiplier += s.sizeStep;
      s.sizeStep *= real(r, 152);
      if (s.sizeMultiplier <= 0)
        return true;
    }
    if (r.bytes[1] != 1 && r.bytes[1]!=3)
      for (unsigned k = 0; k < 3; ++k)
        s.angle[k] += s.angleSpeed[k];
    unsigned fade = half(r, 176), hold = half(r, 178);
    if (fade < s.age) {
      if (fade + hold <= s.age) {
        for (unsigned k = 0; k < 4; ++k)
          s.color[k] = std::min(255.f, s.color[k] * real(r, 160 + k * 4));
        if (s.color[3] < 4)
          return true;
      }
    } else if (fade) {
      float rate = float(s.age) / fade;
      if (r.bytes[194] == 3)
        for (unsigned k = 0; k < 3; ++k)
          s.color[k] = r.bytes[156 + k] * rate;
      s.color[3] = s.startAlpha * rate;
    }
    const unsigned life=half(r,184) ? half(r,184) : r.bytes[1]==7 ? 128u : 0u;
    if (life && life <= s.age)
      return true;
    s.age = (s.age + 1) & 65535;
    if(r.bytes[1]==72) {
      s.sinePhase+=.01f;
      for(unsigned k=0;k<3;++k) {
        s.sineOffset[k]=real(r,216+k*12)*.1f*motion64::sinf(s.sinePhase*real(r,220+k*12));
        s.angle[k]+=s.sineOffset[k];
      }
    }
    if(r.bytes[1]==7 && !(s.bounceFlags&1)) {
      if(!(s.bounceFlags&2)) {
        if(!floor_)throw std::runtime_error("Esp07: Original SAT is missing");
        const CollisionVec top{s.position[0],s.position[1]+600,s.position[2]},
            bottom{s.position[0],s.position[1]-100000,s.position[2]};
        const auto hit=floor_->line(top,bottom,SatSubset::All,0x40,0);
        s.bounceFloor=float(hit ? hit->point[1] : -100000);s.bounceFlags|=2;
      }
      const float halfHeight=s.size[1]*.5f*s.sizeMultiplier;
      if(s.position[1]-halfHeight<s.bounceFloor) {
        s.position[1]=s.bounceFloor+halfHeight;
        s.speed[0]*=real(r,216)*.1f;s.speed[1]*=-real(r,220)*.1f;
        s.speed[2]*=real(r,216)*.1f;
        if(std::sqrt(s.speed[0]*s.speed[0]+s.speed[1]*s.speed[1]+s.speed[2]*s.speed[2])<10) {
          s.bounceFlags=1;s.speed={};s.acceleration={};
        }
      }
    }
    if(r.bytes[1]==3) {
      if(!half(r,180) || half(r,180)<=s.age) {
        const auto previous=s.history[0];
        if(r.bytes[201] && floor) {
          auto from=previous,to=previous;
          for(unsigned k=0;k<3;++k){from[k]+=s.position[k];to[k]=from[k]+s.speed[k];}
          if(floor->line({from[0],from[1],from[2]},{to[0],to[1],to[2]},SatSubset::All,0))return true;
        }
        for(unsigned i=3;i>0;--i)s.history[i]=s.history[i-1];
        for(unsigned k=0;k<3;++k) {s.history[0][k]=previous[k]+s.speed[k];s.speed[k]=(s.speed[k]+s.acceleration[k])*real(r,48);}
      }
      s.lineWidth=unsigned(std::uint16_t(s.size[0]*s.sizeMultiplier/(100.f/3.f)));
      return false; // Esp03 draws untextured GX line strips, no AnmMove.
    }
    if(r.bytes[1]==9) {
      const auto world=s.parented?TransformPoint(*data_->recordParents.at(s.target).at(s.record),s.position):s.position;
      const bool first=s.age==half(r,186)+1;
      if(first)std::fill(s.history.begin(),s.history.end(),world);
      else {
        for(unsigned i=unsigned(s.history.size()-1);i>0;--i)s.history[i]=s.history[i-1];
        s.history[0]=world;
      }
      s.lineWidth=unsigned(std::uint16_t(s.size[0]*s.sizeMultiplier*(.3f*.1f)));
      if(!first && camera) {
        float square=0;
        for(unsigned k=0;k<3;++k)square+=(world[k]-camera->eye[k])*(world[k]-camera->eye[k]);
        const float distance=std::sqrt(square)/5000.f;
        if(distance>1)s.lineWidth=unsigned(std::uint16_t(float(s.lineWidth)/distance));
      }
      return false; // Untextured source Esp09 has no AnmMove.
    }
    if (r.bytes[1] == 70 || r.bytes[1]==17)
      return false; // Esp46 has no AnmMove or texture
    const auto &a = data_->animations[r.animation];
    s.animationCounter =
        (s.animationCounter +
         unsigned(
             std::uint8_t(int(static_cast<std::int8_t>(r.bytes[189])) + 32))) &
        65535;
    // Original uses strict '>' and the current pattern's duration per call.
    unsigned duration = a.duration.at(s.pattern);
    while ((s.animationCounter >> 5) > duration) {
      ++s.pattern;
      s.animationCounter -= duration << 5;
      if (s.pattern >= a.pattern.size()) {
        if (a.loop == 0)
          return true;
        s.pattern = a.loop == 1 ? 0 : unsigned(a.pattern.size() - 1);
      }
    }
    if (r.bytes[1] == 8)
      for (unsigned k = 0; k < 2; ++k) {
        auto signedSpeed = static_cast<std::int32_t>(word(r, 204 + k * 4));
        s.scroll[k] += float(signedSpeed) * .001f;
        while (s.scroll[k] > 1)
          s.scroll[k] -= 1;
        while (s.scroll[k] < 0)
          s.scroll[k] += 1;
      }
    // Esp47's screen wrapping occurs on draw, after CommonMove/AnmMove.
    if (r.bytes[1] == 71) {
      while (s.position[0] > 480)
        s.position[0] -= 448;
      while (s.position[0] < 32)
        s.position[0] += 448;
      while (s.position[1] > 448)
        s.position[1] -= 448;
      while (s.position[1] < 0)
        s.position[1] += 448;
    }
    if (r.bytes[1] == 11) {
      EnvironmentCamera fallback;
      const auto &view = camera ? *camera : fallback;
      std::array<float, 3> look{}, side{};
      float length = 0;
      for (unsigned k = 0; k < 3; ++k) {
        look[k] = s.position[k] - view.eye[k];
        length += look[k] * look[k];
      }
      if (length > 0) {
        length = std::sqrt(length);
        for (auto &v : look)
          v /= length;
      }
      side = {look[1] * view.up[2] - look[2] * view.up[1],
              look[2] * view.up[0] - look[0] * view.up[2],
              look[0] * view.up[1] - look[1] * view.up[0]};
      auto jitter = [&]() {
        unsigned a = visualRnd(state_.random), b = visualRnd(state_.random),
                 c = visualRnd(state_.random);
        unsigned bits = a + (b << 8) + ((c & 0x7f) << 16) + 0x3f800000;
        float v;
        std::memcpy(&v, &bits, 4);
        return v * 2.f - 3.f;
      };
      // fRand1_1 calls are retained even when the authored x/y amplitude is
      // zero.
      const float x = real(r, 216) * jitter(), y = real(r, 220) * jitter(),
                  z = -real(r, 224);
      for (unsigned k = 0; k < 3; ++k) {
        s.cameraOffset[k] = look[k] * z + side[k] * x + view.up[k] * y;
        s.position[k] += s.cameraOffset[k];
      }
    }
    if(r.bytes[1]==18 && !s.history.empty()) {
      for(unsigned i=unsigned(s.history.size()-1);i>0;--i) s.history[i]=s.history[i-1];
      s.history[0]=s.parented?TransformPoint(*data_->recordParents.at(s.target).at(s.record),s.position):s.position;
    }
    for (auto v : s.position)
      if (!std::isfinite(v))
        throw std::runtime_error("Environment: Effect position overflow");
    return false;
  };
  state_.sprites.erase(
      std::remove_if(state_.sprites.begin(), state_.sprites.end(), dead),
      state_.sprites.end());
  ++state_.frames;
  for(unsigned i=0;i<data_->grids.size();++i)tickGrid(i);
}
bool EnvironmentSimulation::restore(const EnvironmentState &state) {
  if (state.signature != data_->signature ||
      state.skyAngles.size() != data_->sky.size() ||
      state.recordCursor.size() != data_->targets.size() ||
      state.sequenceSeed.size() != data_->targets.size() ||
      state.grids.size()!=data_->grids.size() ||
      state.sprites.size() > 512 || state.emitters.size() > 512 || state.externalSpawns.size()>512)
    return false;
  for(unsigned i=0;i<state.grids.size();++i) {
    const auto &g=data_->grids[i];const auto &s=state.grids[i];
    const auto n=(g.width+1)*(g.height+1);
    if(s.frames!=state.frames+g.prerun+1)return false;
    if(s.position.size()!=n || s.speed.size()!=n || s.height.size()!=n || s.normals.size()!=n)return false;
    for(unsigned k=0;k<n;++k) {
      if(!std::isfinite(s.position[k]) || !std::isfinite(s.speed[k]) || !std::isfinite(s.height[k]))return false;
      for(const auto v:s.normals[k])if(!std::isfinite(v))return false;
    }
  }
  for (unsigned t = 0; t < state.recordCursor.size(); ++t)
    if (state.recordCursor[t] > data_->targets[t].records.size())
      return false;
  for (const auto &e : state.emitters) {
    if (e.target >= data_->targets.size() ||
        e.record >= data_->targets[e.target].records.size() || e.next > 255 ||
        e.age > 65535 || !data_->targets[e.target].records[e.record].enabled ||
        data_->targets[e.target].records[e.record].bytes[264] != 1)
      return false;
    auto life = half(data_->targets[e.target].records[e.record], 272);
    if (life && e.age >= life)
      return false;
  }
  for (const auto &s : state.sprites) {
    if (s.target >= data_->targets.size() ||
        s.record >= data_->targets[s.target].records.size())
      return false;
    const auto &r = data_->targets[s.target].records[s.record];
    if (!r.enabled)
      return false;
    if(s.parented && (s.target>=data_->recordParents.size() ||
        s.record>=data_->recordParents[s.target].size() ||
        !data_->recordParents[s.target][s.record] || r.bytes[7]>=0xf8)) return false;
    const unsigned historySize=r.bytes[1]==18 ? unsigned(r.bytes[200])+2 :
        r.bytes[1]==9 ? unsigned(std::clamp(4-int(r.bytes[200]),2,6)) : r.bytes[1]==3 ? 4u : 0u;
    if(s.history.size()!=historySize || s.lineWidth>65535 || s.bounceFlags>3 ||
        !std::isfinite(s.bounceFloor)) return false;
    if(!std::isfinite(s.sinePhase))return false;
    for(float value:s.sineOffset)if(!std::isfinite(value))return false;
    if(s.motionTrailGhost && (r.bytes[1]!=10 || s.motionTrailLife!=r.bytes[200] ||
        (s.motionTrailLife && s.age>s.motionTrailLife)))return false;
    for(const auto &point:s.history)for(float value:point)if(!std::isfinite(value))return false;
    if (r.bytes[1] != 70 && r.bytes[1]!=17 && r.bytes[1]!=9 && r.bytes[1]!=3 && r.bytes[1]!=0xfc &&
        (r.animation >= data_->animations.size() ||
         s.pattern >= data_->animations[r.animation].pattern.size()))
      return false;
    for (const auto *values : {&s.position, &s.cameraOffset})
      for (auto x : *values)
        if (!std::isfinite(x))
          return false;
  }
  for(const auto &spawn:state.externalSpawns)
    if(spawn.target>=data_->targets.size() || spawn.record>=data_->targets[spawn.target].records.size() ||
        !data_->targets[spawn.target].records[spawn.record].externalSeedDraws ||
        !spawn.serial || spawn.serial>state.externalSerial)return false;
  state_ = state;
  return true;
}
std::vector<EnvironmentExternalSpawn> EnvironmentSimulation::drainExternalSpawns() {
  auto result=std::move(state_.externalSpawns);state_.externalSpawns.clear();return result;
}
std::vector<WaterQuad> WaterQuads(const EnvironmentData &data,
                                  const EnvironmentState &state) {
  std::vector<WaterQuad> out;
  for (const auto &s : state.sprites) {
    const auto &r = data.targets.at(s.target).records.at(s.record);
    const auto &a = data.animations.at(r.animation);
    float sx = s.size[0] * s.sizeMultiplier, sy = s.size[1] * s.sizeMultiplier;
    float ox = a.centerX ? float(-a.centerX) / a.width : -.5f;
    float oy = a.centerY ? float(a.centerY) / a.height - 1 : -.5f;
    WaterQuad q;
    q.animation = r.animation;
    q.image = s.pattern;
    q.blend = r.bytes[194];
    q.target = s.target;
    q.part = r.bytes[7];
    q.serial = s.serial;
    q.color = s.color;
    for (auto &c : q.color)
      c = float(std::clamp(int(c), 0, 255)) / 255.f;
    if (r.bytes[1] == 8)
      for (unsigned k = 0; k < 2; ++k)
        q.repeats[k] = r.bytes[200 + k] * .1f + 1;
    if (q.blend == 4) {
      for (unsigned k = 0; k < 3; ++k)
        q.color[k] =
            float(int(s.color[k] * s.color[3] * (1.f / 255.f))) / 255.f;
      q.color[3] = 1;
    }
    q.rgbScale = (s.flags & 0x80) ? 4.f : 1.f;
    q.alphaScale = (s.flags & 0x20000) ? 4.f : 1.f;
    q.scroll = s.scroll;
    q.tiled = r.bytes[1] == 8;
    q.flipS = (s.flags & 2) != 0;
    q.flipT = !(s.flags & 4);
    q.updateAlpha = (s.flags & 0x8000) != 0;
    // Screen sprites flip t by default. Reflect the pivot and rotate the quad
    // exactly as EspCommonTrans's screen matrix; esp08 keeps its unflipped
    // geometry and flips texture coordinates instead.
    float fx = r.bytes[1] != 8 && (s.flags & 2) ? -1 : 1;
    float fy = r.bytes[1] != 8 && !(s.flags & 4) ? -1 : 1;
    float y0 = r.bytes[1] == 8
                   ? (a.centerY ? float(a.centerY) / a.height : .5f) * sy
                   : oy * sy;
    float y1 = r.bytes[1] == 8 ? y0 - sy : (oy + 1) * sy;
    const std::array<std::array<float, 2>, 4> base{{{ox * sx, y0},
                                                    {(ox + 1) * sx, y0},
                                                    {(ox + 1) * sx, y1},
                                                    {ox * sx, y1}}};
    float c = std::cos(s.angle[2]), sn = std::sin(s.angle[2]);
    for (unsigned k = 0; k < 4; ++k) {
      float x = base[k][0] * fx, y = base[k][1] * fy;
      q.position[k] = {s.position[0] + x * c - y * sn,
                       s.position[1] + x * sn + y * c};
    }
    q.uv = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    out.push_back(q);
    if (r.bytes[1] == 71) {
      float w = ox * sx, h = oy * sy;
      std::vector<float> dx{0}, dy{0};
      if (s.position[0] + w < 32)
        dx.push_back(448);
      if (s.position[0] - w > 480)
        dx.push_back(-448);
      if (s.position[1] + h < 0)
        dy.push_back(448);
      if (s.position[1] - h > 448)
        dy.push_back(-448);
      for (auto x : dx)
        for (auto y : dy)
          if (x || y) {
            auto copy = q;
            for (auto &p : copy.position) {
              p[0] += x;
              p[1] += y;
            }
            out.push_back(copy);
          }
    }
  }
  // Source OT buckets: F8 first, FA pre1, F9 pre2, FD screen, FC after1,
  // FB after2. The numeric part order is not the draw order.
  constexpr unsigned rank[6]{6, 4, 5, 1, 2, 3};
  std::stable_sort(out.begin(), out.end(), [&](const auto &a, const auto &b) {
    auto x = rank[a.part - 248], y = rank[b.part - 248];
    return x > y || (x == y && a.serial > b.serial);
  });
  return out;
}
std::string EnvironmentData::describe() const {
  std::ostringstream s;
  s << "Sky: " << sky.size()
    << " original objects · Water: " << targets.size() << " effect targets / "
    << surfaces.size() << " faces / " << grids.size() << " original raster";
  for (const auto &w : warnings)
    s << "\n" << w;
  return s.str();
}
std::vector<WaterGridVertex> WaterGridMesh(const WaterGrid &g,
                                         const WaterGridState &s,
                                         const std::array<float,3> &target) {
  const auto nx=g.width,ny=g.height,n=(nx+1)*(ny+1);
  if(s.height.size()!=n || s.normals.size()!=n)throw std::runtime_error("Water: Raster state");
  SmdPlacement placement;placement.scale={g.size,g.size*.05f+100,g.size};
  placement.rotation=g.angle;
  placement.position=g.position;
  if(g.generator==0x45 && g.followCamera) {placement.position[0]=target[0];placement.position[2]=target[2];}
  auto m=PlacementMatrix(placement);m[5]*=g.heightRate;
  std::vector<WaterGridVertex> out;out.reserve((nx+1)*2*ny+24);
  auto vertex=[&](std::array<float,3> p,std::array<float,3> norm,std::array<float,2> uv) {
    WaterGridVertex v;v.uv=uv;v.normal=norm;
    for(unsigned r=0;r<3;++r) {v.position[r]=m[r*4+3];for(unsigned c=0;c<3;++c)v.position[r]+=m[r*4+c]*p[c];}
    out.push_back(v);
  };
  auto cell=[&](unsigned x,unsigned y) {const auto k=y*(nx+1)+x;
    vertex({float(x)-float(nx/2),s.height[k],float(y)-float(ny/2)},s.normals[k],{float(x)/float(nx+1),float(y)/float(ny+1)});};
  for(unsigned y=0;y<ny;++y) {
    if(!(y&1))for(unsigned x=0;x<=nx;++x) {cell(x,y);cell(x,y+1);}
    else for(int x=int(nx);x>=0;--x) {cell(unsigned(x),y);cell(unsigned(x),y+1);}
  }
  if(g.generator==0x45 && !g.bounded) {
    const float x=float(nx)/2,z=float(ny)/2,far=15;
    struct Corner {float x,z,nx,nz;};
    auto quad=[&](std::array<Corner,4> corners) {
      constexpr unsigned order[6]{0,1,2,0,2,3};constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
      for(auto k:order) {const auto c=corners[k];vertex({c.x,0,c.z},{c.nx,.25f,c.nz},uv[k]);}
    };
    quad({Corner{-x*far,-z*far,-.5f,-.5f*far},Corner{x*far,-z*far,.5f,-.5f*far},Corner{x,-z,.5f,-.5f},Corner{-x,-z,-.5f,-.5f}});
    quad({Corner{x,-z,.5f,-.5f},Corner{x*far,-z*far,.5f*far,-.5f},Corner{x*far,z*far,.5f*far,.5f},Corner{x,z,.5f,.5f}});
    quad({Corner{-x,z,-.5f,.5f},Corner{x,z,.5f,.5f},Corner{x*far,z*far,.5f,.5f*far},Corner{-x*far,z*far,-.5f,.5f*far}});
    quad({Corner{-x*far,-z*far,-.5f*far,-.5f},Corner{-x,-z,-.5f,-.5f},Corner{-x,z,-.5f,.5f},Corner{-x*far,z*far,-.5f*far,.5f}});
  }
  return out;
}
std::optional<EnvironmentWaterContact> QueryEnvironmentWaterContact(
    const EnvironmentData &data, const std::array<double,3> &point,
    const std::array<float,3> &cameraTarget) {
  // These Init functions explicitly disable GetWaterHeight, while their
  // decorative water geometry remains visible.
  switch(data.room) {
  case 0x102:case 0x112:case 0x222:case 0x303:case 0x31a:case 0x31b:return {};
  default:break;
  }
  for(const auto v:point)if(!std::isfinite(v) || std::abs(v)>1e7)return {};
  std::optional<EnvironmentWaterContact> result;
  // Source queries room water42 first, then weather water45. An unbounded
  // water45 plane replaces the height even when a water42 plane is higher.
  for(const auto generator:{0x42u,0x45u})for(unsigned i=0;i<data.grids.size();++i) {
    const auto &g=data.grids[i];if(g.generator!=generator)continue;
    if(!g.width || !g.height || !std::isfinite(g.size) || g.size<=0)continue;
    SmdPlacement p;p.scale={g.size,g.size*.05f+100,g.size};p.rotation=g.angle;p.position=g.position;
    if(generator==0x45 && g.followCamera) {p.position[0]=cameraTarget[0];p.position[2]=cameraTarget[2];}
    auto matrix=PlacementMatrix(p),collision=matrix;
    matrix[5]*=g.heightRate;
    if(generator==0x45)collision=matrix; //42 stores Inv_mat before applying its rate
    float height=p.position[1];
    if(generator==0x42 || g.bounded) {
      using namespace motion64;
      Mtx world,inverse;std::memcpy(world,collision.data(),sizeof(world));
      if(!PSMTXInverse(world,inverse))continue;
      Vec in{float(point[0]),float(point[1]),float(point[2])},local;
      PSMTXMultVec(inverse,&in,&local);
      const auto halfX=float(g.width/2),halfZ=float(g.height/2);
      if(local.x < -halfX || local.x > halfX || local.z < -halfZ || local.z > halfZ)continue;
      // GetWaterHeightCore flattens grid-space y and transforms back. It
      // deliberately does not sample the animated ripple heights.
      height=matrix[4]*local.x+matrix[6]*local.z+matrix[7];
      if(result && height<=result->height)continue;
    }
    result=EnvironmentWaterContact{height,height-float(point[1]),i,generator,float(point[1])<height-90.f};
  }
  return result;
}
} // namespace re4
