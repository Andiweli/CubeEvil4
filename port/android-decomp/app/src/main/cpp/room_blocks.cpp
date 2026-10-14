#include "room_blocks.h"
#include "asset_reader.h"
#include "room_objects.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace re4 {
RoomBlockLayer BuildRoomBlocks(const RoomArchive &room, const Smx *smx,
                               const std::string &path,
                               std::uint16_t roomId) {
  RoomBlockLayer out;
  if (roomId != 0x100) return out;
  const auto *entry = room.find("BLK");
  if (!entry) throw std::runtime_error("r100: Original BLK connections are missing");
  const auto bytes = room.copy(entry - room.entries().data());
  AssetReader r{bytes};
  r.range(0, 24);
  if (r.u32(0) != 0x424c4b00 || r.u16(4) != 0x100 || bytes[7] != 5)
    throw std::runtime_error("r100: Unknown BLK profile");
  const auto link = r.u32(12), area = r.u32(16), connect = r.u32(20);
  const std::size_t areas = r.u16(8), connects = r.u16(10);
  if (areas > 4096 || connects > 4096 || link < 24 ||
      area < std::size_t(link) + 5 * 12 ||
      connect < std::size_t(area) + areas * 56)
    throw std::runtime_error("r100: Invalid BLK tables");
  r.range(link, 5 * 12); r.range(area, areas * 56);
  r.range(connect, connects * 20);
  for(unsigned i=0;i<connects;++i) {
    const auto offset=connect+i*20;
    std::uint32_t mask=0;
    if(bytes[offset]&1) {
      const auto block=bytes[offset+1];
      if(block>=5)throw std::runtime_error("r100: BLK connection block outside profile bounds");
      mask=1u<<block;
      // checkBlockConnect_sub includes both directions of the link table.
      for(unsigned other=0;other<5;++other)for(unsigned k=0;k<8;++k) {
        const auto neighbour=bytes[link+other*12+4+k];
        if(other==block && neighbour!=0xff) {
          if(neighbour>=5)throw std::runtime_error("r100: Invalid BLK neighbor");
          mask|=1u<<neighbour;
        } else if(neighbour==block)mask|=1u<<other;
      }
    }
    out.visibleSets.push_back(mask);
  }
  for(unsigned i=0;i<areas;++i) {
    const auto offset=area+i*56;
    if(!(bytes[offset+52]&1))continue;
    RoomBlockLayer::Area parsed;
    parsed.priority=bytes[offset+55];parsed.number=static_cast<std::int8_t>(bytes[offset+54]);
    if(parsed.priority>7 || parsed.number<0 || unsigned(parsed.number)>=connects)
      throw std::runtime_error("r100: Invalid BLK range");
    auto &volume=parsed.volume;volume.enabled=bytes[offset+4];volume.type=bytes[offset+5];
    volume.floor=r.f32(offset+8);volume.height=r.f32(offset+12);volume.radius=r.f32(offset+16);
    for(unsigned k=0;k<8;++k)volume.points[k/2][k%2]=r.f32(offset+20+k*4);
    out.areas.push_back(parsed);
  }
  std::stable_sort(out.areas.begin(),out.areas.end(),[](const auto &a,const auto &b){return a.priority>b.priority;});
  if (path.size() < 4 || path.substr(path.size() - 4) != ".dat")
    throw std::runtime_error("r100: Invalid block cache path");
  std::vector<std::unique_ptr<Smd>> blocks;
  SmdResourceCounts required{0, 1, 0};
  for (unsigned i = 0; i < 5; ++i) {
    const auto offset = link + i * 12;
    if (!(bytes[offset] & 1))
      throw std::runtime_error("r100: Original room block is disabled");
    for (unsigned k = 0; k < 8; ++k) {
      const auto neighbour = bytes[offset + 4 + k];
      if (neighbour != 0xff && neighbour >= 5)
        throw std::runtime_error("r100: BLK neighbor outside profile bounds");
    }
    std::ostringstream name;
    name << path.substr(0, path.size() - 4) << '_' << std::hex
         << std::setfill('0') << std::setw(2) << i << ".dat";
    const auto block = RoomArchive::Load(name.str());
    if (block.size() > 16u * 1024 * 1024 - out.sourceBytes)
      throw std::runtime_error("r100: Shared block source budget");
    out.sourceBytes += block.size();
    const auto *smd = block.find("SMD");
    if (!smd || block.find("SMD", 1))
      throw std::runtime_error("r100: Block does not contain the original SMD");
    auto parsed = std::make_unique<Smd>(block.copy(smd - block.entries().data()));
    const auto need = parsed->commonRequirements();
    required.bins = std::max(required.bins, need.bins);
    required.palettes = std::max(required.palettes, need.palettes);
    required.motions = std::max(required.motions, need.motions);
    blocks.push_back(std::move(parsed));
  }
  const auto *commonEntry = room.find("SMD", 1);
  if (!commonEntry) throw std::runtime_error("r100: Shared SMD is missing");
  // Common SMD placements are resource definitions, never another instance
  // set. Include requirements from every block before parsing its tables.
  const Smd common(room.copy(commonEntry - room.entries().data()), required);
  for (unsigned index=0;index<blocks.size();++index) {
    const auto &block=blocks[index];
    auto mesh = BuildStaticScene(*block, -1, &common);
    if (mesh.vertices.empty() || mesh.missingCommonPlacements)
      throw std::runtime_error("r100: Room block contains no usable geometry");
    AttachSceneTextures(*block, mesh, &common);
    if (mesh.fallbackTriangles || mesh.unresolvedAlphaTriangles)
      throw std::runtime_error("r100: Original block materials are missing");
    if (smx) ApplySceneSmx(*smx, mesh);
    for(auto &draw:mesh.draws)draw.roomBlock=int(index);
    AppendStaticSceneLayer(out.mesh, mesh);
    ++out.count;
  }
  return out;
}
std::string RoomBlockLayer::describe() const {
  std::ostringstream out;
  out << "Original room blocks " << count << " · Placements "
      << mesh.included << " · Triangles " << mesh.triangles
      << " · Source data " << sourceBytes << " bytes · BLK visibility ranges " << areas.size();
  return out.str();
}
std::uint32_t RoomBlockLayer::visibleMask(const CollisionVec &feet) {
  // Original checkBlockArea adds200Y, keeps the previous area in gaps and
  // resolves overlapping areas through OT priority and the current-area tie.
  auto p=feet;p[1]+=200;
  int next=selectedArea;unsigned priority=0;
  for(const auto &area:areas) {
    const auto &a=area.volume;
    if(p[1]+100<a.floor || p[1]>=a.floor+a.height)continue;
    bool hit=false;
    if(a.type==2)hit=std::hypot(p[0]-a.points[0][0],p[2]-a.points[0][1])<a.radius;
    else if(a.type==1) {
      const auto cross=[&](unsigned from,unsigned to) {
        const auto &x=a.points[from],&y=a.points[to];
        return (y[0]-x[0])*(p[2]-x[1])-(y[1]-x[1])*(p[0]-x[0]);
      };
      hit=!(cross(0,3)>0 || cross(0,1)<0 || cross(2,3)<0 || cross(2,1)>0);
    }
    if(!hit)continue;
    if(priority>area.priority)break;
    next=area.number;priority=area.priority;
    if(next==selectedArea)break;
  }
  selectedArea=next;
  return next>=0?visibleSets.at(unsigned(next)):0xffffffffu;
}
} // namespace re4
