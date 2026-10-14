#include "room_shadows.h"
#include "asset_reader.h"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace re4 {
std::shared_ptr<const RoomShadowData> ReadRoomShadows(const RoomArchive &room) {
  const auto *entry=room.find("SHD");if(!entry)return {};
  const auto bytes=room.copy(entry-room.entries().data());const AssetReader r{bytes};r.range(0,16);
  const unsigned count=r.u16(2),table=r.u32(4);
  if(bytes[0]>0x41 || count>256)
    throw std::runtime_error("SHD: invalid original header");
  if(!count)return std::make_shared<RoomShadowData>();
  if(table<16+count*72 || table>=bytes.size())
    throw std::runtime_error("SHD: invalid original header");
  r.range(16,count*72);std::map<unsigned,std::unique_ptr<SmdModel>> models;
  unsigned largest=0;for(unsigned i=0;i<count;++i)largest=std::max(largest,unsigned(bytes[16+i*72+36]));
  r.range(table,(largest+1)*4);auto result=std::make_shared<RoomShadowData>();
  for(unsigned i=0;i<count;++i) {
    const auto at=16+i*72;const unsigned id=bytes[at+36];auto &model=models[id];
    if(!model) {
      const unsigned begin=table+r.u32(table+id*4);std::size_t end=bytes.size();
      if(begin<table+(largest+1)*4 || begin>=end)throw std::runtime_error("SHD: invalid BIN offset");
      for(unsigned j=0;j<=largest;++j){const auto next=table+r.u32(table+j*4);if(next>begin)end=std::min(end,std::size_t(next));}
      // SHD BINs are unskinned receiver surfaces and have no bone-weight
      // table. Feed the original GX reader an aligned one-model directory.
      std::vector<std::uint8_t> resource(32);resource[0]=0x40;resource[7]=16;resource[19]=16;
      resource.insert(resource.end(),bytes.begin()+begin,bytes.begin()+end);
      Smd parsed(resource,{1,0,0});model=std::make_unique<SmdModel>(std::move(parsed.models.at(0)));
    }
    SmdPlacement pose;for(unsigned k=0;k<3;++k){pose.position[k]=r.f32(at+k*4);pose.rotation[k]=r.f32(at+12+k*4);pose.scale[k]=r.f32(at+24+k*4);}
    const auto matrix=PlacementMatrix(pose);const auto normals=NormalMatrix(matrix);
    for(const auto &primitive:model->primitives)for(const auto &triangle:primitive.triangles)for(const auto corner:triangle) {
      if(result->vertices.size()>=6000000)throw std::runtime_error("SHD: Receiver budget");
      const auto &ref=primitive.vertices.at(corner);
      const auto point=TransformPoint(matrix,model->positions.at(ref.position));
      const auto normal=TransformNormal(normals,model->normals.at(ref.normal));
      result->vertices.insert(result->vertices.end(),point.begin(),point.end());result->vertices.insert(result->vertices.end(),normal.begin(),normal.end());
    }
    ++result->receivers;
  }
  return result;
}
} // namespace re4
