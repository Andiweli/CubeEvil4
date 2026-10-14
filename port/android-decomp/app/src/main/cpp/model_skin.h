#pragma once
#include "scene.h"
namespace re4 {
struct BoneWeight {
  std::array<std::uint16_t, 3> joints{};
  std::array<float, 3> rates{};
  unsigned count{};
};
// Bounded standalone BIN and its skinning palette. Bones refer to the owner's
// skeleton, including attachment models with a smaller private head table.
struct SkinnedModel {
  explicit SkinnedModel(const std::vector<std::uint8_t> &bin);
  SmdModel mesh;
  std::vector<BoneWeight> weights;
  std::vector<std::uint16_t> positionWeights, normalWeights;
  std::vector<std::array<std::uint16_t, 4>> blendJoints;
  void validateSkeleton(std::size_t parts) const;
  std::vector<Matrix34>
  palette(const std::vector<Matrix34> &bones,
          const std::vector<std::array<float, 3>> &bind) const;
};
} // namespace re4
