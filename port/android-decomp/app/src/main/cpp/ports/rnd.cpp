// Port of upstream src/game/rnd.cpp at
// 9b76ff003081d6ece8b838453fb8310743a972c7. Fixed-width integers and memcpy bit
// conversion replace long and strict-aliasing violations. The original
// recurrence and floating-point operation order are preserved.
#include "rnd.h"
static u16 Random;
void RndInit(u16 seed) { Random = seed; }
u16 RndState() { return Random; }
u8 Rnd() {
  const u16 r = Random;
  const u32 n = (static_cast<u32>(static_cast<u8>((r >> 1) + (r >> 8))) << 8) |
                static_cast<u8>(r >> 1);
  u32 m = n & 0xffffu;
  if (m == r)
    m = n + 0x101u;
  Random = static_cast<u16>(m);
  return static_cast<u8>(m >> 8);
}
static f32 nextMantissa() {
  const u32 a = Rnd(), b = Rnd(), c = Rnd();
  return FloatFromBits(a + (b << 8) + ((c & 0x7fu) << 16) + 0x3f800000u);
}
f32 fRand0_1() { return nextMantissa() - 1.0f; }
f32 fRand1_1() { return nextMantissa() * 2.0f - 3.0f; }
f32 fRandSeed0_1(u32 *seed) {
  *seed = *seed * 0x19660du + 0x3c6ef35fu;
  return FloatFromBits((*seed & 0x007fffffu) | 0x3f800000u) - 1.0f;
}
f32 fRandSeed1_1(u32 *seed) {
  *seed = *seed * 0x19660du + 0x3c6ef35fu;
  f32 f = FloatFromBits((*seed & 0x007fffffu) | 0x3f800000u);
  return f * 2.0f - 2.0f - 1.0f;
}
