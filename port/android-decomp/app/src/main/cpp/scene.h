#pragma once
#include "environment.h"
#include "room_lighting.h"
#include "world_effects.h"
#include "laser_sight.h"
#include "smd.h"
#include "smx.h"
#include "texture.h"
namespace re4 {
using Matrix3 = std::array<float, 9>;
Matrix3 NormalMatrix(const std::array<float, 12>
                         &matrix); // inverse transpose, zero if singular.
std::array<float, 3> TransformNormal(const Matrix3 &matrix,
                                     const std::array<float, 3> &normal);
using Matrix34 = std::array<float, 12>;
Matrix34 PlacementMatrix(const SmdPlacement &placement);
std::array<float, 3> TransformPoint(const Matrix34 &matrix,
                                    const std::array<float, 3> &point);
std::vector<std::array<float, 3>> RestJointCenters(const SmdModel &model);
struct SceneDraw {
  std::size_t first{}, count{};
  unsigned work = 0xfe;
  std::array<std::uint8_t, 3> materialRGB{255, 255, 255};
  unsigned palette{}, image{};
  bool commonModel{}; // Common BIN uses DmyZeroTpl plus common palette 0.
  bool repeat{};
  bool vertexColors{};
  int texture = -1;
  bool alphaTest{};
  unsigned alphaImage{}, alphaReference{};
  int alphaTexture = -1;
  bool specularEnabled{},specularLocal{},specularBump{};
  unsigned specularType{},specularImage{};
  float specularScale=.5f;
  std::array<std::uint8_t,3> specularRGB{};
  int specularTexture=-1;
  unsigned blendMode = 0; // commonModelTrans bl[0..4], independent of masks.
  unsigned zMode = 0; // ModelRender: test/write, test only, always/no write.
  unsigned cullMode=2; // Diagnostic geometry stays two-sided.
  std::array<float,3> specularNormalScale{1,1,1};
  unsigned orderType = 3, object = 0;
  std::uint32_t lightMask = 0xffffffffu;
  float lightSize{};
  RoomLightVolume lightVolume;
  bool noDiffuseLight{};
  bool scrollLightAttribute{}; // SMD Attribute1 / SMX flag32: excludes lamp Attribute4.
  // Runtime ownership is independent of the SMD draw-order object number.
  // Original ETS geometry and ITM pickups are uploaded with the room; their
  // visibility follows the object/action state without rebuilding buffers.
  int roomObjectSlot = -1;
  int roomObjectVariant = -1; // r101 window: 0 intact, 1 original broken BIN.
  int roomLockParent = -1; // Separate script lock, follows its ETS door.
  int roomItemIndex = -1;
  int roomPickupIndex = -1; // Independently owned waiting-type0 ITA record.
  int roomBlock=-1; // cObj::blk; ARAM-cached scroll blocks stay hidden.
  int roomItemId = -1; // Real ITM variant selected when a random box breaks.
  int destructionMesh = -1; // Authored cEmBox fragment mesh, runtime transform.
  std::array<float, 3> sortCenter{};
  std::array<float, 2> uvSpeed{}; // Original SMX speed per 30 Hz game frame.
  int sky = -1, water = -1;       // Explicit room environment profile.
  bool authored = false; // Diagnostic meshes keep their own submission order.
};
void SetSceneSpecular(SceneDraw &draw,const std::array<std::uint8_t,32> &header,std::uint32_t modelFlags);
std::vector<Texture> LoadCoreSpecular(const std::string &path);
struct StaticScene;
struct RoomShadowData;
void AttachSceneSpecular(StaticScene &scene,const std::vector<Texture> &bank);
struct StaticScene {
  EnvironmentData environment;
  std::shared_ptr<const RoomLighting> lighting;
  std::shared_ptr<const RoomShadowData> shadows;
  WorldEffectsData worldEffects;
  std::shared_ptr<const LaserSightData> laserSight;
  std::shared_ptr<const Texture> footShadowTexture; // Original CORE Esp0x13.
  std::vector<float> vertices; // World positions, three per triangle corner.
  std::vector<float>
      uv; // Two components per corner, separate GX attribute index.
  std::vector<std::uint8_t>
      colors; // RGBA8 per corner; white for register-color branch.
  std::size_t vertexColorTriangles{};
  std::size_t smxDraws{}, materialColorTriangles{};
  std::vector<float> normals; // Unnormalized world normals, three per corner.
  std::size_t zeroNormals{}, singularNormalPlacements{};
  std::vector<SceneDraw> draws;
  std::vector<Texture> textures;
  std::size_t texturedTriangles{}, fallbackTriangles{};
  std::size_t commonTextureTriangles{}, commonAlphaTriangles{};
  std::size_t commonPlacements{}, missingCommonPlacements{};
  std::size_t alphaTriangles{}, unresolvedAlphaTriangles{};
  std::array<float, 3> minimum{}, maximum{};
  std::size_t included{}, skipped{}, triangles{};
  std::string describe() const;
};
void ApplySceneSmx(const Smx &smx, StaticScene &scene);
std::vector<std::size_t> SceneDrawOrder(const std::vector<SceneDraw> &draws,
                                        const std::array<double, 3> &eye,
                                        const std::array<double, 3> &look);
void AttachSceneTextures(const Smd &smd, StaticScene &scene,
                         const Smd *common = nullptr);
StaticScene BuildStaticScene(const Smd &smd, int placement = -1,
                             const Smd *common = nullptr);
} // namespace re4
