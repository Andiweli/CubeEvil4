#pragma once
#include "room_lighting.h"
#include "texture.h"
#include <array>
namespace re4 {
struct FootShadowQuad {
  std::array<float,3> position{};
  float size{}, alpha{};
};
// Original CORE Esp animation 0x13 / first texture; no generated decal.
Texture BuildFootShadowTexture(const std::vector<std::uint8_t> &coreEff);
// foot_shadow.cpp::DrawFootShadow and foot_shadow_tbl.cpp::pl_fsd_dat.
// Parts have already been transformed through the current actor placement.
// Only real LIT Id4/workKind5 lights may create shadows.
std::vector<FootShadowQuad> BuildPlayerFootShadows(
    const RoomLightCut &cut, const std::vector<std::array<float,12>> &parts,
    const std::array<double,3> &feet, float floorY, unsigned shadowColor,
    unsigned enableMask=1);
} // namespace re4
