#pragma once
#include "subscreen_data.h"
#include <map>
namespace re4 {
// Immutable authored title assets. Decode on a worker, never in Canvas.onDraw.
class TitleData {
 public:
  TitleData(const std::string &discFiles, unsigned language = 1);
  TitleData(const std::vector<std::uint8_t> &textures,const std::vector<std::uint8_t> &layout);
  TitleData(std::vector<SubscreenTexture> textures,const std::vector<std::uint8_t> &layout);
  static TitleData VillageTitle(const std::string &discFiles);
  const std::vector<SubscreenTexture> &textures() const { return textures_; }
  const Texture *texture(unsigned id, unsigned frame = 0) const;
  std::string layoutJson(unsigned group) const;
 private:
  std::vector<SubscreenTexture> textures_;
  std::map<unsigned, std::vector<std::uint8_t>> layouts_;
  std::map<unsigned, std::string> messages_;
  std::map<unsigned,unsigned> frames_;
  std::map<unsigned,unsigned> animationFrames_;
};
}
