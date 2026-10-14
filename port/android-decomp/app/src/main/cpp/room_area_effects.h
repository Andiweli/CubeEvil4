#pragma once
#include "world_effects.h"
#include "room_effect_cloth.h"
namespace re4 {
class RoomArchive;
struct RoomEffectArea {
  unsigned number{}, flags{};
  std::array<std::uint8_t,48> source{};
};
bool OriginalEffectAreaContains(const RoomEffectArea &area,
                               const std::array<float,3> &position);
struct RoomAreaEffectsData {
  struct Bank {unsigned id{}, area{}, target{};};
  WorldEffectsData effects;
  std::vector<RoomEffectArea> areas;
  std::vector<Bank> banks;
  std::vector<std::vector<std::optional<unsigned>>> maskAnimations;
  std::vector<std::string> warnings;
};
RoomAreaEffectsData BuildRoomAreaEffects(const RoomArchive &room,
    std::uint16_t roomId,const std::vector<std::uint8_t> &core);
struct RoomAreaEffectQuad {
  WorldEffectQuad quad;
  std::optional<unsigned> maskAnimation;
  unsigned maskImage{};
  bool maskStretched{};
  float maskAlphaScale=1;
  unsigned shimmerType{},shimmerPower{};
};
// Source EffAreaUpdate / EffSetAreaState / SetSstDispFlag. Player coordinates
// receive the original +100 offset; camera-area mode uses the raw camera eye.
class RoomAreaEffects {
public:
  RoomAreaEffects(const RoomAreaEffectsData &data,
                  std::uint32_t displayMask=0xffffffffu,
                  const WorldEffectCamera *camera=nullptr);
  ~RoomAreaEffects();
  void tick(const std::array<float,3> &player,const WorldEffectCamera &camera,
            std::uint32_t displayMask,std::uint32_t addAreaMask=0,
            bool useCamera=false);
  std::uint32_t areaMask() const;
  bool inRoom() const;
  const WorldEffectsState &state() const;
  std::vector<RoomAreaEffectQuad> quads(const WorldEffectCamera &camera) const;
  std::vector<RoomEffectClothMesh> clothMeshes() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace re4
