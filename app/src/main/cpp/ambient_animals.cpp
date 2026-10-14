#include "ambient_animals.h"
#include "archive.h"
#include "ports/rnd.h"
#include "turn_rules.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr float pi = 3.1415927f;
constexpr std::array<AnimalMotionSource, 5> sources{{
    {8, 30, 5},  // em28_R1_Wait: ARC0c, no SEQ.
    {9, 10, 1},  // Wait alternate: ARC0d, no SEQ.
    {20, 10, 1}, // Wait alternate: ARC18, no SEQ.
    {10, 10, 5}, // em28_R1_Walk: ARC0e, no SEQ.
    {11, 10, 5}, // em28_R1_Walk: ARC0f, no SEQ.
}};
class RandomSession {
public:
  explicit RandomSession(std::uint16_t &state)
      : state_(state), previous_(RndState()) {
    RndInit(state_);
  }
  ~RandomSession() {
    state_ = RndState();
    RndInit(previous_);
  }

private:
  std::uint16_t &state_;
  std::uint16_t previous_;
};
std::vector<std::uint8_t> asset(const RoomArchive &archive, unsigned entry,
                                const std::string &tag) {
  if (entry >= archive.entries().size() ||
      !std::equal(tag.begin(), tag.end(), archive.entries()[entry].tag.begin()))
    throw std::runtime_error("EM28: Original asset is missing or has the wrong type");
  return archive.copy(entry);
}
// Frozen game/sub2.cpp::Muku2, then LIMIT_ANGLE from the player adapter.
float turnToward(float from, float to, float limit) {
  float delta = to - from;
  if (delta < 0)
    delta += 6.2831855f;
  if (delta < pi) {
    if (limit > delta)
      limit = delta;
  } else {
    delta -= 6.2831855f;
    limit = limit > -delta ? delta : -limit;
  }
  return GameCubeLimitAngle(from + limit);
}
bool walking(AnimalMotion motion) {
  return motion == AnimalMotion::Walk || motion == AnimalMotion::WalkAlt;
}
void appendBody(const SkinnedModel &body, StaticScene &mesh,
                std::vector<SkinCorner> &skin) {
  const auto &model = body.mesh;
  for (const auto &primitive : model.primitives) {
    if (primitive.triangles.empty())
      continue;
    const auto &header = model.materialHeaders.at(primitive.displayList);
    SceneDraw draw;
    draw.work = 0;
    draw.first = mesh.vertices.size() / 3;
    draw.count = primitive.triangles.size() * 3;
    draw.image = header[12];
    draw.repeat = model.colored;
    draw.vertexColors = model.colored && model.vertexAmbient;
    draw.alphaTest = (header[11] & 4) != 0;
    draw.alphaImage = header[14];
    draw.alphaReference = header[20];
    mesh.draws.push_back(draw);
    for (const auto &triangle : primitive.triangles)
      for (auto corner : triangle) {
        const auto &ref = primitive.vertices.at(corner);
        const auto &p = model.positions.at(ref.position);
        const auto &n = model.normals.at(ref.normal);
        const auto &uv = model.texcoords.at(ref.texcoord);
        mesh.vertices.insert(mesh.vertices.end(), p.begin(), p.end());
        mesh.normals.insert(mesh.normals.end(), n.begin(), n.end());
        mesh.uv.insert(mesh.uv.end(), uv.begin(), uv.end());
        const auto color =
            draw.vertexColors ? model.colors.at(ref.color)
                              : std::array<std::uint8_t, 4>{255, 255, 255, 255};
        mesh.colors.insert(mesh.colors.end(), color.begin(), color.end());
        skin.push_back({body.weights.at(body.positionWeights.at(ref.position)),
                        body.weights.at(body.normalWeights.at(ref.normal))});
      }
    mesh.triangles += primitive.triangles.size();
  }
  mesh.included = 1;
}
} // namespace

AnimalMotionSource ChickenMotionSource(AnimalMotion motion) {
  const auto index = static_cast<unsigned>(motion);
  if (index >= sources.size())
    throw std::runtime_error("EM28: invalid motion index");
  return sources[index];
}
std::vector<EnemySpawn> VillageChickenSpawns(const EnemySpawns &spawns) {
  std::vector<EnemySpawn> chickens;
  for (const auto &record : spawns.records()) {
    if (record.room != 0x400 || record.id != 0x28 || !record.alive() ||
        record.alreadySet())
      continue;
    if (record.type > 1 || !record.hp || chickens.size() >= 6)
      throw std::runtime_error("r400: Unsupported EM28 type or count");
    chickens.push_back(record);
  }
  if (chickens.size() != 6)
    throw std::runtime_error("r400: six original EM28 entries are missing");
  return chickens;
}
AmbientAnimal::AmbientAnimal(const SatCollision &sat, const EnemySpawn &source,
                             const SkinnedModel &body,
                             const std::vector<AnimationBytes> &clips)
    : source_(source), probe_(std::make_unique<WalkProbe>(
                           sat, WalkConfig{100, 500, 2600, 250, 250})),
      rig_(std::make_unique<AnimationRig>(body, clips)),
      heading_(GameCubeLimitAngle(float(source.heading))) {
  // The SAT adapter is deliberately conservative on authored perches. When
  // their collision belongs to an unported dynamic object, retain ESL Y.
  probe_->place(source.position, 250);
  // Original Init sets ARC0c with hokan0 and evaluates one frame at creation.
  rig_->startActionFlags(0, 0, 0, 5);
  rig_->advanceAction();
  start(AnimalMotion::Idle);
}
AmbientAnimal::AmbientAnimal(AmbientAnimal &&) noexcept = default;
AmbientAnimal &AmbientAnimal::operator=(AmbientAnimal &&) noexcept = default;
AmbientAnimal::~AmbientAnimal() = default;
void AmbientAnimal::start(AnimalMotion motion) {
  const auto source = ChickenMotionSource(motion);
  rig_->startActionFlags(static_cast<unsigned>(motion), source.blendFrames, 0,
                         source.flags);
  motion_ = motion;
  if (walking(motion)) {
    // Source Walk chooses its motion first, then angle and 3..7 loop counter.
    targetHeading_ = fRand1_1() * pi;
    loops_ = unsigned(Rnd() % 5) + 3;
  }
}
CollisionVec AmbientAnimal::feet() const {
  return probe_->ready() ? probe_->position() : source_.position;
}
Matrix34 AmbientAnimal::placement() const {
  SmdPlacement placement;
  const auto position = feet();
  for (unsigned i = 0; i < 3; ++i) {
    placement.position[i] = float(position[i]);
    placement.rotation[i] = float(source_.rotation[i]);
  }
  placement.rotation[1] = heading_;
  placement.scale = {1, 1, 1};
  return PlacementMatrix(placement);
}
void AmbientAnimal::tick() {
  if (walking(motion_)) {
    heading_ = turnToward(heading_, targetHeading_, pi / 128.f);
    if (stuck_ > 30)
      loops_ = 0;
  }
  const auto old = feet();
  const auto root = rig_->advanceRootAction();
  if (probe_->ready()) {
    const float c = std::cos(heading_), s = std::sin(heading_);
    probe_->moveDelta({c * root.translation[0] + s * root.translation[2],
                       root.translation[1],
                       -s * root.translation[0] + c * root.translation[2]});
    const auto now = feet();
    const auto requested = std::hypot(root.translation[0], root.translation[2]);
    if (std::hypot(now[0] - old[0], now[2] - old[2]) < requested * .5)
      ++stuck_;
    else
      stuck_ = 0;
  }
  if (!rig_->status().state)
    return;
  if (walking(motion_)) {
    if (loops_)
      --loops_;
    else
      start(AnimalMotion::Idle);
  } else if (motion_ == AnimalMotion::Idle) {
    if (!(source_.flags & 0x80000000) && (Rnd() & 1)) {
      // Only the two source peck motions are admitted. The egg branch is
      // held out until original pickup/item registration is installed.
      const bool peck = (Rnd() & 3) != 0;
      if (!peck)
        (void)Rnd(); // Preserve the source's extra egg-branch test.
      start(peck ? AnimalMotion::Peck : AnimalMotion::PeckAlt);
    }
  } else {
    if ((Rnd() & 1) || !probe_->ready())
      start(AnimalMotion::Idle);
    else
      start((Rnd() & 1) ? AnimalMotion::Walk : AnimalMotion::WalkAlt);
  }
}
AmbientAnimals::AmbientAnimals(const SatCollision &sat,
                               const std::vector<std::uint8_t> &esl,
                               const std::string &em28Drs) {
  const auto selected = VillageChickenSpawns(EnemySpawns(esl));
  const auto archive = RoomArchive::LoadDrs(em28Drs);
  SkinnedModel body(asset(archive, 0, "BIN"));
  if (body.mesh.joints.size() != 19)
    throw std::runtime_error("EM28: original 19-part skeleton is missing");
  body.validateSkeleton(19);
  appendBody(body, meshes_[0], skin_);
  meshes_[1] = meshes_[0];
  for (unsigned variant = 0; variant < meshes_.size(); ++variant) {
    std::vector<std::uint8_t> emptyHeader(16);
    emptyHeader[0] = 64;
    Smd palettes(emptyHeader);
    palettes.palettes.push_back(asset(archive, 1 + variant, "TPL"));
    AttachSceneTextures(palettes, meshes_[variant]);
    const auto &mesh = meshes_[variant];
    if (mesh.vertices.empty() || mesh.fallbackTriangles ||
        mesh.unresolvedAlphaTriangles || mesh.textures.empty())
      throw std::runtime_error("EM28: Geometry or texture source is missing");
  }
  std::vector<AnimationBytes> clips;
  for (const auto source : sources)
    clips.push_back({asset(archive, source.motionEntry, "FCV"), {}});
  animals_.reserve(selected.size());
  for (const auto &source : selected)
    animals_.push_back(AmbientAnimal(sat, source, body, clips));
}
const StaticScene &AmbientAnimals::mesh(unsigned variant) const {
  if (variant >= meshes_.size())
    throw std::runtime_error("EM28: invalid texture variant");
  return meshes_[variant];
}
void AmbientAnimals::step(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0 || seconds > .25)
    throw std::runtime_error("EM28: invalid simulation timestep");
  accumulator_ += seconds;
  RandomSession random(randomState_);
  constexpr double step = 1. / 30;
  while (accumulator_ + 1e-12 >= step) {
    accumulator_ -= step;
    if (accumulator_ < 0)
      accumulator_ = 0;
    for (auto &animal : animals_)
      animal.tick();
    ++ticks_;
  }
}
std::string AmbientAnimals::describe() const {
  unsigned grounded = 0;
  for (const auto &animal : animals_)
    grounded += animal.onOriginalFloor();
  std::ostringstream out;
  out << "r400: " << animals_.size() << " original chickens · "
      << meshes_[0].triangles << " triangles per animal · 19 joints · " << grounded
      << " SAT floor / " << animals_.size() - grounded
      << " original seated positions";
  return out.str();
}
} // namespace re4
