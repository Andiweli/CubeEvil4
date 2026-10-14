#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace re4 {
struct TextureMip {
  unsigned width{}, height{};
  std::vector<std::uint8_t> rgba;
};
struct Texture {
  unsigned width{}, height{}, format{};
  unsigned wrapS{}, wrapT{};
  unsigned minLOD{}, maxLOD{};
  std::vector<TextureMip> mips; // Stored levels 1..maxLOD; base remains rgba.
  std::vector<std::uint8_t> rgba;
};
Texture DecodeTpl(const std::vector<std::uint8_t> &bytes, unsigned index = 0,
                  unsigned mipLevel = 0);
Texture DecodeTplMipmapped(const std::vector<std::uint8_t> &bytes,
                           unsigned index = 0);
Texture LoadTpl(const std::string &filename, unsigned index = 0);
} // namespace re4
