#include "animation.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>

using Bytes = std::vector<std::uint8_t>;
void u16(Bytes &b, std::size_t p, unsigned v) {
  b.at(p) = v >> 8;
  b.at(p + 1) = v;
}
void u32(Bytes &b, std::size_t p, unsigned v) {
  u16(b, p, v >> 16);
  u16(b, p + 2, v);
}
void f32(Bytes &b, std::size_t p, float v) {
  unsigned bits;
  std::memcpy(&bits, &v, 4);
  u32(b, p, bits);
}
Bytes body() {
  // Three joints, two-joint skin, one position and normal; no display list.
  Bytes b(144);
  u32(b, 0, 64);
  u32(b, 20, 112);
  b[24] = 1;
  b[25] = 3;
  u32(b, 28, 136);
  u32(b, 48, 120);
  u32(b, 52, 128);
  u16(b, 56, 1);
  u16(b, 58, 1);
  u32(b, 60, 0x20010801);
  for (unsigned i = 0; i < 3; ++i) {
    b[64 + i * 16] = i;
    b[65 + i * 16] = i ? i - 1 : 255;
    f32(b, 72 + i * 16, i ? 10.f : 0.f);
  }
  b[112] = 1;
  b[113] = 2;
  b[115] = 2;
  b[116] = 25;
  u16(b, 120, 2);
  u16(b, 128, 16384);
  return b;
}
re4::AnimationBytes clip() {
  re4::AnimationBytes c;
  c.motion.resize(40);
  u16(c.motion, 0, 10);
  c.motion[2] = 1;
  u16(c.motion, 3, 0xf000);
  c.motion[5] = 0;
  u32(c.motion, 8, 40);
  u32(c.motion, 12, 16);
  for (unsigned i = 0; i < 3; ++i) {
    u16(c.motion, 16 + i * 8, 1);
    f32(c.motion, 20 + i * 8, 0);
  }
  c.sequence.resize(12);
  u16(c.sequence, 0, 2);
  u16(c.sequence, 8, 640);
  return c;
}
re4::AnimationBytes rootClip(float distance) {
  re4::AnimationBytes c;
  c.motion.resize(106);
  u16(c.motion, 0, 10);
  c.motion[2] = 1;
  u16(c.motion, 3,
      1); // Original null/root position, two Hermite keys per axis.
  c.motion[5] = 0;
  u32(c.motion, 8, 106);
  u32(c.motion, 12, 16);
  for (unsigned axis = 0; axis < 3; ++axis) {
    auto p = 16 + axis * 30;
    float d = axis == 2 ? distance : 0;
    u16(c.motion, p, 2);
    u16(c.motion, p + 2, 0);
    u16(c.motion, p + 4, 10);
    f32(c.motion, p + 6, 0);
    f32(c.motion, p + 10, d);
    f32(c.motion, p + 14, d);
    f32(c.motion, p + 18, d);
    f32(c.motion, p + 22, d);
    f32(c.motion, p + 26, d);
  }
  c.sequence.resize(44);
  u16(c.sequence, 0, 10);
  for (unsigned i = 0; i < 10; ++i)
    u16(c.sequence, 4 + i * 4, i * 64);
  return c;
}
re4::AnimationBytes positionClip(float x) {
  re4::AnimationBytes c;
  c.motion.resize(58);
  u16(c.motion, 0, 10);
  c.motion[2] = 1;
  u16(c.motion, 3, 0xf004); // Parts position, two constant float keys per axis.
  u32(c.motion, 8, 58);
  u32(c.motion, 12, 16);
  for (unsigned axis = 0; axis < 3; ++axis) {
    auto p = 16 + axis * 14;
    u16(c.motion, p, 2);
    u16(c.motion, p + 4, 10);
    f32(c.motion, p + 6, axis == 0 ? x : 0);
    f32(c.motion, p + 10, axis == 0 ? x : 0);
  }
  return c;
}
void reject(const std::function<void()> &f) {
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  assert(caught);
}
int main() {
  auto bytes = body();
  re4::SkinnedModel model(bytes);
  model.validateSkeleton(3);
  assert(model.weights.size() == 1 && model.weights[0].count == 2);
  assert(model.weights[0].rates[0] == .25f &&
         model.weights[0].rates[1] == .75f);
  auto centers = re4::RestJointCenters(model.mesh);
  assert(centers[1][1] == 10 && centers[2][1] == 20);
  re4::Matrix34 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  // Display smoothing must preserve rotation lengths (matrix-element LERP
  // would shrink a90-degree limb at the middle sample) and shortest yaw path.
  const re4::Matrix34 rotatedScaled{0,0,4,100, 0,3,0,200, -2,0,0,300};
  assert(re4::InterpolateMotionMatrix(identity, rotatedScaled, 0) == identity);
  assert(re4::InterpolateMotionMatrix(identity, rotatedScaled, 1) == rotatedScaled);
  const auto midpoint = re4::InterpolateMotionMatrix(identity, rotatedScaled, .5);
  for (unsigned column = 0; column < 3; ++column) {
    double squared = 0;
    for (unsigned row = 0; row < 3; ++row)
      squared += double(midpoint[row*4+column])*midpoint[row*4+column];
    assert(std::abs(std::sqrt(squared) - (1.5 + .5 * column)) < 1e-5);
  }
  assert(midpoint[3] == 50 && midpoint[7] == 100 && midpoint[11] == 150);
  assert(std::abs(std::atan2(midpoint[2], midpoint[10]) -
                  3.14159265358979323846/4) < 1e-5);
  reject([&] { re4::InterpolateMotionMatrix(identity, identity, NAN); });
  reject([&] { re4::InterpolateMotionMatrix(identity, identity, -1); });
  reject([&] { re4::InterpolateMotionMatrix(identity, identity, 2); });
  std::vector<re4::Matrix34> bones(3, identity);
  bones[1][7] = 10;
  bones[2][7] = 20;
  bones[2][3] = 10;
  auto palette = model.palette(bones, centers);
  assert(palette.size() == 1 && palette[0][3] == 7.5f && palette[0][7] == 0);
  assert(re4::TransformPoint(palette[0], {2, 0, 0})[0] == 9.5f);
  auto bad = bytes;
  bad[115] = 4;
  reject([&] { re4::SkinnedModel x(bad); });
  bad = bytes;
  bad[115] = 3;
  bad[116] = 70;
  bad[117] = 80;
  reject([&] { re4::SkinnedModel x(bad); });
  bad = bytes;
  u16(bad, 126, 1);
  reject([&] { re4::SkinnedModel x(bad); });
  bad = bytes;
  bad[113] = 3;
  reject([&] {
    re4::SkinnedModel x(bad);
    x.validateSkeleton(3);
  });
  bad = bytes;
  bad.resize(123);
  reject([&] { re4::SkinnedModel x(bad); });
  auto c = clip();
  std::array<re4::AnimationBytes, 3> clips{c, c, c};
  re4::AnimationRig rig(model, clips);
  assert(rig.bones().size() == 3 && rig.bones()[1][7] == 10 &&
         rig.bones()[2][7] == 20);
  reject([&] { rig.sample(0, NAN); });
  reject([&] { rig.sample(0, 11); });
  reject([&] { rig.sample(3, 0); });
  // A final FCV key is a real event pose, not the next iteration of a loop.
  // Give it a different root position so a frame-zero wrap is observable.
  auto endPose = positionClip(0);
  f32(endPose.motion, 26, 30);
  auto endPoseB = positionClip(0);
  f32(endPoseB.motion, 26, 50);
  re4::AnimationRig endRig(model, std::vector<re4::AnimationBytes>{endPose, endPoseB, endPose});
  endRig.sample(0, 10);
  assert(endRig.sequenceFrame() == 10);
  assert(std::abs(endRig.jointMatrices()[0][3] - 30) < .0001f);
  endRig.sample(0, 0);
  assert(endRig.jointMatrices()[0][3] == 0);
  endRig.sampleBlend3({0, 1, 2}, .5f, 10);
  assert(endRig.sequenceFrame() == 10);
  assert(std::abs(endRig.jointMatrices()[0][3] - 40) < .0001f);
  reject([&] { rig.update(re4::Locomotion::Idle, NAN); });
  for (unsigned i = 0; i < 6; ++i) {
    auto broken = clips;
    if (i == 0)
      broken[0].motion.resize(39);
    if (i == 1)
      u32(broken[0].motion, 12, 0xfffffffc);
    if (i == 2)
      u16(broken[0].motion, 3, 0x3000);
    if (i == 3)
      broken[0].motion[5] = 250;
    if (i == 4) {
      broken[0]=positionClip(0);
      u16(broken[0].motion,18,11); //11 ->10 is out of order.
    }
    if (i == 5)
      u16(broken[0].sequence, 8, 641);
    reject([&] { re4::AnimationRig x(model, broken); });
  }
  // The original Mine Thrower's ARC23 lasts15 frames but its Hermite
  // control keys run to20. Duration bounds playback, not the key table.
  auto shortPlayback=positionClip(0);u16(shortPlayback.motion,0,5);
  f32(shortPlayback.motion,22,20);f32(shortPlayback.motion,26,20);
  re4::AnimationRig beyondDuration(model,std::vector<re4::AnimationBytes>{shortPlayback});
  beyondDuration.sample(0,5);
  assert(std::abs(beyondDuration.jointMatrices()[0][3]-20)<.0001f);
  reject([&]{beyondDuration.sample(0,6);});
  // Attach-camera channels6/7 use a camera-slot number, not a bone number.
  // The original runtime skips unbound camera slots. The FCV payload itself
  // remains bounded even when an unused slot's curve is never dereferenced.
  for (unsigned cameraChannel : {6u, 7u}) {
    auto camera = clips;
    u16(camera[0].motion, 3, 0xf004 | (cameraChannel << 8));
    camera[0].motion[5] = 250;
    re4::AnimationRig cameraRig(model, camera);
    cameraRig.sample(0, 5);
    assert(cameraRig.jointMatrices() == rig.jointMatrices());
    auto truncated = camera;
    truncated[0].motion.resize(39);
    reject([&] { re4::AnimationRig x(model, truncated); });
  }
  auto invalidChannel = clips;
  u16(invalidChannel[0].motion, 3, 0xf804);
  reject([&] { re4::AnimationRig x(model, invalidChannel); });
  auto invalidBone = clips;
  u16(invalidBone[0].motion, 3, 0xf504);
  invalidBone[0].motion[5] = 250;
  reject([&] { re4::AnimationRig x(model, invalidBone); });
  re4::AnimationRig hz30(model, clips), hz60(model, clips);
  for (unsigned i = 0; i < 300; ++i)
    hz30.update(re4::Locomotion::Walk, 1. / 30);
  for (unsigned i = 0; i < 600; ++i)
    hz60.update(re4::Locomotion::Walk, 1. / 60);
  assert(hz30.bones() == hz60.bones());
  std::vector<re4::AnimationBytes> rooted{
      rootClip(0), rootClip(100), rootClip(200), rootClip(-75),
      rootClip(0), rootClip(0),   rootClip(0)};
  re4::AnimationRig rootRig(model, rooted);
  assert(rootRig.cycle(1).translation[2] == 100 &&
         rootRig.cycle(1).sequenceFrames == 10);
  auto initial = rootRig.tick(re4::Locomotion::Walk);
  assert(initial.translation[2] == 0);
  double travel = 0;
  for (unsigned i = 0; i < 100; ++i) {
    auto move = rootRig.tick(re4::Locomotion::Walk);
    assert(std::abs(move.translation[2] - 10) <
           .001); // Includes ten loop seams.
    travel += move.translation[2];
    // Root motion is returned to collision, never applied to the bone palette.
    assert(rootRig.bones()[2][7] == 20 && rootRig.bones()[2][11] == 0);
  }
  assert(std::abs(travel - 1000) < .001);
  rootRig.reset();
  rootRig.tick(re4::Locomotion::Walk, .25);
  travel = 0;
  for (unsigned i = 0; i < 40; ++i)
    travel += rootRig.tick(re4::Locomotion::Walk, .25).translation[2];
  assert(std::abs(travel - 100) < .001);
  rootRig.reset();
  for (unsigned i = 0; i < 6; ++i)
    rootRig.tick(re4::Locomotion::Walk);
  rootRig.tick(re4::Locomotion::Run);
  assert(rootRig.sequenceFrame() ==
         7); // Carries 60% phase rather than frame zero.
  rootRig.reset();
  unsigned ended = 0;
  for (unsigned i = 0; i < 15; ++i)
    if (rootRig.tick(re4::Locomotion::QuickTurn).ended && !ended)
      ended = i + 1;
  assert(ended ==
         10); // One-shot sequence clamps instead of looping/restarting.
  reject([&] { rootRig.tick(re4::Locomotion::Walk, NAN); });
  reject([&] { rootRig.tick(re4::Locomotion::Walk, 2); });
  reject([&] { rootRig.tick(static_cast<re4::Locomotion>(7)); });
  // Weapon rigs have one clip; action FCVs legitimately omit SEQ.
  auto action = rootClip(100);
  action.sequence.clear();
  re4::AnimationRig single(model, std::vector<re4::AnimationBytes>{action});
  assert(single.cycle(0).sequenceFrames == 10);
  assert(&single.jointMatrices() == &single.bones());
  single.startActionFlags(0, 0, 0, 1); // Source reload root flag suppressed.
  assert(!single.crossedFrame(3));
  for (unsigned i = 0; i < 11; ++i) {
    auto status = single.advanceAction();
    assert(status.curveFrame == i);
    assert(status.sequenceFrame == std::min(i + 1, 10u));
    assert(status.ended == (i == 10));
    assert(single.crossedFrame(3) == (i == 3));
    assert(single.bones()[2][7] == 20 && single.bones()[2][11] == 0);
  }
  assert(single.advanceAction().ended && !single.crossedFrame(3));
  // Rooted enemy actions use the same original motion evaluation, but return
  // the placement delta once instead of modifying the parts matrices.
  single.startActionFlags(0, 0, 0, 1);
  travel = 0;
  for (unsigned i = 0; i < 11; ++i) {
    const auto move = single.advanceRootAction();
    assert(std::abs(move.translation[2] - (i ? 10 : 0)) < .001);
    assert(move.ended == (i == 10));
    travel += move.translation[2];
    assert(single.jointMatrices()[2][11] == 0);
  }
  assert(std::abs(travel - 100) < .001);
  assert(single.advanceRootAction().translation[2] == 0);
  single.startActionFlags(0, 0, 0, 5);
  single.advanceRootAction();
  travel = 0;
  for (unsigned i = 0; i < 100; ++i) {
    const auto move = single.advanceRootAction();
    assert(!move.ended && std::abs(move.translation[2] - 10) < .001);
    travel += move.translation[2];
  }
  assert(std::abs(travel - 1000) < .001);
  reject([&] { single.advanceRootAction(NAN); });
  reject([&] { single.advanceRootAction(-1); });
  // Original SEQ Free bits remain available to the caller's state machine.
  auto freeClip = rootClip(100);
  freeClip.sequence[4 + 3 * 4 + 3] = 4;
  re4::AnimationRig freeRig(model, std::vector<re4::AnimationBytes>{freeClip});
  freeRig.startActionFlags(0, 0, 0, 1);
  for (unsigned i = 0; i < 6; ++i) {
    freeRig.advanceRootAction();
    assert(freeRig.sequenceFree() == (i == 3 ? 4 : 0));
  }
  single.startAction(0, 0);
  for (unsigned i = 0; i < 22; ++i)
    assert(single.advanceAction(.5).ended == (i == 21));
  single.startActionFlags(0, 0, 0,
                          5); // Source holster loops, stays stationary.
  for (unsigned i = 0; i < 10; ++i)
    assert(!single.advanceAction().ended);
  assert(single.status().state == 1 && single.sequenceFrame() == 0);
  single.startAction(0, 0, 65535);
  assert(single.advanceAction(0).curveFrame == 10);
  re4::AnimationRig sequenceAction(model, clips);
  sequenceAction.startAction(0, 0);
  assert(sequenceAction.advanceAction().curveFrame == 0);
  assert(sequenceAction.advanceAction().ended &&
         sequenceAction.curveFrame() == 10 && sequenceAction.crossedFrame(10));
  // A full collection supports body actions after the seven locomotion clips.
  re4::AnimationRig many(model, std::vector<re4::AnimationBytes>(1024, c));
  many.startAction(1023, 0);
  assert(many.advanceAction().curveFrame == 0);
  reject([&] { many.tick(static_cast<re4::Locomotion>(7)); });
  reject(
      [&] { re4::AnimationRig x(model, std::vector<re4::AnimationBytes>{}); });
  reject([&] {
    re4::AnimationRig x(model, std::vector<re4::AnimationBytes>(1025, c));
  });
  for (const Bytes &invalidSeq : {Bytes{0}, Bytes(4), Bytes{0, 2, 0, 0}}) {
    auto broken = action;
    broken.sequence = invalidSeq;
    reject([&] {
      re4::AnimationRig x(model, std::vector<re4::AnimationBytes>{broken});
    });
  }
  reject([&] { single.startAction(1, 0); });
  reject([&] { single.startAction(0, 256); });
  reject([&] { single.startAction(0, 0, 65536); });
  reject([&] { single.advanceAction(NAN); });
  reject([&] { single.advanceAction(2); });
  reject([&] { single.crossedFrame(NAN); });
  reject(
      [&] { single.setWaistYaw(.2f); }); // Player spine absent in weapon rig.
  std::vector<re4::AnimationBytes> poses{positionClip(0), positionClip(10),
                                         positionClip(-20)};
  re4::AnimationRig blendRig(model, poses);
  for (auto pair : {std::array<float, 2>{-1, -20},
                    {-0.5f, -10},
                    {0, 0},
                    {.5f, 5},
                    {1, 10}}) {
    blendRig.sampleBlend3({0, 1, 2}, pair[0], 4.5f);
    assert(std::abs(blendRig.jointMatrices()[0][3] - pair[1]) < .0001f);
  }
  blendRig.startBlend3({0, 1, 2}, 0, 0);
  for (auto pair : {std::array<float, 2>{1, 10}, {0, 0}, {-1, -20}, {2, 10}}) {
    blendRig.setBlend3Rate(pair[0]);
    blendRig.advanceAction();
    assert(std::abs(blendRig.jointMatrices()[0][3] - pair[1]) < .0001f);
  }
  blendRig.sample(1, 0);
  blendRig.startAction(2, 3);
  // Original Hokan saves the newly blended local matrix back to prevMat.
  for (float expected : {0.f, -40.f / 3, -20.f}) {
    blendRig.advanceAction();
    assert(std::abs(blendRig.jointMatrices()[0][3] - expected) < .0001f);
  }
  reject([&] { blendRig.setBlend3Rate(0); }); // A normal action detaches cMot3.
  reject([&] { blendRig.startBlend3({0, 1, 3}, 0, 0); });
  reject([&] { blendRig.startBlend3({0, 1, 2}, NAN, 0); });
  reject([&] { blendRig.sampleBlend3({0, 1, 2}, 0, 11); });
  blendRig.reset();
  assert(blendRig.jointMatrices()[0][3] == 0);
  // Flip maps are owned, bounded, and consumed by the original 0x40 path.
  reject([&] { blendRig.setFlipTable({0, 1}); });
  reject([&] { blendRig.setFlipTable({0, 1, 3}); });
  blendRig.setFlipTable({0, 1, 2});
  blendRig.startActionFlags(1, 0, 0, 0x40);
  blendRig.advanceAction();
  assert(blendRig.jointMatrices()[0][3] == -10);
  blendRig.reset();
  blendRig.startActionFlags(1, 0, 0, 0x40);
  blendRig.advanceAction();
  assert(blendRig.jointMatrices()[0][3] == -10);
  blendRig.setFlipTable({0xffff, 0xffff, 0xffff});
  blendRig.setFlipTable({});
  std::cout << "Native64 animation/skin PASS: synthetic weights/inverse bind, "
               "original joint chain, "
               "30/60 Hz, root cycles/seams/analog/phase/one-shot, "
               "one-clip/optional SEQ/actions/events/cMot3/Hokan, "
               "rooted actions/SEQ Free/owned mirroring, "
               "invalid FCV/SEQ/weights rejected\n";
}
