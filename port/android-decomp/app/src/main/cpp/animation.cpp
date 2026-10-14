#include "animation.h"
#include "asset_reader.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace re4::motion64 {
static Global global{};
Global *pG = &global;
cModel *pPL = nullptr;
static Log log;
Log *pLog = &log;
CamCtrlStub CamCtrl;
SatMgrStub SatMgr;
const Vec vecZero{};
static std::string error;
void Log::err(int, int, const char *format, ...) {
  char message[512]{};
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  error = message;
}
void CamCtrlStub::registAttachCamera(ATTACH_CAMERA *, cModel *) {}
void CamCtrlStub::deleteAttachCamera(ATTACH_CAMERA *, cModel *) {}
float SatMgrStub::getFloor(Vec *, u32 *, float, float, int) {
  return -100000.f; // Only animation IK targets; no guessed original floor IK.
}
void OSReport(const char *, ...) {}
void eprintf(int, int, int, int, const char *, ...) {}
void memclr_asm(void *dst, u32 n) { std::memset(dst, 0, n); }
void cModel_matBlend(cModel *m, float rate) { m->matBlend(rate); }
float SQRTF(float x) { return x <= .00001f ? 0.f : std::sqrt(x); }
float LIMIT_ANGLE(float x) {
  while (x >= PI)
    x -= PI2;
  while (x < -PI)
    x += PI2;
  return x;
}
} // namespace re4::motion64
namespace re4 {
Matrix34 InterpolateMotionMatrix(const Matrix34 &previous,
                                 const Matrix34 &current, double fraction) {
  if (!std::isfinite(fraction) || fraction < 0 || fraction > 1)
    throw std::runtime_error("Motion: invalid rendering fraction");
  if (fraction == 0) return previous;
  if (fraction == 1) return current;
  motion64::Mtx a{}, b{}, result{};
  std::array<std::array<double, 3>, 2> scales{};
  for (unsigned i = 0; i < 2; ++i) {
    const auto &input = i ? current : previous;
    auto &matrix = i ? b : a;
    for (unsigned column = 0; column < 3; ++column) {
      double squared = 0;
      for (unsigned row = 0; row < 3; ++row) {
        const auto v = input[row * 4 + column];
        if (!std::isfinite(v))
          throw std::runtime_error("Motion: invalid rendering matrix");
        squared += double(v) * v;
      }
      const auto scale = std::sqrt(squared);
      if (scale < 1e-8)
        throw std::runtime_error("Motion: singular rendering matrix");
      scales[i][column] = scale;
      for (unsigned row = 0; row < 3; ++row)
        matrix[row][column] = float(input[row * 4 + column] / scale);
    }
  }
  motion64::Quaternion qa{}, qb{}, q{};
  motion64::C_QUATMtx(&qa, a);
  motion64::C_QUATMtx(&qb, b);
  motion64::C_QUATSlerp(&qa, &qb, &q, float(fraction));
  motion64::PSMTXQuat(result, &q);
  Matrix34 out{};
  for (unsigned row = 0; row < 3; ++row) {
    for (unsigned column = 0; column < 3; ++column) {
      const auto scale = scales[0][column] +
          fraction * (scales[1][column] - scales[0][column]);
      out[row * 4 + column] = float(result[row][column] * scale);
    }
    if (!std::isfinite(previous[row * 4 + 3]) ||
        !std::isfinite(current[row * 4 + 3]))
      throw std::runtime_error("Motion: invalid rendering position");
    out[row * 4 + 3] = float(previous[row * 4 + 3] +
        fraction * (double(current[row * 4 + 3]) - previous[row * 4 + 3]));
  }
  return out;
}
const char *LocomotionLabel(Locomotion state) {
  switch (state) {
  case Locomotion::Idle:
    return "Idle";
  case Locomotion::Walk:
    return "Walk";
  case Locomotion::Run:
    return "Run";
  case Locomotion::Back:
    return "Backward";
  case Locomotion::TurnLeft:
    return "Turn left";
  case Locomotion::TurnRight:
    return "Turn right";
  case Locomotion::QuickTurn:
    return "Quick turn";
  }
  return "Unknown";
}
namespace {
using namespace motion64;
struct Clip {
  MotionData data{};
  std::vector<u16> kinds;
  std::vector<u8> parts;
  std::vector<std::vector<u8>> curves;
  std::vector<u8 *> pointers;
  std::vector<u32> sequence; // aligned native SEQ header and 4-byte records.
};
void append16(std::vector<u8> &out, u16 v) {
  out.push_back(v);
  out.push_back(v >> 8);
}
void append32(std::vector<u8> &out, u32 v) {
  append16(out, v);
  append16(out, v >> 16);
}
std::unique_ptr<Clip> parseClip(const AnimationBytes &source,
                                std::size_t joints) {
  AssetReader r{source.motion};
  r.range(0, 3);
  if (source.motion.size() > 16u * 1024 * 1024)
    throw std::runtime_error("FCV: Size limit");
  auto clip = std::make_unique<Clip>();
  auto &c = *clip;
  c.data.maxFrame = r.u16(0);
  c.data.nParts = source.motion[2];
  // Original EM2A_MOT_TRAP1_SET is a real ten-frame FCV with zero
  // channels: MotionMove holds modelInit's authored rest parts.
  if (!c.data.maxFrame || c.data.maxFrame > 1023)
    throw std::runtime_error("FCV: unsupported preview frame range");
  auto table = (3u + unsigned(c.data.nParts) * 3 + 3) & ~3u;
  r.range(table, 4 + std::size_t(c.data.nParts) * 4);
  auto size = r.u32(table);
  if (size > source.motion.size() || size < table + 4 + c.data.nParts * 4u)
    throw std::runtime_error("FCV: invalid payload length");
  static constexpr unsigned formats[16][3] = {
      {4, 4, 4}, {4, 2, 2}, {4, 1, 1}, {},        {2, 4, 4}, {2, 2, 2},
      {2, 1, 1}, {},        {1, 4, 4}, {1, 2, 2}, {1, 1, 1}, {},
      {},        {},        {},        {4, 0, 0}};
  for (unsigned i = 0; i < c.data.nParts; ++i) {
    auto kind = r.u16(3 + i * 2);
    auto part = source.motion[3 + c.data.nParts * 2 + i];
    unsigned format = kind >> 12, channel = (kind >> 8) & 15;
    auto low = kind & 255;
    // Original MotionMoveCore treats channels 6/7 as attach-camera curves,
    // not skeleton parts. EM12's player kick includes these channels; with
    // no attached camera they are skipped by the unchanged original runtime.
    // Bound camera slots retain complete key validation; unbound slots follow
    // MotionSetCore's original skip behavior below.
    const bool camera = channel == 6 || channel == 7;
    if (!formats[format][0] || channel > 7 ||
        (!camera && low != 1 && low != 0x40 && part >= joints) ||
        (!camera && (low & 0x30) && (part + 3u >= joints)))
      throw std::runtime_error("FCV: invalid channel or IK reference");
    c.kinds.push_back(kind);
    c.parts.push_back(part);
    auto p = std::size_t(r.u32(table + 4 + i * 4));
    // MotionSetCore never binds camera position part3 (only 0,1,4), or
    // camera rotation parts other than 2,3. The original rifle knife FCV
    // contains an unused position-part3 pointer into CD alignment padding.
    // Preserve its channel identity without reading a curve the game skips.
    if (camera && (!source.attachCameraCurves ||
                  !((low == 4 && (part == 0 || part == 1 || part == 4)) ||
                    (low == 2 && (part == 2 || part == 3))))) {
      c.curves.emplace_back();
      continue;
    }
    if (p < table + 4 + c.data.nParts * 4u || p >= size)
      throw std::runtime_error("FCV: Key offset outside file bounds");
    std::vector<u8> curve;
    for (unsigned axis = 0; axis < 3; ++axis) {
      auto n = r.u16(p);
      p += 2;
      if (n == 0 || n > 1024)
        throw std::runtime_error("FCV: Key budget");
      append16(curve, n);
      u16 previous = 0;
      for (unsigned key = 0; key < n; ++key) {
        auto frame = r.u16(p);
        p += 2;
        const bool terminalDuplicate=key+1==n && frame==previous && frame==c.data.maxFrame;
        // FCV duration limits playback, not its Hermite control points.
        // Original WEP14 ARC23 lasts15 frames but deliberately reuses root
        // curves with keys through20; the key at16 shapes the pose at15.
        if (key && frame <= previous && !terminalDuplicate)
          throw std::runtime_error("FCV: Unsorted keyframes");
        previous = frame;
        append16(curve, frame);
      }
      for (unsigned key = 0; key < n; ++key)
        for (auto width : formats[format]) {
          if (p > size || width > size - p)
            throw std::runtime_error("FCV: Keys exceed payload bounds");
          if (width == 4) {
            (void)r.f32(p);
            append32(curve, r.u32(p));
          } else if (width == 2)
            append16(curve, r.u16(p));
          else if (width == 1) {
            r.range(p, 1);
            curve.push_back(source.motion[p]);
          }
          p += width;
        }
    }
    if (p > size)
      throw std::runtime_error("FCV: Key axis outside payload bounds");
    c.curves.push_back(std::move(curve));
  }
  for (auto &curve : c.curves)
    c.pointers.push_back(curve.data());
  c.data.kinds = c.kinds.data();
  c.data.parts = c.parts.data();
  c.data.curves = c.pointers.data();
  if (source.sequence.empty())
    return clip;
  AssetReader seq{source.sequence};
  seq.range(0, 4);
  auto n = seq.u16(0);
  if (n == 0 || n > 1023)
    throw std::runtime_error("SEQ: Frame budget");
  seq.range(4, std::size_t(n) * 4);
  c.sequence.resize(n + 1);
  c.sequence[0] =
      n | (u32(source.sequence[2]) << 16) | (u32(source.sequence[3]) << 24);
  for (unsigned i = 0; i < n; ++i) {
    auto frame = seq.u16(4 + i * 4);
    if (frame > c.data.maxFrame * 64u)
      throw std::runtime_error("SEQ: Curve frame outside motion bounds");
    c.sequence[i + 1] = frame | (u32(source.sequence[6 + i * 4]) << 16) |
                        (u32(source.sequence[7 + i * 4]) << 24);
  }
  return clip;
}
void coordInit(cCoord &c) {
  PSMTXIdentity(c.mat);
  PSMTXIdentity(c.l_mat);
  c.scale = c.r_scale = {1, 1, 1};
}
} // namespace
struct AnimationRig::Impl {
  motion64::cEm model{};
  std::vector<motion64::cEm> parts;
  std::vector<std::unique_ptr<Clip>> clips;
  std::vector<std::optional<MotionCycle>> cycles;
  std::vector<motion64::u32> blend;
  std::vector<motion64::u16> flip;
  std::vector<Matrix34> matrices;
  std::vector<std::array<float, 3>> bind;
  motion64::MOTION_INFO secondary{};
  motion64::ATTACH_CAMERA attached{};
  std::array<unsigned, 3> blendClips{};
  float blendRate = 0;
  float posedBlendRate = 0;
  float waistYaw = 0;
  bool blendActive = false;
  bool additive = false;
  unsigned state = 0;
  std::array<unsigned, 7> locomotionClips{0,1,2,3,4,5,6};
  double accumulator = 0;
  void set(unsigned clip, motion64::MOTION_INFO &work, int hokan, bool sequence,
           int frame, motion64::u16 flags) {
    if (clip >= clips.size())
      throw std::runtime_error("Motion: Clip index");
    if (hokan < 0 || hokan > 255 || frame < 0 || frame > 65535)
      throw std::runtime_error("Motion: invalid starting frame or Hokan");
    auto &c = *clips[clip];
    motion64::error.clear();
    motion64::MotionSetCore(&model, &work, &c.data,
                            sequence && !c.sequence.empty() ? c.sequence.data()
                                                            : nullptr,
                            hokan, flags & ~1u, frame);
    if (!motion64::error.empty())
      throw std::runtime_error(motion64::error);
  }
  void start(unsigned clip, int hokan, bool sequence = true, int frame = 0,
             bool loop = true) {
    set(clip, model.Motion, hokan, sequence, frame, loop ? 4 : 0);
    blendActive = false;
    model.pMotionB = nullptr;
    state = clip;
  }
  void setSecondary(unsigned clip, unsigned frame, unsigned hokan) {
    // cMot3::set0 intentionally narrows its sequence frame to u8.
    set(clip, secondary, int(hokan), false, int(motion64::u8(frame)), 4);
    if (additive)
      secondary.Mot_flag |= 0x80000000;
    model.pMotionB = &secondary;
  }
  void waist() {
    if (waistYaw == 0)
      return;
    // cPlBody::waistMove, after MotionMove as in cPlayer::move. The flag is
    // consumed by partsWorldCalc, so the offsets are reapplied each pose.
    for (unsigned i = 1; i <= 3; ++i) {
      parts[i].motParts.flags |= 0x40000000;
      parts[i].inv_offset = {0, i == 3 ? -waistYaw : waistYaw * .5f, 0};
    }
    model.partsWorldCalc();
  }
  void copyMatrices() {
    for (std::size_t i = 0; i < parts.size(); ++i)
      for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 4; ++col) {
          auto f = parts[i].mat[row][col];
          if (!std::isfinite(f))
            throw std::runtime_error("Motion: invalid pose");
          matrices[i][row * 4 + col] = f;
        }
  }
  motion64::u32 pose() {
    posedBlendRate=blendActive?blendRate:0;
    motion64::error.clear();
    auto result = motion64::MotionMove(&model, nullptr);
    if (!motion64::error.empty())
      throw std::runtime_error(motion64::error);
    waist();
    copyMatrices();
    return result;
  }
};
AnimationRig::AnimationRig(const SkinnedModel &body,
                           const std::array<AnimationBytes, 3> &clips)
    : AnimationRig(body,
                   std::vector<AnimationBytes>(clips.begin(), clips.end())) {}
AnimationRig::AnimationRig(const SkinnedModel &body,
                           const std::vector<AnimationBytes> &clips)
    : AnimationRig(body, clips, false) {}
AnimationRig::AnimationRig(const SkinnedModel &body)
    : AnimationRig(body, std::vector<AnimationBytes>{}, true) {}
AnimationRig::AnimationRig(const SkinnedModel &body,
                           const std::vector<AnimationBytes> &clips,
                           bool staticPose)
    : impl_(std::make_unique<Impl>()) {
  auto &h = *impl_;
  if ((!staticPose && clips.empty()) || clips.size() > 1024)
    throw std::runtime_error("Motion: Clip budget");
  auto n = body.mesh.joints.size();
  if (n < 1 || n > 248)
    throw std::runtime_error("Motion: Skeleton budget");
  body.validateSkeleton(n);
  h.bind = RestJointCenters(body.mesh);
  h.parts.resize(n);
  h.matrices.resize(n);
  coordInit(h.model);
  h.model.nParts = static_cast<motion64::u8>(n);
  h.model.pList = &h.parts[0];
  h.model.Motion.Seq_speed = 1;
  h.secondary = {};
  h.blendActive = false;
  h.blendRate = 0;
  h.waistYaw = 0;
  for (std::size_t i = 0; i < n; ++i) {
    auto &p = h.parts[i];
    auto &joint = body.mesh.joints[i];
    if (joint.attachment != i)
      throw std::runtime_error("Motion: Unordered parts");
    coordInit(p);
    p.pList = i + 1 < n ? &h.parts[i + 1] : nullptr;
    p.pParent = joint.parent == 255 ? &h.model : &h.parts[joint.parent];
    p.pos = {joint.center[0], joint.center[1], joint.center[2]};
    motion64::PSMTXIdentity(p.lt_inv_mat);
    for (unsigned k = 0; k < 3; ++k)
      p.lt_inv_mat[k][3] = -h.bind[i][k];
  }
  h.model.partsMatCalc();
  h.model.partsWorldCalc();
  for (auto &p : h.parts)
    p.world_old = p.world_old2 = p.world;
  h.blend.resize(1 + body.blendJoints.size() * 2);
  h.blend[0] = static_cast<motion64::u32>(body.blendJoints.size());
  for (std::size_t i = 0; i < body.blendJoints.size(); ++i) {
    auto b = body.blendJoints[i];
    h.blend[1 + i * 2] = b[0] | (motion64::u32(b[1]) << 16);
    h.blend[2 + i * 2] = b[2] | (motion64::u32(b[3]) << 16);
  }
  if (!body.blendJoints.empty())
    h.model.pDblJnt = reinterpret_cast<motion64::u16 *>(h.blend.data());
  h.clips.resize(clips.size());
  h.cycles.resize(clips.size());
  for (unsigned i = 0; i < clips.size(); ++i)
    h.clips[i] = parseClip(clips[i], n);
  // MotionSetCore prepares the selected action during reset/start. The other
  // clips' diagnostic cycle metadata is calculated lazily on an isolated
  // skeleton, rather than resetting every part for all 92+ clips at startup.
  if (staticPose) h.copyMatrices(); else reset();
}
AnimationRig::~AnimationRig() = default;
unsigned AnimationRig::appendActions(const std::vector<AnimationBytes> &source) {
  auto &h = *impl_;
  if (source.empty() || h.clips.size() > 1024 || source.size() > 1024 - h.clips.size())
    throw std::runtime_error("Motion: Event clip budget");
  std::vector<std::unique_ptr<Clip>> parsed;
  for (const auto &bytes : source) parsed.push_back(parseClip(bytes, h.parts.size()));
  const auto first = unsigned(h.clips.size());
  h.clips.reserve(1024); h.cycles.reserve(1024);
  for (auto &clip : parsed) h.clips.push_back(std::move(clip));
  h.cycles.resize(h.clips.size());
  return first;
}
unsigned AnimationRig::clipCount() const { return unsigned(impl_->clips.size()); }
void AnimationRig::enableAttachedCamera(bool enabled) {
  auto &h = *impl_;
  if (enabled) {
    h.attached = {};
    std::fill(std::begin(h.attached.parts), std::end(h.attached.parts), 0xff);
    h.model.Motion.pAttachCam = &h.attached;
  } else h.model.Motion.pAttachCam = nullptr;
}
AttachedMotionCamera AnimationRig::attachedCamera() const {
  AttachedMotionCamera result;
  const auto *camera = impl_->model.Motion.pAttachCam;
  if (!camera || !camera->type) return result;
  result.type = camera->type;
  result.hasEye = camera->parts[0] != 0xff;
  result.hasTarget = camera->parts[1] != 0xff;
  result.hasRoll = camera->parts[2] != 0xff;
  result.hasFov = camera->parts[3] != 0xff;
  const auto &eye = camera->camera_data[0], &target = camera->camera_data[1];
  result.eye = {eye.x, eye.y, eye.z}; result.target = {target.x, target.y, target.z};
  result.roll = camera->camera_data[2].y; result.fovRadians = camera->camera_data[3].y;
  return result;
}
void AnimationRig::setLocomotionClips(const std::array<unsigned, 7> &clips) {
  for (auto clip : clips) if (clip >= impl_->clips.size())
    throw std::runtime_error("Motion: Weapon footwork clip index");
  impl_->locomotionClips = clips;
}
void AnimationRig::reset() {
  auto &h = *impl_;
  motion64::MotionClear(&h.model, 0);
  h.model.Motion.Seq_speed = 1;
  h.secondary = {};
  h.blendRate = 0;
  h.waistYaw = 0;
  h.accumulator = 0;
  h.start(h.locomotionClips[0], 0);
  motion64::pG->Speed = 0;
  h.pose();
}
void AnimationRig::clearEventMotion(bool rebuildPose) {
  auto &h=*impl_;
  motion64::MotionClear(&h.model,1);
  h.model.pMotionB=nullptr;h.blendActive=false;h.waistYaw=0;
  if(rebuildPose){h.model.partsMatCalc();h.model.partsWorldCalc();h.copyMatrices();}
}
void AnimationRig::update(Locomotion state, double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::runtime_error("Motion: invalid time");
  auto &h = *impl_;
  auto index = static_cast<unsigned>(state);
  if (index > unsigned(Locomotion::QuickTurn))
    throw std::runtime_error("Motion: Clip index");
  auto desired = h.locomotionClips[index];
  if (desired != h.state) {
    h.start(desired, 6);
    motion64::pG->Speed = 0;
    h.pose();
  }
  h.accumulator += std::min(seconds, .05) * 30;
  while (h.accumulator >= 1) {
    motion64::pG->Speed = 1;
    h.pose();
    h.accumulator -= 1;
  }
}
RootMotionStep AnimationRig::tick(Locomotion state, double playbackRate) {
  if (!std::isfinite(playbackRate) || playbackRate < 0 || playbackRate > 1)
    throw std::runtime_error("Motion: invalid playback tick");
  auto &h = *impl_;
  auto index = static_cast<unsigned>(state);
  if (index > unsigned(Locomotion::QuickTurn))
    throw std::runtime_error("Motion: Clip index");
  auto desired = h.locomotionClips[index];
  if (desired != h.state) {
    bool phase = (state == Locomotion::Walk &&
                  h.state == h.locomotionClips[unsigned(Locomotion::Run)]) ||
                 (state == Locomotion::Run &&
                  h.state == h.locomotionClips[unsigned(Locomotion::Walk)]);
    int frame = 0;
    if (phase && h.model.Motion.Seq_frame_num) {
      auto normalized = h.model.Motion.Seq_frame / h.model.Motion.Seq_frame_num;
      frame = int(normalized * cycle(desired).sequenceFrames);
    }
    int hokan =
        phase                                                               ? 5
        : state == Locomotion::QuickTurn                                    ? 5
        : (state == Locomotion::TurnLeft || state == Locomotion::TurnRight) ? 7
                                                                            : 8;
    h.start(desired, hokan, true, frame, state != Locomotion::QuickTurn);
  }
  h.model.Motion.Seq_speed = float(playbackRate);
  motion64::Vec translation{}, rotation{};
  motion64::error.clear();
  // The original samples root speed before MotionMoveCore/SequenceCtrl. Its
  // history and loop correction advance exactly once, while model placement
  // stays zero to avoid applying a second transform in the actor shader.
  motion64::MotionGetSpeed(&h.model, &h.model.Motion, 0, &translation,
                           &rotation);
  if (!motion64::error.empty())
    throw std::runtime_error(motion64::error);
  for (auto f : {translation.x, translation.y, translation.z, rotation.y})
    if (!std::isfinite(f) || std::abs(f) > 100000)
      throw std::runtime_error("Motion: invalid root motion");
  motion64::pG->Speed = 1;
  auto result = h.pose();
  return {{translation.x, translation.y, translation.z},
          rotation.y,
          state == Locomotion::QuickTurn && (result & 12) != 0};
}
void AnimationRig::startAction(unsigned clip, unsigned blendFrames,
                               unsigned frame, bool loop) {
  startActionFlags(clip, blendFrames, frame, loop ? 4 : 0);
}
void AnimationRig::startActionFlags(unsigned clip, unsigned blendFrames,
                                    unsigned frame, std::uint16_t sourceFlags) {
  if (blendFrames > 255 || frame > 65535)
    throw std::runtime_error("Motion: invalid starting frame or Hokan");
  auto &h = *impl_;
  h.set(clip, h.model.Motion, int(blendFrames), true, int(frame), sourceFlags);
  h.blendActive = false;
  h.model.pMotionB = nullptr;
  h.state = clip;
  h.accumulator = 0;
}
AnimationStatus AnimationRig::advanceAction(double playbackRate) {
  if (!std::isfinite(playbackRate) || playbackRate < 0 || playbackRate > 1)
    throw std::runtime_error("Motion: invalid playback tick");
  auto &h = *impl_;
  h.model.Motion.Seq_speed = float(playbackRate);
  motion64::pG->Speed = 1;
  h.pose();
  return status();
}
RootMotionStep AnimationRig::advanceRootAction(double playbackRate,
                                               bool rebuildWorld) {
  if (!std::isfinite(playbackRate) || playbackRate < 0 || playbackRate > 1)
    throw std::runtime_error("Motion: invalid playback tick");
  auto &h = *impl_;
  h.model.Motion.Seq_speed = float(playbackRate);
  motion64::Vec translation{}, rotation{};
  motion64::error.clear();
  motion64::MotionGetSpeed(&h.model, &h.model.Motion, 0, &translation, &rotation);
  if (!motion64::error.empty())
    throw std::runtime_error(motion64::error);
  for (auto f : {translation.x, translation.y, translation.z, rotation.y})
    if (!std::isfinite(f) || std::abs(f) > 100000)
      throw std::runtime_error("Motion: invalid root motion");
  motion64::pG->Speed = 1;
  auto result = h.pose();
  if (rebuildWorld) {
    h.model.partsWorldCalc();
    h.copyMatrices();
  }
  return {{translation.x, translation.y, translation.z}, rotation.y,
          (result & 12) != 0};
}
RootMotionStep AnimationRig::rootMotionSpeed() {
  auto &h=*impl_; motion64::Vec translation{}, rotation{};
  motion64::error.clear();
  motion64::MotionGetSpeed(&h.model,&h.model.Motion,0,&translation,&rotation);
  if(!motion64::error.empty()) throw std::runtime_error(motion64::error);
  for(auto f : {translation.x,translation.y,translation.z,rotation.y})
    if(!std::isfinite(f) || std::abs(f)>100000) throw std::runtime_error("Motion: invalid base speed");
  return {{translation.x,translation.y,translation.z},rotation.y,status().ended};
}
RootMotionStep AnimationRig::advanceBlowAction(double playbackRate,
                                              double firstTranslationScale,
                                              bool stretchRoot) {
  if (!std::isfinite(playbackRate) || playbackRate < 0 || playbackRate > 1 ||
      !std::isfinite(firstTranslationScale) || firstTranslationScale < 0 ||
      firstTranslationScale > 2)
    throw std::runtime_error("Motion: invalid blow playback tick or root factor");
  auto &h = *impl_;
  h.model.Motion.Seq_speed = float(playbackRate);
  motion64::Vec first{}, firstRotation{}, second{}, secondRotation{};
  motion64::error.clear();
  // Both calls write the original Pos/Ang/history/Pos_move_old. Flag 8 would
  // change the second Hokan extraction and therefore the authored movement.
  motion64::MotionGetSpeed(&h.model, &h.model.Motion, 0, &first, &firstRotation);
  motion64::MotionGetSpeed(&h.model, &h.model.Motion, 0, &second, &secondRotation);
  if (!motion64::error.empty())
    throw std::runtime_error(motion64::error);
  // Both original MotionAddSpeed calls use the actor matrix from the start
  // of the tick: pos/ang change, but mat is rebuilt later by MotionMoveCore.
  // Actual Dm_Blow flags 1/0x41 make the second displacement zero; Hokan does
  // not itself enable the optional 0x400 root interpolation flag.
  motion64::Vec scaled{}, combined{};
  motion64::PSVECScale(&first, &scaled, float(firstTranslationScale));
  motion64::PSVECAdd(&scaled, &second, &combined);
  RootMotionStep root{{combined.x, combined.y, combined.z},
                      firstRotation.y + secondRotation.y, false};
  for (auto value : {root.translation[0], root.translation[1],
                     root.translation[2], root.yaw})
    if (!std::isfinite(value) || std::abs(value) > 100000)
      throw std::runtime_error("Motion: invalid blow root motion");
  motion64::pG->Speed = 1;
  const auto result = h.pose();
  root.ended = (result & 12) != 0;
  // Dm_Blow's Timer is 1 for a kick. The outer cEm10::move rebuilds world
  // parts afterwards even when there is no stretch. Rebuilding local parts
  // would erase this authored local-matrix adjustment.
  if (stretchRoot && !root.ended && !(h.model.Motion.Seq_old.Free & 0x80))
    h.parts[0].l_mat[1][3] *= 1.5f;
  h.model.partsWorldCalc();
  h.copyMatrices();
  return root;
}
AnimationStatus AnimationRig::status() const {
  const auto &w = impl_->model.Motion;
  return {w.Mot_frame, w.Seq_frame, w.Mot_state, (w.Mot_state & 12) != 0};
}
float AnimationRig::curveFrame() const { return status().curveFrame; }
bool AnimationRig::crossedFrame(float frame) const {
  if (!std::isfinite(frame) || frame < 0)
    throw std::runtime_error("Motion: invalid event frame");
  return motion64::MotionCheckCrossFrame(&impl_->model.Motion, frame) != 0;
}
std::uint8_t AnimationRig::sequenceFree() const {
  return impl_->model.Motion.Seq_old.Free;
}
std::optional<SequenceSound> AnimationRig::consumeSequenceSound() {
  auto &sequence = impl_->model.Motion.Seq_old;
  if (!sequence.Se) return {};
  const SequenceSound result{sequence.Se, sequence.Free};
  sequence.Se = 0;
  return result;
}
void AnimationRig::setFlipTable(const std::vector<std::uint16_t> &parts) {
  auto &h = *impl_;
  if (!parts.empty() && parts.size() != h.parts.size())
    throw std::runtime_error("Motion: incomplete mirror joints");
  for (auto part : parts)
    if (part != 0xffff && part >= h.parts.size())
      throw std::runtime_error("Motion: Mirror joint outside skeleton bounds");
  h.flip = parts;
  h.model.pXFlip = h.flip.empty() ? nullptr : h.flip.data();
}
void AnimationRig::startBlend3(const std::array<unsigned, 3> &clips, float rate,
                               unsigned blendFrames, unsigned frame, bool loop,
                               bool additive) {
  if (!std::isfinite(rate) || blendFrames > 255 || frame > 65535)
    throw std::runtime_error("Motion: invalid blend start");
  for (auto clip : clips)
    (void)maxFrame(clip);
  auto &h = *impl_;
  // pl_class.cpp cMot3::set, with its PPC narrow argument read as the u8
  // parameter. Both work areas and all FCV curves have stable owning storage.
  h.start(clips[0], int(blendFrames), true, int(frame), loop);
  h.blendClips = clips;
  h.blendRate = 0;
  h.additive = additive;
  h.setSecondary(clips[1], frame, blendFrames);
  h.secondary.Brate = 0;
  h.blendActive = true;
  h.accumulator = 0;
  setBlend3Rate(rate);
}
void AnimationRig::setBlend3Rate(float rate) {
  if (!std::isfinite(rate))
    throw std::runtime_error("Motion: invalid blend rate");
  auto &h = *impl_;
  if (!h.blendActive || h.model.pMotionB != &h.secondary)
    throw std::runtime_error("Motion: no active three-way blend");
  rate = std::clamp(rate, -1.f, 1.f);
  // Actual cMot3::move branches: positive/zero selects m1, negative m2.
  // On a sign crossing the secondary starts at the primary sequence frame,
  // narrowed to u8, with original hokan=2. No per-tick frame resynchronizing.
  if (h.blendRate < 0 && rate >= 0)
    h.setSecondary(h.blendClips[1], unsigned(h.model.Motion.Seq_frame), 2);
  else if (h.blendRate >= 0 && rate < 0)
    h.setSecondary(h.blendClips[2], unsigned(h.model.Motion.Seq_frame), 2);
  h.blendRate = rate;
  h.secondary.Brate = std::abs(rate);
}
float AnimationRig::posedBlend3Rate() const { return impl_->posedBlendRate; }
void AnimationRig::sampleBlend3(const std::array<unsigned, 3> &clips,
                                float rate, float frame) {
  if (!std::isfinite(rate) || !std::isfinite(frame) || frame < 0)
    throw std::runtime_error("Motion: invalid blend frame");
  for (auto clip : clips)
    if (frame > maxFrame(clip))
      throw std::runtime_error("Motion: invalid blend frame");
  auto &h = *impl_;
  rate = std::clamp(rate, -1.f, 1.f);
  // Absolute FCV sampling includes maxFrame. A looping motion excludes
  // that final key and MotionSequenceCtrl would wrap it to frame zero.
  h.start(clips[0], 0, false, 0, false);
  h.blendClips = clips;
  h.blendRate = rate;
  h.additive = false;
  h.set(clips[rate < 0 ? 2 : 1], h.secondary, 0, false, 0, 0);
  h.model.pMotionB = &h.secondary;
  h.blendActive = true;
  h.secondary.Brate = std::abs(rate);
  h.model.Motion.Mot_attr |= 0x8000;
  h.secondary.Mot_attr |= 0x8000;
  h.model.Motion.Seq_frame = frame;
  h.secondary.Seq_frame = frame;
  motion64::pG->Speed = 0;
  h.pose();
}
void AnimationRig::setWaistYaw(float radians) {
  if (!std::isfinite(radians) || std::abs(radians) > PI ||
      (radians != 0 && impl_->parts.size() < 4))
    throw std::runtime_error("Motion: invalid hip rotation");
  auto &h = *impl_;
  h.waistYaw = radians;
  // Refresh the attachment matrices without consuming another motion frame
  // or Hokan step. Rebuilding from local matrices also removes a prior twist.
  h.model.partsWorldCalc();
  h.waist();
  h.copyMatrices();
}
float AnimationRig::sequenceFrame() const {
  return impl_->model.Motion.Seq_frame;
}
void AnimationRig::setPartScale(unsigned part, const std::array<float, 3> &scale) {
  auto &h = *impl_;
  if (part >= h.parts.size() ||
      !std::all_of(scale.begin(), scale.end(), [](float v) { return std::isfinite(v); }))
    throw std::runtime_error("Motion: invalid part scale");
  h.parts[part].scale = {scale[0], scale[1], scale[2]};
  h.parts[part].matCalc();
  h.model.partsWorldCalc();
  h.waist();
  h.copyMatrices();
}
std::array<float, 3> AnimationRig::partScale(unsigned part) const {
  if (part >= impl_->parts.size())
    throw std::runtime_error("Motion: Part scale index");
  const auto &s = impl_->parts[part].scale;
  return {s.x, s.y, s.z};
}
void AnimationRig::setPartPosition(unsigned part,
                                   const std::array<float, 3> &position) {
  auto &h = *impl_;
  if (part >= h.parts.size() ||
      !std::all_of(position.begin(), position.end(), [](float v) {
        return std::isfinite(v) && std::abs(v) <= 100000;
      })) throw std::runtime_error("Motion: invalid part position");
  h.parts[part].pos = {position[0], position[1], position[2]};
  h.parts[part].matCalc(); h.model.partsWorldCalc(); h.waist(); h.copyMatrices();
}
std::array<float, 3> AnimationRig::partPosition(unsigned part) const {
  if (part >= impl_->parts.size())
    throw std::runtime_error("Motion: Part position index");
  const auto &p = impl_->parts[part].pos;
  return {p.x, p.y, p.z};
}
void AnimationRig::setPartRotation(unsigned part,const std::array<float,3> &rotation) {
  auto &h=*impl_;
  if(part>=h.parts.size() || !std::all_of(rotation.begin(),rotation.end(),[](float v){return std::isfinite(v) && std::abs(v)<=1000;}))
    throw std::runtime_error("Motion: invalid part rotation");
  h.parts[part].ang={rotation[0],rotation[1],rotation[2]};
  h.parts[part].matCalc();h.model.partsWorldCalc();h.waist();h.copyMatrices();
}
std::array<float,3> AnimationRig::partRotation(unsigned part) const {
  if(part>=impl_->parts.size())throw std::runtime_error("Motion: Part rotation index");
  const auto &a=impl_->parts[part].ang;return {a.x,a.y,a.z};
}
MotionCycle AnimationRig::cycle(unsigned clip) const {
  if (clip >= impl_->cycles.size())
    throw std::runtime_error("Motion: Clip index");
  auto &h=*impl_;
  if(!h.cycles[clip]) {
    // MotionSetCore changes Hermite/Hokan work even when given a separate
    // MOTION_INFO. Relink a private skeleton before obtaining the exact
    // original cycle; querying metadata never changes the live pose/history.
    auto preview=h.model;auto parts=h.parts;
    preview.Motion.pAttachCam=nullptr;preview.pMotionB=nullptr;
    preview.pList=parts.data();
    for(std::size_t i=0;i<parts.size();++i) {
      parts[i].pList=i+1<parts.size()?&parts[i+1]:nullptr;
      const auto parent=h.parts[i].pParent;
      if(parent==&h.model)parts[i].pParent=&preview;
      else for(std::size_t k=0;k<parts.size();++k)
        if(parent==&h.parts[k]){parts[i].pParent=&parts[k];break;}
    }
    auto &data=*h.clips[clip];motion64::MOTION_INFO work{};
    motion64::error.clear();
    motion64::MotionSetCore(&preview,&work,&data.data,
        data.sequence.empty()?nullptr:data.sequence.data(),0,4,0);
    if(!motion64::error.empty())throw std::runtime_error(motion64::error);
    h.cycles[clip]=MotionCycle{{work.Pos_dist.x,work.Pos_dist.y,work.Pos_dist.z},
                              work.Ang_dist.y,work.Seq_frame_num};
  }
  return *h.cycles[clip];
}
const std::vector<Matrix34> &AnimationRig::bones() const {
  return impl_->matrices;
}
const std::vector<Matrix34> &AnimationRig::jointMatrices() const {
  return impl_->matrices;
}
const std::vector<std::array<float, 3>> &AnimationRig::bindCenters() const {
  return impl_->bind;
}
void AnimationRig::sample(unsigned clip, float frame) {
  if (!std::isfinite(frame) || frame < 0 || frame > maxFrame(clip))
    throw std::runtime_error("Motion: invalid curve frame");
  auto &h = *impl_;
  // Event::ExePacket_Mot uses flags=1 (no loop): its final FCV key is
  // visible before the event hands control back to the scenario.
  h.start(clip, 0, false, 0, false);
  h.model.Motion.Mot_attr |= 0x8000;
  h.model.Motion.Seq_frame = frame;
  motion64::pG->Speed = 0;
  h.pose();
}
float AnimationRig::maxFrame(unsigned clip) const {
  if (clip >= impl_->clips.size())
    throw std::runtime_error("Motion: Clip index");
  return impl_->clips[clip]->data.maxFrame;
}
} // namespace re4
