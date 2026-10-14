#pragma once
#include "world_effects.h"
#include <memory>
namespace re4 {
struct RoomAreaEffectQuad;
struct TrapEffectsPresentationState;
void CreateVillagePropsEffectsGl(const WorldEffectsData &well,const WorldEffectsData &medal);
void DestroyVillagePropsEffectsGl();
void ForgetVillagePropsEffectsGl();
void DrawVillagePropsEffectsGl(const float *matrix,const std::array<float,3> &center,float scale,
    const std::vector<WorldEffectQuad> &well,const std::vector<WorldEffectQuad> &medal);
void CreateProjectileEffectsGl(const WorldEffectsData &data);
void DestroyProjectileEffectsGl();
void ForgetProjectileEffectsGl();
void DrawProjectileEffectsGl(const float *matrix,const std::array<float,3> &center,
    float scale,const std::vector<WorldEffectQuad> &quads);
void CreateVillageBottleEffectsGl(const WorldEffectsData &core,const WorldEffectsData &trail);
void DestroyVillageBottleEffectsGl();
void ForgetVillageBottleEffectsGl();
void DrawVillageBottleEffectsGl(const float *matrix,const std::array<float,3> &center,float scale,
    const std::vector<WorldEffectQuad> &core,const std::vector<WorldEffectQuad> &trail);
void CreateRoomHitEffectsGl(const WorldEffectsData &data);
void DestroyRoomHitEffectsGl();
void ForgetRoomHitEffectsGl();
void DrawRoomHitEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads);
std::shared_ptr<const TrapEffectsPresentationState> SaveTrapEffectsPresentationGl();
bool RestoreTrapEffectsPresentationGl(const std::shared_ptr<const TrapEffectsPresentationState> &state);
void CreateRoomAreaEffectsGl(const WorldEffectsData &data);
void DestroyRoomAreaEffectsGl();
void ForgetRoomAreaEffectsGl();
void DrawRoomAreaEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<RoomAreaEffectQuad> &quads,
                         bool beforeWorld = false);
void CreateItemBillboardsGl(const WorldEffectsData &data);
void DestroyItemBillboardsGl();
void ForgetItemBillboardsGl();
void DrawItemBillboardsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads);
void CreateWorldEffectsGl(const WorldEffectsData &data);
void DestroyWorldEffectsGl();
// Context loss: discard stale GL names without deleting them in a new context.
void ForgetWorldEffectsGl();
void CreateMuzzleEffectsGl(const WorldEffectsData &data);
void DestroyMuzzleEffectsGl();
void ForgetMuzzleEffectsGl();
void CreateEnemyEffectsGl(const WorldEffectsData &data);
void DestroyEnemyEffectsGl();
void ForgetEnemyEffectsGl();
void CreateTrapEffectsGl(const WorldEffectsData &data);
void DestroyTrapEffectsGl();
void ForgetTrapEffectsGl();
void DrawTrapEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads);
void DrawTrapEffectLinesGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectLine> &lines);
void DrawTrapEffectLensesGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const WorldEffectCamera &camera,
                         const std::vector<WorldEffectLens> &lenses,
                         bool cameraCut = false);
// Event EST/EFF resources have their own GPU owner. CPU event state supplies
// the exact source clock; this slot never advances or replaces room effects.
void CreateEventEffectsGl(const WorldEffectsData &data);
void DestroyEventEffectsGl();
void ForgetEventEffectsGl();
void DrawEventEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads,
                         bool beforeWorld = false);
void DrawEnemyEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads);
void DrawMuzzleEffectsGl(const float *matrix, const std::array<float,3> &center,
                         float scale, const std::vector<WorldEffectQuad> &quads);
void DrawWorldEffectsGl(const float *matrix, const std::array<float, 3> &center,
                        float scale, const WorldEffectCamera &camera,
                        double frames);
std::shared_ptr<const WorldEffectsState> SaveWorldEffectsState();
void RestoreWorldEffectsState(
    const std::shared_ptr<const WorldEffectsState> &state);
// Call on the same GL/render thread as DrawWorldEffectsGl. Uses textures loaded
// with the room and records the event in pause/context snapshots.
bool TriggerWorldEffectsGl(unsigned objectId,
                           const std::array<float, 3> &position,
                           const std::array<float, 3> &rotation,
                           std::uint64_t eventSerial);
} // namespace re4
