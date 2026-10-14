#pragma once
#include "animation.h"
namespace re4 {
struct LadderSound { unsigned block{}, number{}; };
struct LadderPlayback {
  bool up{}, complete{};
  unsigned phase{}, middleLeft{}, ticks{};
};
LadderPlayback StartLadderMotion(AnimationRig &rig,
                                 const std::array<unsigned, 6> &clips, int level);
// player.cpp::pl_R1_Ladder: PL_ARC27..2c, exact Hokan/flags, loop counts,
// authored root/yaw and per-frame SndCall pairs. No SAT, input or RNG owner.
RootMotionStep AdvanceLadderMotion(AnimationRig &rig,
                                    const std::array<unsigned, 6> &clips,
                                    LadderPlayback &state,
                                    std::vector<LadderSound> &sounds);
// obj13.cpp::plobjLadderClimb, not the separate PlSetLadder tower routine.
// ET06 has six rungs/type0; ET08 has eight rungs/type1 (hatch dismount).
struct ObjectLadderPlayback {
  unsigned phase{}, rungsLeft{}, ticks{}, exitTicks{}, type{};
  bool startPending{}, complete{};
};
ObjectLadderPlayback StartObjectLadderMotion(
    AnimationRig &rig, const std::array<unsigned, 4> &clips,
    unsigned rungs, unsigned type);
RootMotionStep AdvanceObjectLadderMotion(
    AnimationRig &rig, const std::array<unsigned, 4> &clips,
    ObjectLadderPlayback &state, std::vector<LadderSound> &sounds);
} // namespace re4
