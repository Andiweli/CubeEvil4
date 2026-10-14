#pragma once
#include "scene.h"
#include "follow_camera.h"
namespace re4 {
void CreateSceneGl(const StaticScene &scene);
void DestroySceneGl(); // Only with the owning current GL context.
void ForgetSceneGl();  // Context lost: discard names, never delete in a new
                       // context.
class SatCollision;
class LeonPreview;
class EnemyEncounter;
class EnemyGroup;
class OriginalEvent;
class AmbientAnimals;
class ObjectActions;
class RoomInteractions;
class RoomPickups;
class DestructionEffectsSimulation;
class MuzzleEffects;
class RoomLadders;
class ForestAnimals;
class VillageAnimals;
class DogTraps;
class R100Secondary;
struct R100RoomMessage;
struct EventCamera;
struct RoomFilterState;
class ItemEffects;
class RoomAreaEffects;
class RoomItemEvents;
struct TrapEffectsPresentationState;
class GenericEventActor;
class VillageBottles;
class PlayerProjectiles;
struct SceneProbe {
  bool gameplayDepthOfField{}; // Opt-in world pass, excludes menus/events.
  const PlayerProjectiles *projectiles=nullptr;
  const VillageBottles *villageBottles=nullptr;
  const GenericEventActor *radioActor=nullptr;
  Matrix34 radioMatrix{1,0,0,0,0,1,0,0,0,0,1,0};
  const SatCollision *collision = nullptr;
  std::array<double, 3> feet{};
  double radius = 300, height = 1600;
  bool blocked{};
  bool showBody = true;
  bool showAimRay = false;
  std::array<double, 3> rayStart{}, rayEnd{}; // Source marker line, no damage.
  bool showAimPoint{};
  std::array<double, 3> aimPoint{};
  const EnemyEncounter *enemy = nullptr;
  const EnemyGroup *enemies = nullptr;
  const OriginalEvent *event = nullptr;
  const R100Secondary *r100Secondary = nullptr;
  const EventCamera *roomCamera = nullptr;
  std::optional<unsigned> roomLightCut;
  const R100RoomMessage *roomMessage = nullptr;
  const EnemyEncounter *houseEnemy = nullptr;
  bool hidePlayer{};
  bool sourceSuspended{};
  std::vector<unsigned> noSuspendEnemies;
  bool radioHand{};
  const AmbientAnimals *animals = nullptr;
  const ForestAnimals *forest = nullptr;
  const VillageAnimals *villageAnimals = nullptr;
  const DogTraps *dogTraps = nullptr;
  const ObjectActions *objects = nullptr;
  const RoomInteractions *interactions = nullptr;
  const RoomLadders *ladders = nullptr;
  const WorldEffectsData *enemyEffectsData = nullptr;
  const WorldEffectsSimulation *enemyEffects = nullptr;
  const WorldEffectsData *trapEffects = nullptr;
  const WorldEffectsState *trapState = nullptr;
  const RoomAreaEffects *roomAreaEffects = nullptr;
  const WorldEffectsData *roomHitData = nullptr;
  const WorldEffectsState *roomHitState = nullptr;
  const RoomPickups *pickups = nullptr;
  const RoomItemEvents *itemEvents = nullptr;
  const ItemEffects *itemEffects = nullptr;
  std::uint32_t roomBlocks=0xffffffffu;
  const DestructionEffectsSimulation *destruction = nullptr;
  const MuzzleEffects *muzzle = nullptr;
  const RoomLighting *coreLighting = nullptr;
  bool followInputAdvanced{}; // Bridge samples look before the player's tick.
  std::array<std::array<double, 3>, 3> groundTriangle{};
};
FollowCameraState SceneFollowState();
void SceneAdvanceFollowInput(double horizontal, double vertical, double seconds);
void SceneSeedFollowView(double relativeYaw, double pitch);
FollowCameraPose SceneFollowPose();
float SceneCameraYaw();
float SceneCameraDistance();
void SceneFollowDistance(float distance);
void SceneFollowHeading(float actorHeading);
void SceneCameraTurn(float angle);
struct SceneViewState {
  float yaw, pitch, distance, followDistance;
  double animationFrames;
  std::shared_ptr<const EnvironmentState> environment;
  FollowCameraState followCamera;
  std::shared_ptr<const WorldEffectsState> worldEffects;
  double followHeading{}; // Body heading independent of diagnostic view yaw.
  unsigned footShadowColor{}; // cPlayer::Shd_color; 255 hides the pass.
  double footShadowFrame{};
  std::shared_ptr<const TrapEffectsPresentationState> trapPresentation;
  bool sourceAlphaDraw2{};
  std::shared_ptr<const RoomFilterState> roomFilter;
  RoomLightTransition lightTransition;
};
SceneViewState SaveSceneView();
void RestoreSceneView(const SceneViewState &state);
void DrawSceneGl(int width, int height, float horizontal, float vertical,
                 float zoom, bool normalView = false,
                 const SceneProbe *probe = nullptr,
                 const LeonPreview *actor = nullptr, double seconds = -1);
} // namespace re4
