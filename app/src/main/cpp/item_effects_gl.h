#pragma once
#include "item_effects.h"
namespace re4 {
void CreateItemEffectsGl(const ItemEffects &effects);
void DestroyItemEffectsGl();
void ForgetItemEffectsGl();
void DrawItemEffectsGl(const float *camera,const std::array<float,3> &worldCenter,
                      float worldScale,const WorldEffectCamera &view,
                      const ItemEffects &effects,bool normalView=false);
} // namespace re4
