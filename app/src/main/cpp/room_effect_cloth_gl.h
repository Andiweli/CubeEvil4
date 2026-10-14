#pragma once
#include "room_effect_cloth.h"
#include "world_effects.h"
namespace re4 {
void CreateRoomEffectClothGl(const WorldEffectsData &data);
void DrawRoomEffectClothGl(const float *matrix,const std::array<float,3> &center,
                           float scale,const std::vector<RoomEffectClothMesh> &meshes);
void DestroyRoomEffectClothGl();
void ForgetRoomEffectClothGl();
} // namespace re4
