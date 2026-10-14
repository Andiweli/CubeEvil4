#include "scene.h"
#include "archive.h"
#include "asset_reader.h"
#include <map>
#include <stdexcept>
#include <tuple>
namespace re4 {
void SetSceneSpecular(SceneDraw &draw,const std::array<std::uint8_t,32> &h,std::uint32_t flags) {
  draw.specularBump=(h[11]&0x80)!=0;
  draw.specularEnabled=(h[11]&0x13)!=0&&!draw.specularBump&&h[19]<=1;
  draw.specularLocal=(h[11]&0x10)!=0;draw.specularType=h[19];
  draw.specularImage=draw.specularLocal?h[23]:h[15]==255?0:h[15];
  draw.specularScale=h[21]?h[21]*.01f:.5f;
  if(flags&0x20000000u)draw.specularScale*=.5f;
  for(unsigned k=0;k<3;++k)draw.specularRGB[k]=h[16+k];
}
std::vector<Texture> LoadCoreSpecular(const std::string &path) {
  // CORE_ARC(4) is offset-table word4, the first ArchiveEntry.
  const auto core=RoomArchive::LoadDrs(path);const auto palette=core.copy(0);AssetReader r{palette};
  const auto count=r.u32(4);if(r.u32(0)!=0x20af30||count>9||!count)throw std::runtime_error("CORE: Specular palette");
  std::vector<Texture> bank;for(unsigned n=0;n<count;++n)bank.push_back(DecodeTpl(palette,n));return bank;
}
void AttachSceneSpecular(StaticScene &scene,const std::vector<Texture> &bank) {
  std::map<unsigned,int> loaded;
  for(auto &d:scene.draws)if(d.specularEnabled&&!d.specularLocal&&d.specularTexture<0&&!bank.empty()) {
    const auto [at,added]=loaded.emplace(d.specularImage,int(scene.textures.size()));
    if(added){scene.textures.push_back(bank.at(d.specularImage));}d.specularTexture=at->second;
  }
}
void AttachSceneTextures(const Smd &s, StaticScene &scene, const Smd *common) {
  scene.textures.clear();
  scene.texturedTriangles = scene.fallbackTriangles = 0;
  scene.alphaTriangles = scene.unresolvedAlphaTriangles = 0;
  scene.commonTextureTriangles = scene.commonAlphaTriangles = 0;
  std::map<std::tuple<bool, unsigned, unsigned, bool>, int> loaded;
  auto count = [](const std::vector<std::uint8_t> &data) {
    if (data.size() < 12)
      throw std::runtime_error("TPL: Truncated palette");
    auto word = [&](std::size_t p) {
      return (std::uint32_t(data[p]) << 24) |
             (std::uint32_t(data[p + 1]) << 16) |
             (std::uint32_t(data[p + 2]) << 8) | data[p + 3];
    };
    if (word(0) != 0x0020af30)
      throw std::runtime_error("TPL: Invalid palette");
    return word(4);
  };
  unsigned commonCount = 0;
  if (common) {
    if (common->palettes.empty())
      throw std::runtime_error("TPL: shared palette is missing");
    commonCount = count(common->palettes[0]);
  }
  std::size_t bytes = 0;
  for (auto &draw : scene.draws) {
    draw.texture = -1;
    const std::vector<std::uint8_t> *local = nullptr;
    unsigned localCount = 0;
    if (!draw.commonModel) {
      if (draw.palette >= s.palettes.size())
        throw std::runtime_error("TPL: local palette is missing");
      local = &s.palettes[draw.palette];
      localCount = count(*local);
    }
    // trans.cpp: local descriptors first, then common palette 0.
    // Common BINs use the zero-descriptor DmyZeroTpl. 0xf8+ are runtime IDs.
    auto resolve = [&](unsigned image, bool &fromCommon) {
      fromCommon = false;
      if (image >= 0xf8)
        return -1;
      const auto *data = local;
      unsigned palette = draw.palette;
      if (image >= localCount) {
        image -= localCount;
        if (!common || image >= commonCount)
          return -1;
        fromCommon = true;
        data = &common->palettes[0];
        palette = 0;
      }
      auto key = std::make_tuple(fromCommon, palette, image, draw.repeat);
      auto it = loaded.find(key);
      if (it == loaded.end()) {
        auto texture = DecodeTplMipmapped(*data, image);
        auto textureBytes = texture.rgba.size();
        for (const auto &mip : texture.mips)
          textureBytes += mip.rgba.size();
        if (textureBytes > 128u * 1024 * 1024 - bytes)
          throw std::runtime_error("TPL: Scene texture budget");
        bytes += textureBytes;
        if (draw.repeat)
          texture.wrapS = texture.wrapT = 1;
        int id = static_cast<int>(scene.textures.size());
        scene.textures.push_back(std::move(texture));
        it = loaded.emplace(key, id).first;
      }
      return it->second;
    };
    bool commonBase = false, commonAlpha = false;
    draw.texture = resolve(draw.image, commonBase);
    draw.alphaTexture =
        draw.alphaTest ? resolve(draw.alphaImage, commonAlpha) : -1;
    bool commonSpecular=false;
    draw.specularTexture=draw.specularEnabled&&draw.specularLocal?resolve(draw.specularImage,commonSpecular):-1;
    if (draw.texture >= 0 && commonBase)
      scene.commonTextureTriangles += draw.count / 3;
    if (draw.alphaTest && draw.texture >= 0 && draw.alphaTexture >= 0 &&
        commonAlpha)
      scene.commonAlphaTriangles += draw.count / 3;
    if (draw.texture >= 0)
      scene.texturedTriangles += draw.count / 3;
    else
      scene.fallbackTriangles += draw.count / 3;
    if (draw.alphaTest) {
      if (draw.texture >= 0 && draw.alphaTexture >= 0)
        scene.alphaTriangles += draw.count / 3;
      else
        scene.unresolvedAlphaTriangles += draw.count / 3;
    }
  }
}
} // namespace re4
