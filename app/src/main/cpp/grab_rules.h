#pragma once
#include "collision_query.h"
#include <cstdint>

namespace re4 {
constexpr unsigned OriginalLeonLife = 1200;

// PlGachaMove's three original Key.trg groups, translated to input.h actions.
// Horizontal main-stick/D-pad edges count in two groups. A/B aliases count
// together once; neither right-stick input nor weapon triggers count here.
unsigned GrabPressCount(std::uint64_t pressedActions);

// Original Game_level0..10, unarmoured Leon, no debug cheats and rnd=0. Uses the
// existing owned Rnd stream: two draws always, a third when pre-hit life>300.
// Keep-one and the original 1/4 survival roll preserve a last life point.
unsigned GrabPlayerLifeDown(unsigned life, unsigned damage, bool keepOne,
                            unsigned rank = 5);

// Normal type-0 Ganado / Leon multipliers. The original flags bit2 (value4)
// reduces these base values outside Hard mode; difficulty scaling belongs
// to LifeDownSet2, not these pre-scaling values.
unsigned GrabHoldDamage(std::uint32_t enemyFlags, bool hardMode = false);
unsigned GrabFailedReleaseDamage(std::uint32_t playerFlags,
                                bool hardMode = false);

// Original em10CatchCk bounds, in the enemy's inverse-placement space.
// The caller separately owns HP, damage/caught state, SEQ bit2 and EAT rays.
bool GrabBoxContains(const CollisionVec &localPlayer);

struct CatchAlignment {
  float enemyTurn{}, playerTurn{};
  CollisionVec enemyAdjustment{}, playerAdjustment{};
};
// EmCatchPLSet's normal Leon front hold: angle0, type2, offset(-180,0,470.18).
// Both actors keep half the closure adjustment; EmCatchMotionMove consumes
// 20% per tick and 30% of the remaining yaw. Y correction is not applied.
CatchAlignment AlignCatch(const CollisionVec &enemyFeet, float enemyHeading,
                          const CollisionVec &playerFeet, float playerHeading,
                          float offsetX=-180.f,float offsetZ=470.18f);
} // namespace re4
