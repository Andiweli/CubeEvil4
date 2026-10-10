#pragma once
#include "re4/port/gx_scene.h"

namespace re4::port {
// std140 transport. These are native GX states, never GameCube structure overlays.
struct GxMaterialInput {
    u8 flags{}, materialTexture{}, alphaTexture{}, alphaRef{}, alphaOverride=255;
    u8 scaleGroup{}, texFlags{}, blendType{};
    u16 blendRatio{};
    float u{},v{};
    std::vector<u8> blendTable,animation;
    u16 animationFrame{};
};
GxMaterialState captureOriginalMaterial(const GxMaterialInput &,u32 textureBase,u32 textureCount,u8 environmentScale=2);
// Original OT bodies execute callbacks in native storage. The returned indices
// address immutable scene draws, not sorted copies of geometry or room rules.
void buildOriginalOrderingTables(GxScene &);
// Executes original loadLit/setModel2/LightSetModel for a frozen original cut.
// Model-bound light coordinates and live SAR changes remain explicit boundaries.
void captureOriginalRoomLighting(GxScene &,EngineLoader &,EngineConsumers &,ResourceLease roomBody,ResourceLease lit,u32 cut);
int verifyOriginalGxControl();
} // namespace re4::port
