#pragma once
#include "texture.h"
#include <memory>
#include <array>
namespace re4 {
struct PlayerShot;
class SatCollision;
class EnemyEncounter;
class EnemyGroup;
class ObjectActions;
class RoomInteractions;
class ForestAnimals;
class DogTraps;
class VillageAnimals;
struct LaserSightData {
  Texture pointTexture;
  std::array<float, 2> pointSize{};
  std::array<float, 4> pointColor{};
  std::array<float, 4> lineColor{};
  double maxLineLength{};
  unsigned blend{};
};
// EFF_CORE EST3 (Esp19 line) and EST0x50 select0 (Esp00 point), no assets
// bundled. Selection consumes no visual/combat RNG and has no simulation clock.
LaserSightData BuildLaserSightData(const std::vector<std::uint8_t> &coreEff);
struct LaserSightView {
  bool line{}, point{};
  std::array<double, 3> lineStart{}, lineEnd{}, target{};
  // GetWepTargetPos's winning collision result. Surface attributes belong
  // only to result1; keeping the winner prevents a wall behind an actor
  // from incorrectly suppressing firing.
  unsigned hitResult{};
  std::uint32_t attributes{};
  bool targetDontFire{};
  double distanceSquared{};
};
LaserSightView ResolveLaserSight(const PlayerShot &marker,
                                 const SatCollision *effect,
                                 const EnemyEncounter *enemy,
                                 const ObjectActions *objects,
                                 const RoomInteractions *interiors,
                                 double maxLineLength, const EnemyGroup *group = nullptr,
                                 const ForestAnimals *forest = nullptr,
                                 const DogTraps *traps = nullptr,
                                 const VillageAnimals *village = nullptr);
} // namespace re4
