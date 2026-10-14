#pragma once
#include "collision_query.h"
#include "scene.h"
#include <memory>

namespace re4 {
class RoomArchive;
// Bounded original Efm05 subset: ET01/ET02 crates, ET11 wooden barrels and
// ET2D explosive barrel fragments and ET00/ET07/ET25 glass/wood windows.
// Every fragment is an original BIN part with its original TPL; no generated
// shard geometry. Other sequence sprites/sounds remain separate backends.
struct DestructionMesh {
  unsigned efm{}, part{};
  std::array<float, 3> restCenter{};
  StaticScene mesh; // Vertices relative to this part's original bind centre.
};
struct DestructionModel {
  unsigned efm{}, firstMesh{}, parts{};
};
struct DestructionRecord {
  unsigned objectId{}, sequence{}, sourceRecord{}, model{};
  std::array<std::uint8_t, 300> bytes{};
};
struct DestructionEffectsData {
  std::uint16_t room{};
  std::uint64_t signature{};
  std::vector<DestructionMesh> meshes;
  std::vector<DestructionModel> models;
  std::vector<DestructionRecord> records;
  std::vector<std::string> warnings;
  std::string describe() const;
};
DestructionEffectsData
BuildDestructionEffects(const RoomArchive &room,
                        const std::vector<std::uint8_t> &coreEff,
                        std::uint16_t roomId);
// Original EM2A blast/bear-snap Kaboom records and CORE model8/model14.
DestructionEffectsData BuildTrapDestructionEffects(
    const std::vector<std::uint8_t> &em2aEff,
    const std::vector<std::uint8_t> &coreEff,
    std::uint16_t roomId=0x100);
DestructionEffectsData BuildEventDestructionEffects(const std::vector<std::uint8_t> &room,
    const std::vector<std::uint8_t> &core,const std::vector<unsigned> &sequences,
    const std::vector<std::vector<std::uint8_t>> &banks={});
// Merge immutable source owners before constructing their shared simulation
// or appending the scene. Model/mesh indices are rebased transactionally.
void AppendDestructionData(DestructionEffectsData &destination,
                           const DestructionEffectsData &source);
// Append immutable local geometry once. Runtime SceneGL instances supply its
// world transform; these hidden-by-default draws do not expand room bounds.
void AppendDestructionScene(StaticScene &scene, const DestructionEffectsData &data);
struct DestructionPartState {
  std::array<float, 3> position{}, world{}, rotation{}, speed{}, angularSpeed{};
  unsigned flag{}; // Original Kaboom_flg: waiting / flying / resting.
};
struct DestructionModelState {
  unsigned record{}, slot{}, age{};
  std::uint64_t serial{};
  std::uint32_t seed{};
  std::array<float, 3> position{}, rotation{}, angularSpeed{}, burstCenter{};
  std::array<float, 4> color{};
  float sizeX{}, sizeY{}, sizeMultiplier{1}, sizeStep{};
  Matrix34 matrix{}; // Previous original parent transform for part physics.
  std::vector<DestructionPartState> parts;
};
struct DestructionEmitterState {
  unsigned record{}, slot{}, age{}, next{};
  std::uint32_t seed{};
  Matrix34 placement{};
  unsigned interval{}; // Source Next_max includes its one-time R_inter offset.
};
struct DestructionEventKey {
  unsigned slot{};
  std::uint64_t serial{}, spawnSerial{};
};
struct DestructionPendingRecord {
  unsigned record{}, slot{}, remaining{};
  std::uint32_t seed{};
  Matrix34 placement{};
};
struct DestructionEffectsState {
  std::uint64_t signature{}, frames{}, serial{};
  std::uint16_t random{0x4e22}; // Isolated visual Rnd stream; never combat Rnd.
  std::vector<DestructionEventKey> events;
  std::vector<DestructionEmitterState> emitters;
  std::vector<DestructionPendingRecord> pending;
  std::vector<DestructionModelState> models;
};
struct DestructionInstance {
  unsigned mesh{}, slot{}, blend{}, zMode{};
  std::uint64_t serial{};
  Matrix34 matrix{};
  std::array<float, 4> color{}; // Original model color/invisible_factor, 0..1.
};
class DestructionEffectsSimulation {
public:
  // Collision is borrowed and must outlive this simulation. The source floor
  // call is EatMgr.getFloor(above=600, below=100000); this adapter queries the
  // existing immutable effect-mode SAT ground subset, without dynamic EAT.
  // Tool_flg2 metal fragments use the separate original sight-mode SAT
  // segment query and normalized source reflection; it precedes floor checks.
  explicit DestructionEffectsSimulation(const DestructionEffectsData &data,
                                       const SatCollision *collision = nullptr,
                                       const SatCollision *scenario = nullptr);
  // Source breakKind: 0 handgun/knife, 1 near shotgun, 2 high power/bomb.
  // An accepted event is applied
  // once, including both its flying sequence and original resting fallback.
  bool trigger(unsigned objectId, unsigned boxType, unsigned slot,
               const std::array<float, 3> &position,
               const std::array<float, 3> &rotation, unsigned breakKind,
               std::uint64_t eventSerial, std::uint32_t seed);
  // emwindow::SetBreakEsp has its own direction/size/event EST table;
  // it must not use embox's resting fallback or projectile sequence rules.
  bool triggerWindow(unsigned objectId, unsigned sequence, unsigned slot,
                     const std::array<float, 3> &position,
                     const std::array<float, 3> &rotation,
                     std::uint64_t eventSerial, std::uint32_t seed);
  bool triggerTrap(unsigned sequence,unsigned slot,
                   const std::vector<Matrix34> &posedWorldParts,
                   std::uint64_t eventSerial,std::uint32_t seed);
  // Mixed EST owns the original generator and calls this once per emitted FE
  // record, with the seed captured before EfmSetObj05's 13 initialization draws.
  bool triggerOriginalRecord(unsigned owner,unsigned sequence,unsigned sourceRecord,unsigned slot,
      const Matrix34 &placement,std::uint64_t eventSerial,std::uint64_t spawnSerial,std::uint32_t seed);
  bool triggerTrapRecord(unsigned sequence,unsigned sourceRecord,unsigned slot,
                         const Matrix34 &placement,std::uint64_t eventSerial,
                         std::uint64_t spawnSerial,std::uint32_t seed);
  // Original setEff on room re-entry: resting fallback only, no new explosion.
  bool restoreBroken(unsigned objectId, unsigned boxType, unsigned slot,
                     const std::array<float, 3> &position,
                     const std::array<float, 3> &rotation);
  void tick(); // Original ObjMgr then Espgen order, fixed 30 Hz.
  const DestructionEffectsState &state() const { return state_; }
  bool restore(const DestructionEffectsState &state);
  std::vector<DestructionInstance> instances() const;
private:
  const DestructionEffectsData *data_;
  const SatCollision *collision_;
  const SatCollision *scenario_;
  DestructionEffectsState state_;
  void sequence(unsigned objectId, unsigned sequence, unsigned slot,
                const Matrix34 &placement, std::uint32_t seed);
  void spawn(unsigned record, unsigned slot, const Matrix34 &placement,
             std::uint32_t &seed);
  void activate(unsigned record, unsigned slot, const Matrix34 &placement,
                std::uint32_t &seed);
  bool move(DestructionModelState &model);
};
} // namespace re4
