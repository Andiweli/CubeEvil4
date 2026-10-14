// Original disk/resource binding: game/item_model.cpp. Pickup placement:
// game/sce_at.cpp::sceAtSetItem/SceAtSetItemModel and game/obj19.cpp.
#include "room_items.h"
#include "asset_reader.h"
#include "object_actions.h"
#include "room_objects.h"
#include "model_skin.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr std::size_t MaxBytes = 64u * 1024 * 1024;
void appendItem(StaticScene &scene, const StaticScene &model,
                const RoomItemPlacement &item,
                const std::vector<int> &textures) {
  if (model.triangles > 1000000 - scene.triangles)
    throw std::runtime_error("Room items: Triangle budget");
  SmdPlacement placement;
  placement.position = item.position;
  placement.rotation = item.rotation;
  placement.scale = {1, 1, 1};
  const auto matrix = PlacementMatrix(placement);
  const auto normalMatrix = NormalMatrix(matrix);
  const auto first = scene.vertices.size() / 3;
  for (std::size_t i = 0; i < model.vertices.size(); i += 3) {
    const auto point =
        TransformPoint(matrix, {model.vertices[i], model.vertices[i + 1],
                                model.vertices[i + 2]});
    const auto normal =
        TransformNormal(normalMatrix, {model.normals[i], model.normals[i + 1],
                                       model.normals[i + 2]});
    if (scene.vertices.empty())
      scene.minimum = scene.maximum = point;
    for (unsigned k = 0; k < 3; ++k) {
      scene.minimum[k] = std::min(scene.minimum[k], point[k]);
      scene.maximum[k] = std::max(scene.maximum[k], point[k]);
    }
    scene.vertices.insert(scene.vertices.end(), point.begin(), point.end());
    scene.normals.insert(scene.normals.end(), normal.begin(), normal.end());
  }
  scene.uv.insert(scene.uv.end(), model.uv.begin(), model.uv.end());
  scene.colors.insert(scene.colors.end(), model.colors.begin(),
                      model.colors.end());
  for (auto draw : model.draws) {
    draw.first += first;
    draw.object = item.index;
    draw.sortCenter = TransformPoint(matrix,draw.sortCenter);
    // obj19.cpp::setItemObj: sphere at parts0, size500/500/0, mask0x20.
    draw.lightVolume.type=1;draw.lightVolume.size={500,500,0};draw.lightSize=500;
    draw.lightVolume.center=draw.sortCenter;
    for(unsigned axis=0;axis<3;++axis) {
      std::array<float,3> basis{};
      for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)
        basis[k]+=matrix[k*4+j]*draw.lightVolume.axes[axis*3+j];
      std::copy(basis.begin(),basis.end(),draw.lightVolume.axes.begin()+axis*3);
    }
    draw.roomObjectSlot = -1;
    draw.roomItemIndex = static_cast<int>(item.index);
    draw.roomItemId = static_cast<int>(item.id);
    // setItemObj retains cModel::ot_type0: items follow scenery in the
    // depth-sorted model OT0xD. Submitting alpha leaves in the scenery OT
    // writes their edge depth before the background and exposes its clear
    // fog colour as a pale rim.
    draw.orderType = 0;
    draw.texture = textures.at(draw.texture);
    if(draw.specularTexture>=0)draw.specularTexture=textures.at(draw.specularTexture);
    if (draw.alphaTexture >= 0)
      draw.alphaTexture = textures.at(draw.alphaTexture);
    scene.draws.push_back(draw);
  }
  scene.triangles += model.triangles;
  scene.texturedTriangles += model.texturedTriangles;
  scene.alphaTriangles += model.alphaTriangles;
  scene.vertexColorTriangles += model.vertexColorTriangles;
  ++scene.included;
}
} // namespace

RoomItemArchive::RoomItemArchive(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
  const AssetReader reader{bytes_};
  reader.range(0, 16);
  // The uploaded GC packs carry 3 in ItemModelPack::x0. Its source field is
  // unnamed, so validate this supported header without inventing a version.
  if (bytes_.size() > MaxBytes || reader.u32(0) != 3)
    throw std::runtime_error("Room items: ITM header/size");
  const std::array<std::size_t, 3> tables{reader.u32(4), reader.u32(8),
                                          reader.u32(12)};
  if (tables[0] < 16 || tables[0] % 4)
    throw std::runtime_error("Room items: ITM table offset");
  // cItmSys::DataLoad only dereferences BIN/TPL tables inside the idTbl
  // loop. Nine original room packs have zero IDs and null BIN/TPL offsets.
  // Their valid empty model registry must not disable unrelated ITA items.
  if (!reader.u32(tables[0])) return;
  unsigned count = 0;
  std::array<std::pair<std::size_t, std::size_t>, 3> spans;
  for (unsigned i = 0; i < tables.size(); ++i) {
    if (tables[i] < 16 || tables[i] % 4)
      throw std::runtime_error("Room items: ITM table offset");
    const auto n = reader.u32(tables[i]);
    if (n > 256 || (i && n != count))
      throw std::runtime_error("Room items: ITM table count");
    count = n;
    const auto size = 4 + std::size_t(n) * (i == 0 ? 8 : 4);
    reader.range(tables[i], size);
    spans[i] = {tables[i], tables[i] + size};
  }
  for (unsigned i = 0; i < spans.size(); ++i)
    for (unsigned j = i + 1; j < spans.size(); ++j)
      if (spans[i].first < spans[j].second && spans[j].first < spans[i].second)
        throw std::runtime_error("Room items: ITM tables overlap");
  std::set<unsigned> ids;
  std::set<std::size_t> dataOffsets;
  std::vector<std::size_t> boundaries{bytes_.size()};
  for (const auto &span : spans)
    boundaries.push_back(span.first);
  for (unsigned i = 0; i < count; ++i) {
    RoomItemAsset asset;
    asset.id = reader.u16(tables[0] + 4 + i * 8);
    if (asset.id > 255 || !ids.insert(asset.id).second)
      throw std::runtime_error("Room items: Invalid/duplicate ITM ID");
    for (unsigned kind = 1; kind <= 2; ++kind) {
      const auto relative = reader.u32(tables[kind] + 4 + i * 4);
      const auto tableSize = spans[kind].second - spans[kind].first;
      if (relative < tableSize || relative > bytes_.size() - tables[kind])
        throw std::runtime_error("Room items: ITM file offset");
      const auto start = tables[kind] + relative;
      reader.range(start, kind == 1 ? 64 : 12);
      for (const auto &span : spans)
        if (start >= span.first && start < span.second)
          throw std::runtime_error("Room items: File is in the ITM table");
      if (!dataOffsets.insert(start).second)
        throw std::runtime_error("Room items: ITM files overlap");
      boundaries.push_back(start);
      if (kind == 1)
        asset.binOffset = start;
      else {
        if (reader.u32(start) != 0x0020af30)
          throw std::runtime_error("Room items: ITM TPL is missing");
        asset.tplOffset = start;
      }
    }
    assets_.push_back(asset);
  }
  std::sort(boundaries.begin(), boundaries.end());
  for (auto &asset : assets_) {
    auto extent = [&](std::size_t offset, std::size_t minimum) {
      const auto next =
          std::upper_bound(boundaries.begin(), boundaries.end(), offset);
      if (next == boundaries.end() || *next - offset < minimum)
        throw std::runtime_error("Room items: Truncated ITM file range");
      return *next - offset;
    };
    asset.binExtent = extent(asset.binOffset, 64);
    asset.tplExtent = extent(asset.tplOffset, 12);
  }
}
const RoomItemAsset *RoomItemArchive::find(unsigned id) const {
  const auto asset = std::find_if(assets_.begin(), assets_.end(),
                                  [&](const auto &a) { return a.id == id; });
  return asset == assets_.end() ? nullptr : &*asset;
}
std::vector<EnvironmentMatrix> RoomItemArchive::effectParts(unsigned id,
    const std::array<float,3> &position,const std::array<float,3> &rotation) const {
  if(!find(id))return {};
  auto found=effectCenters_.find(id);
  if(found==effectCenters_.end())
    found=effectCenters_.emplace(id,RestJointCenters(SkinnedModel(bin(id)).mesh)).first;
  SmdPlacement placement;placement.position=position;placement.rotation=rotation;placement.scale={1,1,1};
  const auto root=PlacementMatrix(placement);
  std::vector<EnvironmentMatrix> parts;
  for(const auto &center:found->second) {
    auto matrix=root;const auto p=TransformPoint(root,center);
    matrix[3]=p[0];matrix[7]=p[1];matrix[11]=p[2];parts.push_back(matrix);
  }
  return parts;
}
std::vector<std::uint8_t> RoomItemArchive::bin(unsigned id) const {
  const auto *asset = find(id);
  if (!asset)
    throw std::runtime_error("Room items: Original BIN is missing");
  return {bytes_.begin() + asset->binOffset,
          bytes_.begin() + asset->binOffset + asset->binExtent};
}
std::vector<std::uint8_t> RoomItemArchive::tpl(unsigned id) const {
  const auto *asset = find(id);
  if (!asset)
    throw std::runtime_error("Room items: Original TPL is missing");
  return {bytes_.begin() + asset->tplOffset,
          bytes_.begin() + asset->tplOffset + asset->tplExtent};
}

namespace {
RoomItemLayer buildRoomItems(const RoomArchive &room,
                             const std::vector<RoomItemPlacement> &placements,
                             bool variants) {
  RoomItemLayer result;
  if (placements.empty())
    return result;
  if (placements.size() > (variants ? 64 * 9 : 64))
    throw std::runtime_error("Room items: Placement budget");
  const auto *entry = room.find("ITM");
  if (!entry)
    throw std::runtime_error("Room items: Original ITM is missing");
  const RoomItemArchive archive(room.copy(entry - room.entries().data()));
  struct Model {
    StaticScene mesh;
    std::vector<int> textures;
  };
  std::map<unsigned, Model> models;
  std::set<unsigned> indices;
  std::set<std::pair<unsigned, unsigned>> modelVariants;
  std::map<unsigned, RoomItemPlacement> owners;
  std::size_t textureBytes = 0;
  for (const auto &item : placements) {
    if (item.index >= 64 || item.id > 255 ||
        (!variants && !indices.insert(item.index).second) ||
        !modelVariants.insert({item.index, item.id}).second)
      throw std::runtime_error("Room items: Placement ID/index");
    const auto previous = owners.find(item.index);
    if (previous != owners.end() &&
        (previous->second.position != item.position ||
         previous->second.rotation != item.rotation))
      throw std::runtime_error("Room items: Inconsistent variant owner");
    owners.emplace(item.index, item);
    for (unsigned k = 0; k < 3; ++k)
      if (!std::isfinite(item.position[k]) ||
          std::abs(item.position[k]) > 10000000 ||
          !std::isfinite(item.rotation[k]) || std::abs(item.rotation[k]) > 100)
        throw std::runtime_error("Room items: Invalid placement");
    // SceAtCreateItemAt/sceAtSetItem retain the pickup area when the room's
    // ItemGetBinTplAddr fails: only setItemObj is skipped, with the original
    // item effect carrying the visual cue. Random ITA variants legitimately
    // lack models in some rooms; never disable their whole container owner,
    // fabricate a substitute mesh or change its RNG choices for that.
    if (!archive.find(item.id)) continue;
    if (!models.count(item.id)) {
      Model model;
      model.mesh =
          BuildRoomModelScene(archive.bin(item.id), archive.tpl(item.id));
      for (const auto &texture : model.mesh.textures) {
        auto size = texture.rgba.size();
        for (const auto &mip : texture.mips)
          size += mip.rgba.size();
        if (size > MaxBytes - textureBytes)
          throw std::runtime_error("Room items: Texture budget");
        textureBytes += size;
        model.textures.push_back(static_cast<int>(result.mesh.textures.size()));
        result.mesh.textures.push_back(texture);
      }
      models.emplace(item.id, std::move(model));
    }
    const auto &model = models.at(item.id);
    appendItem(result.mesh, model.mesh, item, model.textures);
    ++result.rendered;
  }
  result.placements = placements;
  result.uniqueModels = models.size();
  result.owners = owners.size();
  return result;
}
} // namespace
RoomItemLayer BuildRoomItems(const RoomArchive &room,
                             const std::vector<RoomItemPlacement> &placements,bool variants) {
  return buildRoomItems(room, placements, variants);
}
RoomItemLayer BuildRoomItems(const RoomArchive &room,
                             const ObjectActions &actions) {
  std::vector<RoomItemPlacement> placements;
  placements.reserve(actions.objects().size());
  for (std::size_t i = 0; i < actions.objects().size(); ++i) {
    const auto &object = actions.objects()[i];
    if (!object.hasItem)
      continue; // Explosive barrels have no bound ITA pickup.
    const auto &item = object.item;
    RoomItemPlacement placement;
    placement.index = static_cast<unsigned>(i);
    for (unsigned k = 0; k < 3; ++k) {
      if (!std::isfinite(item.position[k]) ||
          std::abs(item.position[k]) > 10000000)
        throw std::runtime_error("Room items: Invalid content position");
      placement.position[k] = static_cast<float>(item.position[k]);
    }
    placement.rotation = item.rotation;
    if (item.sourceId == 0x1001 || item.sourceId == 0x1002) {
      // SceAtCheckSystemItemSet resolves RandomItemCk at break time. Upload
      // its genuine room ITM choices now, then draw only states.itemId.
      const auto ids = ObjectRandomDropModelIds(actions.room() == 0x400);
      if (ids.empty() || ids.size() > 9)
        throw std::runtime_error("Room items: Random model budget");
      for (const auto id : ids) {
        placement.id = id;
        placements.push_back(placement);
      }
    } else {
      placement.id = item.id;
      placements.push_back(placement);
    }
  }
  return buildRoomItems(room, placements, true);
}
void AppendRoomItemScene(StaticScene &scene, const RoomItemLayer &items) {
  RoomObjectLayer layer;
  layer.mesh = items.mesh;
  AppendRoomObjectScene(scene, layer);
}
std::string RoomItemLayer::describe() const {
  std::ostringstream description;
  description << "Original ITM owners " << owners << " · Variants "
              << rendered << " · Models " << uniqueModels << " · Triangles "
              << mesh.triangles;
  return description.str();
}
} // namespace re4
