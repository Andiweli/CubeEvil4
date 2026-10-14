#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace re4 {
// card.cpp's bh4_data00..19 payload. No SAVE_DATA_HEAD pointer is written.
constexpr unsigned OriginalSaveSlotCount=20;
constexpr std::size_t OriginalSaveCardBytes=0x10000,OriginalSaveMeaningfulBytes=0xeafc;
constexpr std::size_t OriginalSaveGlobalBytes=0x36f8,OriginalSaveInventoryBytes=0x1204;
constexpr std::size_t OriginalSaveRoomCapacity=0x7ea0,OriginalSaveMerchantBytes=0x304;
struct OriginalSaveCalendar {
  std::int32_t second{},minute{},hour{},day=1,month{},year=2000;
  std::int32_t weekday{},yearday{},millisecond{},microsecond{};
};
struct OriginalSaveSections {
  std::vector<std::uint8_t> global,inventory,rooms,subscreen,merchant;
};
// Raw original TPL descriptor data: banner RGB5A3, icon CI8, shared RGB5A3 TLUT.
// The art is supplied by the asset reader, never synthesized from screen RGBA.
struct OriginalSaveCardArt {
  std::array<std::uint8_t,0x1800> banner{};
  std::array<std::uint8_t,0x400> icon{};
  std::array<std::uint8_t,0x200> palette{};
};
struct OriginalSaveMetadata {
  OriginalSaveCalendar calendar;
  std::uint32_t mode{},playSeconds{};
  std::uint16_t saveCount{},roundCount{},room{};
  std::uint8_t difficulty{},chapter{};
};
struct OriginalSaveCard {
  OriginalSaveSections sections;
  OriginalSaveMetadata metadata;
  OriginalSaveCardArt art;
};
std::uint32_t OriginalSaveCrc(const std::uint8_t *bytes,std::size_t length);
std::string OriginalSaveSlotName(unsigned slot);
void ValidateOriginalSaveSections(const OriginalSaveSections &sections);
// Implements makeSaveData: increments the u16 save count in a private copy,
// sets SaveKind/mode and the header display byte, then both original CRCs.
std::vector<std::uint8_t> EncodeOriginalSaveCard(
    const OriginalSaveSections &sections,const OriginalSaveCardArt &art,
    const OriginalSaveCalendar &calendar,unsigned slot,unsigned mode=1,
    bool japanese=false);
// Listing validates the source 0x200-byte header CRC/version; full loading
// additionally validates the original whole-file CRC and section bounds.
OriginalSaveMetadata DecodeOriginalSaveHeader(const std::uint8_t *header,std::size_t size);
OriginalSaveCard DecodeOriginalSaveCard(const std::vector<std::uint8_t> &bytes);
} // namespace re4
