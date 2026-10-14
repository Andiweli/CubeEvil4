#pragma once
#include "room_lighting.h"
namespace re4 {
// Filter00 history belongs to the scene clock and survives a lost EGL context.
struct RoomFilterState {
  std::uint64_t frame{};
  bool initialized{},baseValid{},outputValid{};
  std::vector<unsigned char> base,output;
  std::shared_ptr<const RoomLighting> room;
};
bool ApplyRoomFilterGl(int width,int height,const RoomLightCut &cut,std::uint64_t frame,
                       std::shared_ptr<const RoomLighting> room={});
std::shared_ptr<const RoomFilterState> SaveRoomFilterGl();
void RestoreRoomFilterGl(const std::shared_ptr<const RoomFilterState> &state);
void ForgetRoomFilterGl();
void DestroyRoomFilterGl();
} // namespace re4
