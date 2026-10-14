#include "room_state.h"
namespace re4 {
bool RoomSaveState::object(unsigned slot, std::uint16_t mask) const {
  if (slot >= etc.size() || !mask)
    throw std::out_of_range("ETC-Flag: Invalid slot/mask");
  return (etc[slot] & mask) != 0;
}
void RoomSaveState::setObject(unsigned slot, std::uint16_t mask, bool value) {
  if (slot >= etc.size() || !mask)
    throw std::out_of_range("ETC-Flag: Invalid slot/mask");
  if (value) etc[slot] |= mask;
  else etc[slot] &= std::uint16_t(~mask);
}
RoomSaveState &ScenarioState::room(std::uint16_t id) {
  if (id > 0x7ff) throw std::out_of_range("Room flag: Room outside GC stages");
  return rooms[id];
}
const RoomSaveState &ScenarioState::room(std::uint16_t id) const {
  if (id > 0x7ff) throw std::out_of_range("Room flag: Room outside GC stages");
  static const RoomSaveState empty;
  const auto at = rooms.find(id);
  return at == rooms.end() ? empty : at->second;
}
VillageRoomProfile VillageProfile(const RoomSaveState &state) {
  VillageRoomProfile profile;
  profile.fight = state.room.test(6);
  profile.bell = state.room.test(7);
  profile.house = state.room.test(8);
  profile.hiddenLadders = !profile.house;
  profile.protectedWindows = !profile.bell && !profile.house;
  return profile;
}
} // namespace re4
