// Source: frozen game/game.cpp, eff_sys.cpp, est.cpp, esp_sub.cpp and
// esp08.cpp, esp0b.cpp, esp46.cpp and st1/r101.cpp.
#include "world_effects.h"
#include "laser_sight.h"
#include "archive.h"
#include "room_objects.h"
#include "scene.h"
#include "ports/motion64/runtime.h"
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
      throw std::runtime_error("Room effect: Truncated subfile");
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
      throw std::runtime_error("Room effect: Table limit");
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
        throw std::runtime_error("Room effect: Table limit");
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
      throw std::runtime_error("Room effect: Table index");
    auto delta = u32(table + 4 + std::size_t(i) * 4);
    if (delta < 4 + u32(table) * 4 || delta > b.size() - table)
      throw std::runtime_error("Room effect: invalid relative offset");
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
    throw std::runtime_error("Room effect: NaN/Inf in effect");
  return f;
}
std::uint64_t hash(std::uint64_t h, const std::vector<std::uint8_t> &b) {
  for (auto x : b)
    h = (h ^ x) * 1099511628211ULL;
  return h;
}
void validateEff(const Reader &r) {
  r.range(0, 48);
  if (r.b.size() > 16 * 1024 * 1024 || r.u32(0) != 11)
    throw std::runtime_error("Room effect: EFF version/size");
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
      throw std::runtime_error("Room effect: TPL/ANM count");
    unsigned bytes = b[anm + 12] ? n * 2 : 0;
    r.range(anm + 16, bytes);
    if (anm + 16 + bytes > r.end(anm))
      throw std::runtime_error("Room effect: ANM range");
    WaterAnimation out;
    out.id = id;
    out.width = r.u16(anm);
    out.height = r.u16(anm + 2);
    out.centerX = static_cast<std::int16_t>(r.u16(anm + 4));
    out.centerY = static_cast<std::int16_t>(r.u16(anm + 6));
    out.loop = b[anm + 11] & 3;
    if (out.loop == 3)
      throw std::runtime_error("Room effect: ANM loop mode");
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
        throw std::runtime_error("Room effect: Effect texture budget");
      // EspTexSet binds Ptn_no directly. The first Frames bytes are not an
      // image-index lookup (the authored data can contain 0xff here).
      out.pattern.push_back(k);
      unsigned time = b[anm + 12] ? b[anm + 16 + n + k] : 1;
      if (!time)
        throw std::runtime_error("Room effect: Zero ANM duration");
      out.duration.push_back(time);
      auto t = DecodeTpl(palette, k);
      t.wrapS = t.wrapT = 0;
      if (t.width > 1024 || t.height > 1024 ||
          t.rgba.size() > 8 * 1024 * 1024 - budget)
        throw std::runtime_error("Room effect: Effect texture budget");
      budget += t.rgba.size();
      out.images.push_back(std::move(t));
    }
    return out;
  }
  throw std::runtime_error("Room effect: Effect texture missing: " +
                           std::to_string(id));
}

std::vector<WaterRecord> worldRecords(const std::vector<std::uint8_t> &b,
                                      unsigned id) {
  Reader r{b};
  validateEff(r);
  auto ids = r.table(12, 8), offsets = r.table(36, 4);
  if (r.u32(ids) != r.u32(offsets))
    throw std::runtime_error("Room effect: SST table count");
  for (unsigned i = 0; i < r.u32(ids); ++i) {
    auto list = ids + 4 + i * 8;
    if (r.u16(list) != id)
      continue;
    // Room-global SstSet calls only type FFFF; area activation stays with EAR.
    if (r.u16(list + 2) != 0xffff || r.u32(list + 4))
      throw std::runtime_error("Room effect: SST is not globally active");
    auto p = r.entry(offsets, i, 48);
    unsigned n = r.u16(p);
    if (!n || n > 256 || b[p + 36] != 16 || b[p + 10] != 0xfe)
      throw std::runtime_error("Room effect: SST header");
    r.range(p + 48, std::size_t(n) * 300);
    if (p + 48 + std::size_t(n) * 300 > r.end(p) || r.u16(p + 8) ||
        r.u32(p + 12) || r.u32(p + 16) || r.u32(p + 20) || r.u32(p + 24) ||
        r.u32(p + 28) || r.u32(p + 32))
      throw std::runtime_error("Room effect: SST range/transform");
    std::vector<WaterRecord> out(n);
    for (unsigned k = 0; k < n; ++k)
      std::copy_n(b.begin() + p + 48 + k * 300, 300, out[k].bytes.begin());
    return out;
  }
  throw std::runtime_error("Room effect: SST is missing");
}
std::vector<WaterRecord> startupRecords(const std::vector<std::uint8_t> &b,
                                        unsigned id,
                                        bool explicitPlacement = false) {
  Reader r{b};
  validateEff(r);
  auto ids = r.table(8, 8), offsets = r.table(32, 4);
  if (r.u32(ids) != r.u32(offsets))
    throw std::runtime_error("Room effect: EST table count");
  for (unsigned i = 0; i < r.u32(ids); ++i) {
    auto list = ids + 4 + i * 8;
    if (r.u16(list) != id)
      continue;
    // Explicit EstSet resolves EST id/type 0; it is separate from the SST
    // type-FFFF table consumed by room-global EffAreaUpdate.
    if (r.u16(list + 2) != 0 || r.u32(list + 4))
      throw std::runtime_error("Room effect: EST is not globally active");
    auto p = r.entry(offsets, i, 48);
    unsigned n = r.u16(p);
    if (!n || n > 256 || b[p + 36] != 16 || b[p + 10] != 0xfe)
      throw std::runtime_error("Room effect: EST header");
    r.range(p + 48, std::size_t(n) * 300);
    if (p + 48 + std::size_t(n) * 300 > r.end(p) || r.u16(p + 8) ||
        (!explicitPlacement &&
         (r.u32(p + 12) || r.u32(p + 16) || r.u32(p + 20) || r.u32(p + 24) ||
          r.u32(p + 28) || r.u32(p + 32))))
      throw std::runtime_error("Room effect: EST range/transform");
    std::vector<WaterRecord> out(n);
    for (unsigned k = 0; k < n; ++k)
      std::copy_n(b.begin() + p + 48 + k * 300, 300, out[k].bytes.begin());
    return out;
  }
  throw std::runtime_error("Room effect: EST is missing");
}
void checkRecord(const WaterRecord &r) {
  const auto &b = r.bytes;
  // Bounded, source-backed Esp00/Esp08 world subset. No masked sprites,
  // parent models, light channels, shimmer or camera-dependent flags.
  if (b[0] != 1 || (b[1] != 0 && b[1] != 8) || b[3] || b[6] || b[7] != 0xfe ||
      b[195] || b[194] > 5 || b[264] || (word(r, 8) & ~0x40009fu))
    throw std::runtime_error("Room effect: unsupported world sprite");
  for (unsigned p = 12; p <= 152; p += 4)
    if (std::abs(real(r, p)) > 100000)
      throw std::runtime_error("Room effect: Parameter budget");
  for (unsigned p = 160; p <= 172; p += 4)
    if (real(r, p) < 0 || real(r, p) > 4)
      throw std::runtime_error("Room effect: Color budget");
  if (real(r, 136) <= 0 || real(r, 140) <= 0 || real(r, 136) > 20000 ||
      real(r, 140) > 20000 || real(r, 48) < 0 || real(r, 48) > 1 ||
      real(r, 152) < 0 || real(r, 152) > 1)
    throw std::runtime_error("Room effect: Scale budget");
  for (unsigned p : {204u, 208u})
    if (std::abs(std::int64_t(static_cast<std::int32_t>(word(r, p)))) > 1000)
      throw std::runtime_error("Room effect: Scroll budget");
  if (b[1] == 8 && (b[202] || b[203]))
    throw std::runtime_error("Room effect: Esp08 mask/inner fade");
  if (b[198] && b[199] >= b[198])
    throw std::runtime_error("Room effect: Distance fade");
}
void checkPyreRecord(const WaterRecord &r) {
  const auto &b = r.bytes;
  if (b[0] != 1 || (b[1] != 0 && b[1] != 11 && b[1] != 70) || b[3] || b[6] ||
      b[7] != 0xfe || b[195] || b[194] > 5 || (word(r, 8) & ~0x8809fu) ||
      b[264] > 1 ||
      (b[264] && (b[265] != 0 || b[266] || (b[267] & ~2u) || b[269] > 15 ||
                  half(r, 272) || b[296] > 16)))
    throw std::runtime_error(
        "Room effect: unsupported r101 fire record");
  for (unsigned p = 12; p <= 152; p += 4)
    if (std::abs(real(r, p)) > 100000)
      throw std::runtime_error("Room effect: Fire parameter budget");
  for (unsigned p = 216; p <= 224; p += 4)
    if (std::abs(real(r, p)) > 10000)
      throw std::runtime_error("Room effect: Esp0b jitter budget");
  for (unsigned p = 160; p <= 172; p += 4)
    if (real(r, p) < 0 || real(r, p) > 4)
      throw std::runtime_error("Room effect: Fire color budget");
  if (real(r, 136) <= 0 || real(r, 140) <= 0 || real(r, 136) > 10000 ||
      real(r, 140) > 10000 || real(r, 48) < 0 || real(r, 48) > 1 ||
      real(r, 152) < 0 || real(r, 152) > 1)
    throw std::runtime_error("Room effect: Fire scale budget");
}
void checkExplosionRecord(const WaterRecord &r) {
  const auto &b = r.bytes;
  if (b[0] != 1 || (b[1] != 0 && b[1] != 11 && b[1] != 70) || b[3] || b[6] ||
      b[7] != 0xff || b[195] || b[194] > 5 || (word(r, 8) & ~0x280b8u) ||
      b[264] > 1 ||
      (b[264] &&
       (b[265] || b[266] || (b[267] & ~3u) || b[269] > 31 || b[296] > 16)))
    throw std::runtime_error(
        "Room effect: unsupported explosion record");
  for (unsigned p = 12; p <= 152; p += 4)
    if (std::abs(real(r, p)) > 100000)
      throw std::runtime_error("Room effect: Explosion parameter budget");
  for (unsigned p = 216; p <= 224; p += 4)
    if (std::abs(real(r, p)) > 10000)
      throw std::runtime_error("Room effect: Explosion jitter budget");
  for (unsigned p = 160; p <= 172; p += 4)
    if (real(r, p) < 0 || real(r, p) > 4)
      throw std::runtime_error("Room effect: Explosion color budget");
  if (real(r, 136) <= 0 || real(r, 140) <= 0 || real(r, 136) > 10000 ||
      real(r, 140) > 10000 || real(r, 48) < 0 || real(r, 48) > 1 ||
      real(r, 152) < 0 || real(r, 152) > 1)
    throw std::runtime_error("Room effect: Explosion scale budget");
}
bool hasAnimation(const std::vector<std::uint8_t> &b, unsigned id) {
  Reader r{b};
  validateEff(r);
  auto table = r.table(4, 8);
  for (unsigned k = 0; k < r.u32(table); ++k)
    if (r.u16(table + 4 + k * 8) == id)
      return true;
  return false;
}
float dot(const std::array<float, 3> &a, const std::array<float, 3> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
std::vector<std::array<float,4>> eventPath(const std::vector<std::uint8_t> &bytes,unsigned id) {
  Reader r{bytes};validateEff(r);
  const auto ids=r.table(16,8),table=r.table(40,4);
  if(r.u32(ids)!=r.u32(table)) throw std::runtime_error("EVD EST: Path table count");
  for(unsigned i=0;i<r.u32(ids);++i) if(r.u16(ids+4+i*8)==id) {
    const auto p=r.entry(table,i,4);const auto n=r.u16(p);
    if(n<2 || n>4096 || p+4+std::size_t(n)*40>r.end(p))
      throw std::runtime_error("EVD EST: Path range");
    r.range(p+4,std::size_t(n)*40);std::vector<std::array<float,4>> points(n);
    for(unsigned k=0;k<n;++k) {
      const auto q=p+4+k*40;
      if(bytes[q+31]) throw std::runtime_error("EVD EST: unsupported weighted path");
      for(unsigned axis=0;axis<4;++axis) {
        const auto bits=r.u32(q+(axis==3?24:axis*4));
        std::memcpy(&points[k][axis],&bits,4);
        if(!std::isfinite(points[k][axis]) || std::abs(points[k][axis])>100000)
          throw std::runtime_error("EVD EST: Path parameter budget");
      }
      if((k==0 && points[k][3]!=0) || (k && points[k][3]<=points[k-1][3]))
        throw std::runtime_error("EVD EST: Unsorted path lengths");
    }
    if(points.back()[3]<=1.01f) throw std::runtime_error("EVD EST: Path too short");
    return points;
  }
  throw std::runtime_error("EVD EST: Path is missing");
}
} // namespace
std::vector<WaterRecord> ReadOriginalEffectSequence(const std::vector<std::uint8_t> &eff,
                                                  unsigned sequence) {
  return startupRecords(eff,sequence,true);
}
std::vector<std::array<float,4>> ReadOriginalEffectPath(const std::vector<std::uint8_t> &eff,
                                                      unsigned path) {
  return eventPath(eff,path);
}
void RegisterOriginalEffectAnimation(EnvironmentData &data,
    const std::vector<std::uint8_t> &eff,const std::vector<std::uint8_t> &core,
    WaterRecord &record) {
  const unsigned id=record.bytes[2];
  const auto found=std::find_if(data.animations.begin(),data.animations.end(),
      [&](const auto &a){return a.id==id;});
  if(found==data.animations.end()) {
    data.animations.push_back(hasAnimation(core,id)?animation(core,id):animation(eff,id));
    record.animation=unsigned(data.animations.size()-1);
  } else record.animation=unsigned(found-data.animations.begin());
  if(record.bytes[188]>=data.animations[record.animation].pattern.size())
    throw std::runtime_error("Original-EFF: Starting pattern outside ANM bounds");
}
LaserSightData BuildLaserSightData(const std::vector<std::uint8_t> &core) {
  const auto line = startupRecords(core, 3, true);
  const auto point = startupRecords(core, 0x50, true);
  if (line.size() != 1 || point.empty() || line[0].bytes[0] != 1 ||
      line[0].bytes[1] != 0x19 || point[0].bytes[0] != 1 ||
      point[0].bytes[1] != 0 || point[0].bytes[3] ||
      point[0].bytes[7] != 0xff || point[0].bytes[264] ||
      word(point[0], 8) || point[0].bytes[194] > 5)
    throw std::runtime_error("Laser: unsupported original CORE profile");
  LaserSightData data;
  const auto texture = animation(core, point[0].bytes[2]);
  if (texture.images.empty()) throw std::runtime_error("Laser: Point texture is missing");
  data.pointTexture = texture.images.front();
  for (unsigned k = 0; k < 2; ++k) {
    data.pointSize[k] = real(point[0], 136 + k * 4);
    if (data.pointSize[k] <= 0 || data.pointSize[k] > 1000)
      throw std::runtime_error("Laser: Point size limit");
  }
  for (unsigned k = 0; k < 4; ++k) {
    data.pointColor[k] = point[0].bytes[156 + k] / 255.f;
    data.lineColor[k] = line[0].bytes[156 + k] / 255.f;
  }
  data.blend = point[0].bytes[194];
  data.maxLineLength = real(line[0], 228);
  if (data.maxLineLength == 0) data.maxLineLength = 12000;
  if (data.maxLineLength < 500 || data.maxLineLength > 100000)
    throw std::runtime_error("Laser: Line length limit");
  return data;
}
WorldEffectsData BuildWeaponWorldEffects(const std::vector<std::uint8_t> &weapon,
                                         const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;
  data.sprites.signature=hash(hash(1469598103934665603ULL,weapon),core);
  data.sprites.sourceSequenceSeeds=true;
  WaterTarget target;
  target.sequence=0;
  const auto records=startupRecords(weapon,0,true);
  if(records.empty())throw std::runtime_error("Muzzle flash: Empty EST");
  data.weaponPart=records.front().bytes[7];
  if(data.weaponPart>=16)throw std::runtime_error("Muzzle flash: Invalid weapon parent");
  unsigned unsupported=0;
  for(auto record:records) {
    const auto &b=record.bytes;
    if(b[1]==0x11) {
      if(data.weaponLight || b[3] || b[6] || b[7]!=data.weaponPart || word(record,8) ||
          b[200] || b[201]!=8 || b[202] || b[203]!=1 || b[264] ||
          !half(record,184) || half(record,184)>2 || half(record,186) ||
          real(record,144)!=0 || real(record,148)!=0)
        throw std::runtime_error("Muzzle flash: unsupported Esp11 light profile");
      data.weaponLight=record;continue;
    }
    if(b[1]!=0) {++unsupported;continue;}
    // The Mine Thrower's final EST0 card stays on a separate moving part4.
    // The detached muzzle owner handles the part6 flash/smoke; retain its
    // renderer while diagnosing that independently attached card.
    if(b[7]!=data.weaponPart) {++unsupported;continue;}
    // Launcher EST0 also has an indirect-texture card. Retain the ordinary
    // flash sprites; that separate GX texture-coordinate effect is not a
    // reason to discard the weapon's complete GPU owner.
    if(b[195]==1 && word(record,8)==0x4018u){++unsupported;continue;}
    if(b[3] || b[6] || b[7]!=data.weaponPart || b[192] || b[195] || b[194]>5 ||
       b[264]>1 || (b[264] && (b[265] || b[266] || (b[267]&~3u) || b[269]>31 ||
                   b[296]>16)) || (word(record,8)&~0x9au))
      throw std::runtime_error("Muzzle flash: unsupported WEP Esp00 profile");
    for(unsigned p=12;p<=172;p+=4) {
      if(p==156) continue;
      if(std::abs(real(record,p))>100000)
        throw std::runtime_error("Muzzle flash: Parameter budget");
    }
    // CommonMove release_time0 bakes the authored weapon part3 pose into
    // world coordinates before its first displacement; physics stays once.
    record.bytes[7]=0xff;
    auto found=std::find_if(data.sprites.animations.begin(),data.sprites.animations.end(),
                          [&](const auto&a){return a.id==b[2];});
    if(found==data.sprites.animations.end()) {
      data.sprites.animations.push_back(hasAnimation(weapon,b[2])?animation(weapon,b[2]):animation(core,b[2]));
      record.animation=unsigned(data.sprites.animations.size()-1);
    } else record.animation=unsigned(found-data.sprites.animations.begin());
    if(b[188]>=data.sprites.animations[record.animation].pattern.size())
      throw std::runtime_error("Muzzle flash: Starting pattern");
    target.records.push_back(record);
  }
  if(target.records.empty()) throw std::runtime_error("Muzzle flash: WEP Esp00 is missing");
  data.sprites.transformSpawn=true;
  data.sprites.targets.push_back(std::move(target));
  data.sequences.push_back(0);
  data.targetPrerun.push_back(0);
  if(unsupported) data.sprites.warnings.push_back("WEP EST0: "+std::to_string(unsupported)+" separate strips/indirect cards not rendered yet");
  return data;
}
WorldEffectsData BuildEventWorldEffects(const std::vector<std::uint8_t> &event,
                                      const std::vector<std::uint8_t> &core,
                                      const std::vector<std::uint8_t> &room,
                                      const std::vector<unsigned> &onlySequences,
                                      const std::vector<std::vector<std::uint8_t>> &modelBanks) {
  Reader reader{event};validateEff(reader);
  const auto ids=reader.table(8,8);
  WorldEffectsData data;
  data.sprites.signature=hash(hash(hash(1469598103934665603ULL,event),core),room);
  data.sprites.sourceSequenceSeeds=true;
  for(const auto &bank:modelBanks)data.sprites.signature=hash(data.sprites.signature,bank);
  for(unsigned i=0;i<reader.u32(ids);++i) {
    const unsigned sequence=reader.u16(ids+4+i*8);
    if(!onlySequences.empty() && std::find(onlySequences.begin(),onlySequences.end(),sequence)==onlySequences.end())continue;
    WaterTarget target;target.sequence=sequence;
    target.records=startupRecords(event,sequence,true);
    std::vector<std::vector<std::array<float,4>>> paths(target.records.size());
    unsigned previous=0,prerun=0;
    for(auto &record:target.records) {
      const auto &b=record.bytes;
      if(half(record,4)<previous) throw std::runtime_error("EVD EST: Unsorted times");
      previous=half(record,4);
      if(b[264]==1 && b[265]==255) {
        // EspgenSeqSet's 0xff is a pre-run request, never a sprite/emitter.
        if(previous || prerun || half(record,272)>1024)
          throw std::runtime_error("EVD EST: unsupported loop marker");
        prerun=half(record,272);record.enabled=false;continue;
      }
      const bool model=b[1]==0xfc, fragment=b[1]==0xfe, light=b[1]==0x11;
      if(b[0]!=1 || (b[1]!=0 && b[1]!=1 && b[1]!=3 && b[1]!=8 && b[1]!=10 && b[1]!=16 && b[1]!=17 &&
          b[1]!=18 && b[1]!=24 && b[1]!=27 && b[1]!=72 && b[1]!=70 && !model && !fragment) || b[3] ||
          (b[7]>=0xf8 && b[7]!=0xff && b[7]!=0xfd && b[7]!=0xfe) || b[194]>5 ||
          (b[195] && (b[1]!=18 || b[195]!=1)) || (word(record,8)&~0x7284ffu) ||
          b[264]>1 || (b[264] && (b[265]!=0 && b[265]!=2)) ||
          (b[267]&~3u) || b[269]>127 || b[296]>16 || half(record,272)>600)
        throw std::runtime_error("EVD EST: unsupported original profile (Id="+
              std::to_string(b[1])+", sequence="+std::to_string(sequence)+", parts="+std::to_string(b[7])+", flags="+std::to_string(word(record,8))+", fields="+std::to_string(b[0])+"/"+std::to_string(b[3])+"/"+std::to_string(b[194])+"/"+std::to_string(b[195])+"/"+std::to_string(b[264])+"/"+std::to_string(b[265])+"/"+std::to_string(b[267])+"/"+std::to_string(b[269])+"/"+std::to_string(b[296])+"/"+std::to_string(half(record,272))+")");
      if(b[1]==3 && (b[200]==10 || b[201]>1 || b[202] || b[203]))
        throw std::runtime_error("EVD EST: unsupported Esp03 profile");
      if(fragment) {
        data.fragmentRecords.push_back({unsigned(data.sprites.targets.size()),unsigned(&record-target.records.data()),record});
        record.externalSeedDraws=13;continue;
      }
      if(b[1]==3 || b[1]==70) {record.animation=~0u;continue;}
      if(model && (b[7]!=0xfe || word(record,8)!=0x40u || b[264]))
        throw std::runtime_error("EVD EST: unsupported Efm04 profile");
      if(light && (b[200]!=0 || b[201]!=8 || b[202]!=0 || b[203]!=1 || b[264]))
        throw std::runtime_error("EVD EST: unsupported Esp11 light profile");
      if(b[1]==10 && (b[201] || b[202] || b[203] || !b[200] || b[200]>127 || b[264]))
        throw std::runtime_error("EVD EST: unsupported Esp0a trail profile");
      if(b[1]==27 && (int(static_cast<std::int8_t>(b[200]))+4<2 ||
                     int(static_cast<std::int8_t>(b[200]))+4>64))
        throw std::runtime_error("EVD EST: Esp1b geometry budget");
      if(b[1]==24 || b[1]==27 || b[1]==72)
        for(unsigned p=216;p<=248;p+=4)
          if(std::abs(real(record,p))>200000)
            throw std::runtime_error("EVD EST: FreeWork vector budget");
      if(b[1]==18 && b[200]>123) throw std::runtime_error("EVD EST: Ribbon budget");
      if(b[265]==2 && (b[266] || b[296] || b[299] || b[260] || b[7]!=0xfe))
        throw std::runtime_error("EVD EST: unsupported path orientation");
      if(b[265]==2) paths[std::size_t(&record-target.records.data())]=eventPath(core,b[261]);
      for(unsigned p=12;p<=172;p+=4) {
        if(p==156) continue;
        if(std::abs(real(record,p))>200000)
          throw std::runtime_error("EVD EST: Parameter budget");
      }
      // Opening foliage cards are authored up to44788.5625 world units.
      if(real(record,136)<=0 || real(record,140)<=0 || real(record,136)>50000 ||
          real(record,140)>50000 || real(record,48)<0 || real(record,48)>1 ||
          real(record,152)<0 || real(record,152)>1)
        throw std::runtime_error("EVD EST: Scale budget");
      if(model) {
        data.eventModels.push_back({unsigned(data.sprites.targets.size()),
            unsigned(&record-target.records.data()),record});
        continue;
      }
      if(light) continue; // LightMgr owns its draw; CommonMove has no AnmMove.
      const auto found=std::find_if(data.sprites.animations.begin(),data.sprites.animations.end(),
          [&](const auto &a){return a.id==b[2];});
      if(found==data.sprites.animations.end()) {
        // EffArea registration is CORE first, then room, then EVD effects.
        // Opening tree/road cards refer to the authored r120 texture bank.
        std::optional<WaterAnimation> selected;
        if(hasAnimation(core,b[2]))selected=animation(core,b[2]);
        else if(!room.empty() && hasAnimation(room,b[2]))selected=animation(room,b[2]);
        else for(const auto &bank:modelBanks)if(hasAnimation(bank,b[2])){selected=animation(bank,b[2]);break;}
        data.sprites.animations.push_back(selected?std::move(*selected):animation(event,b[2]));
        record.animation=unsigned(data.sprites.animations.size()-1);
      } else record.animation=unsigned(found-data.sprites.animations.begin());
      if(b[188]>=data.sprites.animations[record.animation].pattern.size())
        throw std::runtime_error("EVD EST: Starting pattern");
    }
    data.sprites.recordParents.emplace_back(target.records.size());
    data.sprites.recordEmitMatrices.emplace_back(target.records.size());
    data.sprites.recordPaths.push_back(std::move(paths));
    data.sprites.targets.push_back(std::move(target));
    data.sequences.push_back(sequence);data.targetPrerun.push_back(prerun);
  }
  return data;
}
WorldEffectsData BuildTrapWorldEffects(const std::vector<std::uint8_t> &enemy,
                                       const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;
  data.sprites.signature=hash(hash(1469598103934665603ULL,enemy),core);
  data.sprites.sourceSequenceSeeds=true;data.triggerInstanceLimit=32;
  // cEm2a's actual dry-land explosions, Leon catch, bear closure and armed
  // tripwire banks. Water EST6 is a separate Esp40 surface path.
  for(unsigned sequence:{0u,1u,2u,3u,4u,5u,9u}) {
    WaterTarget target;target.sequence=sequence;
    target.records=startupRecords(enemy,sequence,true);
    unsigned previous=0;
    for(auto &record:target.records) {
      const auto &b=record.bytes;const unsigned index=unsigned(&record-target.records.data());
      if(half(record,4)<previous)throw std::runtime_error("EM2A: Unsorted EST times");
      previous=half(record,4);
      const bool fragment=b[1]==0xfe;
      if(b[0]!=1 || (b[1]!=0 && b[1]!=7 && b[1]!=9 && b[1]!=11 && b[1]!=14 &&
          b[1]!=16 && b[1]!=17 && b[1]!=70 && !fragment) || b[3] || b[6] ||
          (b[7]!=0 && b[7]!=2 && b[7]!=23 && b[7]!=0xff) || b[194]>5 || b[195] ||
          (word(record,8)&~0x80fbu) || b[264]>1 ||
          (b[264] && (b[265] || b[266] || (b[267]&~3u) || b[269]>31 ||
                      b[296]>16 || half(record,272)>600)))
        throw std::runtime_error("EM2A EST"+std::to_string(sequence)+" Record"+
            std::to_string(index)+": unsupported original profile");
      if(fragment) {
        data.fragmentRecords.push_back({unsigned(data.sprites.targets.size()),index,record});
        // EfmSetObj05 consumes Pos3+Ang3+AngPlus3+Size1+KaboomPos3.
        // cObj05::move subsequently uses its own saved pre-spawn seed.
        record.externalSeedDraws=13;continue;
      }
      if(b[1]==7 && (b[212] || b[213] || b[214]))
        throw std::runtime_error("EM2A: Esp07 HitType/EstCall/sound profile");
      if(b[1]==9 && (b[2]!=255 || b[200] || b[201] || b[202] || b[203]))
        throw std::runtime_error("EM2A: Esp09 original line profile");
      if(b[1]==14 && (real(record,240)!=0 || real(record,244)!=0 || real(record,248)!=0 ||
          real(record,24)!=0 || real(record,28)!=0 || real(record,32)!=0))
        throw std::runtime_error("EM2A: Esp0e direction/jitter profile");
      if(b[1]==16 && (b[200] || b[201] || b[202] || b[203]))
        throw std::runtime_error("EM2A: Esp10 original floor profile");
      if(b[1]==17 && (b[200] || b[201]!=8 || b[202] || b[203]!=1 || b[264]))
        throw std::runtime_error("EM2A: Esp11 original light profile");
      for(unsigned p=12;p<=172;p+=4) {
        if(p==156)continue;
        if(std::abs(real(record,p))>100000)throw std::runtime_error("EM2A: Parameter budget");
      }
      if(real(record,136)<=0 || real(record,140)<=0 || real(record,136)>20000 ||
          real(record,140)>20000 || real(record,48)<0 || real(record,48)>1 ||
          real(record,152)<0 || real(record,152)>1)
        throw std::runtime_error("EM2A: Scale budget");
      for(unsigned p=216;p<=248;p+=4)
        if(std::abs(real(record,p))>100000)throw std::runtime_error("EM2A: FreeWork budget");
      if(b[1]==9 || b[1]==17 || b[1]==70) {record.animation=~0u;continue;}
      const auto found=std::find_if(data.sprites.animations.begin(),data.sprites.animations.end(),
          [&](const auto &a){return a.id==b[2];});
      if(found==data.sprites.animations.end()) {
        data.sprites.animations.push_back(hasAnimation(core,b[2])?animation(core,b[2]):animation(enemy,b[2]));
        record.animation=unsigned(data.sprites.animations.size()-1);
      } else record.animation=unsigned(found-data.sprites.animations.begin());
      if(b[188]>=data.sprites.animations[record.animation].pattern.size())
        throw std::runtime_error("EM2A: Starting pattern outside ANM bounds");
    }
    data.triggerBanks.push_back({sequence,sequence,unsigned(data.sprites.targets.size()),0});
    data.sprites.recordParents.emplace_back(target.records.size());
    data.sprites.targets.push_back(std::move(target));
    data.sequences.push_back(sequence);data.targetPrerun.push_back(~0u);
  }
  return data;
}
WorldEffectsData BuildEnemyWorldEffects(const std::vector<std::uint8_t> &enemy,
                                        const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;
  data.sprites.signature = hash(hash(1469598103934665603ULL, enemy), core);
  data.sprites.sourceSequenceSeeds = true;
  data.originalEnemyBlood = true;
  data.triggerInstanceLimit = 32;
  for (unsigned sequence : {1u, 2u, 0xcu, 0x29u, 0x35u, 0x36u, 0x37u, 0x87u}) {
    WaterTarget target;
    target.sequence = sequence;
    target.records = startupRecords(enemy, sequence, true);
    unsigned unsupported = 0, previous = 0;
    for (auto &record : target.records) {
      const auto &b = record.bytes;
      if (half(record, 4) < previous)
        throw std::runtime_error("Ganado blood: Unsorted EST times");
      previous = half(record, 4);
      // Shimmer owns a framebuffer refraction pass. Retain its source record
      // index, never substitute an ordinary coloured quad for refraction.
      if ((b[1] != 0 && b[1] != 1 && b[1] != 11 && b[1] != 16) || b[195]) {
        record.enabled = false; ++unsupported; continue;
      }
      if (b[0] != 1 || b[3] || b[6] || b[7] != 0xff || (b[192] && b[192]!=0xff) ||
          b[194] > 5 || (word(record, 8) & ~0x420cbfu) ||
          b[264] > 1 ||
          (b[264] && (b[265] || b[266] || (b[267] & ~3u) || b[269] > 31 ||
                      b[296] > 16 || half(record, 272) > 600)))
        throw std::runtime_error("Ganado blood EST" + std::to_string(sequence) +
            " Record" + std::to_string(&record - target.records.data()) +
            ": unsupported original profile");
      if (b[1] == 16 && (b[200] || b[201] || b[202] || b[203]))
        throw std::runtime_error("Ganado blood: Esp10 room/water rule pending");
      if (b[1] == 1 && (b[200] > 12 || b[201] > 32 || b[202] || b[203]))
        throw std::runtime_error("Ganado blood: Esp01 strip budget");
      for (unsigned p = 12; p <= 172; p += 4) {
        if (p == 156) continue;
        if (std::abs(real(record, p)) > 100000)
          throw std::runtime_error("Ganado blood: Parameter budget");
      }
      if (real(record, 136) <= 0 || real(record, 140) <= 0 ||
          real(record, 136) > 20000 || real(record, 140) > 20000 ||
          real(record, 48) < 0 || real(record, 48) > 1 ||
          real(record, 152) < 0 || real(record, 152) > 1 ||
          (b[198] && b[199] >= b[198]))
        throw std::runtime_error("Ganado blood: Scale/distance budget");
      for (unsigned p = 160; p <= 172; p += 4)
        if (real(record, p) < 0 || real(record, p) > 4)
          throw std::runtime_error("Ganado blood: Color budget");
      const auto texture = b[2];
      auto found = std::find_if(data.sprites.animations.begin(),
                                 data.sprites.animations.end(),
                                 [&](const auto &a) { return a.id == texture; });
      if (found == data.sprites.animations.end()) {
        data.sprites.animations.push_back(hasAnimation(core, texture)
                                             ? animation(core, texture)
                                             : animation(enemy, texture));
        record.animation = unsigned(data.sprites.animations.size() - 1);
      } else record.animation = unsigned(found - data.sprites.animations.begin());
      if (b[188] >= data.sprites.animations[record.animation].pattern.size())
        throw std::runtime_error("Ganado blood: Starting pattern outside ANM bounds");
    }
    const auto count = unsigned(target.records.size());
    if (count == unsupported)
      throw std::runtime_error("Ganado blood: EST has no renderable original records");
    data.triggerBanks.push_back({sequence, sequence,
                                 unsigned(data.sprites.targets.size()), unsupported});
    data.sprites.targets.push_back(std::move(target));
    data.sequences.push_back(sequence);
    data.targetPrerun.push_back(~0u);
    if (unsupported)
      data.sprites.warnings.push_back("EM12 EST" + std::to_string(sequence) +
          ": " + std::to_string(unsupported) +
          " shimmer records pending separately; original spray/strips/ground decals active");
  }
  return data;
}
WorldEffectsData BuildRoomWorldEffects(const RoomArchive &room,
                                       std::uint16_t id,
                                       const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;
  data.sprites.room = id;
  data.sprites.signature = 1469598103934665603ULL ^ id;
  if (id != 0x400 && id != 0x101)
    return data;
  auto *entry = room.find("EFF");
  if (!entry) {
    data.sprites.warnings.push_back("Room effects: EFF is missing");
    return data;
  }
  auto eff = room.copy(entry - room.entries().data());
  data.sprites.signature = hash(hash(data.sprites.signature, eff), core);
  // Authored globally active fireplace layers and ambient light shafts.
  // Interior torch SST5 and other area sequences are deliberately not forced
  // on outside their original EAR area lifecycle.
  for (unsigned id : {3u, 13u}) {
    auto animationCount = data.sprites.animations.size();
    try {
      WaterTarget target;
      target.sequence = id;
      target.records = worldRecords(eff, id);
      unsigned lastTime = 0;
      for (auto &record : target.records) {
        checkRecord(record);
        if (half(record, 4) < lastTime)
          throw std::runtime_error("Room effect: Unsorted SST times");
        lastTime = half(record, 4);
        unsigned texture = record.bytes[2];
        auto found = std::find_if(
            data.sprites.animations.begin(), data.sprites.animations.end(),
            [&](const auto &a) { return a.id == texture; });
        if (found == data.sprites.animations.end()) {
          // The original refuses duplicate texture registrations: core first.
          auto decoded = hasAnimation(core, texture) ? animation(core, texture)
                                                     : animation(eff, texture);
          data.sprites.animations.push_back(std::move(decoded));
          record.animation = unsigned(data.sprites.animations.size() - 1);
        } else
          record.animation = unsigned(found - data.sprites.animations.begin());
        if (record.bytes[188] >=
            data.sprites.animations[record.animation].pattern.size())
          throw std::runtime_error("Room effect: Starting pattern outside ANM bounds");
      }
      data.sprites.targets.push_back(std::move(target));
      data.sequences.push_back(id);
      data.targetPrerun.push_back(200);
    } catch (const std::exception &e) {
      data.sprites.animations.resize(animationCount);
      data.sprites.warnings.push_back("SST " + std::to_string(id) + ": " +
                                      e.what());
    }
  }
  if (id == 0x101) {
    auto animationCount = data.sprites.animations.size();
    try {
      WaterTarget pyre;
      pyre.sequence = 0;
      pyre.records = startupRecords(eff, 0);
      if (pyre.records.size() != 13 || pyre.records[0].bytes[1] != 70)
        throw std::runtime_error("Room effect: Original EST0 profile is missing");
      unsigned previous = 0;
      for (auto &record : pyre.records) {
        checkPyreRecord(record);
        if (half(record, 4) < previous)
          throw std::runtime_error("Room effect: Unsorted EST0 times");
        previous = half(record, 4);
        if (record.bytes[1] == 70) {
          record.animation = ~0u;
          continue;
        }
        unsigned texture = record.bytes[2];
        auto found = std::find_if(
            data.sprites.animations.begin(), data.sprites.animations.end(),
            [&](const auto &a) { return a.id == texture; });
        if (found == data.sprites.animations.end()) {
          data.sprites.animations.push_back(hasAnimation(core, texture)
                                                ? animation(core, texture)
                                                : animation(eff, texture));
          record.animation = unsigned(data.sprites.animations.size() - 1);
        } else
          record.animation = unsigned(found - data.sprites.animations.begin());
        if (record.bytes[188] >=
            data.sprites.animations[record.animation].pattern.size())
          throw std::runtime_error(
              "Room effect: Fire starting pattern outside ANM bounds");
      }
      data.sprites.targets.push_back(std::move(pyre));
      data.sequences.push_back(0);
      data.targetPrerun.push_back(0);
      data.originalPyre = true;
      // Filter03 is an alpha-buffer feedback glow, not a flat screen tint.
      // Its CPU request is kept; the current full renderer does not yet expose
      // the original shared alpha pass needed to render it correctly.
      data.sprites.warnings.push_back("r101: Esp46/Filter03 alpha glow still "
                                      "pending; original flames/smoke active");
      data.sprites.warnings.push_back(
          "r101: additional EAR/SST special effects pending (not globally "
          "guessed interior effects)");
    } catch (const std::exception &error) {
      data.sprites.animations.resize(animationCount);
      data.sprites.warnings.push_back(std::string("EST0: ") + error.what());
    }
  }
  if (const auto *etm = room.find("ETM")) {
    try {
      RoomObjectArchive objects(room.copy(etm - room.entries().data()));
      for (const auto &owner : std::vector<std::pair<unsigned, std::string>>{
               {0x12, "et12.eff"}, {0x2d, "et2d.eff"}, {0x3c, "et3c.eff"}}) {
        if (!objects.find(owner.second))
          continue;
        const auto animationCount = data.sprites.animations.size();
        try {
          auto bytes = objects.copy(owner.second);
          WaterTarget bank;
          bank.sequence = 0;
          bank.records = startupRecords(bytes, 0, true);
          unsigned previous = 0, unsupported = 0;
          for (auto &record : bank.records) {
            if (half(record, 4) < previous)
              throw std::runtime_error(
                  "Room effect: Unsorted explosion times");
            previous = half(record, 4);
            if (record.bytes[1] != 0 && record.bytes[1] != 11 &&
                record.bytes[1] != 70) {
              record.enabled = false;
              ++unsupported;
              continue;
            }
            checkExplosionRecord(record);
            if (record.bytes[1] == 70) {
              record.animation = ~0u;
              continue;
            }
            const unsigned texture = record.bytes[2];
            auto found = std::find_if(
                data.sprites.animations.begin(), data.sprites.animations.end(),
                [&](const auto &a) { return a.id == texture; });
            if (found == data.sprites.animations.end()) {
              const auto &source = hasAnimation(core, texture)  ? core
                                   : hasAnimation(eff, texture) ? eff
                                                                : bytes;
              data.sprites.animations.push_back(animation(source, texture));
              record.animation = unsigned(data.sprites.animations.size() - 1);
            } else
              record.animation =
                  unsigned(found - data.sprites.animations.begin());
            if (record.bytes[188] >=
                data.sprites.animations[record.animation].pattern.size())
              throw std::runtime_error(
                  "Room effect: Explosion starting pattern outside ANM bounds");
          }
          data.sprites.signature = hash(data.sprites.signature, bytes);
          data.triggerBanks.push_back({owner.first, 0,
                                       unsigned(data.sprites.targets.size()),
                                       unsupported});
          data.sprites.targets.push_back(std::move(bank));
          data.sequences.push_back(0);
          data.targetPrerun.push_back(~0u);
          data.sprites.warnings.push_back(
              owner.second +
              ": original Esp00/Esp0b explosion active; "
              "EFM in separate model backend; "
              "Decals/light/flare/trail/quake/Filter03 pending");
        } catch (const std::exception &error) {
          data.sprites.animations.resize(animationCount);
          data.sprites.warnings.push_back(owner.second + ": " + error.what());
        }
      }
    } catch (const std::exception &error) {
      data.sprites.warnings.push_back(std::string("Explosion ETM: ") +
                                      error.what());
    }
  }
  return data;
}
namespace {
bool spriteEqual(const WaterSprite &a, const WaterSprite &b) {
  return a.target == b.target && a.record == b.record && a.flags == b.flags &&
         a.pattern == b.pattern && a.animationCounter == b.animationCounter &&
         a.age == b.age && a.serial == b.serial && a.position == b.position &&
         a.speed == b.speed && a.acceleration == b.acceleration &&
         a.angle == b.angle && a.angleSpeed == b.angleSpeed &&
         a.color == b.color && a.size == b.size && a.scroll == b.scroll &&
         a.sizeMultiplier == b.sizeMultiplier && a.sizeStep == b.sizeStep &&
         a.cameraOffset == b.cameraOffset && a.startAlpha == b.startAlpha &&
         a.parented == b.parented && a.history == b.history &&
         a.sinePhase==b.sinePhase && a.sineOffset==b.sineOffset &&
         a.motionTrailGhost==b.motionTrailGhost && a.motionTrailLife==b.motionTrailLife &&
         a.lineWidth==b.lineWidth && a.bounceFloor==b.bounceFloor && a.bounceFlags==b.bounceFlags;
}
bool stateEqual(const EnvironmentState &a, const EnvironmentState &b) {
  if (a.signature != b.signature || a.frames != b.frames ||
      a.serial != b.serial || a.random != b.random ||
      a.skyAngles != b.skyAngles || a.recordCursor != b.recordCursor ||
      a.sequenceSeed != b.sequenceSeed ||
      a.externalSerial!=b.externalSerial || a.externalSpawns.size()!=b.externalSpawns.size() ||
      a.sprites.size() != b.sprites.size() ||
      a.emitters.size() != b.emitters.size())
    return false;
  for(unsigned i=0;i<a.externalSpawns.size();++i) {
    const auto &x=a.externalSpawns[i],&y=b.externalSpawns[i];
    if(x.target!=y.target || x.record!=y.record || x.seed!=y.seed || x.serial!=y.serial)return false;
  }
  for (unsigned i = 0; i < a.sprites.size(); ++i)
    if (!spriteEqual(a.sprites[i], b.sprites[i]))
      return false;
  for (unsigned i = 0; i < a.emitters.size(); ++i) {
    const auto &x = a.emitters[i], &y = b.emitters[i];
    if (x.target != y.target || x.record != y.record || x.next != y.next ||
        x.interval != y.interval || x.seed != y.seed || x.age != y.age)
      return false;
  }
  return true;
}
bool validDynamic(const EnvironmentState &state) {
  if (state.frames > 1000000200ULL || state.sprites.size() > 512 ||
      state.emitters.size() > 512)
    return false;
  for (const auto &e : state.emitters)
    if (e.next > 255 || e.interval > 255 || e.age > 65535)
      return false;
  for (const auto &sprite : state.sprites) {
    if (!sprite.serial || sprite.serial > state.serial ||
        !std::isfinite(sprite.sizeMultiplier) || sprite.sizeMultiplier <= 0 ||
        !std::isfinite(sprite.sizeStep) || (sprite.flags & ~0x4a8cbfu) ||
        !std::isfinite(sprite.startAlpha) || sprite.startAlpha < 0 ||
        sprite.startAlpha > 255)
      return false;
    for (const auto *v :
         {&sprite.position, &sprite.speed, &sprite.acceleration, &sprite.angle,
          &sprite.angleSpeed, &sprite.cameraOffset})
      for (float x : *v)
        if (!std::isfinite(x))
          return false;
    for (float x : sprite.color)
      if (!std::isfinite(x) || x < 0 || x > 255)
        return false;
    for (float x : sprite.size)
      if (!std::isfinite(x) || x <= 0)
        return false;
    for (float x : sprite.scroll)
      if (!std::isfinite(x) || x < 0 || x > 1)
        return false;
  }
  return true;
}
EnvironmentData simulationData(const EnvironmentData &source) {
  EnvironmentData data;
  data.room = source.room;
  data.signature = source.signature;
  data.sky = source.sky;
  data.targets = source.targets;
  data.surfaces = source.surfaces;
  data.sourceSequenceSeeds = source.sourceSequenceSeeds;
  data.initialRandom = source.initialRandom;
  data.spawnMatrix = source.spawnMatrix;
  data.transformSpawn = source.transformSpawn;
  data.recordParents=source.recordParents;
  data.recordEmitMatrices=source.recordEmitMatrices;
  data.recordPaths=source.recordPaths;
  // CPU CommonMove/AnmMove only need immutable animation metadata. Pixel
  // storage remains with the room/GPU owner rather than being copied for
  // every active barrel and every checkpoint validation candidate.
  for (const auto &sourceAnimation : source.animations) {
    WaterAnimation animation;
    animation.id = sourceAnimation.id;
    animation.width = sourceAnimation.width;
    animation.height = sourceAnimation.height;
    animation.loop = sourceAnimation.loop;
    animation.centerX = sourceAnimation.centerX;
    animation.centerY = sourceAnimation.centerY;
    animation.pattern = sourceAnimation.pattern;
    animation.duration = sourceAnimation.duration;
    data.animations.push_back(std::move(animation));
  }
  return data;
}
} // namespace
struct WorldEffectsSimulation::Impl {
  struct Stream {
    unsigned prerun{};
    EnvironmentData data;
    std::unique_ptr<EnvironmentSimulation> simulation;
  };
  const WorldEffectsData *source;
  std::vector<Stream> streams;
  struct Instance {
    unsigned bank{};
    std::uint64_t eventSerial{}, startFrame{};
    std::array<float, 3> position{}, rotation{};
    std::vector<EnvironmentMatrix> sourceWorldParts;
    EnvironmentData data;
    std::unique_ptr<EnvironmentSimulation> simulation;
  };
  std::vector<std::unique_ptr<Instance>> instances;
  std::vector<std::uint64_t> acceptedEvents;
  std::uint16_t eventRandom = 0x4e22;
  std::vector<WorldEffectFragmentSpawn> pendingFragments;
  mutable WorldEffectsState cached;
  explicit Impl(const WorldEffectsData &data) : source(&data) {
    if (!data.triggerInstanceLimit || data.triggerInstanceLimit > 32)
      throw std::runtime_error("Room effect: invalid trigger instance budget");
    if (!data.targetPrerun.empty() &&
        data.targetPrerun.size() != data.sprites.targets.size())
      throw std::runtime_error("Room effect: invalid starting profile");
    std::vector<unsigned> starts{200};
    for (auto value : data.targetPrerun) {
      if (value == ~0u)
        continue; // Triggered EST banks never activate at room load.
      if (value != 0 && value != 200)
        throw std::runtime_error("Room effect: invalid prerun");
      if (std::find(starts.begin(), starts.end(), value) == starts.end())
        starts.push_back(value);
    }
    streams.reserve(starts.size());
    for (unsigned start : starts) {
      streams.emplace_back();
      auto &stream = streams.back();
      stream.prerun = start;
      stream.data = simulationData(data.sprites);
      stream.data.sourceSequenceSeeds = start == 0;
      for (unsigned t = 0; t < data.sprites.targets.size(); ++t)
        if (!data.targetPrerun.empty() && data.targetPrerun[t] != start)
          stream.data.targets[t].records.clear();
    }
    // Vector storage is now stable before each simulation borrows its data.
    for (auto &stream : streams) {
      stream.simulation = std::make_unique<EnvironmentSimulation>(stream.data);
      for (unsigned i = 0; i < stream.prerun; ++i)
        stream.simulation->tick();
    }
    for (const auto &bank : data.triggerBanks)
      if (bank.target >= data.sprites.targets.size() ||
          data.targetPrerun.at(bank.target) != ~0u)
        throw std::runtime_error("Room effect: invalid trigger bank");
  }
  std::unique_ptr<Instance> createInstance(unsigned bank, std::uint64_t serial,
                                           std::uint64_t start,
                                           const std::array<float, 3> &position,
                                           const std::array<float, 3> &rotation,
                                           std::uint16_t seed,
                                           const SatCollision *floor = nullptr,
                                           const std::vector<EnvironmentMatrix> &parts = {}) const {
    auto result = std::make_unique<Instance>();
    result->bank = bank;
    result->eventSerial = serial;
    result->startFrame = start;
    result->position = position;
    result->rotation = rotation;
    result->sourceWorldParts=parts;
    result->data = simulationData(source->sprites);
    const auto target = source->triggerBanks.at(bank).target;
    for (unsigned t = 0; t < result->data.targets.size(); ++t)
      if (t != target)
        result->data.targets[t].records.clear();
    result->data.sky.clear();
    result->data.surfaces.clear();
    result->data.sourceSequenceSeeds = true;
    result->data.initialRandom = seed;
    result->data.transformSpawn = true;
    SmdPlacement placement;
    placement.scale = {1, 1, 1};
    placement.position = position;
    placement.rotation = rotation;
    result->data.spawnMatrix = PlacementMatrix(placement);
    if(!bindParts(*result,parts))throw std::runtime_error("Room effect: Original skeleton parent is missing");
    result->simulation = std::make_unique<EnvironmentSimulation>(result->data, floor);
    return result;
  }
  bool bindParts(Instance &instance,const std::vector<EnvironmentMatrix> &parts) const {
    const auto target=source->triggerBanks.at(instance.bank).target;
    auto &data=instance.data;
    if(data.recordParents.size()<data.targets.size())data.recordParents.resize(data.targets.size());
    data.recordParents[target].resize(data.targets[target].records.size());
    for(unsigned i=0;i<data.targets[target].records.size();++i) {
      const auto &r=data.targets[target].records[i];
      if(!r.enabled || r.externalSeedDraws || r.bytes[7]>=0xf8)continue;
      if(r.bytes[7]>=parts.size())return false;
      data.recordParents[target][i]=parts[r.bytes[7]];
    }
    instance.sourceWorldParts=parts;
    return true;
  }
  void collectFragments(Instance &instance) {
    for(const auto &spawn:instance.simulation->drainExternalSpawns()) {
      const auto &r=instance.data.targets.at(spawn.target).records.at(spawn.record);
      const auto placement=r.bytes[7]<0xf8 ? instance.sourceWorldParts.at(r.bytes[7]) : instance.data.spawnMatrix;
      pendingFragments.push_back({source->sequences.at(spawn.target),spawn.record,
          instance.eventSerial,spawn.serial,spawn.seed,placement});
    }
  }
  unsigned spriteCount() const {
    unsigned count = 0;
    for (const auto &stream : streams)
      count += unsigned(stream.simulation->state().sprites.size());
    for (const auto &instance : instances)
      count += unsigned(instance->simulation->state().sprites.size());
    return count;
  }
  const WorldEffectsState &snapshot() const {
    cached = {};
    static_cast<EnvironmentState &>(cached) =
        streams.front().simulation->state();
    cached.sprites.clear();
    cached.emitters.clear();
    cached.serial = 0;
    for (unsigned index = 0; index < streams.size(); ++index) {
      const auto &stream = streams[index];
      const auto &state = stream.simulation->state();
      cached.streams.push_back(state);
      const auto prefix = std::uint64_t(index) << 48;
      cached.serial = std::max(cached.serial, prefix | state.serial);
      for (auto sprite : state.sprites) {
        sprite.serial |= prefix;
        cached.sprites.push_back(sprite);
      }
      cached.emitters.insert(cached.emitters.end(), state.emitters.begin(),
                             state.emitters.end());
      for (unsigned t = 0; t < stream.data.targets.size(); ++t)
        if (!stream.data.targets[t].records.empty()) {
          cached.recordCursor[t] = state.recordCursor[t];
          cached.sequenceSeed[t] = state.sequenceSeed[t];
        }
    }
    cached.acceptedEvents = acceptedEvents;
    cached.eventRandom = eventRandom;
    cached.pendingFragments=pendingFragments;
    for (const auto &instance : instances) {
      const auto &state = instance->simulation->state();
      cached.instances.push_back({instance->bank, instance->eventSerial,
                                  instance->startFrame, instance->position,
                                  instance->rotation, instance->sourceWorldParts, state});
      const auto prefix =
          (std::uint64_t(2) << 48) | (instance->eventSerial << 16);
      cached.serial = std::max(cached.serial, prefix | state.serial);
      for (auto sprite : state.sprites) {
        if(sprite.parented) {
          ApplyEnvironmentSpriteMatrix(sprite,*instance->data.recordParents.at(sprite.target).at(sprite.record));
          sprite.parented=false;
        }
        sprite.serial |= prefix;
        cached.sprites.push_back(sprite);
      }
      cached.emitters.insert(cached.emitters.end(), state.emitters.begin(),
                             state.emitters.end());
      const auto target = source->triggerBanks[instance->bank].target;
      cached.recordCursor[target] = state.recordCursor[target];
      cached.sequenceSeed[target] = state.sequenceSeed[target];
    }
    return cached;
  }
};
WorldEffectsSimulation::WorldEffectsSimulation(const WorldEffectsData &data)
    : impl_(std::make_unique<Impl>(data)) {}
WorldEffectsSimulation::~WorldEffectsSimulation() = default;
const WorldEffectsState &WorldEffectsSimulation::state() const {
  return impl_->snapshot();
}
void WorldEffectsSimulation::tick(const WorldEffectCamera *camera,
                                    const SatCollision *floor) {
  EnvironmentCamera view;
  if (camera) {
    view.eye = camera->eye;
    view.up = camera->up;
  }
  unsigned count = impl_->spriteCount();
  auto update = [&](EnvironmentSimulation &simulation) {
    const unsigned before = unsigned(simulation.state().sprites.size());
    simulation.tick(camera ? &view : nullptr, 512 - (count - before), floor);
    count = count - before + unsigned(simulation.state().sprites.size());
  };
  for (auto &stream : impl_->streams)
    update(*stream.simulation);
  for (auto &instance : impl_->instances) {
    update(*instance->simulation);
    impl_->collectFragments(*instance);
  }
  impl_->instances.erase(
      std::remove_if(impl_->instances.begin(), impl_->instances.end(),
                     [&](const auto &instance) {
                       const auto &state = instance->simulation->state();
                       const auto target =
                           impl_->source->triggerBanks[instance->bank].target;
                       return state.sprites.empty() && state.emitters.empty() &&
                              state.recordCursor[target] ==
                                  instance->data.targets[target].records.size();
                     }),
      impl_->instances.end());
}
bool WorldEffectsSimulation::trigger(unsigned objectId,
                                     const std::array<float, 3> &position,
                                     const std::array<float, 3> &rotation,
                                     std::uint64_t serial, const SatCollision *floor) {
  return triggerAttached(objectId,position,rotation,serial,{},floor);
}
bool WorldEffectsSimulation::triggerAttached(unsigned objectId,
                                     const std::array<float, 3> &position,
                                     const std::array<float, 3> &rotation,
                                     std::uint64_t serial,
                                     const std::vector<EnvironmentMatrix> &parts,
                                     const SatCollision *floor) {
  if (!serial || serial > 0xffffffffULL || impl_->instances.size() >= impl_->source->triggerInstanceLimit ||
      impl_->acceptedEvents.size() >= 1024 ||
      std::find(impl_->acceptedEvents.begin(), impl_->acceptedEvents.end(),
                serial) != impl_->acceptedEvents.end())
    return false;
  for (float x : position)
    if (!std::isfinite(x) || std::abs(x) > 100000)
      return false;
  for (float x : rotation)
    if (!std::isfinite(x) || std::abs(x) > 100)
      return false;
  const auto &banks = impl_->source->triggerBanks;
  auto bank = std::find_if(banks.begin(), banks.end(), [&](const auto &b) {
    return b.objectId == objectId;
  });
  if (bank == banks.end())
    return false;
  if(parts.size()>256)return false;
  for(const auto &matrix:parts)for(float x:matrix)
    if(!std::isfinite(x) || std::abs(x)>1000000)return false;
  for(const auto &r:impl_->source->sprites.targets[bank->target].records)
    if(r.enabled && r.bytes[7]<0xf8 && r.bytes[7]>=parts.size())return false;
  auto instance = impl_->createInstance(
      unsigned(bank - banks.begin()), serial,
      impl_->streams.front().simulation->state().frames - 200, position,
      rotation, impl_->eventRandom, floor,parts);
  if (instance->simulation->state().sprites.size() > 512 - impl_->spriteCount())
    return false;
  impl_->eventRandom = instance->simulation->state().random;
  impl_->collectFragments(*instance);
  impl_->instances.push_back(std::move(instance));
  impl_->acceptedEvents.push_back(serial);
  return true;
}
bool WorldEffectsSimulation::updateInstanceParts(std::uint64_t serial,
                                  const std::vector<EnvironmentMatrix> &parts) {
  if(parts.size()>256)return false;
  for(const auto &matrix:parts)for(float x:matrix)
    if(!std::isfinite(x) || std::abs(x)>1000000)return false;
  const auto found=std::find_if(impl_->instances.begin(),impl_->instances.end(),
      [&](const auto &instance){return instance->eventSerial==serial;});
  if(found==impl_->instances.end())return false;
  const auto target=impl_->source->triggerBanks[(*found)->bank].target;
  for(const auto &r:(*found)->data.targets[target].records)
    if(r.enabled && r.bytes[7]<0xf8 && r.bytes[7]>=parts.size())return false;
  return impl_->bindParts(**found,parts);
}
bool WorldEffectsSimulation::stopInstance(std::uint64_t serial) {
  const auto found=std::find_if(impl_->instances.begin(),impl_->instances.end(),
      [&](const auto &instance){return instance->eventSerial==serial;});
  if(found==impl_->instances.end())return false;
  impl_->instances.erase(found);return true;
}
std::vector<WorldEffectFragmentSpawn> WorldEffectsSimulation::drainFragmentSpawns() {
  auto result=std::move(impl_->pendingFragments);impl_->pendingFragments.clear();return result;
}
bool WorldEffectsSimulation::restore(const WorldEffectsState &snapshot) {
  // state() exposes a cached reference; keep input independent while rebuilding
  // the aggregate during atomic validation.
  const WorldEffectsState state = snapshot;
  if (state.signature != impl_->source->sprites.signature ||
      state.frames < 200 || state.streams.size() != impl_->streams.size() ||
      !validDynamic(state) || state.instances.size() > impl_->source->triggerInstanceLimit ||
      state.acceptedEvents.size() > 1024)
    return false;
  if(state.pendingFragments.size()>512)return false;
  for(const auto &spawn:state.pendingFragments) {
    const auto record=std::find_if(impl_->source->fragmentRecords.begin(),impl_->source->fragmentRecords.end(),
        [&](const auto &r){return impl_->source->sequences[r.target]==spawn.sequence && r.record==spawn.sourceRecord;});
    if(record==impl_->source->fragmentRecords.end() || !spawn.spawnSerial ||
        std::find(state.acceptedEvents.begin(),state.acceptedEvents.end(),spawn.eventSerial)==state.acceptedEvents.end())return false;
    for(float x:spawn.placement)if(!std::isfinite(x) || std::abs(x)>1000000)return false;
  }
  for (unsigned i = 0; i < state.acceptedEvents.size(); ++i) {
    auto serial = state.acceptedEvents[i];
    if (!serial || serial > 0xffffffffULL ||
        std::find(state.acceptedEvents.begin(),
                  state.acceptedEvents.begin() + i,
                  serial) != state.acceptedEvents.begin() + i)
      return false;
  }
  std::vector<std::unique_ptr<EnvironmentSimulation>> validated;
  for (unsigned i = 0; i < impl_->streams.size(); ++i) {
    const auto &snapshot = state.streams[i];
    const auto &stream = impl_->streams[i];
    if (!validDynamic(snapshot) ||
        snapshot.frames != state.frames - 200 + stream.prerun)
      return false;
    auto candidate = std::make_unique<EnvironmentSimulation>(stream.data);
    if (!candidate->restore(snapshot))
      return false;
    for (const auto &sprite : snapshot.sprites) {
      const auto &record =
          stream.data.targets[sprite.target].records[sprite.record];
      if ((sprite.flags & ~6u) != (word(record, 8) & ~6u))
        return false;
    }
    validated.push_back(std::move(candidate));
  }
  std::vector<std::unique_ptr<Impl::Instance>> instances;
  for (const auto &saved : state.instances) {
    if (saved.bank >= impl_->source->triggerBanks.size() ||
        saved.startFrame > state.frames - 200 ||
        saved.state.frames != state.frames - 200 - saved.startFrame ||
        !validDynamic(saved.state) || saved.state.serial >= 65536 ||
        std::find(state.acceptedEvents.begin(), state.acceptedEvents.end(),
                  saved.eventSerial) == state.acceptedEvents.end())
      return false;
    for (const auto &previous : instances)
      if (previous->eventSerial == saved.eventSerial)
        return false;
    for (float x : saved.position)
      if (!std::isfinite(x) || std::abs(x) > 100000)
        return false;
    for (float x : saved.rotation)
      if (!std::isfinite(x) || std::abs(x) > 100)
        return false;
    if(saved.sourceWorldParts.size()>256)return false;
    for(const auto &matrix:saved.sourceWorldParts)for(float x:matrix)
      if(!std::isfinite(x) || std::abs(x)>1000000)return false;
    for(const auto &r:impl_->source->sprites.targets[impl_->source->triggerBanks[saved.bank].target].records)
      if(r.enabled && r.bytes[7]<0xf8 && r.bytes[7]>=saved.sourceWorldParts.size())return false;
    auto candidate =
        impl_->createInstance(saved.bank, saved.eventSerial, saved.startFrame,
                              saved.position, saved.rotation, 0x4e22,nullptr,saved.sourceWorldParts);
    if (!candidate->simulation->restore(saved.state))
      return false;
    for (const auto &sprite : saved.state.sprites) {
      const auto &record =
          candidate->data.targets[sprite.target].records[sprite.record];
      if (!record.enabled || (sprite.flags & ~6u) != (word(record, 8) & ~6u))
        return false;
    }
    instances.push_back(std::move(candidate));
  }
  // The public aggregate draw state must agree exactly with its saved streams.
  auto old = std::move(impl_->streams[0].simulation);
  impl_->streams[0].simulation = std::move(validated[0]);
  std::vector<std::unique_ptr<EnvironmentSimulation>> previous;
  for (unsigned i = 1; i < impl_->streams.size(); ++i) {
    previous.push_back(std::move(impl_->streams[i].simulation));
    impl_->streams[i].simulation = std::move(validated[i]);
  }
  auto oldInstances = std::move(impl_->instances);
  impl_->instances = std::move(instances);
  auto oldEvents = std::move(impl_->acceptedEvents);
  impl_->acceptedEvents = state.acceptedEvents;
  auto oldRandom = impl_->eventRandom;
  impl_->eventRandom = state.eventRandom;
  auto oldFragments=std::move(impl_->pendingFragments);
  impl_->pendingFragments=state.pendingFragments;
  const bool equal = stateEqual(state, impl_->snapshot());
  if (!equal) {
    impl_->streams[0].simulation = std::move(old);
    for (unsigned i = 1; i < impl_->streams.size(); ++i)
      impl_->streams[i].simulation = std::move(previous[i - 1]);
    impl_->instances = std::move(oldInstances);
    impl_->acceptedEvents = std::move(oldEvents);
    impl_->eventRandom = oldRandom;
    impl_->pendingFragments=std::move(oldFragments);
  }
  return equal;
}
std::vector<WorldEffectQuad> WorldEffectQuads(const WorldEffectsData &data,
                                              const WorldEffectsState &state,
                                              const WorldEffectCamera &camera) {
  if (state.signature != data.sprites.signature)
    throw std::runtime_error("Room effect: State belongs to a different room");
  for (const auto *v :
       {&camera.eye, &camera.right, &camera.up, &camera.forward})
    for (auto x : *v)
      if (!std::isfinite(x))
        throw std::runtime_error("Room effect: invalid camera");
  std::vector<WorldEffectQuad> out;
  for (const auto &sprite : state.sprites) {
    const auto &r =
        data.sprites.targets.at(sprite.target).records.at(sprite.record);
    if (r.bytes[1] == 70 || r.bytes[1]==17 || r.bytes[1]==0xfc || r.bytes[1]==9 || r.bytes[1]==3 || r.bytes[1]==14 || r.bytes[1]==78)
      continue; // Esp46 only publishes a Filter03 request
    const auto &a = data.sprites.animations.at(r.animation);
    WorldEffectQuad q;
    q.animation = r.animation;
    q.image = sprite.pattern;
    q.sequence = data.sequences.at(sprite.target);
    q.record = sprite.record;
    q.serial = sprite.serial;
    q.blend = r.bytes[194];
    q.flags=sprite.flags;q.type=r.bytes[1];q.screen=r.bytes[7]>=0xf8 && r.bytes[7]<=0xfd;
    q.sourceAlpha=sprite.color[3];q.blurRate=r.bytes[1]==24 ? -real(r,216) : 0;
    q.color = sprite.color;
    if (q.blend == 4) {
      for (unsigned k = 0; k < 3; ++k)
        q.color[k] = float(int(sprite.color[k] * sprite.color[3] / 255.f));
      q.color[3] = 255;
    }
    std::array<float, 3> toSprite{};
    for (unsigned k = 0; k < 3; ++k)
      toSprite[k] = sprite.position[k] - camera.eye[k];
    q.depth = dot(toSprite, camera.forward);
    // ChannelSet uses Del_far/near scaled by 10 twice (record bytes -> member
    // distance -> compare), preserving the original near-camera fade.
    const int farCode = r.bytes[1]==21 ? std::int8_t(r.bytes[200]) : r.bytes[198];
    const int nearCode = r.bytes[1]==21 ? std::int8_t(r.bytes[201]) : r.bytes[199];
    if (farCode) {
      float far = float(farCode) * 100.f, near = float(nearCode) * 100.f;
      if (q.depth < far) {
        float rate = std::clamp(1 - (far - q.depth) / (far - near), 0.f, 1.f);
        if (q.blend == 4)
          for (unsigned k = 0; k < 3; ++k)
            q.color[k] *= rate;
        else
          q.color[3] *= rate;
      }
    }
    for (auto &c : q.color)
      c = float(std::clamp(int(c), 0, 255)) / 255.f;
    if (!q.color[3])
      continue;
    q.rgbScale = (sprite.flags & 0x80) ? 4.f : 1.f;
    q.alphaScale = (sprite.flags & 0x20000) ? 4.f : 1.f;
    q.scroll = sprite.scroll;
    q.tiled = r.bytes[1] == 8;
    const bool directGeometry=q.tiled || r.bytes[1]==24;
    q.flipS = (sprite.flags & 2) != 0;
    q.flipT = q.screen ? !(sprite.flags&4) : (sprite.flags&4)!=0;
    if(r.bytes[1]==18) {
      // Esp12_Trans's history strip, including its staggered S/T vertices.
      const auto count=std::min(unsigned(sprite.history.size()),sprite.age+1);
      if(count<2) continue;
      const float step=1.f/float(sprite.history.size());
      std::array<float,3> normal{},direction{};
      std::array<std::array<float,3>,2> previous{};
      SmdPlacement rotation;rotation.scale={1,1,1};
      constexpr float worldRadians=0.017453292f/(3.14f/180.f);
      for(unsigned axis=0;axis<3;++axis) rotation.rotation[axis]=sprite.angle[axis]*worldRadians;
      const auto matrix=PlacementMatrix(rotation);
      for(unsigned i=0;i<count;++i) {
        if(i+1<count) for(unsigned axis=0;axis<3;++axis)
          direction[axis]=sprite.history[i+1][axis]-sprite.history[i][axis];
        std::array<float,3> relative{};
        for(unsigned axis=0;axis<3;++axis)relative[axis]=sprite.history[i][axis]-camera.eye[axis];
        const std::array<float,3> cross{direction[1]*relative[2]-direction[2]*relative[1],
            direction[2]*relative[0]-direction[0]*relative[2],direction[0]*relative[1]-direction[1]*relative[0]};
        const float length=std::sqrt(dot(cross,cross));
        if(length)for(unsigned axis=0;axis<3;++axis)normal[axis]=cross[axis]/length;
        const float rate=float(i)/float(count-1);
        const float width=(rate*sprite.size[1]+(1-rate)*sprite.size[0])*sprite.sizeMultiplier;
        std::array<std::array<float,3>,2> current{};
        for(unsigned axis=0;axis<3;++axis) {
          current[0][axis]=sprite.history[i][axis]+normal[axis]*width;
          current[1][axis]=sprite.history[i][axis]-normal[axis]*width;
        }
        for(auto &point:current)point=TransformPoint(matrix,point);
        if(i) {
          auto strip=q;strip.position={previous[0],previous[1],current[1],current[0]};
          const float t=float(i-1)*step;
          strip.uv={{{0,t},{1,t+step},{1,t+step*2},{0,t+step}}};
          strip.flipS=strip.flipT=false;out.push_back(strip);
        }
        previous=current;
      }
      continue;
    }
    if (r.bytes[1] == 1) {
      // Literal esp01Trans_sub in world coordinates. A rigid camera view
      // preserves cross products, so cross(d,point-eye) is the source view
      // cross transformed back to world. The original saved BasePos, speed
      // and acceleration stay immutable; replay does not move the sprite.
      const unsigned segments = std::min(15u, r.bytes[200] > 12
                                                   ? 2u : 15u - r.bytes[200]);
      const unsigned interval = unsigned(r.bytes[201]) + 1;
      std::array<unsigned, 16> frames{};
      unsigned maximum = 0, minimum = 999999;
      for (unsigned i = 0; i <= segments; ++i) {
        frames[i] = sprite.age > i * interval ? sprite.age - i * interval : 0;
        maximum = std::max(maximum, frames[i]);
        minimum = std::min(minimum, frames[i]);
      }
      SmdPlacement orientation;
      orientation.scale = {1,1,1};
      constexpr float worldRadians = 0.017453292f / (3.14f / 180.f);
      for (unsigned axis = 0; axis < 3; ++axis)
        orientation.rotation[axis] = sprite.angle[axis] * worldRadians;
      const auto matrix = PlacementMatrix(orientation);
      auto rotate = [&](const auto &value) {
        std::array<float,3> result{};
        for (unsigned row = 0; row < 3; ++row)
          for (unsigned col = 0; col < 3; ++col)
            result[row] += matrix[row*4+col] * value[col];
        return result;
      };
      const auto acceleration = rotate(sprite.acceleration);
      const auto speed = rotate(sprite.speed);
      std::array<std::array<float,3>,16> points{};
      if (maximum - minimum <= 47) {
        std::array<std::array<float,3>,48> samples{};
        auto position = sprite.position, step = speed;
        samples[0] = position;
        for (unsigned j = 1; j <= maximum; ++j) {
          for (unsigned axis = 0; axis < 3; ++axis) position[axis] += step[axis];
          if (j >= minimum) samples[j-minimum] = position;
          for (unsigned axis = 0; axis < 3; ++axis) {
            step[axis] += acceleration[axis]; step[axis] *= real(r,48);
          }
        }
        for (unsigned i = 0; i <= segments; ++i) points[i] = samples[frames[i]-minimum];
      } else {
        for (unsigned i = 0; i <= segments; ++i) {
          points[i] = sprite.position;
          auto step = speed;
          for (unsigned j = 0; j < frames[i]; ++j)
            for (unsigned axis = 0; axis < 3; ++axis) {
              points[i][axis] += step[axis];
              step[axis] += acceleration[axis]; step[axis] *= real(r,48);
            }
        }
      }
      std::array<std::array<float,3>,2> previous{};
      bool hasPrevious = false;
      for (unsigned i = 0; i < segments; ++i) {
        std::array<float,3> delta{}, relative{};
        for (unsigned axis = 0; axis < 3; ++axis) {
          delta[axis] = points[i+1][axis] - points[i][axis];
          relative[axis] = points[i][axis] - camera.eye[axis];
        }
        std::array<float,3> side{delta[1]*relative[2]-delta[2]*relative[1],
                                 delta[2]*relative[0]-delta[0]*relative[2],
                                 delta[0]*relative[1]-delta[1]*relative[0]};
        const float magnitude = std::sqrt(dot(side,side));
        if (!magnitude) continue;
        const float rate = float(i) / float(segments-1);
        const float halfWidth = (rate*sprite.size[1]+(1-rate)*sprite.size[0]) *
                                  sprite.sizeMultiplier;
        WorldEffectQuad strip = q;
        for (unsigned axis = 0; axis < 3; ++axis) {
          const float width = side[axis] / magnitude * halfWidth;
          strip.position[0][axis] = hasPrevious ? previous[0][axis] : points[i][axis]+width;
          strip.position[1][axis] = hasPrevious ? previous[1][axis] : points[i][axis]-width;
          strip.position[2][axis] = points[i+1][axis]-width;
          strip.position[3][axis] = points[i+1][axis]+width;
        }
        previous = {strip.position[3],strip.position[2]}; hasPrevious = true;
        const float t = q.flipT ? 1.f-float(i)/segments : float(i)/segments;
        const float end = t+1.f/segments;
        const float left = q.flipS ? 1.f : 0.f, right = q.flipS ? 0.f : 1.f;
        strip.uv = q.flipT
            ? std::array<std::array<float,2>,4>{{{left,end},{right,end},{right,t},{left,t}}}
            : std::array<std::array<float,2>,4>{{{left,t},{right,t},{right,end},{left,end}}};
        strip.flipS = strip.flipT = false; // Source flip baked into UVs.
        out.push_back(strip);
      }
      continue;
    }
    float sx = sprite.size[0] * sprite.sizeMultiplier;
    float sy = sprite.size[1] * sprite.sizeMultiplier;
    float ox = a.centerX ? float(-a.centerX) / a.width : -.5f;
    float oy = a.centerY ? float(a.centerY) / a.height - 1 : -.5f;
    // EspInit's plain display list starts at normalized (0,1), UV(0,0).
    // Esp08 emits its top edge directly. Both start at the upper world edge.
    float y0 = directGeometry ? (a.centerY ? float(a.centerY) / a.height : .5f) * sy
                       : (oy + 1) * sy;
    float y1 = directGeometry ? y0 - sy : oy * sy;
    const float z=r.bytes[1]==24 || r.bytes[195] ? 1.f : 0.f;
    std::array<std::array<float, 3>, 4> local{{{ox * sx, y0, z},
                                               {(ox + 1) * sx, y0, z},
                                               {(ox + 1) * sx, y1, z},
                                               {ox * sx, y1, z}}};
    if (q.tiled)
      for (unsigned k = 0; k < 2; ++k)
        q.repeats[k] = r.bytes[200 + k] * .1f + 1;
    // Esp00 flips geometry; Esp08 flips only texture coordinates.
    if (!directGeometry)
      for (auto &p : local) {
        if (q.flipS)
          p[0] = -p[0];
        if (q.flipT)
          p[1] = -p[1];
      }
    SmdPlacement placement;
    placement.position = sprite.position;
    // EspSeqSet uses DEG2RAD=0.017453292. The existing texture-pass
    // engine stores legacy 3.14/180 angles; convert only for world geometry.
    constexpr float worldRadians = 0.017453292f / (3.14f / 180.f);
    for (unsigned axis = 0; axis < 3; ++axis)
      placement.rotation[axis] = r.bytes[1]==0x48 || r.bytes[1]==6 || r.bytes[1]==21 ?
          sprite.angle[axis] : sprite.angle[axis] * worldRadians;
    placement.scale = {1, 1, 1};
    if (q.screen || (sprite.flags & 0x80001)) {
      auto matrix = PlacementMatrix(placement);
      if (sprite.flags & 0x80000) {
        // EspTrans computes atan2(look.x,look.z) * 57.295776 degrees;
        // EspCommonTrans applies this Y-pan before translating world position.
        SmdPlacement pan;
        pan.scale = {1, 1, 1};
        pan.rotation[1] = std::atan2(camera.forward[0], camera.forward[2]) *
                          57.295776f * (3.1415927f / 180.f);
        auto yaw = PlacementMatrix(pan);
        auto origin = placement.position;
        placement.position = {};
        auto localMatrix = PlacementMatrix(placement);
        EnvironmentMatrix composed{};
        for (unsigned row = 0; row < 3; ++row) {
          for (unsigned col = 0; col < 3; ++col)
            for (unsigned k = 0; k < 3; ++k)
              composed[row * 4 + col] +=
                  yaw[row * 4 + k] * localMatrix[k * 4 + col];
          composed[row * 4 + 3] = origin[row];
        }
        matrix = composed;
      }
      for (unsigned k = 0; k < 4; ++k)
        q.position[k] = TransformPoint(matrix, local[k]);
    } else {
      // Original billboard retains its z rotation in camera-space.
      float c = std::cos(placement.rotation[2]),
            s = std::sin(placement.rotation[2]);
      for (unsigned k = 0; k < 4; ++k) {
        float x = local[k][0] * c - local[k][1] * s;
        float y = local[k][0] * s + local[k][1] * c;
        for (unsigned axis = 0; axis < 3; ++axis)
          q.position[k][axis] = sprite.position[axis] + x * camera.right[axis] +
                                y * camera.up[axis] - z * camera.forward[axis];
      }
    }
    q.uv = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    if(r.bytes[1]==24) {
      for(auto &uv:q.uv) {
        if(q.flipS)uv[0]=1-uv[0];
        if(q.flipT)uv[1]=1-uv[1];
      }
      q.flipS=q.flipT=false; // Source Esp18 texture flips baked once.
    }
    if(r.bytes[1]==27) {
      // Esp1b_SpTrans/GetPosXY: source's bilinear unit patch (not a guessed
      // Bezier ribbon), divided Work8[0]+4 times along each direction.
      std::array<std::array<float,3>,4> control{{{0,1,1},{1,1,1},{1,0,1},{0,0,1}}};
      for(unsigned point=1;point<4;++point)for(unsigned axis=0;axis<3;++axis)
        control[point][axis]+=real(r,216+(point-1)*12+axis*4)*.005f;
      auto patch=[&](float u,float v) {
        std::array<float,3> p{};
        for(unsigned axis=0;axis<3;++axis) {
          const float first=(control[1][axis]-control[0][axis])*u+control[0][axis];
          const float second=(control[2][axis]-control[3][axis])*u+control[3][axis];
          p[axis]=(second-first)*v+first;
        }
        p[0]=(p[0]+ox)*sx;p[1]=(p[1]+oy)*sy;
        if(q.flipS)p[0]=-p[0];
        if(q.flipT)p[1]=-p[1];
        if(q.screen || (sprite.flags&0x80001))return TransformPoint(PlacementMatrix(placement),p);
        const float c=std::cos(placement.rotation[2]),s=std::sin(placement.rotation[2]);
        const float x=p[0]*c-p[1]*s,y=p[0]*s+p[1]*c;
        for(unsigned axis=0;axis<3;++axis)p[axis]=sprite.position[axis]+x*camera.right[axis]+y*camera.up[axis]-p[2]*camera.forward[axis];
        return p;
      };
      const unsigned div=unsigned(int(static_cast<std::int8_t>(r.bytes[200]))+4);
      const float step=1.f/float(div);float v=0;
      for(unsigned row=0;row<div;++row) {
        float u=0;const float nextV=v+step;
        for(unsigned col=0;col<div;++col) {
          const float nextU=u+step;auto part=q;
          part.position={patch(u,v),patch(nextU,v),patch(nextU,nextV),patch(u,nextV)};
          part.uv={{{u,v},{nextU,v},{nextU,nextV},{u,nextV}}};
          part.flipS=part.flipT=false;out.push_back(part);u=nextU;
        }
        v=nextV;
      }
      continue;
    }
    out.push_back(q);
  }
  std::stable_sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    return a.depth > b.depth || (a.depth == b.depth && a.serial > b.serial);
  });
  return out;
}
std::vector<WorldEffectModel> WorldEffectModels(const WorldEffectsData &data,
                                               const WorldEffectsState &state) {
  if(state.signature!=data.sprites.signature)throw std::runtime_error("EVD EFM: State identity");
  std::vector<WorldEffectModel> out;
  for(const auto &s:state.sprites) {
    const auto &r=data.sprites.targets.at(s.target).records.at(s.record);
    if(r.bytes[1]!=0xfc)continue;
    WorldEffectModel model;model.model=r.bytes[2];model.lightMask=16;
    // EspgenEfmSet/EfmSetObj04 choose the original light class and clear
    // SelectMask unless Tool40 explicitly allows local lighting. In the
    // driving intro those copies include the road/landscape scroll models.
    if(s.flags&0x80)model.lightMask=4;
    if(s.flags&0x20000)model.lightMask=8;
    if(!(s.flags&0x40))model.selectMask=0;
    model.target=s.target;model.record=s.record;model.serial=s.serial;
    SmdPlacement p;p.position=s.position;p.scale={s.size[0]*s.sizeMultiplier,
        s.size[1]*s.sizeMultiplier,s.size[0]*s.sizeMultiplier};
    for(unsigned k=0;k<3;++k)p.rotation[k]=s.angle[k];
    motion64::Mtx matrix;
    motion64::Vec angle{p.rotation[0],p.rotation[1],p.rotation[2]},
        pos{p.position[0],p.position[1],p.position[2]},
        scale{p.scale[0],p.scale[1],p.scale[2]};
    motion64::RotMatrix(matrix,&angle);motion64::TransMatrix(matrix,&pos);
    motion64::ScaleMatrix(matrix,&scale);
    std::memcpy(model.matrix.data(),matrix,sizeof matrix);
    for(unsigned k=0;k<3;++k)model.color[k]=float(std::clamp(int(s.color[k]),0,255))/255.f;
    model.color[3]=std::clamp(s.color[3],0.f,255.f)/255.f;
    out.push_back(model);
  }
  return out;
}
std::vector<WorldEffectLight> WorldEffectLights(const WorldEffectsData &data,
                                               const WorldEffectsState &state) {
  if(state.signature!=data.sprites.signature)throw std::runtime_error("EVD light: State identity");
  std::vector<WorldEffectLight> out;
  for(const auto &s:state.sprites) {
    const auto &r=data.sprites.targets.at(s.target).records.at(s.record);
    if(r.bytes[1]!=17)continue;
    WorldEffectLight light;light.kind=r.bytes[200];light.cut=r.bytes[201];
    light.number=r.bytes[202];light.position=s.position;light.serial=s.serial;
    light.radius=s.size[0]*s.sizeMultiplier*10.f;
    for(unsigned k=0;k<4;++k)light.color[k]=s.color[k]/255.f;
    out.push_back(light);
  }
  return out;
}
std::vector<WorldEffectLine> WorldEffectLines(const WorldEffectsData &data,
                                             const WorldEffectsState &state) {
  if(state.signature!=data.sprites.signature)throw std::runtime_error("Esp09: State identity");
  std::vector<WorldEffectLine> out;
  for(const auto &s:state.sprites) {
    const auto &r=data.sprites.targets.at(s.target).records.at(s.record);
    if((r.bytes[1]!=9 && r.bytes[1]!=3) || s.age<=half(r,186))continue;
    WorldEffectLine line;line.serial=s.serial;line.sourceSignature=data.sprites.signature;
    line.flags=s.flags;line.blend=r.bytes[194];line.sourceAge=s.age;line.width=s.lineWidth;
    line.rgbScale=(s.flags&0x80)?4.f:1.f;line.position=s.history;
    if(r.bytes[1]==3) {
      SmdPlacement placement;placement.position=s.position;placement.rotation=s.angle;placement.scale={1,1,1};
      const auto matrix=PlacementMatrix(placement);
      const unsigned count=unsigned(std::clamp(4-int(r.bytes[200]),2,6));
      line.position.resize(count);
      for(unsigned i=0;i<count;++i)line.position[i]=TransformPoint(matrix,s.history[i%4]);
    }
    auto alpha=std::uint8_t(std::clamp(int(s.color[3]),0,255));
    const auto step=std::uint8_t(s.color[3]/float(s.history.size()-1));
    for(unsigned i=0;i<line.position.size();++i) {
      std::array<float,4> color{};
      for(unsigned k=0;k<3;++k)color[k]=float(std::clamp(int(s.color[k]),0,255))/255.f;
      color[3]=float(alpha)/255.f;line.color.push_back(color);if(r.bytes[1]!=3)alpha=std::uint8_t(alpha-step);
    }
    out.push_back(std::move(line));
  }
  return out;
}
std::vector<WorldEffectLens> WorldEffectLenses(const WorldEffectsData &data,
                                             const WorldEffectsState &state) {
  if(state.signature!=data.sprites.signature)throw std::runtime_error("Esp0e: State identity");
  std::vector<WorldEffectLens> out;
  for(const auto &s:state.sprites) {
    const auto &r=data.sprites.targets.at(s.target).records.at(s.record);
    if(r.bytes[1]!=14)continue;
    const auto &a=data.sprites.animations.at(r.animation);
    WorldEffectLens lens;lens.serial=s.serial;lens.sourceSignature=data.sprites.signature;
    lens.animation=r.animation;lens.image=s.pattern;lens.flags=s.flags;lens.blend=r.bytes[194];
    lens.sourceAge=s.age;lens.position=s.position;lens.angle=s.angle;
    for(float &angle:lens.angle)angle*=.017453292f/(3.14f/180.f);
    lens.size={s.size[0]*s.sizeMultiplier,s.size[1]*s.sizeMultiplier};
    lens.pivotNormalized={float(a.centerX)/float(a.width),float(a.centerY)/float(a.height)};
    lens.color=s.color;lens.sourceAlpha=s.color[3];
    if(lens.blend==4) {
      for(unsigned k=0;k<3;++k)lens.color[k]=float(int(s.color[k]*s.color[3]/255.f));
      lens.color[3]=255;
    }
    for(float &c:lens.color)c=float(std::clamp(int(c),0,255))/255.f;
    lens.centerRatio=std::min(1.f,1.f-real(r,216)*.01f);
    lens.sizeRatio=real(r,220)*.01f;lens.delDistance=real(r,224);lens.hideRadius=real(r,228);
    lens.rgbScale=(s.flags&0x80)?4.f:1.f;lens.alphaScale=(s.flags&0x20000)?4.f:1.f;
    out.push_back(lens);
  }
  return out;
}
std::vector<WorldEffectGlowRequest>
WorldEffectGlowRequests(const WorldEffectsData &data,
                        const WorldEffectsState &state) {
  if (state.signature != data.sprites.signature)
    throw std::runtime_error("Room effect: Glow state belongs to a different room");
  std::vector<WorldEffectGlowRequest> out;
  for (const auto &sprite : state.sprites) {
    const auto &record =
        data.sprites.targets.at(sprite.target).records.at(sprite.record);
    if (record.bytes[1] != 70)
      continue;
    WorldEffectGlowRequest request;
    request.level = static_cast<std::int8_t>(record.bytes[200]);
    request.priority = record.bytes[201];
    request.special = record.bytes[202];
    request.sequence = data.sequences.at(sprite.target);
    request.serial = sprite.serial;
    for (unsigned k = 0; k < 3; ++k)
      request.color[k] = static_cast<std::uint8_t>(std::clamp(
          int(sprite.color[k] * sprite.color[3] * (1.f / 255.f)), 0, 255));
    out.push_back(request);
  }
  return out;
}
std::string WorldEffectsData::describe() const {
  unsigned count = 0;
  for (unsigned t = 0; t < sprites.targets.size(); ++t)
    if (targetPrerun.empty() || targetPrerun[t] != ~0u)
      count += unsigned(sprites.targets[t].records.size());
  std::ostringstream s;
  s << "Room effects: " << count << " original room records / "
    << sprites.animations.size() << " texture animations";
  if (originalPyre)
    s << " / r101 original EST0: Flames and smoke";
  for (const auto &bank : triggerBanks)
    s << (originalEnemyBlood ? " / EM12 blood EST" + std::to_string(bank.sequence) + ": " : " / ET explosion: ")
      << sprites.targets[bank.target].records.size() - bank.unsupported << '/'
      << sprites.targets[bank.target].records.size() << " Records";
  for (const auto &w : sprites.warnings)
    s << "\n" << w;
  return s.str();
}
} // namespace re4
