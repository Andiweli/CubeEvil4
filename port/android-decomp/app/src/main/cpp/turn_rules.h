#pragma once
#include "animation.h"
#include <cstdint>

namespace re4 {
// Key.on bits 0..3 from game/pad.cpp. Names describe physical controller
// directions; the original JOY_SLEFT bit is physical stick-right.
enum GameCubeMoveKey : std::uint8_t {
  MoveForward = 1,
  MoveBack = 2,
  MoveRight = 4,
  MoveLeft = 8
};
// Original sector test on already PADClamp-compatible stick coordinates.
std::uint8_t GameCubeStickDirections(float x, float y);
// The platform mapper applies a .15 radial dead zone. Undo only that radius
// remapping, then adapt the physical stick's full radius to integer GC axes72.
std::uint8_t GameCubeMappedDirections(double x, double y);
Locomotion GameCubeLocomotion(std::uint8_t keys, bool runHeld);
// Source LIMIT_ANGLE retains float arithmetic and the half-open [-PI,PI).
float GameCubeLimitAngle(float heading);
// Source pl_R1_Walk/Back/Run add their fixed steering AFTER motionMove.
float GameCubeSteer(float heading, Locomotion state, std::uint8_t keys);
// wep02_r3_ready10 transfers the remaining shoulder look over frames 0..4.
float GameCubeDrawTurn(float sequenceFrame, float &remainingYaw);
} // namespace re4
