#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace re4 {
// Original flag.h: index 0 is the high bit, not the low bit. Native words
// replace the GC pointer casts; no relocated 32-bit address reaches ARM64.
template <std::size_t N> class SourceFlags {
public:
  bool test(unsigned index) const {
    check(index);
    return (words_.at(index >> 5) & (0x80000000u >> (index & 31))) != 0;
  }
  void set(unsigned index, bool value = true) {
    check(index);
    const auto bit = 0x80000000u >> (index & 31);
    auto &word = words_.at(index >> 5);
    if (value) word |= bit;
    else word &= ~bit;
  }
  const std::array<std::uint32_t, N> &words() const { return words_; }
  void clear() { words_.fill(0); }
  void clearFrameWords() {
    static_assert(N == 4, "Only Room_flg has two per-frame words");
    words_[2] = words_[3] = 0;
  }
private:
  std::array<std::uint32_t, N> words_{};
  static void check(unsigned index) {
    if (index >= N * 32) throw std::out_of_range("Original flag: Index outside table bounds");
  }
};

// RoomData's ROOM_SAVE_DATA: one MSB-first room-save word, four taken-item
// words, four found-item words, then 64 ordinary u16 object words.
// ETC bits are masks (window broken=1, left lock broken=4),
// not flag.h indices. They must not share the four transient Room_flg words.
struct RoomSaveState {
  SourceFlags<1> room;
  SourceFlags<4> items, itemsFound;
  std::array<std::uint16_t, 64> etc{};
  bool object(unsigned slot, std::uint16_t mask) const;
  void setObject(unsigned slot, std::uint16_t mask, bool value = true);
};
struct ScenarioState {
  SourceFlags<8> scenario;
  SourceFlags<2> key;
  SourceFlags<4> transient;
  SourceFlags<4> globalItems, globalItemsFound;
  // Native session inventory for authored loose ITA pickups. The existing
  // container inventory retains its checksummed per-room owner.
  std::map<unsigned, unsigned> carriedItems;
  unsigned pesetas{};
  std::map<std::uint16_t, RoomSaveState> rooms;
  RoomSaveState &room(std::uint16_t id);
  const RoomSaveState &room(std::uint16_t id) const;
};
// R101Init, USA profile. No pretend house event or bell timer sets these
// flags. The real event runtime can later use the same authoritative words.
struct VillageRoomProfile {
  bool fight{}, bell{}, house{}, hiddenLadders{}, protectedWindows{};
};
VillageRoomProfile VillageProfile(const RoomSaveState &state);
} // namespace re4
