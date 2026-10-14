#pragma once
namespace re4 {
struct OriginalShotCounterChange {bool shot{},hit{};};
// pl_wep.cpp::PlWepHitCheck2 results-counter tail, including all original
// no-stat weapon modes and secondary-shotgun flag4 / no-stat flag2.
inline OriginalShotCounterChange OriginalShotCounters(unsigned weaponNo,unsigned flags,
                                                       unsigned targetCount,unsigned playerId=0) {
  if(playerId!=0)return {};
  switch(weaponNo) {
  case 0:case 0xd:case 0xe:case 0x10:case 0x12:case 0x13:case 0x14:case 0x16:case 0x17:
  case 0x19:case 0x1a:case 0x1f:case 0x20:return {};
  case 7:case 8:case 0x21:if(flags&4)return {};break;
  default:break;
  }
  if(flags&2)return {};
  return {true,targetCount!=0};
}
} // namespace re4
