#pragma once
#include "gx_stream.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace re4 {
struct SmdPlacement {
  std::array<float, 3> position{}, rotation{}, scale{};
  std::uint8_t bin{}, tpl{}, motion{}, work{};
  std::uint32_t flags{};
};
struct SmdJoint {
  std::uint8_t attachment{}, parent{};
  std::array<float, 3> center{};
};
struct SmdModel {
  std::size_t offset{}, streamBytes{};
  std::uint32_t version{};
  std::uint32_t sourceFlags{};
  std::uint16_t vertices{}, displayLists{};
  std::uint8_t parts{}, shift{}, weightPalette{};
  std::uint16_t weightExt{};
  std::vector<SmdJoint> joints;
  std::vector<std::array<float, 3>> positions;
  std::vector<std::array<float, 3>>
      normals; // GX S8/64 or BE S16/16384; no normalization.
  std::vector<std::array<float, 2>> texcoords;
  std::vector<std::array<std::uint8_t, 4>>
      colors;           // GX_RGBA8, separate indices.
  bool vertexAmbient{}; // BIN 0x40000000: vertex ambient RGB / material alpha.
  bool colored{};
  std::array<float, 3> minimum{}, maximum{};
  std::vector<std::array<std::uint8_t, 32>> materialHeaders;
  std::vector<GxPrimitive> primitives;
};
struct SmdResourceCounts {
  unsigned bins{}, palettes{}, motions{};
};
// Reads immutable file offsets, never relocates GameCube pointers into ARM64.
// Only the nModel base placements are records; group counts are separate.
class Smd {
public:
  explicit Smd(const std::vector<std::uint8_t> &bytes,
               SmdResourceCounts required = {});
  SmdResourceCounts commonRequirements() const;
  std::uint8_t version{}, flags{};
  std::uint32_t logicalWorkCount{};
  std::size_t workOffset{};
  std::vector<std::uint32_t> groups;
  std::vector<SmdPlacement> placements;
  std::vector<std::size_t> binOffsets, tplOffsets, motionOffsets;
  std::vector<SmdModel> models;
  std::vector<std::vector<std::uint8_t>> palettes;
  std::string describe() const;
};
} // namespace re4
