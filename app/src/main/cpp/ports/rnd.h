#pragma once
#include "types.h"
void RndInit(u16 seed);
// Host adapter can preserve one owned byte-RNG stream across scene rollback.
u16 RndState();
u8 Rnd();
f32 fRand0_1();
f32 fRand1_1();
f32 fRandSeed0_1(u32 *seed);
f32 fRandSeed1_1(u32 *seed);
