#pragma once
#include "world_effects.h"
#include "collision_query.h"
namespace re4 {
struct R100SurfaceEffect {
  unsigned owner{}, sequence{};
  bool operator==(const R100SurfaceEffect &other) const {
    return owner==other.owner && sequence==other.sequence;
  }
};
struct R100AtEffectInfo {
  unsigned flags{};
  std::array<R100SurfaceEffect,8> effects{};
};
std::optional<R100AtEffectInfo> OriginalR100AtEffectInfo(unsigned effectType);
std::optional<R100SurfaceEffect> OriginalR100WeaponSurfaceEffect(
    const R100AtEffectInfo &info,unsigned weapon);
struct R100SurfaceImpactPlan {
  unsigned effectType{},flags{};
  std::array<float,3> rotation{};
  std::vector<R100SurfaceEffect> effects;
  std::optional<unsigned> sound; // Source SndCall block2.
  std::string diagnostic;
};
// Source EatGetEffectType / AtEffInfo::getWepEff / EspSetEatEffect.
// `puddle` is the source FLR eff_type==1 test; normal.y must also exceed .9.
R100SurfaceImpactPlan OriginalR100SurfaceImpact(std::uint32_t attributes,
    const std::array<float,3> &normal,unsigned weapon,bool puddle=false);
WorldEffectsData BuildR100RoomHitEffects(const std::vector<std::uint8_t> &roomEff,
    const std::vector<std::uint8_t> &coreEff);
struct R100SurfaceSound {
  unsigned block=2,number{};
  std::array<float,3> position{};
};
struct R100UnsupportedSurfaceEffect {
  R100SurfaceEffect effect;
  std::array<float,3> position{},rotation{};
  std::uint64_t serial{};
};
// An isolated original EST owner for actual r100 ordinary firearm impacts.
// Source Esp05 uses global visual Rnd, distinct from each EST's LCG seed.
// Esp40's actual FF parents are detached: only SetFreeWork queries water.
class R100RoomHitEffects {
public:
  explicit R100RoomHitEffects(WorldEffectsData data);
  ~R100RoomHitEffects();
  bool trigger(const SatHit &hit,unsigned weapon,std::uint64_t serial,
      const EnvironmentData *water=nullptr,bool puddle=false);
  void tick(const WorldEffectCamera &camera,const EnvironmentData *water=nullptr,
      const SatCollision *floor=nullptr);
  const WorldEffectsData &data() const;
  const WorldEffectsState &state() const;
  std::vector<R100SurfaceSound> drainSounds();
  std::vector<R100UnsupportedSurfaceEffect> drainUnsupported();
  const std::vector<std::string> &warnings() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace re4
