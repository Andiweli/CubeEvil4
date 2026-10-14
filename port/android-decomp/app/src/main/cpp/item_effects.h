#pragma once
#include "scene.h"
#include <map>
#include <memory>

namespace re4 {
// sceAtItemEffSet receives item_pos, which already includes the source
// SceAtCreateItemAt +100 Y (or +1000 for effect8). Pickup/model visibility
// is owned by the area runtime, independently of this effect owner.
struct ItemEffectSource {
  std::uint64_t key{};
  unsigned id{};
  int effectType{};
  std::array<float,3> position{},offset{};
  bool disappearing{},onModel{};
  std::vector<EnvironmentMatrix> parentParts;
};
struct ItemEffectRequest {
  unsigned sequence{};
  std::array<float,3> position{};
  bool attached{};
};
unsigned OriginalItemEffectType(unsigned id);
// Runtime -1/0/6 auto-select colour; static0 stays disabled and static6
// auto-selects. This distinction is present in the original source.
unsigned ResolveOriginalItemEffectType(unsigned id,int effect,bool runtime);
std::vector<ItemEffectRequest> OriginalItemEffectRequests(
    const ItemEffectSource &source,bool shootingMode=false);
WorldEffectsData BuildCoreItemWorldEffects(const std::vector<std::uint8_t> &coreEff);
std::map<unsigned,StaticScene> BuildCoreItemEffectModels(
    const std::vector<std::uint8_t> &coreEff);
struct ItemEffectModel {
  WorldEffectModel model;
  unsigned blend{},zMode{};
};
struct ItemEffectsState {
  struct Instance {
    std::uint64_t owner{},serial{};
    unsigned sequence{};
    std::array<float,3> position{};
    std::vector<EnvironmentMatrix> parentParts;
    EnvironmentState state;
    struct Model {
      unsigned target{},record{};
      std::uint64_t serial{};
      WaterSprite state;
    };
    std::vector<Model> models;
    std::vector<WaterSprite> pathSprites;
    std::vector<EnvironmentExternalSpawn> pending;
  };
  std::uint64_t signature{},ticks{},serial{};
  std::uint16_t random{0x4e22};
  std::vector<ItemEffectSource> sources;
  std::vector<Instance> instances;
};
// Immutable CORE banks and original 30Hz EST clocks. Each original item
// kind owns all its nested Esp0c sets and Efm04 models; removing a source
// removes those together, as sceAtItemEffDelete's three source deletions.
class ItemEffects {
public:
  explicit ItemEffects(const std::vector<std::uint8_t> &coreEff);
  ~ItemEffects();
  ItemEffects(const ItemEffects&)=delete;
  const WorldEffectsData &data() const;
  const std::map<unsigned,StaticScene> &modelMeshes() const;
  void sync(const std::vector<ItemEffectSource> &sources,
            const SatCollision *floor=nullptr,bool shootingMode=false);
  void tick(const WorldEffectCamera *camera=nullptr,
            const SatCollision *floor=nullptr);
  std::vector<WorldEffectQuad> quads(const WorldEffectCamera &camera) const;
  std::vector<ItemEffectModel> effectModels() const;
  ItemEffectsState snapshot() const;
  bool restore(const ItemEffectsState &state,const SatCollision *floor=nullptr);
  std::uint64_t ticks() const;
  std::size_t sourceCount() const;
  std::size_t instanceCount() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace re4
