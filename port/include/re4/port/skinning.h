#pragma once
#include "re4/port/model_types.h"
#include <vector>

namespace re4::port {
// Packed output of CalcSk1_x/CalcSk1_x2, before GX fixed-point decoding.
// Owner is independent of the frame allocator, model pool and GPU submission.
struct SkinningResult {
    ResourceLease source;
    std::vector<s16> positions, normals16;
    std::vector<s8> normals8;
    std::vector<std::array<float,12>> palette; // PSMTXReorder column order.
    u32 boneCount{}, influenceCount{};
    bool extended{}, staticBypass{};
    Vec position(u32 index) const;
    Vec normal(u32 index) const;
};
// Uses the original calcWeightMat / MakeWeightPalette{Ext} bodies and a scalar
// paired-single/GQR adapter. Active shape morphs are an explicit dependency.
SkinningResult skinModelCpu(ModelCpu &, ModelInfoView &);
// Bounded PSQ adapter, exposed for independent quantisation/conformance tests.
std::vector<s16> skinS16(const std::vector<std::array<float,12>> &,
                        const s16 *source, u32 count, u32 gqr);
std::vector<s8> skinS8(const std::vector<std::array<float,12>> &,
                      const u8 *source, u32 count, u32 gqr);
} // namespace re4::port
