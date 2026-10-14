// Source: frozen game/EtcModel.cpp, embox.cpp, eff_sys.cpp, esp_efm.cpp,
// espgen00.cpp and obj05.cpp. This is the authored Efm05 crate/barrel subset.
#include "destruction_effects.h"
#include "archive.h"
#include "asset_reader.h"
#include "model_skin.h"
#include "ports/rnd.h"
#include "room_objects.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace re4 {
namespace {
using V = std::array<float, 3>;
constexpr unsigned None = ~0u;
constexpr float Pi = 3.1415927f, Deg = 3.14f / 180.f; // Literal esp_efm.cpp conversion.
constexpr std::size_t MaxModels = 512, MaxEvents = 1024;
struct Eff {
  const std::vector<std::uint8_t> &bytes;
  AssetReader r;
  explicit Eff(const std::vector<std::uint8_t> &b) : bytes(b), r{b} {
    r.range(0, 48);
    if (b.size() > 16u * 1024 * 1024 || r.u32(0) != 11)
      throw std::runtime_error("Destruction EFF: Version/size");
  }
  std::size_t table(unsigned field, unsigned stride) const {
    const auto p = r.u32(field);
    if (p < 48 || r.u32(p) > 4096)
      throw std::runtime_error("Destruction EFF: Table");
    r.range(p + 4, std::size_t(r.u32(p)) * stride);
    return p;
  }
  std::size_t entry(std::size_t t, unsigned i, unsigned minimum) const {
    const auto n = r.u32(t);
    if (i >= n) throw std::runtime_error("Destruction EFF: Table index");
    const auto d = r.u32(t + 4 + i * 4);
    if (d < 4 + n * 4 || d > bytes.size() - t)
      throw std::runtime_error("Destruction EFF: Relative offset");
    r.range(t + d, minimum);
    return t + d;
  }
  std::size_t end(std::size_t p) const {
    auto limit = bytes.size();
    for (unsigned f = 4; f <= 44; f += 4) {
      if (!r.u32(f)) continue;
      const auto t = table(f, f < 24 ? 8 : 4);
      if (t > p) limit = std::min(limit, t);
      if (f < 24) continue;
      for (unsigned i = 0; i < r.u32(t); ++i) {
        const auto q = entry(t, i, 1);
        if (q > p) limit = std::min(limit, q);
      }
    }
    return limit;
  }
  std::vector<std::uint8_t> sub(std::size_t e, unsigned field) const {
    const auto d = r.u32(e + field);
    if (d < 20 || d > bytes.size() - e)
      throw std::runtime_error("Destruction EFF: Model file offset");
    const auto begin = e + d;
    auto finish = end(e);
    for (unsigned f = 4; f <= 16; f += 4) {
      const auto next = r.u32(e + f);
      if (!next) continue;
      if (next < 20 || next > bytes.size() - e)
        throw std::runtime_error("Destruction EFF: Subfile offset");
      if (e + next > begin) finish = std::min(finish, e + next);
    }
    if (finish <= begin) throw std::runtime_error("Destruction EFF: Model range");
    return {bytes.begin() + begin, bytes.begin() + finish};
  }
};
std::uint64_t hash(std::uint64_t h, const std::vector<std::uint8_t> &b) {
  for (auto c : b) h = (h ^ c) * 1099511628211ULL;
  return h;
}
unsigned word(const DestructionRecord &r, unsigned p) {
  return (unsigned(r.bytes[p]) << 24) | (unsigned(r.bytes[p+1]) << 16) |
         (unsigned(r.bytes[p+2]) << 8) | r.bytes[p+3];
}
unsigned half(const DestructionRecord &r, unsigned p) {
  return unsigned(r.bytes[p]) * 256 + r.bytes[p+1];
}
float real(const DestructionRecord &r, unsigned p) {
  auto bits = word(r,p); float v; std::memcpy(&v,&bits,4);
  if (!std::isfinite(v)) throw std::runtime_error("Destruction EFF: NaN/Inf");
  return v;
}
V vec(const DestructionRecord &r, unsigned p) {
  return {real(r,p),real(r,p+4),real(r,p+8)};
}
V add(const V &a,const V &b) { return {a[0]+b[0],a[1]+b[1],a[2]+b[2]}; }
V sub(const V &a,const V &b) { return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
V scale(const V &a,float s) { return {a[0]*s,a[1]*s,a[2]*s}; }
float length(const V &v) { return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }
V sr(const Matrix34 &m,const V &v) {
  V out{}; for(unsigned i=0;i<3;++i)for(unsigned k=0;k<3;++k)out[i]+=m[i*4+k]*v[k];
  return out;
}
Matrix34 concat(const Matrix34 &a,const Matrix34 &b) {
  Matrix34 out{};
  for(unsigned i=0;i<3;++i) {
    for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)out[i*4+j]+=a[i*4+k]*b[k*4+j];
    out[i*4+3]=a[i*4+3];
    for(unsigned k=0;k<3;++k)out[i*4+3]+=a[i*4+k]*b[k*4+3];
  } return out;
}
Matrix34 matrix(const V &p,const V &r,const V &s={1,1,1}) {
  SmdPlacement t; t.position=p;t.rotation=r;t.scale=s;return PlacementMatrix(t);
}
Matrix34 inverse(const Matrix34 &m) {
  const auto normal = NormalMatrix(m); Matrix34 out{};
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)out[i*4+j]=normal[j*3+i];
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)out[i*4+3]-=out[i*4+j]*m[j*4+3];
  return out;
}
V angles(const Matrix34 &m) {
  // Frozen Matrix2AxisAngle: undo x/y first, then read residual z.
  V out{std::atan2(m[9],m[10]),-std::asin(std::clamp(m[8],-1.f,1.f)),0};
  const auto residual=concat(m,inverse(matrix({},out)));
  out[2]=std::atan2(residual[4],residual[0]);return out;
}
float wrap(float x) { while(x>Pi)x-=2*Pi;while(x<-Pi)x+=2*Pi;return x; }
std::uint8_t visualRnd(std::uint16_t &v) {
  const auto old=v;unsigned n=(unsigned(std::uint8_t((old>>1)+(old>>8)))<<8)|std::uint8_t(old>>1);
  v=std::uint16_t(n==old?n+0x101:n);return std::uint8_t(v>>8);
}
std::uint32_t seed3(std::uint16_t &s) {
  const auto a=visualRnd(s), b=visualRnd(s), c=visualRnd(s);
  return unsigned(a)|(unsigned(b)<<8)|(unsigned(c)<<16);
}
V randomVec(const DestructionRecord&r,unsigned p,unsigned spread,std::uint32_t&seed) {
  auto v=vec(r,p), d=vec(r,spread);
  for(unsigned k=0;k<3;++k)v[k]+=d[k]*fRandSeed1_1(&seed);
  return v;
}
bool finite(const V &v,float limit=10000000) {
  return std::all_of(v.begin(),v.end(),[&](float x){return std::isfinite(x)&&std::abs(x)<=limit;});
}
void validateRecord(const DestructionRecord&r) {
  const auto &b=r.bytes;
  const bool trap=r.objectId==0x2a,event=r.objectId==0xc4;
  if(b[0]!=1 || b[1]!=0xfe || b[6] ||
     (trap?(b[7]!=0 && b[7]!=2):event?b[7]!=0xfe:b[7]!=0xff) || b[194]>5 ||
     b[252] || b[253] || b[254] || b[255] || (word(r,8)&~0x6bu) || b[264]>1 ||
     (b[264] && (b[265] || b[267]>3 || b[268]>30 || b[269]>15 || !half(r,272) ||
                  half(r,272)>120 || b[296]>16 || b[292] || b[293] || b[294] || b[295])))
    throw std::runtime_error("Destruction EFF: unsupported Efm05/generator profile");
  for(unsigned p=12;p<=152;p+=4)if(std::abs(real(r,p))>(event?200000:10000))
    throw std::runtime_error("Destruction EFF: Motion budget");
  for(unsigned p:{216u,220u,224u,228u,232u,236u,240u,244u,248u})
    if(std::abs(real(r,p))>(event?200000:10000))throw std::runtime_error("Destruction EFF: Vector budget");
  for(unsigned p=160;p<=172;p+=4)if(real(r,p)<0||real(r,p)>4)
    throw std::runtime_error("Destruction EFF: Color budget");
  if(real(r,136)<=0||real(r,140)<=0||word(r,204)>10000||word(r,208)>1000||word(r,212)>10000)
    throw std::runtime_error("Destruction EFF: Scale/physics budget");
}
std::vector<DestructionMesh> extractMeshes(unsigned id,const std::vector<std::uint8_t>&bin,
                                          const std::vector<std::uint8_t>&tpl) {
  SkinnedModel model(bin); model.validateSkeleton(model.mesh.joints.size());
  const auto full=BuildRoomModelScene(bin,tpl);
  const auto &m=model.mesh;const auto centers=RestJointCenters(m);
  if(m.joints.empty()||m.joints.size()>128||!model.blendJoints.empty())
    throw std::runtime_error("Destruction EFM: Skeleton profile");
  for(const auto&j:m.joints)if(j.parent!=255)throw std::runtime_error("Destruction EFM: Parent profile");
  for(const auto&w:model.weights)if(w.count!=1||w.rates[0]!=1)
    throw std::runtime_error("Destruction EFM: no independent original fragment");
  std::vector<DestructionMesh> result(m.joints.size());
  for(unsigned p=0;p<result.size();++p) {
    result[p].efm=id;result[p].part=p;result[p].restCenter=centers[p];
    result[p].mesh.textures=full.textures;
    result[p].mesh.minimum={INFINITY,INFINITY,INFINITY};result[p].mesh.maximum={-INFINITY,-INFINITY,-INFINITY};
  }
  unsigned drawIndex=0;
  for(const auto&primitive:m.primitives) {
    if(primitive.triangles.empty())continue;
    const auto source=full.draws.at(drawIndex++);
    std::vector<SceneDraw> draws(result.size(),source);
    std::vector<std::size_t> counts(result.size());
    for(unsigned p=0;p<result.size();++p)draws[p].first=result[p].mesh.vertices.size()/3;
    for(const auto&triangle:primitive.triangles) {
      unsigned part=None;
      for(auto corner:triangle) {
        const auto&v=primitive.vertices.at(corner);
        const auto pj=model.weights.at(model.positionWeights.at(v.position)).joints[0];
        const auto nj=model.weights.at(model.normalWeights.at(v.normal)).joints[0];
        if(part==None)part=pj;
        if(pj!=part||nj!=part)throw std::runtime_error("Destruction EFM: triangle spanning multiple joints");
      }
      auto &out=result.at(part).mesh;
      for(auto corner:triangle) {
        const auto&v=primitive.vertices.at(corner);auto point=sub(m.positions.at(v.position),centers[part]);
        out.vertices.insert(out.vertices.end(),point.begin(),point.end());
        auto n=m.normals.at(v.normal);out.normals.insert(out.normals.end(),n.begin(),n.end());
        auto uv=m.texcoords.at(v.texcoord);out.uv.insert(out.uv.end(),uv.begin(),uv.end());
        auto color=source.vertexColors?m.colors.at(v.color):std::array<std::uint8_t,4>{255,255,255,255};
        out.colors.insert(out.colors.end(),color.begin(),color.end());
        for(unsigned k=0;k<3;++k){out.minimum[k]=std::min(out.minimum[k],point[k]);out.maximum[k]=std::max(out.maximum[k],point[k]);}
      }
      counts[part]+=3;out.triangles++;out.texturedTriangles++;
      if(source.vertexColors)out.vertexColorTriangles++;
    }
    for(unsigned p=0;p<result.size();++p)if(counts[p]) {
      draws[p].count=counts[p];result[p].mesh.draws.push_back(draws[p]);
    }
  }
  for(const auto&part:result)if(part.mesh.vertices.empty())throw std::runtime_error("Destruction EFM: Empty original fragment");
  return result;
}
} // namespace

DestructionEffectsData BuildDestructionEffects(const RoomArchive &room,
    const std::vector<std::uint8_t>&core,std::uint16_t roomId) {
  DestructionEffectsData data;data.room=roomId;data.signature=1469598103934665603ULL^roomId;
  const auto*etm=room.find("ETM");if(!etm)return data;
  RoomObjectArchive archive(room.copy(etm-room.entries().data()));
  std::vector<std::vector<std::uint8_t>> owners{core};
  std::map<unsigned,unsigned> ownerIndex;
  for(const auto &p:std::vector<std::pair<unsigned,std::string>>{{1,"et01.eff"},{2,"et02.eff"},{17,"et11.eff"},{45,"et2d.eff"},
      {0,"et00.eff"},{7,"et07.eff"},{0x25,"et25.eff"}}) {
    if(!archive.find(p.second))continue;
    ownerIndex[p.first]=owners.size();owners.push_back(archive.copy(p.second));
  }
  if(ownerIndex.count(17))ownerIndex[30]=ownerIndex[17];
  std::map<unsigned,std::pair<unsigned,std::size_t>> modelSources;
  for(unsigned owner=0;owner<owners.size();++owner) {
    if(owners[owner].empty())continue;
    Eff e(owners[owner]);data.signature=hash(data.signature,owners[owner]);
    const auto ids=e.table(20,8);
    if (!e.r.u32(ids) && !e.r.u32(44)) continue; // Valid owner without local EFM resources.
    const auto offsets=e.table(44,4);
    if(e.r.u32(ids)!=e.r.u32(offsets))throw std::runtime_error("Destruction EFF: EFM count");
    for(unsigned i=0;i<e.r.u32(ids);++i) {
      const auto id=e.r.u16(ids+4+i*8);
      if(id>255)throw std::runtime_error("Destruction EFF: EFM ID");
      // Source registration: earliest registered id survives later owners.
      modelSources.emplace(id,std::make_pair(owner,e.entry(offsets,i,20)));
    }
  }
  std::map<unsigned,unsigned> modelIndices;
  for(const auto&owner:ownerIndex) {
    Eff e(owners[owner.second]);const auto ids=e.table(8,8), offsets=e.table(32,4);
    if(e.r.u32(ids)!=e.r.u32(offsets))throw std::runtime_error("Destruction EFF: EST count");
    const bool window=owner.first==0||owner.first==7||owner.first==0x25;
    const std::set<unsigned> wanted=window?std::set<unsigned>{0,1,3,6,7}:
        owner.first==45?std::set<unsigned>{0}:owner.first==30?std::set<unsigned>{5,6,7,8}:std::set<unsigned>{0,1,2,4};
    for(unsigned i=0;i<e.r.u32(ids);++i) {
      const auto seq=e.r.u16(ids+4+i*8);if(!wanted.count(seq))continue;
      const auto p=e.entry(offsets,i,48);const auto n=e.r.u16(p);
      if(!n||n>256||owners[owner.second][p+36]!=16||owners[owner.second][p+10]!=254||e.r.u16(p+8))
        throw std::runtime_error("Destruction EFF: EST header");
      e.r.range(p+48,std::size_t(n)*300);
      if(p+48+std::size_t(n)*300>e.end(p))throw std::runtime_error("Destruction EFF: EST range");
      for(unsigned j=0;j<n;++j) {
        DestructionRecord rec;rec.objectId=owner.first;rec.sequence=seq;rec.sourceRecord=j;rec.model=None;
        std::copy_n(owners[owner.second].begin()+p+48+j*300,300,rec.bytes.begin());
        if(rec.bytes[1]==0xfe) {
          validateRecord(rec);const auto id=rec.bytes[2];auto found=modelIndices.find(id);
          if(found==modelIndices.end()) {
            const auto source=modelSources.find(id);
            if(source==modelSources.end())throw std::runtime_error("Destruction EFM: Original model is missing");
            Eff modelEff(owners[source->second.first]);
            auto parts=extractMeshes(id,modelEff.sub(source->second.second,4),modelEff.sub(source->second.second,8));
            const auto index=unsigned(data.models.size());
            data.models.push_back({id,unsigned(data.meshes.size()),unsigned(parts.size())});
            for(auto &part:parts)data.meshes.push_back(std::move(part));
            modelIndices.emplace(id,index);rec.model=index;
          }else rec.model=found->second;
        }
        data.records.push_back(rec);
      }
    }
  }
  if(!data.records.empty())data.warnings.push_back("Efm05 wood/metal/glass fragments; dust/sprite/sound entries use separate backends");
  return data;
}
DestructionEffectsData BuildEventDestructionEffects(const std::vector<std::uint8_t> &room,
    const std::vector<std::uint8_t> &core,const std::vector<unsigned> &sequences,
    const std::vector<std::vector<std::uint8_t>> &banks) {
  DestructionEffectsData data;data.room=0x101;data.signature=hash(hash(1469598103934665603ULL,room),core);
  Eff e(room);const auto ids=e.table(8,8),offsets=e.table(32,4);
  std::vector<const std::vector<std::uint8_t>*> resources{&core,&room};
  for(const auto &bank:banks){resources.push_back(&bank);data.signature=hash(data.signature,bank);}
  std::map<unsigned,unsigned> models;
  for(unsigned i=0;i<e.r.u32(ids);++i) {
    const auto sequence=e.r.u16(ids+4+i*8);
    if(std::find(sequences.begin(),sequences.end(),sequence)==sequences.end())continue;
    const auto p=e.entry(offsets,i,48);const auto count=e.r.u16(p);
    if(count>256)throw std::runtime_error("EVD-FEFF: Record budget");
    e.r.range(p+48,std::size_t(count)*300);
    for(unsigned j=0;j<count;++j) {
      if(e.bytes[p+48+j*300+1]!=0xfe)continue;
      DestructionRecord record;record.objectId=0xc4;record.sequence=sequence;record.sourceRecord=j;
      std::copy_n(e.bytes.begin()+p+48+j*300,300,record.bytes.begin());validateRecord(record);
      const unsigned id=record.bytes[2];auto found=models.find(id);
      if(found==models.end()) {
        bool resolved=false;
        for(const auto *resource:resources) {
          Eff bank(*resource);const auto modelIds=bank.table(20,8),modelOffsets=bank.table(44,4);
          for(unsigned k=0;k<bank.r.u32(modelIds);++k)if(bank.r.u16(modelIds+4+k*8)==id) {
            const auto model=bank.entry(modelOffsets,k,20);
            auto parts=extractMeshes(id,bank.sub(model,4),bank.sub(model,8));
            record.model=data.models.size();models.emplace(id,record.model);
            data.models.push_back({id,unsigned(data.meshes.size()),unsigned(parts.size())});
            for(auto &part:parts)data.meshes.push_back(std::move(part));
            resolved=true;break;
          }
          if(resolved)break;
        }
        if(!resolved)throw std::runtime_error("EVD-FEFF: Original model missing");
      } else record.model=found->second;
      data.records.push_back(std::move(record));
    }
  }
  return data;
}
DestructionEffectsData BuildTrapDestructionEffects(const std::vector<std::uint8_t> &trap,
    const std::vector<std::uint8_t> &core,std::uint16_t roomId) {
  DestructionEffectsData data;data.room=roomId;
  data.signature=hash(hash(1469598103934665603ULL,trap),core);
  Eff e(trap),c(core);const auto ids=e.table(8,8),offsets=e.table(32,4);
  if(e.r.u32(ids)!=e.r.u32(offsets))throw std::runtime_error("EM2A-FEFF: EST count");
  const auto modelIds=c.table(20,8),modelOffsets=c.table(44,4);
  if(c.r.u32(modelIds)!=c.r.u32(modelOffsets))throw std::runtime_error("EM2A-FEFF: CORE model count");
  std::map<unsigned,unsigned> models;
  for(unsigned i=0;i<e.r.u32(ids);++i) {
    const auto seq=e.r.u16(ids+4+i*8);
    if(seq!=0 && seq!=2 && seq!=4 && seq!=6)continue;
    if(e.r.u16(ids+6+i*8) || e.r.u32(ids+8+i*8))throw std::runtime_error("EM2A-FEFF: EST selection profile");
    const auto p=e.entry(offsets,i,48);const auto n=e.r.u16(p);
    if(!n || n>256 || e.r.u16(p+8) || e.bytes[p+10]!=0xfe || e.bytes[p+36]!=16)
      throw std::runtime_error("EM2A-FEFF: EST header");
    e.r.range(p+48,std::size_t(n)*300);
    if(p+48+std::size_t(n)*300>e.end(p))throw std::runtime_error("EM2A-FEFF: EST range");
    for(unsigned j=0;j<n;++j) {
      if(e.bytes[p+48+j*300+1]!=0xfe)continue;
      DestructionRecord rec;rec.objectId=0x2a;rec.sequence=seq;rec.sourceRecord=j;
      std::copy_n(e.bytes.begin()+p+48+j*300,300,rec.bytes.begin());validateRecord(rec);
      const auto id=unsigned(rec.bytes[2]);if(id!=8 && id!=14)throw std::runtime_error("EM2A-FEFF: non-original fragment model");
      auto found=models.find(id);
      if(found==models.end()) {
        std::optional<std::size_t> model;
        for(unsigned k=0;k<c.r.u32(modelIds);++k)if(c.r.u16(modelIds+4+k*8)==id)model=c.entry(modelOffsets,k,20);
        if(!model)throw std::runtime_error("EM2A-FEFF: Original CORE fragment is missing");
        auto parts=extractMeshes(id,c.sub(*model,4),c.sub(*model,8));
        rec.model=data.models.size();models.emplace(id,rec.model);
        data.models.push_back({id,unsigned(data.meshes.size()),unsigned(parts.size())});
        for(auto &part:parts)data.meshes.push_back(std::move(part));
      } else rec.model=found->second;
      data.records.push_back(std::move(rec));
    }
  }
  if(data.models.size()!=2 || data.records.size()!=13)
    throw std::runtime_error("EM2A-FEFF: Incomplete original blast/bear trap profile");
  return data;
}
void AppendDestructionData(DestructionEffectsData &destination,const DestructionEffectsData &source) {
  if(destination.room!=source.room)throw std::runtime_error("Destruction EFF: Room mismatch while merging");
  auto data=destination;const unsigned meshBase=data.meshes.size(),modelBase=data.models.size();
  data.meshes.insert(data.meshes.end(),source.meshes.begin(),source.meshes.end());
  for(auto model:source.models){model.firstMesh+=meshBase;data.models.push_back(model);}
  for(auto record:source.records){if(record.model!=None)record.model+=modelBase;data.records.push_back(record);}
  data.warnings.insert(data.warnings.end(),source.warnings.begin(),source.warnings.end());
  data.signature=(data.signature^source.signature)*1099511628211ULL;destination=std::move(data);
}
void AppendDestructionScene(StaticScene &scene,const DestructionEffectsData&data) {
  for(unsigned index=0;index<data.meshes.size();++index) {
    const auto&mesh=data.meshes[index].mesh;
    if(mesh.triangles>1000000-scene.triangles)throw std::runtime_error("Destruction EFM: Scene budget");
    const auto vertexBase=scene.vertices.size()/3;const int textureBase=scene.textures.size();
    scene.vertices.insert(scene.vertices.end(),mesh.vertices.begin(),mesh.vertices.end());
    scene.normals.insert(scene.normals.end(),mesh.normals.begin(),mesh.normals.end());
    scene.uv.insert(scene.uv.end(),mesh.uv.begin(),mesh.uv.end());
    scene.colors.insert(scene.colors.end(),mesh.colors.begin(),mesh.colors.end());
    scene.textures.insert(scene.textures.end(),mesh.textures.begin(),mesh.textures.end());
    for(auto draw:mesh.draws) {
      draw.first+=vertexBase;draw.destructionMesh=int(index);draw.work=0xfe;
      draw.sky=draw.water=-1;draw.roomObjectSlot=draw.roomItemIndex=-1;
      if(draw.texture>=0)draw.texture+=textureBase;
      if(draw.alphaTexture>=0)draw.alphaTexture+=textureBase;
      scene.draws.push_back(draw);
    }
    scene.triangles+=mesh.triangles;scene.texturedTriangles+=mesh.texturedTriangles;
    scene.vertexColorTriangles+=mesh.vertexColorTriangles;
  }
}
std::string DestructionEffectsData::describe()const {
  std::size_t triangles=0;for(const auto&m:meshes)triangles+=m.mesh.triangles;
  std::ostringstream s;s<<"Break Efm05: "<<models.size()<<" original models, "<<meshes.size()<<" original parts, "<<triangles<<" textured triangles";return s.str();
}
DestructionEffectsSimulation::DestructionEffectsSimulation(const DestructionEffectsData&data,const SatCollision*collision,const SatCollision*scenario)
    :data_(&data),collision_(collision),scenario_(scenario){state_.signature=data.signature;}
void DestructionEffectsSimulation::sequence(unsigned objectId,unsigned seq,unsigned slot,
                                            const Matrix34&placement,std::uint32_t seed) {
  for(unsigned i=0;i<data_->records.size();++i) {
    const auto&r=data_->records[i];if(r.objectId!=objectId||r.sequence!=seq)continue;
    if (r.model==None) continue;
    if (half(r,4)) {
      if (state_.pending.size() >= 1024) throw std::runtime_error("Break Efm05: Event pool full");
      state_.pending.push_back({i,slot,half(r,4),seed,placement});
    } else activate(i,slot,placement,seed);
  }
}
void DestructionEffectsSimulation::activate(unsigned i,unsigned slot,
                                             const Matrix34 &placement,std::uint32_t &seed) {
    const auto &r=data_->records.at(i);
    if(r.bytes[264]) {
      unsigned interval=r.bytes[268];
      if(r.bytes[296]) interval=(interval+visualRnd(state_.random)%r.bytes[296])&255;
      auto emitterSeed=(r.bytes[267]&2)?0x12345678u+std::int8_t(r.bytes[270]):seed3(state_.random);
      if(r.model!=None) {
        state_.emitters.push_back({i,slot,0,0,emitterSeed,placement});
        auto &emitter=state_.emitters.back();
        emitter.interval=interval;
        for(unsigned k=0;k<=r.bytes[269];++k)spawn(i,slot,placement,emitter.seed);
        emitter.age=1;emitter.next=interval;
        if (r.bytes[296]) emitter.next = unsigned(std::clamp(int(emitter.next) +
            int(visualRnd(state_.random) % (r.bytes[296] * 2)) - r.bytes[296], 0, 255));
        if(emitter.age>=half(r,272))state_.emitters.pop_back();
      }
    }else if(r.model!=None)spawn(i,slot,placement,seed);
}
bool DestructionEffectsSimulation::trigger(unsigned objectId,unsigned boxType,unsigned slot,
    const V&position,const V&rotation,unsigned kind,std::uint64_t eventSerial,std::uint32_t seed) {
  if(kind>2||slot>65535||!finite(position)||!finite(rotation,100)||state_.events.size()>=MaxEvents)
    return false;
  if(std::any_of(state_.events.begin(),state_.events.end(),[&](const auto&k){return k.slot==slot&&k.serial==eventSerial;}))return false;
  const bool explosive=objectId==45;
  const unsigned sequenceId=explosive?0:boxType==5?kind+5:kind, fallback=boxType==5?8:4;
  if(!std::any_of(data_->records.begin(),data_->records.end(),[&](const auto&r){return r.objectId==objectId&&r.sequence==sequenceId&&r.model!=None;}))return false;
  // Build transactionally, so an exhausted bounded visual pool cannot mutate
  // gameplay or leave only part of a source destruction event applied.
  const auto previous=state_;
  try {
    state_.events.push_back({slot,eventSerial});const auto pose=matrix(position,rotation);
    sequence(objectId,sequenceId,slot,pose,seed);
    if(!explosive)sequence(objectId,fallback,slot,pose,seed3(state_.random));
    return true;
  }catch(...){state_=previous;return false;}
}
bool DestructionEffectsSimulation::restoreBroken(unsigned objectId,unsigned boxType,unsigned slot,
                                                 const V&position,const V&rotation) {
  if(objectId==45||slot>65535||!finite(position)||!finite(rotation,100))return false;
  if(std::any_of(state_.models.begin(),state_.models.end(),[&](const auto&m){return m.slot==slot;}))return false;
  const unsigned seq=boxType==5?8:4;
  if(!std::any_of(data_->records.begin(),data_->records.end(),[&](const auto&r){return r.objectId==objectId&&r.sequence==seq&&r.model!=None;}))return false;
  const auto previous=state_;try{sequence(objectId,seq,slot,matrix(position,rotation),seed3(state_.random));return true;}
  catch(...){state_=previous;return false;}
}
bool DestructionEffectsSimulation::triggerWindow(unsigned objectId,unsigned seq,unsigned slot,
    const V&position,const V&rotation,std::uint64_t eventSerial,std::uint32_t seed) {
  if((objectId!=0&&objectId!=7&&objectId!=0x25)||
     (seq!=0&&seq!=1&&seq!=3&&seq!=6&&seq!=7)||slot>65535||
     !finite(position)||!finite(rotation,100)||state_.events.size()>=MaxEvents) return false;
  if(std::any_of(state_.events.begin(),state_.events.end(),[&](const auto&k){return k.slot==slot&&k.serial==eventSerial;}))return false;
  if(!std::any_of(data_->records.begin(),data_->records.end(),[&](const auto&r){return r.objectId==objectId&&r.sequence==seq&&r.model!=None;}))return false;
  const auto previous=state_;
  try {
    state_.events.push_back({slot,eventSerial});
    sequence(objectId,seq,slot,matrix(position,rotation),seed);
    return true;
  }catch(...){state_=previous;return false;}
}
bool DestructionEffectsSimulation::triggerTrap(unsigned sequence,unsigned slot,
    const std::vector<Matrix34> &parts,std::uint64_t eventSerial,std::uint32_t seed) {
  if((sequence!=0 && sequence!=2 && sequence!=4 && sequence!=6) || slot>65535 ||
      parts.empty() || parts.size()>248 || state_.events.size()>=MaxEvents)return false;
  for(const auto &matrix:parts)for(float value:matrix)
    if(!std::isfinite(value) || std::abs(value)>10000000)return false;
  if(std::any_of(state_.events.begin(),state_.events.end(),[&](const auto &e){return e.slot==slot && e.serial==eventSerial;}))return false;
  const auto previous=state_;bool emitted=false;
  try {
    for(unsigned i=0;i<data_->records.size();++i) {
      const auto &r=data_->records[i];if(r.objectId!=0x2a || r.sequence!=sequence || r.model==None)continue;
      if(r.bytes[7]>=parts.size())throw std::runtime_error("EM2A-FEFF: Original parent part is missing");
      const auto &matrix=parts[r.bytes[7]];
      if(half(r,4))state_.pending.push_back({i,slot,half(r,4),seed,matrix});
      else activate(i,slot,matrix,seed);
      emitted=true;
    }
    if(emitted)state_.events.push_back({slot,eventSerial});
    return emitted;
  }catch(...){state_=previous;return false;}
}
bool DestructionEffectsSimulation::triggerTrapRecord(unsigned sequence,unsigned sourceRecord,unsigned slot,
    const Matrix34 &placement,std::uint64_t eventSerial,std::uint64_t spawnSerial,std::uint32_t seed) {
  return triggerOriginalRecord(0x2a,sequence,sourceRecord,slot,placement,eventSerial,spawnSerial,seed);
}
bool DestructionEffectsSimulation::triggerOriginalRecord(unsigned owner,unsigned sequence,
    unsigned sourceRecord,unsigned slot,const Matrix34 &placement,
    std::uint64_t eventSerial,std::uint64_t spawnSerial,std::uint32_t seed) {
  if(slot>65535 || !spawnSerial || state_.events.size()>=MaxEvents)return false;
  for(float value:placement)
    if(!std::isfinite(value) || std::abs(value)>10000000)return false;
  if(std::any_of(state_.events.begin(),state_.events.end(),[&](const auto &key) {
    return key.slot==slot && key.serial==eventSerial && key.spawnSerial==spawnSerial;
  }))return false;
  const auto record=std::find_if(data_->records.begin(),data_->records.end(),[&](const auto &r) {
    return r.objectId==owner && r.sequence==sequence && r.sourceRecord==sourceRecord && r.model!=None;
  });
  if(record==data_->records.end())return false;
  const auto previous=state_;
  try {
    // No activate(): the source Espgen schedule and seed are owned by the
    // mixed world backend, including its neighboring sprite records.
    spawn(unsigned(record-data_->records.begin()),slot,placement,seed);
    state_.events.push_back({slot,eventSerial,spawnSerial});
    return true;
  }catch(...){state_=previous;return false;}
}
void DestructionEffectsSimulation::spawn(unsigned record,unsigned slot,const Matrix34&placement,std::uint32_t&seed) {
  if(state_.models.size()>=MaxModels)throw std::runtime_error("Break Efm05: Model pool full");
  const auto&r=data_->records.at(record);const auto&group=data_->models.at(r.model);
  DestructionModelState m;m.record=record;m.slot=slot;m.serial=++state_.serial;m.seed=seed;
  m.position=randomVec(r,12,24,seed);m.rotation=scale(randomVec(r,88,100,seed),Deg);
  m.angularSpeed=scale(randomVec(r,112,124,seed),Deg);
  m.sizeX=real(r,136)*.005f;m.sizeY=real(r,140)*.005f;
  const auto spread=real(r,144)*fRandSeed1_1(&seed);m.sizeX+=spread;m.sizeY+=spread;
  m.sizeStep=real(r,148);for(unsigned k=0;k<4;++k)m.color[k]=r.bytes[156+k];
  m.burstCenter=randomVec(r,216,240,seed);
  m.position=TransformPoint(placement,m.position);
  const auto rotation=concat(placement,matrix({},m.rotation));m.rotation=angles(rotation);
  m.burstCenter=sr(matrix({},m.rotation),m.burstCenter);m.age=half(r,186);
  // EfmSetObj05 calls matUpdate with default scale before its first move.
  m.matrix=matrix(m.position,m.rotation);m.parts.resize(group.parts);
  for(unsigned i=0;i<group.parts;++i) {
    auto&p=m.parts[i];p.position=data_->meshes.at(group.firstMesh+i).restCenter;
    p.world=TransformPoint(m.matrix,p.position);
  }
  if(move(m))state_.models.push_back(std::move(m));
}
bool DestructionEffectsSimulation::move(DestructionModelState&m) {
  const auto&r=data_->records.at(m.record);const auto flags=word(r,8);
  if(half(r,182)<=m.age) {
    m.sizeMultiplier+=m.sizeStep;m.sizeStep*=real(r,152);
    if(m.sizeMultiplier<=0)return false;
  }
  m.rotation=add(m.rotation,m.angularSpeed);
  const auto fade=half(r,176),delay=half(r,178);
  if(fade<m.age) {
    if(fade+delay<=m.age) {
      for(unsigned k=0;k<4;++k)m.color[k]=std::min(255.f,m.color[k]*real(r,160+k*4));
      if(m.color[3]<4)return false;
    }
  }else if(fade)m.color[3]=float(r.bytes[159])*float(m.age)/float(fade);
  if(half(r,184)&&half(r,184)<=m.age)return false;
  m.age=std::uint16_t(m.age+1); // Original OBJ05_FREE::Life_time is u16.
  const float range=float((m.age+1)*(unsigned(r.bytes[201])*2+1))+1.f;
  const float power=float(r.bytes[200])*4096.f/range;
  const float random=float(r.bytes[202])/32.f, rotationAmp=float(r.bytes[203])*.005f;
  const float damping=1.f-float(word(r,208))*.001f, gravity=-float(word(r,204))*.1f;
  const auto rebound=scale(vec(r,228),.1f);const auto inv=inverse(m.matrix);
  for(auto&p:m.parts) {
    if(!p.flag) {
      auto d=sub(p.world,add(m.position,m.burstCenter));
      if(length(d)<range||r.bytes[201]==255) {
        p.flag=1;if(d==V{})d[1]=1;
        p.speed=scale(d,power/length(d));
        for(unsigned k=0;k<3;++k)p.speed[k]+=power*random*fRandSeed1_1(&m.seed);
        for(unsigned k=0;k<3;++k)p.angularSpeed[k]=rotationAmp*fRandSeed1_1(&m.seed);
      }
    }
    if(p.flag==1) {
      p.position=add(p.position,sr(inv,p.speed));p.speed=scale(p.speed,damping);p.speed[1]+=gravity;
      p.rotation=add(p.rotation,p.angularSpeed);for(auto&angle:p.rotation)angle=wrap(angle);
      const auto old=p.world;
      p.world=TransformPoint(m.matrix,p.position);bool hit=false;
      if(flags&2) {
        if(scenario_) {
        const CollisionVec from{old[0],old[1],old[2]}, to{p.world[0],p.world[1],p.world[2]};
        if(const auto contact=scenario_->line(from,to,SatSubset::All,0,0)) {
          V normal{float(contact->normal[0]),float(contact->normal[1]),float(contact->normal[2])};
          const auto magnitude=length(p.speed);const auto normalMagnitude=length(normal);
          if(magnitude>0&&normalMagnitude>0) {
            normal=scale(normal,-1.f/normalMagnitude);
            const auto incoming=scale(p.speed,-1.f/magnitude);
            const float dot=incoming[0]*normal[0]+incoming[1]*normal[1]+incoming[2]*normal[2];
            auto reflection=sub(scale(normal,2.f*dot),incoming);
            const float reflectionMagnitude=length(reflection);
            if(reflectionMagnitude>0)p.speed=scale(reflection,magnitude*rebound[0]/reflectionMagnitude);
          }
          for(unsigned k=0;k<3;++k)p.world[k]=float(contact->point[k]+contact->normal[k]);
          p.angularSpeed=scale(p.angularSpeed,-.8f);hit=true;
        }
        }
      }else if((flags&1)&&collision_) {
        const CollisionVec at{p.world[0],p.world[1],p.world[2]};
        const auto floor=collision_->ground(at,600,100000);const float offset=float(word(r,212));
        if(floor&&p.world[1]-offset<floor->point[1]) {
          p.speed[0]*=rebound[0];p.speed[1]*=-rebound[1];p.speed[2]*=rebound[0];
          p.world[1]=float(floor->point[1])+offset;hit=true;p.angularSpeed=scale(p.angularSpeed,.8f);
          if(flags&8) {
            p.rotation[0]+=Pi/2;p.angularSpeed=scale(p.angularSpeed,-.9f);
            const auto y=p.rotation[1];p.rotation=scale(p.rotation,.55f);p.rotation[1]=y;p.rotation[0]-=Pi/2;
          }
        }
      }
      if(hit) {
        p.position=TransformPoint(inv,p.world);
        if(length(p.speed)<15) {
          if((flags&8)&&std::abs(p.rotation[0]+Pi/2)>.4f) {
            for(auto&a:p.rotation)a=wrap(a);
            p.rotation[0]+=Pi/2;p.angularSpeed=scale(p.angularSpeed,-.7f);
            const auto y=p.rotation[1];p.rotation=scale(p.rotation,.8f);p.rotation[1]=y;p.rotation[0]-=Pi/2;
          }else p.flag=2;
        }
      }
    }
  }
  const auto s=V{m.sizeX*m.sizeMultiplier,m.sizeY*m.sizeMultiplier,m.sizeX*m.sizeMultiplier};
  m.matrix=matrix(m.position,m.rotation,s);
  for(auto&p:m.parts)p.world=TransformPoint(m.matrix,p.position);
  return finite(m.position)&&finite(m.rotation)&&std::isfinite(m.sizeMultiplier);
}
void DestructionEffectsSimulation::tick() {
  // game.cpp: ObjMgr.move precedes EspgenMove. New EFM calls move once at
  // construction; retained objects receive their next update next game tick.
  state_.models.erase(std::remove_if(state_.models.begin(),state_.models.end(),[&](auto&m){return !move(m);}),state_.models.end());
  for(auto&e:state_.emitters) {
    const auto&r=data_->records.at(e.record);
    if(!e.next) {
      e.next=e.interval;
      if (r.bytes[296]) e.next = unsigned(std::clamp(int(e.next) +
          int(visualRnd(state_.random) % (r.bytes[296] * 2)) - r.bytes[296], 0, 255));
      for(unsigned i=0;i<=r.bytes[269];++i)spawn(e.record,e.slot,e.placement,e.seed);
    }else --e.next;
    ++e.age;
  }
  state_.emitters.erase(std::remove_if(state_.emitters.begin(),state_.emitters.end(),[&](const auto&e){return e.age>=half(data_->records.at(e.record),272);}),state_.emitters.end());
  // Timed EST records create their generators only at the source Set_time;
  // a newly created generator receives its construction update once.
  for (auto &pending:state_.pending) if (pending.remaining) --pending.remaining;
  for (auto &pending:state_.pending) if (!pending.remaining)
    activate(pending.record,pending.slot,pending.placement,pending.seed);
  state_.pending.erase(std::remove_if(state_.pending.begin(),state_.pending.end(),
      [](const auto &p){return !p.remaining;}),state_.pending.end());
  ++state_.frames;
}
std::vector<DestructionInstance> DestructionEffectsSimulation::instances()const {
  std::vector<DestructionInstance> out;
  for(const auto&m:state_.models) {
    const auto&r=data_->records.at(m.record);const auto&group=data_->models.at(r.model);
    for(unsigned i=0;i<group.parts;++i) {
      DestructionInstance instance;instance.mesh=group.firstMesh+i;instance.slot=m.slot;
      instance.blend=r.bytes[194];instance.zMode=(word(r,8)&0x200000)?1:0;
      instance.serial=m.serial;
      // cModel::partsWorldCalc removes non-uniform parent scale before
      // composing local rotation, then reapplies it on the right.
      const V scaleValue{m.sizeX*m.sizeMultiplier,m.sizeY*m.sizeMultiplier,m.sizeX*m.sizeMultiplier};
      instance.matrix=concat(matrix({},m.rotation),matrix({},m.parts[i].rotation,scaleValue));
      for(unsigned k=0;k<3;++k)instance.matrix[k*4+3]=m.parts[i].world[k];
      for(unsigned k=0;k<3;++k)instance.color[k]=float(std::uint8_t(std::clamp(m.color[k],0.f,255.f)))/255.f;
      instance.color[3]=std::clamp(m.color[3]/255.f,0.f,1.f);out.push_back(instance);
    }
  }return out;
}
bool DestructionEffectsSimulation::restore(const DestructionEffectsState&s) {
  if(s.signature!=data_->signature||s.models.size()>MaxModels||s.events.size()>MaxEvents||s.emitters.size()>1024||s.pending.size()>1024)return false;
  std::set<std::uint64_t> serials;std::set<std::tuple<unsigned,std::uint64_t,std::uint64_t>>events;
  for(const auto&event:s.events)if(event.slot>65535||!events.emplace(event.slot,event.serial,event.spawnSerial).second)return false;
  for(const auto&e:s.emitters) {
    if(e.record>=data_->records.size())return false;
    const auto&r=data_->records[e.record];
    if(r.model==None||!r.bytes[264]||e.slot>65535||e.age>=half(r,272)||e.next>255||e.interval>255)return false;
    for(auto x:e.placement)if(!std::isfinite(x)||std::abs(x)>10000000)return false;
  }
  for(const auto&p:s.pending) {
    if(p.record>=data_->records.size()||p.slot>65535||!p.remaining)return false;
    const auto&r=data_->records[p.record];
    if(r.model==None||p.remaining>half(r,4))return false;
    for(auto x:p.placement)if(!std::isfinite(x)||std::abs(x)>10000000)return false;
  }
  for(const auto&m:s.models) {
    if(m.record>=data_->records.size()||!m.serial||m.serial>s.serial||!serials.insert(m.serial).second)return false;
    const auto&r=data_->records[m.record];if(r.model>=data_->models.size()||m.parts.size()!=data_->models[r.model].parts||m.slot>65535||m.age>65535)return false;
    if(!finite(m.position)||!finite(m.rotation)||!finite(m.angularSpeed)||!finite(m.burstCenter)||
       !std::isfinite(m.sizeX)||!std::isfinite(m.sizeY)||!std::isfinite(m.sizeMultiplier)||!std::isfinite(m.sizeStep)||std::abs(m.sizeX)>10000||std::abs(m.sizeY)>10000||m.sizeMultiplier<=0||m.sizeMultiplier>10000||std::abs(m.sizeStep)>10000)return false;
    for(auto c:m.color)if(!std::isfinite(c)||c<0||c>255)return false;
    for(auto x:m.matrix)if(!std::isfinite(x)||std::abs(x)>10000000)return false;
    for(const auto&p:m.parts)if(p.flag>2||!finite(p.position)||!finite(p.world)||!finite(p.rotation)||!finite(p.speed)||!finite(p.angularSpeed))return false;
  }
  state_=s;return true;
}
} // namespace re4
