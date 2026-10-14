#pragma once
#include "enemy_preview.h"
#include "ambient_animals.h"
#include "leon_preview.h"
#include "weapon_preview.h"
#include "room_lighting.h"
namespace re4 {
// Immutable CORE palette survives EGL recreation; set only on the GL owner.
void SetActorSpecularBank(std::shared_ptr<const std::vector<Texture>> bank);
struct GenericActorStyle {
  std::array<float,4> color{1,1,1,1};
  unsigned blendMode{},zMode{},alphaOmit=255;
  // cModel::CullMode (trans.cpp): 0 the usual side, 1 the other side, 2 both
  unsigned cullMode{};
  std::uint32_t selectMask=0xffffffffu;
  unsigned projectiveImage{};
  std::array<float,16> projectiveMatrix{};
  bool projectiveMirror{};
  bool shadowMask{};
  bool unlit{};
  int globalIlluminationImage=-1;
  float globalIlluminationScale=.5f;
  unsigned tevScaleGroup{}; // cModel0; player/partner/NPC1.
  std::optional<RoomLightVolume> lightVolume;
};
void CreateActorGl(const LeonPreview &actor);
void DestroyActorGl();
void ForgetActorGl();
void DrawActorGl(const float *camera, const std::array<float, 3> &worldCenter,
                 float worldScale, const LeonPreview &actor, bool normalView,
                 const CollisionVec *resolvedEye = nullptr);
// Independent weapon GPU data, posed through the original right-hand part.
// Destroy/ForgetActorGl also clear this attachment for room/context teardown.
void CreateWeaponGl(const WeaponPreview &weapon);
void DestroyWeaponGl();
void ForgetWeaponGl();
void DrawWeaponGl(const float *camera, const std::array<float, 3> &worldCenter,
                  float worldScale, const WeaponPreview &weapon,
                  const Matrix34 &hand, const Matrix34 &actorPlacement,
                  bool normalView);
void DrawWeaponGl(const float *camera,const std::array<float,3> &worldCenter,
                  float worldScale,const LeonPreview &actor,bool normalView);
// Independent original Ganado mesh, skeleton palette and texture resources.
// Placement is supplied by the encounter; actor/context teardown clears it.
void CreateEnemyGl(const EnemyPreview &enemy);
void DestroyEnemyGl();
void DestroyEnemyGl(const EnemyPreview &enemy);
void ForgetEnemyGl();
void DrawEnemyGl(const float *camera, const std::array<float, 3> &worldCenter,
                 float worldScale, const EnemyPreview &enemy,
                 const Matrix34 &placement, bool normalView);
void DrawEnemyShadowMaskGl(const float *camera,const EnemyPreview &enemy,
                          const Matrix34 &placement,float alpha);
// Stable owner-keyed GPU instances for original event model families. Pose
// matrices use the same model-space parts/bind centers as AnimationRig.
void CreateGenericActorGl(const void *owner, const StaticScene &mesh,
                           const std::vector<SkinCorner> &skin);
// Original FCV shape packets change positions/normals without changing the
// authored strips, material palette or skin weights. Call on meshRevision.
void UpdateGenericActorGl(const void *owner, const StaticScene &mesh);
void DrawGenericActorGl(const void *owner, const float *camera,
                         const std::array<float,3> &worldCenter,float worldScale,
                         const std::vector<Matrix34> &pose,
                         const std::vector<std::array<float,3>> &bind,
                         const Matrix34 &placement,bool normalView,
                         unsigned lightMask=1,
                         const std::array<float,3> *lightPosition=nullptr,
                         float lightRadius=1000,
                         const GenericActorStyle *style=nullptr);
void DestroyGenericActorGl();
void ForgetGenericActorGl();
void DestroyGenericActorGl(const void *owner);
void ForgetGenericActorGl(const void *owner);
void CreateAnimalsGl(const AmbientAnimals &animals);
void DestroyAnimalsGl();
void ForgetAnimalsGl();
void DrawAnimalsGl(const float *camera, const std::array<float, 3> &worldCenter,
                   float worldScale, const AmbientAnimals &animals, bool normalView);
} // namespace re4
