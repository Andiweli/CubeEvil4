#include "save_game.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
constexpr std::size_t Header=0x2000,Global=0x2034,Inventory=0x572c,Rooms=0x6930;
constexpr std::size_t Subscreen=0xe7d0,Merchant=0xe7d4,Crc=0xeaf8;
std::uint32_t be32(const std::uint8_t *p) {
  return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];
}
std::uint16_t be16(const std::uint8_t *p) {return std::uint16_t((unsigned(p[0])<<8)|p[1]);}
void put32(std::uint8_t *p,std::uint32_t x) {p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
void put16(std::uint8_t *p,std::uint16_t x) {p[0]=x>>8;p[1]=x;}
void calendarValid(const OriginalSaveCalendar &c) {
  if(c.second<0 || c.second>60 || c.minute<0 || c.minute>59 || c.hour<0 || c.hour>23 ||
     c.day<1 || c.day>31 || c.month<0 || c.month>11 || c.year<0 || c.year>9999 ||
     c.weekday<0 || c.weekday>6 || c.yearday<0 || c.yearday>365 ||
     c.millisecond<0 || c.millisecond>999 || c.microsecond<0 || c.microsecond>999)
    throw std::runtime_error("Original save: Calendar outside bounds");
}
std::array<std::int32_t,10> calendarWords(const OriginalSaveCalendar &c) {
  return {c.second,c.minute,c.hour,c.day,c.month,c.year,c.weekday,c.yearday,c.millisecond,c.microsecond};
}
std::size_t roomLength(const std::uint8_t *p,std::size_t n) {
  if(n<16)throw std::runtime_error("Original save: Room header is missing");
  const auto size=be32(p),count=be32(p+4);
  if(count>(OriginalSaveRoomCapacity-16)/0xd8 || size!=16+std::size_t(count)*0xd8 || size>n)
    throw std::runtime_error("Original save: Room table outside bounds");
  for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j)
    if(be16(p+16+i*0xd8)==be16(p+16+j*0xd8))
      throw std::runtime_error("Original save: duplicate room");
  return size;
}
}
std::uint32_t OriginalSaveCrc(const std::uint8_t *p,std::size_t n) {
  if(!p && n)throw std::invalid_argument("Original save: CRC source is missing");
  static const auto table=[] {
    std::array<std::uint32_t,256> a{};
    for(unsigned i=0;i<256;++i) {
      auto c=std::uint32_t(i)<<24;
      for(unsigned bit=0;bit<8;++bit)c=(c&0x80000000u)?(c<<1)^0x04c11db7u:c<<1;
      a[i]=c;
    }
    return a;
  }();
  std::uint32_t crc=0;
  for(std::size_t i=0;i<n;++i)crc=(crc<<8)^table[(crc>>24)^p[i]];
  return crc;
}
std::string OriginalSaveSlotName(unsigned slot) {
  if(slot>=OriginalSaveSlotCount)throw std::out_of_range("Original save: File number");
  char name[16];std::snprintf(name,sizeof(name),"bh4_data%02u",slot);return name;
}
void ValidateOriginalSaveSections(const OriginalSaveSections &s) {
  if(s.global.size()!=OriginalSaveGlobalBytes || s.inventory.size()!=OriginalSaveInventoryBytes ||
     s.subscreen.size()!=4 || s.merchant.size()!=OriginalSaveMerchantBytes ||
     s.rooms.size()>OriginalSaveRoomCapacity || roomLength(s.rooms.data(),s.rooms.size())!=s.rooms.size())
    throw std::runtime_error("Original save: wrong section size");
}
std::vector<std::uint8_t> EncodeOriginalSaveCard(const OriginalSaveSections &s,
    const OriginalSaveCardArt &art,const OriginalSaveCalendar &calendar,unsigned slot,
    unsigned mode,bool japanese) {
  ValidateOriginalSaveSections(s);calendarValid(calendar);OriginalSaveSlotName(slot);
  if(mode<1 || mode>3)throw std::runtime_error("Original save: unknown mode");
  std::vector<std::uint8_t> bytes(OriginalSaveCardBytes);
  auto *p=bytes.data();
  std::copy(art.banner.begin(),art.banner.end(),p+0x40);
  std::copy(art.icon.begin(),art.icon.end(),p+0x1840);
  std::copy(art.palette.begin(),art.palette.end(),p+0x1c40);
  if(japanese) {
    std::snprintf(reinterpret_cast<char*>(p),32,"biohazard4 FILE%02u",slot+1);
    char comment[128];
    const auto length=std::snprintf(comment,sizeof(comment),"%04d/%02d/%02d %02d:%02d:%02d \x8dX\x90V",
        calendar.year,calendar.month+1,calendar.day,calendar.hour,calendar.minute,calendar.second);
    if(length<0 || length>=32)throw std::runtime_error("Original save: Comment exceeds original field size");
    std::memcpy(p+32,comment,std::size_t(length)+1);
  } else {
    std::memcpy(p,"resident evil 4",16);
    std::snprintf(reinterpret_cast<char*>(p+32),32,"FILE %02u",slot+1);
  }
  std::copy(s.global.begin(),s.global.end(),p+Global);
  std::copy(s.inventory.begin(),s.inventory.end(),p+Inventory);
  std::copy(s.rooms.begin(),s.rooms.end(),p+Rooms);
  std::copy(s.subscreen.begin(),s.subscreen.end(),p+Subscreen);
  std::copy(s.merchant.begin(),s.merchant.end(),p+Merchant);
  put16(p+Global+0xc,std::uint16_t(be16(p+Global+0xc)+1));
  put32(p+Global+0x33d8,mode); // GLOBAL_WK::SaveKind 0x8358.
  put32(p+Header+4,0x116);put32(p+Header+0x30,mode);
  p[Header+0x3d]=p[0x5408]; // Original game_mode display copy, within the global block.
  const auto words=calendarWords(calendar);
  for(unsigned i=0;i<words.size();++i)put32(p+Header+8+i*4,std::uint32_t(words[i]));
  put32(p+Header,OriginalSaveCrc(p+Header+4,0x1fc));
  put32(p+Crc,OriginalSaveCrc(p,Crc));
  return bytes;
}
OriginalSaveMetadata DecodeOriginalSaveHeader(const std::uint8_t *p,std::size_t n) {
  if(!p || n<0x200)throw std::runtime_error("Original save: incomplete header");
  if(be32(p+4)!=0x116)throw std::runtime_error("Original save: different version");
  if(be32(p)!=OriginalSaveCrc(p+4,0x1fc))throw std::runtime_error("Original save: Header-CRC");
  OriginalSaveMetadata m;
  const auto v=[&](unsigned word){return std::int32_t(be32(p+8+word*4));};
  m.calendar={v(0),v(1),v(2),v(3),v(4),v(5),v(6),v(7),v(8),v(9)};
  calendarValid(m.calendar);
  m.mode=be32(p+0x30);m.difficulty=p[0x3d];m.chapter=p[0x3e];
  m.saveCount=be16(p+0x40);m.roundCount=be16(p+0x42);
  m.playSeconds=be32(p+0x48);m.room=be16(p+0x50);
  if(m.mode<1 || m.mode>3)throw std::runtime_error("Original save: unknown mode");
  return m;
}
OriginalSaveCard DecodeOriginalSaveCard(const std::vector<std::uint8_t> &bytes) {
  if(bytes.size()<OriginalSaveMeaningfulBytes || bytes.size()>OriginalSaveCardBytes)
    throw std::runtime_error("Original save: wrong file size");
  const auto *p=bytes.data();OriginalSaveCard card;
  card.metadata=DecodeOriginalSaveHeader(p+Header,bytes.size()-Header);
  if(be32(p+Crc)!=OriginalSaveCrc(p,Crc))throw std::runtime_error("Original save: File CRC");
  auto copy=[&](std::size_t o,std::size_t n){return std::vector<std::uint8_t>(p+o,p+o+n);};
  card.sections={copy(Global,OriginalSaveGlobalBytes),copy(Inventory,OriginalSaveInventoryBytes),
    copy(Rooms,roomLength(p+Rooms,OriginalSaveRoomCapacity)),copy(Subscreen,4),copy(Merchant,OriginalSaveMerchantBytes)};
  ValidateOriginalSaveSections(card.sections);
  std::copy_n(p+0x40,card.art.banner.size(),card.art.banner.data());
  std::copy_n(p+0x1840,card.art.icon.size(),card.art.icon.data());
  std::copy_n(p+0x1c40,card.art.palette.size(),card.art.palette.data());
  return card;
}
} // namespace re4
