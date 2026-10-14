#pragma once
#include "room_shadows.h"
namespace re4 {
class EnemyEncounter;
void CreateRoomShadowsGl(const std::shared_ptr<const RoomShadowData> &receivers);
// Source fit shadow path used by ESL25 in r100_HouseEvent.
void DrawRoomEnemyShadowGl(const float *camera,const std::array<float,3> &center,
                          float scale,const EnemyEncounter &enemy);
void ForgetRoomShadowsGl();
void DestroyRoomShadowsGl();
} // namespace re4
