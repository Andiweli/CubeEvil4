#pragma once
#include "world_effects.h"
#include "model_skin.h"
#include "room_lighting.h"
namespace re4 {
struct MuzzleEffectsInstanceState {
  unsigned serial{}, age{};
  EnvironmentState state;
  Matrix34 parent{1,0,0,0,0,1,0,0,0,0,1,0};
  // EstSet needs the solved weapon part3 matrix before it emits frame zero.
  // A queued shot may survive a pause before that first simulation tick.
  bool pending = true;
};
struct MuzzleEffectsState {
  std::uint64_t signature{};
  unsigned lastSerial{};
  std::vector<MuzzleEffectsInstanceState> instances;
};
// Simulation belongs to the 30Hz gameplay owner, independently of renderer
// refresh rate. Parent is the actual posed weapon part3, never Leon's chest.
class MuzzleEffects {
public:
  explicit MuzzleEffects(const WorldEffectsData &data);
  ~MuzzleEffects();
  bool trigger(unsigned shotSerial);
  // Source eff_sys.cpp::EspRoomInit discards live effects. Keep loaded banks
  // and shot deduplication; a same-room EGL restore must not call this.
  void clear();
  void tick(const Matrix34 &weaponPart3);
  std::vector<WorldEffectQuad> quads(const WorldEffectCamera &camera) const;
  std::vector<RoomLight> lights(const RoomLighting &coreLighting) const;
  MuzzleEffectsState state() const;
  bool restore(const MuzzleEffectsState &state);
  const WorldEffectsData &data() const { return data_; }
private:
  struct Instance;
  WorldEffectsData data_;
  unsigned lastSerial_{};
  std::vector<std::unique_ptr<Instance>> instances_;
};
}
