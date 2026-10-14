#pragma once
#include "room_flow.h"
#include "room_state.h"
namespace re4 {
bool IsVillageRoom(unsigned room);
bool IsAuthoredVillageDoor(const RoomDoor &door);
unsigned VillageEnemyList(unsigned room,bool night,unsigned previous=0);
std::string VillageDoorReason(const RoomDoor &door,const ScenarioState &state,
    unsigned part=0,unsigned entryTicks=120);
bool UnlockVillageDoor(const RoomDoor &door,ScenarioState &state);
} // namespace re4
