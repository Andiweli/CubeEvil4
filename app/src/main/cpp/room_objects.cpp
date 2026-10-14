// Disk layouts and model/palette selection: frozen upstream game/EtcModel.cpp,
// et00.cpp, eff_sys.cpp, obj13.cpp, st4/r400.cpp and st1/r101.cpp.
#include "room_objects.h"
#include "asset_reader.h"
#include "model_skin.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr std::size_t MaxAssetBytes = 64u * 1024 * 1024;
std::string hexadecimal(unsigned n) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out(2, '0');
  out[0] = digits[(n >> 4) & 15];
  out[1] = digits[n & 15];
  return out;
}
// A bounded counterpart of EspGetEfmTplAddr. EFF object-id and offset tables
// must agree; every offset remains relative to its original disk structure.
std::vector<std::uint8_t> effectPalette(const std::vector<std::uint8_t> &eff,
                                        unsigned id) {
  if (eff.empty())
    return {};
  AssetReader r{eff};
  r.range(0, 48);
  if (eff.size() > MaxAssetBytes || r.u32(0) != 11)
    throw std::runtime_error("Room objects: EFF version/size");
  const auto ids = r.u32(20), offsets = r.u32(44);
  if (ids < 48)
    throw std::runtime_error("Room objects: EFM table offset");
  const auto n = r.u32(ids);
  if (!n) {
    if (offsets && (offsets < 48 || r.u32(offsets)))
      throw std::runtime_error("Room objects: Empty EFM table");
    return {};
  }
  if (offsets < 48)
    throw std::runtime_error("Room objects: EFM table offset");
  if (n > 256 || n != r.u32(offsets))
    throw std::runtime_error("Room objects: EFM table count");
  r.range(ids + 4, std::size_t(n) * 8);
  r.range(offsets + 4, std::size_t(n) * 4);
  std::vector<std::size_t> entries;
  for (unsigned i = 0; i < n; ++i) {
    const auto delta = r.u32(offsets + 4 + i * 4);
    if (delta < 4 + n * 4 || delta > eff.size() - offsets)
      throw std::runtime_error("Room objects: EFM entry offset");
    const auto entry = offsets + delta;
    r.range(entry, 20);
    entries.push_back(entry);
  }
  for (unsigned i = 0; i < n; ++i) {
    if (r.u16(ids + 4 + i * 8) != id)
      continue;
    const auto entry = entries[i];
    const auto delta = r.u32(entry + 8);
    if (delta < 20 || delta > eff.size() - entry)
      throw std::runtime_error("Room objects: EFM TPL offset");
    const auto begin = entry + delta;
    r.range(begin, 12);
    std::size_t end = eff.size();
    for (const auto next : entries)
      if (next > begin)
        end = std::min(end, next);
    for (unsigned field = 4; field <= 16; field += 4) {
      const auto sub = r.u32(entry + field);
      if (!sub)
        continue;
      if (sub < 20 || sub > eff.size() - entry)
        throw std::runtime_error("Room objects: EFM subfile");
      if (entry + sub > begin)
        end = std::min(end, std::size_t(entry + sub));
    }
    if (r.u32(begin) != 0x0020af30 || end < begin + 12)
      throw std::runtime_error("Room objects: EFM TPL range");
    return {eff.begin() + begin, eff.begin() + end};
  }
  return {};
}
struct ModelSource {
  unsigned bin{};
  int effectPalette = -1;
  enum Kind { Window, Door, Ladder, Container, Light, Other } kind = Other;
};
bool source(unsigned id, ModelSource &out) {
  out.bin = id;
  // These ids use a shared EFM palette rather than an etNN00.tpl file.
  const std::map<unsigned, unsigned> palettes{
      {1, 4},     {2, 4},     {3, 0x0a},  {4, 0},     {5, 3},     {10, 0x4b},
      {11, 0x0d}, {16, 0x0f}, {17, 9},    {18, 0x0b}, {19, 0x0c}, {24, 0x1e},
      {30, 9},    {32, 0x27}, {34, 0x2c}, {45, 0x44}, {46, 0x45}, {47, 0x46},
      {60, 0x51}, {61, 0x55}, {64, 0x83}, {66, 0x5d}};
  const std::set<unsigned> windows{0,  7,  29, 37, 41, 44, 53, 54,  68, 72,
                                   74, 80, 81, 82, 83, 84, 85, 86,  87, 88,
                                   90, 91, 92, 93, 94, 95, 96, 100, 101};
  const std::set<unsigned> doors{3,  9,  13, 19, 22, 23, 24, 32, 33, 34,
                                 35, 36, 39, 42, 43, 49, 50, 51, 52, 55,
                                 57, 59, 61, 62, 63, 64, 65, 67, 69, 70,
                                 71, 73, 75, 77, 79, 89, 97, 98, 99, 103};
  const std::set<unsigned> lights{10, 11, 12, 16, 20, 25, 28, 56};
  const std::set<unsigned> containers{1,  2,  4,  5,  17, 18,
                                      21, 30, 45, 46, 47, 60};
  if (id >= 104)
    return false; // PS2 additions are not GameCube objects.
  if (id == 25)
    out.bin = 20; // Et19_init explicitly uses et1400.bin / et1400.tpl.
  if (id == 56)
    out.bin = 12; // Et38_init reuses et0c00.bin / et0c00.tpl.
  if (windows.count(id))
    out.kind = ModelSource::Window;
  else if (doors.count(id))
    out.kind = ModelSource::Door;
  else if (id == 6 || id == 8)
    out.kind = ModelSource::Ladder;
  else if (lights.count(id))
    out.kind = ModelSource::Light;
  else if (containers.count(id))
    out.kind = ModelSource::Container;
  const auto p = palettes.find(id);
  if (p != palettes.end())
    out.effectPalette = p->second;
  return true;
}
StaticScene modelScene(const std::vector<std::uint8_t> &bin,
                       const std::vector<std::uint8_t> &tpl, bool ladder) {
  const SkinnedModel model(bin);
  model.validateSkeleton(model.mesh.joints.size());
  const auto &mesh = model.mesh;
  StaticScene scene;
  // setPartsOffset computes inverse rest translations, so unanimated model
  // skinning is identity. Ladders rotate their root part by -110 degrees in
  // objLadder_R1_Set; apply that authored standing pose around its rest pivot.
  SmdPlacement root;
  root.scale = {1, 1, 1};
  if (ladder)
    root.rotation[0] = -1.9198622f;
  const auto rotation = PlacementMatrix(root);
  const auto pivot = RestJointCenters(mesh).at(0);
  for (const auto &primitive : mesh.primitives) {
    if (primitive.triangles.empty())
      continue;
    if (primitive.triangles.size() > 1000000 - scene.triangles)
      throw std::runtime_error("Room objects: Model triangle budget");
    const auto &header = mesh.materialHeaders.at(primitive.displayList);
    SceneDraw draw;
    draw.first = scene.vertices.size() / 3;
    draw.count = primitive.triangles.size() * 3;
    draw.work = 0xfe; // Not a room SMD work number.
    draw.authored = true;
    draw.sortCenter=pivot;
    for(unsigned axis=0;axis<3;++axis) {
      draw.lightVolume.center[axis]=(mesh.minimum[axis]+mesh.maximum[axis])*.5f;
      draw.lightVolume.size[axis]=(mesh.maximum[axis]-mesh.minimum[axis])*.5f;
      draw.lightSize+=draw.lightVolume.size[axis]*draw.lightVolume.size[axis];
    }
    draw.lightSize=std::sqrt(draw.lightSize);
    if(ladder) {
      auto center=draw.lightVolume.center;
      for(unsigned k=0;k<3;++k)center[k]-=pivot[k];
      center=TransformPoint(rotation,center);
      for(unsigned k=0;k<3;++k)draw.lightVolume.center[k]=center[k]+pivot[k];
      for(unsigned axis=0;axis<3;++axis)for(unsigned k=0;k<3;++k)
        draw.lightVolume.axes[axis*3+k]=rotation[k*4+axis];
    }
    draw.image = header[12];
    SetSceneSpecular(draw,header,mesh.sourceFlags);
    draw.repeat = mesh.colored;
    draw.vertexColors = mesh.colored && mesh.vertexAmbient;
    draw.alphaTest = (header[11] & 4) != 0;
    draw.alphaImage = header[14];
    draw.alphaReference = header[20];
    scene.draws.push_back(draw);
    for (const auto &triangle : primitive.triangles)
      for (const auto corner : triangle) {
        const auto &ref = primitive.vertices.at(corner);
        auto point = mesh.positions.at(ref.position);
        auto normal = mesh.normals.at(ref.normal);
        if (ladder) {
          for (unsigned k = 0; k < 3; ++k)
            point[k] -= pivot[k];
          point = TransformPoint(rotation, point);
          for (unsigned k = 0; k < 3; ++k)
            point[k] += pivot[k];
          normal = TransformNormal(NormalMatrix(rotation), normal);
        }
        if(scene.vertices.empty())scene.minimum=scene.maximum=point;
        for(unsigned k=0;k<3;++k){scene.minimum[k]=std::min(scene.minimum[k],point[k]);scene.maximum[k]=std::max(scene.maximum[k],point[k]);}
        scene.vertices.insert(scene.vertices.end(), point.begin(), point.end());
        scene.normals.insert(scene.normals.end(), normal.begin(), normal.end());
        const auto uv = mesh.texcoords.at(ref.texcoord);
        scene.uv.insert(scene.uv.end(), uv.begin(), uv.end());
        const auto color =
            draw.vertexColors ? mesh.colors.at(ref.color)
                              : std::array<std::uint8_t, 4>{255, 255, 255, 255};
        scene.colors.insert(scene.colors.end(), color.begin(), color.end());
      }
    scene.triangles += primitive.triangles.size();
    if (draw.vertexColors)
      scene.vertexColorTriangles += primitive.triangles.size();
  }
  std::vector<std::uint8_t> empty(16);
  empty[0] = 64;
  Smd palettes(empty);
  palettes.palettes.push_back(tpl);
  AttachSceneTextures(palettes, scene);
  if (scene.vertices.empty() || scene.fallbackTriangles ||
      scene.unresolvedAlphaTriangles)
    throw std::runtime_error("Room objects: Model or original texture is missing");
  return scene;
}
void splitCabinetGroundShadow(StaticScene &scene) {
  // ET04's image1/image2 material shares one display list between its
  // soft ground shadow (local Y=4, left half of the palette) and handles.
  // ETS instances have later object numbers than our resident room draws:
  // drawing this translucent plane in that opaque order writes depth over
  // the floor and blends onto the fog clear color. Keep solid handles in
  // their original pass; submit only the authored plane in the existing
  // depth-sorted translucent pass, with depth testing and no depth writes.
  std::vector<SceneDraw> split;
  for (const auto &draw : scene.draws) {
    if (!draw.alphaTest || draw.image != 1 || draw.alphaImage != 2) {
      split.push_back(draw);
      continue;
    }
    const auto begin = split.size();
    for (std::size_t first = draw.first; first < draw.first + draw.count;
         first += 3) {
      bool ground = true;
      for (std::size_t corner = first; corner < first + 3; ++corner)
        ground &= std::abs(scene.vertices.at(corner * 3 + 1) - 4.f) < .001f &&
                  scene.uv.at(corner * 2) < .5f;
      const unsigned order = ground ? 1 : draw.orderType;
      const unsigned depth = ground ? 1 : draw.zMode;
      if (split.size() > begin && split.back().first + split.back().count == first &&
          split.back().orderType == order && split.back().zMode == depth &&
          split.back().texture == draw.texture &&
          split.back().alphaTexture == draw.alphaTexture) {
        split.back().count += 3;
      } else {
        auto part = draw;
        part.first = first; part.count = 3;
        part.orderType = order; part.zMode = depth;
        split.push_back(part);
      }
    }
  }
  scene.draws = std::move(split);
}
void appendInstance(StaticScene &out, const StaticScene &model,
                    const RoomObjectPlacement &placement,
                    const std::vector<int> &textures, bool ets = true) {
  if (model.triangles > 1000000 - out.triangles)
    throw std::runtime_error("Room objects: Scene triangle budget");
  SmdPlacement pose;
  pose.position = placement.position;
  pose.rotation = placement.rotation;
  pose.scale = {1, 1, 1};
  const auto matrix = PlacementMatrix(pose);
  const auto normals = NormalMatrix(matrix);
  const auto first = out.vertices.size() / 3;
  for (std::size_t i = 0; i < model.vertices.size(); i += 3) {
    const auto p =
        TransformPoint(matrix, {model.vertices[i], model.vertices[i + 1],
                                model.vertices[i + 2]});
    const auto n =
        TransformNormal(normals, {model.normals[i], model.normals[i + 1],
                                  model.normals[i + 2]});
    if (out.vertices.empty())
      out.minimum = out.maximum = p;
    for (unsigned k = 0; k < 3; ++k) {
      out.minimum[k] = std::min(out.minimum[k], p[k]);
      out.maximum[k] = std::max(out.maximum[k], p[k]);
    }
    out.vertices.insert(out.vertices.end(), p.begin(), p.end());
    out.normals.insert(out.normals.end(), n.begin(), n.end());
  }
  out.uv.insert(out.uv.end(), model.uv.begin(), model.uv.end());
  out.colors.insert(out.colors.end(), model.colors.begin(), model.colors.end());
  for (auto draw : model.draws) {
    draw.first += first;
    draw.object = placement.slot;
    draw.roomObjectSlot = ets ? static_cast<int>(placement.slot) : -1;
    draw.sortCenter = TransformPoint(matrix,draw.sortCenter);
    draw.lightVolume.center=TransformPoint(matrix,draw.lightVolume.center);
    for(unsigned axis=0;axis<3;++axis) {
      std::array<float,3> basis{};
      for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)
        basis[k]+=matrix[k*4+j]*draw.lightVolume.axes[axis*3+j];
      std::copy(basis.begin(),basis.end(),draw.lightVolume.axes.begin()+axis*3);
    }
    draw.texture = textures.at(draw.texture);
    if(draw.specularTexture>=0)draw.specularTexture=textures.at(draw.specularTexture);
    if (draw.alphaTexture >= 0)
      draw.alphaTexture = textures.at(draw.alphaTexture);
    out.draws.push_back(draw);
  }
  out.triangles += model.triangles;
  out.texturedTriangles += model.texturedTriangles;
  out.alphaTriangles += model.alphaTriangles;
  out.vertexColorTriangles += model.vertexColorTriangles;
  ++out.included;
}
} // namespace

StaticScene BuildRoomModelScene(const std::vector<std::uint8_t> &bin,
                                const std::vector<std::uint8_t> &tpl,
                                bool ladder) {
  return modelScene(bin, tpl, ladder);
}
void ApplyRoomScriptVisibility(std::uint16_t room, const RoomSaveState &state,
                                StaticScene &scene) {
  if (room != 0x100) return;
  const bool afterCarEvent = state.room.test(10);
  scene.draws.erase(std::remove_if(scene.draws.begin(), scene.draws.end(),
      [afterCarEvent](const SceneDraw &draw) {
        return afterCarEvent ? draw.work == 0x23 || draw.work == 0x44
                             : draw.work == 0x37 || draw.work == 0x38;
      }), scene.draws.end());
}

std::vector<RoomObjectPlacement>
ReadRoomObjectPlacements(const std::vector<std::uint8_t> &ets) {
  AssetReader r{ets};
  r.range(0, 16);
  const auto n = r.u16(0);
  if (n > 64)
    throw std::runtime_error("Room objects: ETS slot limit");
  r.range(16, std::size_t(n) * 40);
  std::set<unsigned> slots;
  std::vector<RoomObjectPlacement> result;
  result.reserve(n);
  for (unsigned i = 0; i < n; ++i) {
    RoomObjectPlacement p;
    p.recordOffset = 16 + i * 40;
    p.id = r.u16(p.recordOffset);
    p.slot = r.u16(p.recordOffset + 2);
    if (p.slot >= 64 || p.id >= 104 || !slots.insert(p.slot).second)
      throw std::runtime_error("Room objects: Invalid ETS ID/slot");
    std::copy_n(ets.begin() + p.recordOffset, 40, p.raw.begin());
    for (unsigned k = 0; k < 3; ++k) {
      p.rotation[k] = r.f32(p.recordOffset + 16 + k * 4);
      p.position[k] = r.f32(p.recordOffset + 28 + k * 4);
      if (std::abs(p.rotation[k]) > 100 || std::abs(p.position[k]) > 10000000)
        throw std::runtime_error("Room objects: ETS transform budget");
    }
    result.push_back(p);
  }
  return result;
}

RoomObjectArchive::RoomObjectArchive(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
  AssetReader r{bytes_};
  r.range(0, 32);
  const auto n = r.u32(0);
  if (bytes_.size() > MaxAssetBytes || n > 4096)
    throw std::runtime_error("Room objects: ETM size/count");
  std::size_t p = 32;
  std::set<std::string> names;
  for (unsigned i = 0; i < n; ++i) {
    r.range(p, 64);
    const auto size = r.u32(p);
    if (size < 64 || size > bytes_.size() - p)
      throw std::runtime_error("Room objects: ETM file size");
    auto begin = bytes_.begin() + p + 32;
    const auto end = std::find(begin, begin + 32, 0);
    if (begin == end || end == begin + 32)
      throw std::runtime_error("Room objects: ETM filename");
    std::string name(begin, end);
    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789_.") != std::string::npos ||
        !names.insert(name).second)
      throw std::runtime_error("Room objects: Invalid/duplicate ETM filename");
    assets_.push_back({name, p, p + 64, size - 64});
    p += size;
  }
}
const RoomObjectAsset *RoomObjectArchive::find(const std::string &name) const {
  const auto entry =
      std::find_if(assets_.begin(), assets_.end(),
                   [&](const auto &a) { return a.name == name; });
  return entry == assets_.end() ? nullptr : &*entry;
}
std::vector<std::uint8_t>
RoomObjectArchive::copy(const std::string &name) const {
  const auto *entry = find(name);
  if (!entry)
    throw std::runtime_error("Room objects: ETM file missing: " + name);
  return {bytes_.begin() + entry->dataOffset,
          bytes_.begin() + entry->dataOffset + entry->extent};
}

RoomObjectLayer BuildRoomObjects(const RoomArchive &room,
                                 const std::vector<std::uint8_t> &coreEff,
                                 std::uint16_t roomId,
                                 const RoomSaveState *state) {
  RoomObjectLayer result;
  const auto village = VillageProfile(state ? *state : RoomSaveState{});
  const auto *list = room.find("ETS");
  if (!list)
    return result;
  result.placements =
      ReadRoomObjectPlacements(room.copy(list - room.entries().data()));
  if (result.placements.empty())
    return result;
  const auto *models = room.find("ETM");
  if (!models)
    throw std::runtime_error("Room objects: ETS present, ETM missing");
  const RoomObjectArchive archive(room.copy(models - room.entries().data()));
  std::vector<std::uint8_t> roomEff;
  if (const auto *e = room.find("EFF"))
    roomEff = room.copy(e - room.entries().data());
  struct Cached {
    StaticScene mesh;
    std::vector<int> textures;
  };
  std::map<std::string, Cached> loaded;
  // efmRegist preserves the first registered owner of a shared EFM id.
  // ET01 and ET02 both provide id4; ET1E reuses ET11's id9 palette.
  std::map<unsigned, std::vector<std::uint8_t>> registeredPalettes;
  std::size_t textureBytes = 0;
  const auto cacheTextures = [&](const StaticScene &mesh) {
    std::vector<int> textures;
    for (const auto &texture : mesh.textures) {
      auto bytes = texture.rgba.size();
      for (const auto &mip : texture.mips)
        bytes += mip.rgba.size();
      if (bytes > MaxAssetBytes - textureBytes)
        throw std::runtime_error("Room objects: Texture budget");
      textureBytes += bytes;
      textures.push_back(static_cast<int>(result.mesh.textures.size()));
      result.mesh.textures.push_back(texture);
    }
    return textures;
  };
  for (const auto &p : result.placements) {
    if (state && (p.id == 6 || p.id == 8) && state->object(p.slot, 1)) {
      result.hiddenSlots.push_back(p.slot); // obj13.cpp::SetLadder: broken owner is not created.
      continue;
    }
    // R101Init::setOff clears the ladder's model-visible bit until the house
    // event sets room flag 8. This test profile starts before that event.
    if (roomId == 0x101 && village.hiddenLadders &&
        (p.slot == 5 || p.slot == 6 || p.slot == 7)) {
      if (p.id != 6 && p.id != 8)
        throw std::runtime_error(
            "r101: Original ladder slot mismatch");
      result.hiddenSlots.push_back(p.slot);
      continue;
    }
    ModelSource selected;
    if (!source(p.id, selected)) {
      ++result.unsupported;
      result.warnings.push_back("ETS-ID 0x" + hexadecimal(p.id) +
                                " not supported yet");
      continue;
    }
    const auto name = "et" + hexadecimal(selected.bin);
    const bool broken = roomId == 0x400 && p.id == 0 && p.slot == 0;
    const auto binName = name + (broken ? "01.bin" : "00.bin");
    const auto found = loaded.find(binName);
    if (found == loaded.end()) {
      const auto *bin = archive.find(binName);
      if (!bin) {
        ++result.unsupported;
        result.warnings.push_back("ETS slot " + std::to_string(p.slot) + ": " +
                                  binName + " missing");
        continue;
      }
      std::vector<std::uint8_t> tpl;
      if (selected.effectPalette >= 0) {
        const auto registered = registeredPalettes.find(selected.effectPalette);
        if (registered != registeredPalettes.end()) {
          tpl = registered->second;
        } else {
          tpl = effectPalette(coreEff, selected.effectPalette);
          if (tpl.empty())
            tpl = effectPalette(roomEff, selected.effectPalette);
          if (tpl.empty()) {
            const auto effectId = p.id == 30 ? 17 : p.id;
            const auto effect = "et" + hexadecimal(effectId) + ".eff";
            if (archive.find(effect))
              tpl = effectPalette(archive.copy(effect), selected.effectPalette);
          }
          if (!tpl.empty())
            registeredPalettes.emplace(selected.effectPalette, tpl);
        }
      } else if (archive.find(name + "00.tpl")) {
        tpl = archive.copy(name + "00.tpl");
      }
      if (tpl.empty()) {
        ++result.unsupported;
        result.warnings.push_back("ETS slot " + std::to_string(p.slot) +
                                  ": Original palette is missing");
        continue;
      }
      Cached cache;
      cache.mesh = BuildRoomModelScene(archive.copy(binName), tpl,
                                       selected.kind == ModelSource::Ladder);
      // emwindow.cpp::WindowData.type2=1 selects the translucent OT, for
      // every glass type rather than only the two village instances.
      // Wood/fences (type2=0) keep their original opaque model ordering.
      static const std::set<unsigned> translucentWindows{
          0x00, 0x1d, 0x25, 0x29, 0x2c, 0x35, 0x36, 0x48, 0x4a,
          0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
          0x5b, 0x5c, 0x64, 0x65};
      if (translucentWindows.count(p.id))
        // cEmWindow changes ot_type, retaining cModel's z_mode0. Its
        // alpha compare discards the holes before the original depth write.
        for (auto &draw : cache.mesh.draws) { draw.orderType = 1; draw.zMode = 0; }
      if (p.id == 4) splitCabinetGroundShadow(cache.mesh);
      cache.textures = cacheTextures(cache.mesh);
      loaded.emplace(binName, std::move(cache));
    }
    const auto &cached = loaded.at(binName);
    auto initial = p;
    // R101Init moves this rack after reading ETS. Preserve the disk record
    // in placements/raw while drawing its actual initial script position.
    if (roomId == 0x101 && p.slot == 0x11) {
      if (p.id != 4)
        throw std::runtime_error("r101: Original shelf slot mismatch");
      initial.position = {6656.f, 902.f, 8925.f};
    }
    const auto drawBegin = result.mesh.draws.size();
    appendInstance(result.mesh, cached.mesh, initial, cached.textures);
    if (p.id == 0 || p.id == 7 || p.id == 0x25) {
      for (auto i = drawBegin; i < result.mesh.draws.size(); ++i)
        result.mesh.draws[i].roomObjectVariant = broken ? 1 : 0;
      // WindowData: ET00/ET07 use their original replacement BIN with the
      // intact palette; ET25 hides its entire model after destruction.
      if (p.id != 0x25 && !broken) {
        const auto brokenName = name + "01.bin";
        if (!loaded.count(brokenName)) {
          Cached brokenCache;
          brokenCache.mesh = BuildRoomModelScene(archive.copy(brokenName),
                                                  archive.copy(name + "00.tpl"));
          if (p.id == 0) for (auto &draw : brokenCache.mesh.draws) { draw.orderType = 1; draw.zMode = 0; }
          brokenCache.textures = cacheTextures(brokenCache.mesh);
          loaded.emplace(brokenName, std::move(brokenCache));
        }
        const auto &brokenCache = loaded.at(brokenName);
        const auto begin = result.mesh.draws.size();
        appendInstance(result.mesh, brokenCache.mesh, initial, brokenCache.textures);
        for (auto i = begin; i < result.mesh.draws.size(); ++i)
          result.mesh.draws[i].roomObjectVariant = 1;
      }
    }
    ++result.rendered;
    switch (selected.kind) {
    case ModelSource::Door:
      ++result.doors;
      break;
    case ModelSource::Window:
      ++result.windows;
      break;
    case ModelSource::Ladder:
      ++result.ladders;
      break;
    case ModelSource::Container:
      ++result.containers;
      break;
    case ModelSource::Light:
      ++result.lights;
      break;
    default:
      break;
    }
  }
  if (roomId == 0x101) {
    // SetObj00 in R101Init uses ROOM_ARC_PTR indices 0x20/0x21. Its ofs[]
    // begins at the whole archive base: the four header words precede the
    // file offsets, so these are directory entries 28/29, BIN1/TPL1.
    RoomObjectLayer::ScriptObject pyre{
        RoomObjectLayer::ScriptObject::VillagePyre,
        1,
        1,
        -1,
        {0.f, -1.5707964f, 0.f},
        {-2880.f, 50.f, 2810.f}};
    result.scriptObjects.push_back(pyre);
    const auto door =
        std::find_if(result.placements.begin(), result.placements.end(),
                     [](const auto &p) { return p.slot == 0x0b; });
    if (door == result.placements.end() || door->id != 0x0d)
      throw std::runtime_error("r101: Original locked door is missing");
    // cEmDoor::setLock(...,side=0,strong=0): rotate the exact local offset
    // by the authored door transform; its lock faces the other side (+PI).
    SmdPlacement doorPose;
    doorPose.position = door->position;
    doorPose.rotation = door->rotation;
    doorPose.scale = {1, 1, 1};
    RoomObjectLayer::ScriptObject lock{
        RoomObjectLayer::ScriptObject::DoorLock,
        0,
        0,
        0x0b,
        door->rotation,
        TransformPoint(PlacementMatrix(doorPose), {-1160.f, 1050.f, -80.f})};
    lock.rotation[1] += 3.1415927f;
    if (lock.rotation[1] > 3.1415927f)
      lock.rotation[1] -= 6.2831855f;
    result.scriptObjects.push_back(lock);
    for (std::size_t i = 0; i < result.scriptObjects.size(); ++i) {
      const auto &script = result.scriptObjects[i];
      const auto *bin = room.find("BIN", script.binOccurrence);
      const auto *tpl = room.find("TPL", script.tplOccurrence);
      if (!bin || !tpl)
        throw std::runtime_error("r101: Original script model is missing");
      const auto model =
          BuildRoomModelScene(room.copy(bin - room.entries().data()),
                              room.copy(tpl - room.entries().data()));
      RoomObjectPlacement placement;
      placement.slot = 64 + static_cast<unsigned>(i);
      placement.position = script.position;
      placement.rotation = script.rotation;
      appendInstance(result.mesh, model, placement, cacheTextures(model),
                     false);
      if (script.kind == RoomObjectLayer::ScriptObject::DoorLock)
        for (auto &draw : result.mesh.draws)
          if (draw.object == placement.slot)
            draw.roomLockParent = script.parentSlot;
    }
  }
  return result;
}
void AppendRoomObjectScene(StaticScene &scene, const RoomObjectLayer &objects) {
  AppendStaticSceneLayer(scene, objects.mesh);
}
void AppendStaticSceneLayer(StaticScene &scene, const StaticScene &mesh) {
  if (mesh.vertices.empty())
    return;
  if (scene.triangles > 2000000 || mesh.triangles > 2000000 - scene.triangles)
    throw std::runtime_error("Room objects: Shared triangle budget");
  std::size_t textureBytes = 0;
  const std::array<const std::vector<Texture> *, 2> textureSets{&scene.textures,
                                                                &mesh.textures};
  for (const auto *textures : textureSets)
    for (const auto &texture : *textures) {
      auto bytes = texture.rgba.size();
      for (const auto &mip : texture.mips)
        bytes += mip.rgba.size();
      constexpr std::size_t maximum = 128u * 1024 * 1024;
      if (bytes > maximum - textureBytes)
        throw std::runtime_error("Room objects: Shared texture budget");
      textureBytes += bytes;
    }
  const auto first = scene.vertices.size() / 3;
  const auto textures = scene.textures.size();
  unsigned objectBase = 0;
  for (const auto &draw : scene.draws)
    objectBase = std::max(objectBase, draw.object + 1);
  if (scene.vertices.empty())
    scene.minimum = scene.maximum = mesh.minimum;
  for (unsigned k = 0; k < 3; ++k) {
    scene.minimum[k] = std::min(scene.minimum[k], mesh.minimum[k]);
    scene.maximum[k] = std::max(scene.maximum[k], mesh.maximum[k]);
  }
  scene.vertices.insert(scene.vertices.end(), mesh.vertices.begin(),
                        mesh.vertices.end());
  scene.normals.insert(scene.normals.end(), mesh.normals.begin(),
                       mesh.normals.end());
  scene.uv.insert(scene.uv.end(), mesh.uv.begin(), mesh.uv.end());
  scene.colors.insert(scene.colors.end(), mesh.colors.begin(),
                      mesh.colors.end());
  scene.textures.insert(scene.textures.end(), mesh.textures.begin(),
                        mesh.textures.end());
  for (auto draw : mesh.draws) {
    draw.first += first;
    draw.object += objectBase;
    if (draw.texture >= 0)
      draw.texture += static_cast<int>(textures);
    if (draw.alphaTexture >= 0)
      draw.alphaTexture += static_cast<int>(textures);
    if(draw.specularTexture>=0)draw.specularTexture+=static_cast<int>(textures);
    scene.draws.push_back(draw);
  }
  scene.triangles += mesh.triangles;
  scene.texturedTriangles += mesh.texturedTriangles;
  scene.alphaTriangles += mesh.alphaTriangles;
  scene.vertexColorTriangles += mesh.vertexColorTriangles;
  scene.included += mesh.included;
  scene.skipped += mesh.skipped;
  scene.fallbackTriangles += mesh.fallbackTriangles;
  scene.commonTextureTriangles += mesh.commonTextureTriangles;
  scene.commonAlphaTriangles += mesh.commonAlphaTriangles;
  scene.commonPlacements += mesh.commonPlacements;
  scene.missingCommonPlacements += mesh.missingCommonPlacements;
  scene.unresolvedAlphaTriangles += mesh.unresolvedAlphaTriangles;
  scene.smxDraws += mesh.smxDraws;
  scene.materialColorTriangles += mesh.materialColorTriangles;
  scene.zeroNormals += mesh.zeroNormals;
  scene.singularNormalPlacements += mesh.singularNormalPlacements;
}
std::string RoomObjectLayer::describe() const {
  std::ostringstream out;
  out << "Original room objects " << rendered << '/' << placements.size()
      << " · Doors " << doors << " · Windows " << windows << " · Ladders "
      << ladders << " · Containers " << containers << " · Lights " << lights
      << " · Script objects " << scriptObjects.size() << " · Hidden "
      << hiddenSlots.size() << " · Triangles " << mesh.triangles << " · Open "
      << unsupported;
  return out.str();
}
} // namespace re4
