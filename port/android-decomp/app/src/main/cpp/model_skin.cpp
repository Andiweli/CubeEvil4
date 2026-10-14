#include "model_skin.h"
#include "asset_reader.h"
#include <algorithm>
#include <stdexcept>
namespace re4 {
SkinnedModel::SkinnedModel(const std::vector<std::uint8_t> &bin) {
  AssetReader r{bin};
  r.range(0, 64);
  if (bin.size() > 16u * 1024 * 1024)
    throw std::runtime_error("BIN: Model budget");
  // Reuse the checked BIN/GX reader with an empty SMD resource directory.
  // The 32-byte prefix preserves the original display-list alignment.
  std::vector<std::uint8_t> resource(32);
  resource[0] = 0x40;
  resource[7] = 16;
  resource[19] = 16;
  resource.insert(resource.end(), bin.begin(), bin.end());
  Smd parsed(resource, {1, 0, 0});
  mesh = std::move(parsed.models.at(0));
  mesh.offset = 0;
  unsigned count = mesh.weightExt > 255 ? mesh.weightExt : mesh.weightPalette;
  unsigned stride = mesh.weightExt > 255 ? 12 : 8;
  auto offset = r.u32(20);
  if(!count && mesh.parts==1 && mesh.joints.size()==1 &&
      mesh.joints[0].attachment==0 && mesh.joints[0].parent==0xff) {
    // commonModelTrans's unweighted rigid BIN branch. Vertex positions
    // are local to parts0; the skinned shader expects world bind positions.
    BoneWeight rigid;rigid.count=1;rigid.joints[0]=0;rigid.rates[0]=1;
    weights.push_back(rigid);
    positionWeights.assign(mesh.positions.size(),0);normalWeights.assign(mesh.normals.size(),0);
    for(auto &position:mesh.positions)for(unsigned k=0;k<3;++k)position[k]+=mesh.joints[0].center[k];
    return;
  }
  if (count == 0 || offset < 64)
    throw std::runtime_error("BIN: Weight table is missing");
  r.range(offset, std::size_t(count) * stride);
  weights.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    auto p = offset + i * stride;
    BoneWeight weight;
    weight.count = stride == 12 ? r.u16(p + 6) : bin[p + 3];
    if (weight.count < 1 || weight.count > 3)
      throw std::runtime_error("BIN: invalid joint weights");
    float total = 0;
    for (unsigned j = 0; j < weight.count; ++j) {
      weight.joints[j] = stride == 12 ? r.u16(p + j * 2) : bin[p + j];
      if (weight.joints[j] >= 248)
        throw std::runtime_error("BIN: Joint palette exceeded");
      weight.rates[j] = j + 1 == weight.count
                            ? 1.f - total
                            : bin[p + (stride == 12 ? 8 : 4) + j] * .01f;
      if (weight.rates[j] < 0 || weight.rates[j] > 1)
        throw std::runtime_error("BIN: Weight outside 0..1 bounds");
      total += weight.rates[j];
    }
    weights.push_back(weight);
  }
  auto positions = r.u32(48), normals = r.u32(52);
  unsigned normalStride = (r.u32(32) & 0x20000000u) ? 4 : 8;
  for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
    auto index = r.u16(positions + i * 8 + 6);
    if (std::size_t(index) >= weights.size())
      throw std::runtime_error("BIN: Vertex weight is missing");
    positionWeights.push_back(index);
  }
  for (std::size_t i = 0; i < mesh.normals.size(); ++i) {
    auto index = normalStride == 4 ? bin[normals + i * 4 + 3]
                                   : r.u16(normals + i * 8 + 6);
    if (std::size_t(index) >= weights.size())
      throw std::runtime_error("BIN: Normal weight is missing");
    normalWeights.push_back(index);
  }
  if (mesh.version == 0x20030818 && r.u32(64)) {
    auto table = r.u32(64);
    auto n = r.u32(table);
    if (n > 248)
      throw std::runtime_error("BIN: Blended joint budget");
    r.range(table + 4, n * 8);
    for (unsigned i = 0; i < n; ++i) {
      std::array<std::uint16_t, 4> joint{};
      for (unsigned j = 0; j < 4; ++j)
        joint[j] = r.u16(table + 4 + i * 8 + j * 2);
      if (joint[3] > 100)
        throw std::runtime_error("BIN: Blended joint percentage");
      blendJoints.push_back(joint);
    }
  }
}
void SkinnedModel::validateSkeleton(std::size_t parts) const {
  for (const auto &weight : weights)
    for (unsigned j = 0; j < weight.count; ++j)
      if (weight.joints[j] >= parts)
        throw std::runtime_error("BIN: Weight references a missing joint");
  for (const auto &blend : blendJoints)
    for (unsigned j = 0; j < 3; ++j)
      if (blend[j] >= parts)
        throw std::runtime_error("BIN: Blended joint references outside skeleton bounds");
}
std::vector<Matrix34>
SkinnedModel::palette(const std::vector<Matrix34> &bones,
                      const std::vector<std::array<float, 3>> &bind) const {
  if (bones.size() != bind.size())
    throw std::runtime_error("BIN: incomplete bind pose");
  validateSkeleton(bones.size());
  std::vector<Matrix34> result(weights.size());
  for (std::size_t i = 0; i < weights.size(); ++i) {
    const auto &weight = weights[i];
    for (unsigned j = 0; j < weight.count; ++j) {
      auto bone = weight.joints[j];
      auto matrix = bones[bone];
      for (unsigned row = 0; row < 3; ++row)
        matrix[row * 4 + 3] -= matrix[row * 4] * bind[bone][0] +
                               matrix[row * 4 + 1] * bind[bone][1] +
                               matrix[row * 4 + 2] * bind[bone][2];
      for (unsigned k = 0; k < 12; ++k)
        result[i][k] += weight.rates[j] * matrix[k];
    }
  }
  return result;
}
} // namespace re4
