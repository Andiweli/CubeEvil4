#pragma once
#include "foot_shadow.h"
#include <memory>
namespace re4 {
void CreateFootShadowGl(const std::shared_ptr<const Texture> &texture);
void DestroyFootShadowGl();
void ForgetFootShadowGl();
void DrawFootShadowGl(const float *camera,const std::array<float,3> &center,
                      float scale,const std::vector<FootShadowQuad> &quads);
} // namespace re4
