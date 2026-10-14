#pragma once
#include "environment.h"
#include <optional>
namespace re4 {
class RoomArchive;
struct WorldEffectCamera {
  std::array<float, 3> eye{}, right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, -1};
};
// The GC world sprites share CommonMove/AnmMove with the existing texture-pass
// effects. Reuse that tested 30 Hz CPU engine; world geometry is separate.
struct WorldEffectsData {
  struct TriggerBank {
    unsigned objectId{}, sequence{}, target{}, unsupported{};
  };
  EnvironmentData sprites;
  std::vector<unsigned> sequences;
  std::vector<unsigned> targetPrerun; // SstSet: 200; R101Init EstSet: 0
  bool originalPyre{};
  bool originalEnemyBlood{};
  unsigned triggerInstanceLimit = 8;
  std::vector<TriggerBank> triggerBanks;
  std::optional<WaterRecord> weaponLight;
  unsigned weaponPart=3; // Authored EST parent: e.g. Mauser5/TMP2/Magnum4.
  struct EventModelRecord { unsigned target{}, record{}; WaterRecord source; };
  // EfmSeqSet idFC resolves SmdGetGroupObjPtr(Tex_id), not an ANM texture.
  std::vector<EventModelRecord> eventModels;
  // IdFE is owned by the real cObj05 destruction simulation; these records
  // retain their original target/index for cross-backend source validation.
  std::vector<EventModelRecord> fragmentRecords;
  std::string describe() const;
};
// Shared immutable original-EFF readers for independently owned source
// adapters. They preserve authored records and registration order.
std::vector<WaterRecord> ReadOriginalEffectSequence(
    const std::vector<std::uint8_t> &eff,unsigned sequence);
std::vector<std::array<float,4>> ReadOriginalEffectPath(
    const std::vector<std::uint8_t> &eff,unsigned path);
void RegisterOriginalEffectAnimation(EnvironmentData &data,
    const std::vector<std::uint8_t> &eff,const std::vector<std::uint8_t> &core,
    WaterRecord &record);
WorldEffectsData
BuildRoomWorldEffects(const RoomArchive &room, std::uint16_t roomId,
                      const std::vector<std::uint8_t> &coreEff);
// WEP01/WEP02/WEP07 EstSet(id0), original detached Esp00 flash/smoke.
// Esp11 carries its immutable CORE-cut light record; Esp03 streaks stay
// explicitly diagnosed separately.
WorldEffectsData BuildWeaponWorldEffects(const std::vector<std::uint8_t> &weaponEff,
                                        const std::vector<std::uint8_t> &coreEff);
// Original cEm10's unarmoured pistol/knife/shotgun/rifle/machine blood EST banks.
// Esp10 floor decals require the room SAT on trigger/tick. Esp01 replays its
// original strip path; unresolved shimmer layers are explicitly diagnosed.
WorldEffectsData BuildEnemyWorldEffects(const std::vector<std::uint8_t> &enemyEff,
                                       const std::vector<std::uint8_t> &coreEff);
WorldEffectsData BuildTrapWorldEffects(const std::vector<std::uint8_t> &enemyEff,
                                      const std::vector<std::uint8_t> &coreEff);
// EventCutEstSet uses decimal cut numbers encoded as BCD EST identifiers.
// Parent indices address SetOm registration order, not SetParts attachments.
WorldEffectsData BuildEventWorldEffects(const std::vector<std::uint8_t> &eventEff,
                                       const std::vector<std::uint8_t> &coreEff,
                                       const std::vector<std::uint8_t> &roomEff = {},
    const std::vector<unsigned> &onlySequences = {},
    const std::vector<std::vector<std::uint8_t>> &modelBanks = {});
struct WorldEffectFragmentSpawn {
  unsigned sequence{}, sourceRecord{};
  std::uint64_t eventSerial{}, spawnSerial{};
  std::uint32_t seed{};
  EnvironmentMatrix placement{};
};
struct WorldEffectsState : EnvironmentState {
  struct Instance {
    unsigned bank{};
    std::uint64_t eventSerial{}, startFrame{};
    std::array<float, 3> position{}, rotation{};
    std::vector<EnvironmentMatrix> sourceWorldParts;
    EnvironmentState state;
  };
  std::vector<EnvironmentState>
      streams; // independent source clocks / RNG / pool state
  std::vector<Instance> instances;
  std::vector<std::uint64_t> acceptedEvents;
  std::uint16_t eventRandom = 0x4e22;
  std::vector<WorldEffectFragmentSpawn> pendingFragments;
};
class WorldEffectsSimulation {
public:
  explicit WorldEffectsSimulation(const WorldEffectsData &data);
  ~WorldEffectsSimulation();
  void tick(const WorldEffectCamera *camera = nullptr,
            const SatCollision *floor = nullptr);
  // Immutable ETM EST0 bank, original explicit world placement. Duplicate
  // event serials do not restart the sequence; no pre-run on this EstSet.
  bool trigger(unsigned objectId, const std::array<float, 3> &position,
               const std::array<float, 3> &rotation, std::uint64_t eventSerial,
               const SatCollision *floor = nullptr);
  bool triggerAttached(unsigned sequence, const std::array<float,3> &position,
                       const std::array<float,3> &rotation,
                       std::uint64_t eventSerial,
                       const std::vector<EnvironmentMatrix> &sourceWorldParts,
                       const SatCollision *floor = nullptr);
  bool updateInstanceParts(std::uint64_t eventSerial,
                           const std::vector<EnvironmentMatrix> &sourceWorldParts);
  bool stopInstance(std::uint64_t eventSerial);
  std::vector<WorldEffectFragmentSpawn> drainFragmentSpawns();
  const WorldEffectsState &state() const;
  bool restore(const WorldEffectsState &state);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct WorldEffectQuad {
  unsigned animation{}, image{}, blend{}, sequence{}, record{};
  std::uint64_t serial{};
  std::array<std::array<float, 3>, 4> position{};
  std::array<std::array<float, 2>, 4> uv{};
  std::array<float, 4> color{};
  std::array<float, 2> repeats{1, 1}, scroll{};
  bool tiled{}, flipS{}, flipT{};
  unsigned flags{}, type{};
  bool screen{}; // Parts F8..FD: original 512x448 orthographic coordinates.
  float rgbScale = 1, alphaScale = 1, depth{};
  float sourceAlpha{}, blurRate{}; // Esp18 uses unquantized CommonMove alpha.
};
struct WorldEffectModel {
  unsigned model{}, lightMask{}, target{}, record{};
  std::uint32_t selectMask=0xffffffffu;
  std::uint64_t serial{};
  EnvironmentMatrix matrix{};
  std::array<float,4> color{};
};
std::vector<WorldEffectModel> WorldEffectModels(const WorldEffectsData &data,
                                               const WorldEffectsState &state);
struct WorldEffectLight {
  unsigned kind{}, cut{}, number{};
  std::uint64_t serial{};
  std::array<float,3> position{};
  float radius{};
  std::array<float,4> color{};
};
std::vector<WorldEffectLight> WorldEffectLights(const WorldEffectsData &data,
                                               const WorldEffectsState &state);
struct WorldEffectLine {
  std::uint64_t serial{}, sourceSignature{};
  unsigned flags{}, blend{}, sourceAge{}, width{};
  float rgbScale=1;
  std::vector<std::array<float,3>> position;
  std::vector<std::array<float,4>> color;
};
std::vector<WorldEffectLine> WorldEffectLines(const WorldEffectsData &data,
                                              const WorldEffectsState &state);
struct WorldEffectLens {
  std::uint64_t serial{}, sourceSignature{};
  unsigned animation{}, image{}, flags{}, blend{}, sourceAge{};
  std::array<float,3> position{}, angle{};
  std::array<float,2> size{}, pivotNormalized{};
  std::array<float,4> color{};
  float sourceAlpha{}, centerRatio{}, sizeRatio{}, delDistance{}, hideRadius{};
  float rgbScale=1, alphaScale=1;
};
std::vector<WorldEffectLens> WorldEffectLenses(const WorldEffectsData &data,
                                              const WorldEffectsState &state);
// Esp46_Trans publishes a Filter03 alpha-buffer glow request, never a sprite.
// The main renderer must supply the original alpha pass before compositing it.
struct WorldEffectGlowRequest {
  int level{};
  unsigned priority{}, special{}, sequence{};
  std::uint64_t serial{};
  std::array<std::uint8_t, 3> color{};
};
std::vector<WorldEffectGlowRequest>
WorldEffectGlowRequests(const WorldEffectsData &data,
                        const WorldEffectsState &state);
// Returns world coordinates, including authored orientation (Tool_flg bit 0),
// animation pivot, original blend/color envelope and tiled Esp08 UV state.
// Back-to-front ordering; draw with LEQUAL, no depth writes and no culling.
std::vector<WorldEffectQuad> WorldEffectQuads(const WorldEffectsData &data,
                                              const WorldEffectsState &state,
                                              const WorldEffectCamera &camera);
} // namespace re4
