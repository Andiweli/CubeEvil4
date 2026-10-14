#include "ladder_motion.h"
#include <cmath>
#include <stdexcept>
namespace re4 {
LadderPlayback StartLadderMotion(AnimationRig &rig,
                                 const std::array<unsigned, 6> &clips, int level) {
  // The original no-middle down branch never selects its exit FCV. Keep that
  // unverified -2 case outside this adapter instead of silently repairing it.
  if (level < -127 || level > 127 || (level >= -2 && level <= 1))
    throw std::runtime_error("Ladder: unverified step count");
  LadderPlayback state;
  state.up = level > 0;
  state.middleLeft = unsigned(std::abs(level) - 2);
  rig.setWaistYaw(0);
  rig.startActionFlags(clips[state.up ? 0 : 3], 5, 0, 1);
  return state;
}
RootMotionStep AdvanceLadderMotion(AnimationRig &rig,
                                    const std::array<unsigned, 6> &clips,
                                    LadderPlayback &state,
                                    std::vector<LadderSound> &sounds) {
  if (state.complete || state.phase > 2 || state.ticks >= 4096)
    throw std::runtime_error("Ladder: invalid playback tick");
  const auto frame = rig.sequenceFrame();
  auto foot = [&](unsigned at, unsigned block, unsigned sound) {
    if (std::abs(frame - float(at)) < .3f) {
      sounds.push_back({block, sound});
      sounds.push_back({1, 0x2a});
    }
  };
  if (state.phase == 0) {
    if (state.up) foot(12, 6, 0x61);
    else { foot(8, 6, 0x60); foot(26, 6, 0x61); }
  } else if (state.phase == 1) {
    foot(1, 6, 0x60); foot(13, 6, 0x61);
  } else if (state.up) {
    foot(1, 6, 0x60); foot(14, 6, 0x61); foot(24, 6, 0x60);
    foot(34, 5, 3); foot(39, 5, 2);
  } else {
    foot(4, 6, 0x60); foot(10, 6, 0x61); foot(23, 6, 0x60);
  }
  auto root = rig.advanceRootAction();
  ++state.ticks;
  const bool looped = (rig.status().state & 3) != 0;
  if (state.phase == 2 && root.ended) state.complete = true;
  else if (state.phase == 0 && root.ended) {
    state.phase = state.middleLeft ? 1 : 2;
    rig.startActionFlags(clips[(state.up ? 0 : 3) + state.phase],
                         state.phase == 1 ? 3 : 5, 0, state.phase == 1 ? 5 : 1);
  } else if (state.phase == 1 && looped) {
    if (!state.middleLeft) throw std::runtime_error("Ladder: negative center section count");
    if (--state.middleLeft == 0) {
      state.phase = 2;
      rig.startActionFlags(clips[state.up ? 2 : 5], 5, 0, 1);
      if (!state.up) {
        // Original case0xC calls MotionMove of the exit immediately. Compose
        // its second root step about the heading after the first, exactly once.
        const auto extra = rig.advanceRootAction();
        const auto c = std::cos(root.yaw), s = std::sin(root.yaw);
        root.translation[0] += c * extra.translation[0] + s * extra.translation[2];
        root.translation[1] += extra.translation[1];
        root.translation[2] += -s * extra.translation[0] + c * extra.translation[2];
        root.yaw += extra.yaw;
      }
    }
  }
  root.ended = state.complete;
  return root;
}
ObjectLadderPlayback StartObjectLadderMotion(
    AnimationRig &rig, const std::array<unsigned, 4> &clips,
    unsigned rungs, unsigned type) {
  if ((rungs != 6 || type != 0) && (rungs != 8 || type != 1))
    throw std::runtime_error("Object ladder: unverified ET06/ET08 profile");
  ObjectLadderPlayback state;
  state.rungsLeft = rungs;
  state.type = type;
  rig.setWaistYaw(0);
  rig.startActionFlags(clips[0], 5, 0, 1);
  return state;
}
RootMotionStep AdvanceObjectLadderMotion(
    AnimationRig &rig, const std::array<unsigned, 4> &clips,
    ObjectLadderPlayback &state, std::vector<LadderSound> &sounds) {
  if (state.complete || state.phase > 2 || state.ticks >= 1024 || state.type > 1)
    throw std::runtime_error("Object ladder: invalid playback tick");
  // In the source, cases2/4 set the next FCV on the following world tick;
  // they then fall through to cases3/5 and evaluate it once in that tick.
  if (state.startPending) {
    rig.startActionFlags(clips[state.phase == 1 ? 1 : 2 + state.type],
                         5, 0, state.phase == 1 ? 5 : 1);
    state.startPending = false;
  }
  const auto frame = rig.sequenceFrame();
  const auto sound = [&](unsigned at, unsigned block, unsigned number) {
    if (std::abs(frame - float(at)) < .3f) sounds.push_back({block, number});
  };
  if (state.phase == 0) {
    sound(10, 6, 0x43); sound(18, 6, 0x42);
  } else if (state.phase == 1) {
    sound(12, 6, 0x43); sound(22, 6, 0x42);
  } else {
    ++state.exitTicks;
    sound(11, 6, 0x43);
    sound(state.type ? 33 : 25, 5, 0x0d);
    sound(state.type ? 36 : 35, 5, 0x0e);
  }
  auto root = rig.advanceRootAction();
  ++state.ticks;
  if (state.phase == 0 && root.ended) {
    if (state.rungsLeft < 4) throw std::runtime_error("Object ladder: invalid rungs");
    state.rungsLeft -= 4;
    state.phase = state.rungsLeft ? 1 : 2;
    state.startPending = true;
  } else if (state.phase == 1 && (rig.status().state & 3)) {
    if (state.rungsLeft < 2) throw std::runtime_error("Object ladder: negative rung count");
    state.rungsLeft -= 2;
    if (!state.rungsLeft) { state.phase = 2; state.startPending = true; }
  } else if (state.phase == 2 && root.ended) state.complete = true;
  root.ended = state.complete;
  return root;
}
} // namespace re4
