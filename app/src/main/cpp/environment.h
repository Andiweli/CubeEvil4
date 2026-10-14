#pragma once
#include "smd.h"
#include "smx.h"
#include "texture.h"
#include <memory>
#include <optional>
#include <functional>
namespace re4 {
class RoomArchive;
class SatCollision;
struct StaticScene;
struct SkyObject {
  unsigned work{};
  SmdPlacement placement;
  std::array<float, 3> joint{}, speed{};
  bool rotateJoint{};
};
struct WaterAnimation {
  unsigned id{}, width{}, height{}, loop{};
  int centerX{}, centerY{};
  std::vector<unsigned> pattern, duration;
  std::vector<Texture> images;
};
// Selected immutable GC cEspSeqTbl bytes. Never relocate GC pointers.
struct WaterRecord {
  std::array<std::uint8_t, 300> bytes{};
  unsigned animation{};
  bool enabled = true; // Unsupported id: retain original bytes, never guess.
  unsigned externalSeedDraws{}; // Real cObj05 backend shares EST's sequence seed.
};
struct WaterTarget {
  unsigned width = 128, height = 128, sequence{};
  std::vector<WaterRecord> records;
};
struct WaterSurface {
  unsigned work{};
  int target = -1; // Missing/unmatched effect: preserve authored base texture.
  unsigned colorAlpha = 255, shader{}, refractPower{}, refractRatio = 255;
  bool textureAlpha{};
};
// Espgen42/Espgen45 create their geometry from EFF, not SMD. Keep authored
// grid dimensions/placement and the 60 room 0xfe I8 noise frames immutable.
struct WaterGrid {
  unsigned generator{}, sequence{}, width{}, height{}, type{}, reflection{};
  unsigned prerun{};
  float size{}, originalSize{}, heightRate{}, damping = .05f, spread = .95f;
  std::array<float, 3> position{}, angle{};
  std::array<std::uint8_t, 4> color{}, ambient{};
  float shimmerS{}, shimmerT{};
  bool bounded{}, masked{};
  bool followCamera = true; // Esp4c supplies fixed x/z and size where authored.
  Texture specular, mask;
};
struct WaterGridState {
  std::uint64_t frames{};
  std::vector<float> position, speed, height;
  std::vector<std::array<float, 3>> normals;
};
struct WaterSprite;
struct EnvironmentState;
struct EnvironmentData {
  std::uint16_t room{};
  std::uint64_t signature{};
  std::vector<SkyObject> sky;
  std::vector<WaterAnimation> animations;
  std::vector<WaterTarget> targets;
  std::vector<WaterSurface> surfaces;
  std::vector<WaterGrid> grids;
  std::vector<std::vector<std::uint8_t>> gridNoise; // raw GX I8 64x64 byte order
  std::vector<std::string> warnings;
  bool sourceSequenceSeeds{}; // original EstSet consumes three visual Rnd bytes
  std::uint16_t initialRandom = 0x4e22;
  std::array<float, 12> spawnMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  bool
      transformSpawn{}; // Explicit EstSet matrix for Parts_no FF world effects.
  // Event-only authored parts parents and espgen02 path placement. Matrices
  // are refreshed after the original actor pose; empty tables keep room paths.
  std::vector<std::vector<std::optional<std::array<float,12>>>> recordParents;
  std::vector<std::vector<std::optional<std::array<float,12>>>> recordEmitMatrices;
  // Fixed source PATH vertices (xyz + cumulative length) for espgen02.
  std::vector<std::vector<std::vector<std::array<float,4>>>> recordPaths;
  // Independent source adapters can supply original SetFreeWork/move bodies
  // for their authored special IDs while sharing the original sprite pool.
  std::function<bool(WaterSprite &,const WaterRecord &,std::uint32_t &)> spriteInitializer;
  std::function<std::optional<bool>(WaterSprite &,const WaterRecord &,EnvironmentState &)> spriteMover;
  std::string describe() const;
};
// Room scripts r100/r300/r301 + immutable SMD/SMX/EFF. Unsupported sequences
// remain a diagnosed base-texture fallback, never a guessed effect id.
EnvironmentData BuildRoomEnvironment(const RoomArchive &room, const Smd &smd,
                                     const Smx &smx, std::uint16_t id,
                                     const std::vector<std::uint8_t> &coreEff);
void AttachSceneEnvironment(const EnvironmentData &data, StaticScene &scene);
using EnvironmentMatrix = std::array<float, 12>;
EnvironmentMatrix SkyDeltaMatrix(const SkyObject &object,
                                 const std::array<float, 3> &angle);
struct WaterSprite {
  unsigned target{}, record{}, flags{}, pattern{}, animationCounter{}, age{};
  std::uint64_t serial{};
  std::array<float, 3> position{}, speed{}, acceleration{}, angle{},
      angleSpeed{};
  std::array<float, 4> color{};
  std::array<float, 2> size{}, scroll{};
  std::array<float, 3> cameraOffset{}; // Esp0b offset removed before CommonMove
  float sizeMultiplier = 1, sizeStep{}, startAlpha{};
  // Esp48's independent oscillation phase and the previous added angle.
  float sinePhase{};
  std::array<float,3> sineOffset{};
  // Esp0a type0 creates frozen id0 copies; its record still identifies the
  // source controller, so retain the copy's overridden life explicitly.
  bool motionTrailGhost{};
  unsigned motionTrailLife{};
  bool parented{};
  std::vector<std::array<float,3>> history; // Esp12 recent world positions.
  // Esp09 GX line width is stored in sixth-pixel units after source distance
  // attenuation. Esp07 HitType0 caches its first original SAT floor probe.
  unsigned lineWidth{};
  float bounceFloor{};
  unsigned bounceFlags{};
};
void ApplyEnvironmentSpriteMatrix(WaterSprite &sprite,
                                 const EnvironmentMatrix &matrix);
struct WaterEmitter {
  unsigned target{}, record{}, next{}, interval{};
  std::uint32_t seed{};
  unsigned age{}; // Espgen00 Time_cnt / finite lifetime and rate curves.
};
struct EnvironmentExternalSpawn {
  unsigned target{}, record{};
  std::uint32_t seed{};
  std::uint64_t serial{};
};
struct EnvironmentState {
  std::uint64_t signature{}, frames{}, serial{};
  std::uint16_t random = 0x4e22; // Visual preview stream; never combat Rnd().
  std::vector<std::array<float, 3>> skyAngles;
  std::vector<unsigned> recordCursor;
  std::vector<std::uint32_t> sequenceSeed;
  std::vector<WaterEmitter> emitters;
  std::vector<WaterSprite> sprites;
  std::vector<WaterGridState> grids;
  std::uint64_t externalSerial{};
  std::vector<EnvironmentExternalSpawn> externalSpawns;
};
struct EnvironmentCamera {
  std::array<float, 3> eye{}, up{0, 1, 0};
};
class EnvironmentSimulation {
public:
  explicit EnvironmentSimulation(const EnvironmentData &data,
                                  const SatCollision *floor = nullptr);
  void tick(const EnvironmentCamera *camera = nullptr,
            unsigned spriteBudget = 512,
            const SatCollision *floor = nullptr); // Original30Hz.
  // A room has multiple independent EST controllers in one global pool.
  // Original EspgenMove precedes EspMove for the entire pool each frame.
  void tickGenerators(const EnvironmentCamera *camera = nullptr,
                      unsigned spriteBudget = 512,const SatCollision *floor = nullptr);
  void tickSprites(const EnvironmentCamera *camera = nullptr,
                   const SatCollision *floor = nullptr);
  const EnvironmentState &state() const { return state_; }
  // Source SetFreeWork bodies can consume the same visual Rnd between
  // generator emissions. This accessor adds no instance fields or layout.
  std::uint16_t &visualRandom() { return state_.random; }
  bool restore(const EnvironmentState &state);
  std::vector<EnvironmentExternalSpawn> drainExternalSpawns();

private:
  const EnvironmentData *data_;
  EnvironmentState state_;
  unsigned spriteBudget_ = 512;
  const SatCollision *floor_{}; // Borrowed only during constructor/tick.
  void activate();
  bool spawn(unsigned target, unsigned record, std::uint32_t &seed,
             bool spread = false, float spreadAngle = 0);
  void tickGrid(unsigned index);
};
struct WaterGridVertex {
  std::array<float, 3> position{}, normal{};
  std::array<float, 2> uv{};
};
// Original zigzag triangle strip and Espgen45's four 15x border quads.
std::vector<WaterGridVertex> WaterGridMesh(const WaterGrid &grid,
                                        const WaterGridState &state,
                                        const std::array<float, 3> &cameraTarget);
struct EnvironmentWaterContact {
  float height{}, depth{};
  unsigned grid{}, generator{};
  bool footImmersed{};
  // EspPlWaterCall uses a strict 90-unit immersion test at the sound's
  // authored foot/root point. Rendering the water does not imply contact.
  bool footSound() const { return footImmersed; }
};
// Source Espgen42.cpp::GetWaterHeight: the authored water plane, inclusive
// raster bounds and room Espgen42SetNoWater flags; never a guessed SAT tag.
std::optional<EnvironmentWaterContact> QueryEnvironmentWaterContact(
    const EnvironmentData &data, const std::array<double, 3> &point,
    const std::array<float, 3> &cameraTarget = {});
// Original esp08/esp47 quad geometry in the 512 x 448 effect screen.
struct WaterQuad {
  unsigned animation{}, image{}, blend{}, target{}, part{};
  std::uint64_t serial{};
  std::array<std::array<float, 2>, 4> position{}, uv{};
  std::array<float, 4> color{};
  std::array<float, 2> repeats{1, 1}, scroll{};
  bool tiled{}, flipS{}, flipT{}, updateAlpha{};
  float rgbScale = 1, alphaScale = 1;
};
std::vector<WaterQuad> WaterQuads(const EnvironmentData &data,
                                  const EnvironmentState &state);
} // namespace re4
