#pragma once
#include "enemy_spawn.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include <memory>

namespace re4 {
// Bounded r400 village adapter. It owns the original six EM28 chickens,
// original model/texture variants and the Wait/Walk motion subset. Campaign
// spawning, eggs, flight, damage, sounds and the bell are not installed here.
enum class AnimalMotion { Idle, Peck, PeckAlt, Walk, WalkAlt };
struct AnimalMotionSource {
  unsigned motionEntry{}, blendFrames{};
  std::uint16_t flags{};
};
AnimalMotionSource ChickenMotionSource(AnimalMotion motion);
// Select active unclaimed EM28 records in authored order. Unsupported EM ids
// are not substituted with chickens; all source flags and bytes are retained.
std::vector<EnemySpawn> VillageChickenSpawns(const EnemySpawns &spawns);

class AmbientAnimal {
public:
  AmbientAnimal(AmbientAnimal &&) noexcept;
  AmbientAnimal &operator=(AmbientAnimal &&) noexcept;
  ~AmbientAnimal();
  AmbientAnimal(const AmbientAnimal &) = delete;
  AmbientAnimal &operator=(const AmbientAnimal &) = delete;
  const EnemySpawn &source() const { return source_; }
  unsigned variant() const { return source_.type; }
  CollisionVec feet() const;
  float heading() const { return heading_; }
  Matrix34 placement() const;
  const AnimationRig &rig() const { return *rig_; }
  AnimalMotion motion() const { return motion_; }
  // Perches without matching static SAT retain their authored placement.
  // They animate in place rather than snapping to a lower unrelated floor.
  bool onOriginalFloor() const { return probe_->ready(); }

private:
  friend class AmbientAnimals;
  AmbientAnimal(const SatCollision &sat, const EnemySpawn &source,
                const SkinnedModel &body,
                const std::vector<AnimationBytes> &clips);
  void start(AnimalMotion motion);
  void tick();
  EnemySpawn source_;
  std::unique_ptr<WalkProbe> probe_;
  std::unique_ptr<AnimationRig> rig_;
  AnimalMotion motion_{AnimalMotion::Idle};
  float heading_{}, targetHeading_{};
  unsigned loops_{}, stuck_{};
};

class AmbientAnimals {
public:
  AmbientAnimals(const SatCollision &sat,
                 const std::vector<std::uint8_t> &omake01Esl,
                 const std::string &em28Drs);
  const std::vector<AmbientAnimal> &animals() const { return animals_; }
  const StaticScene &mesh(unsigned variant) const;
  const std::vector<SkinCorner> &skin() const { return skin_; }
  // Pure original 30-Hz motion/state ticks, independent of render rate.
  void step(double seconds);
  void suspendClock() { accumulator_ = 0; }
  std::uint64_t ticks() const { return ticks_; }
  std::string describe() const;

private:
  std::array<StaticScene, 2> meshes_;
  std::vector<SkinCorner> skin_;
  std::vector<AmbientAnimal> animals_;
  double accumulator_{};
  std::uint64_t ticks_{};
  std::uint16_t randomState_{0x2840};
};
} // namespace re4
