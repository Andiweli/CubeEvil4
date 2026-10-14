#pragma once
#include "archive.h"
#include <array>
#include <memory>
namespace re4 {
struct RoomFog {
  unsigned type{};
  float start{}, end{};
  std::array<unsigned char, 4> color{};
};
struct RoomLight {
  unsigned flags{}, type{}, id{}, enableMask{}, parent{}, kind{}, attribute{};
  std::array<float, 3> position{}, direction{};
  std::array<unsigned char, 4> color{};
  float radius{}, intensity{}, edge{}, cutoff{};
  unsigned directionFlags{};
  std::uint32_t parentNo{};
  // Type4 is raw GX attenuation, unrelated to Id4 (shadow move handler).
  std::array<float, 6> customAttenuation{1,0,0,1,0,0};
  struct Shadow {
    unsigned flags{}, kind{}, texture{}, fovy{}, selfLevel{}, softLevel{}, fovySub{};
    std::array<float,3> sourcePosition{};
    int angleX{}, angleY{};
  } shadow;
};
struct RoomLightCut {
  RoomFog fog, mirrorFog;
  std::array<unsigned char, 2> tevScale{};
  unsigned minimumLod{},maximumLod=5,anisotropy{};
  float lodBias{};
  float farRatio{};
  unsigned fogFrames{};
  unsigned blurRate{},blurType{};
  int focusDepth{};
  unsigned focusLevel{},focusMode{};
  int blurPower{};
  std::array<unsigned char,3> contrast{};
  std::array<unsigned char, 4> screenAmbient{}, actorAmbient{}, effectAmbient{};
  std::vector<RoomLight> lights;
};
struct RoomLightVolume {
  unsigned type=2; // LightInfo0: two endpoint spheres; 1/3 sphere; 2 box.
  std::array<float,3> center{},size{};
  std::array<float,9> axes{1,0,0,0,1,0,0,0,1}; // world columns
};
bool RoomLightTouches(const RoomLightVolume &volume,const std::array<float,3> &light,float radius);
struct RoomLightArea {
  unsigned flags{}, number{}, attributes{}, character{};
  float direction{}, floor{}, height{};
  std::vector<std::array<float,3>> vertices;
  bool contains(const std::array<double,3> &position, float heading) const;
};
// LIT v0x2c: a BE offset directory, 0x104-byte cLightEnv and 0x12c-byte
// cLightWork records. No GC pointer relocation or host structure casts.
struct RoomLighting {
  explicit RoomLighting(const std::vector<unsigned char> &bytes);
  unsigned sourceVersion{};
  unsigned normalizedParents{};
  unsigned maximumLights{}; // cLit::versionUp/getMaxLight, including absent renderer cuts.
  // Cut0 is required; other source cuts may be absent or unusable. A cut
  // may be selected only when present=true, never by substituting values.
  std::vector<unsigned> unavailableCuts;
  std::vector<RoomLightCut> cuts;
  std::vector<bool> present; // cLit::getCut returns null for offset zero.
  std::vector<RoomLightArea> areas;
  void readCameraAreas(const std::vector<unsigned char> &bytes);
  unsigned selectCut(const std::array<double,3> &position, float heading,
                     unsigned previous=0, unsigned attributes=3) const;
  std::string describe() const;
};
std::shared_ptr<const RoomLighting> ReadRoomLighting(const RoomArchive &room);
float RoomFogAmount(const RoomFog &fog, float cameraDepth);
// VIEW's visibility far plane is independent of the GX projection far plane.
float RoomVisibilityFar(const RoomFog &fog, float farRatio);
bool RoomModelVisible(const std::array<float,3> &center,float radius,
    const std::array<float,3> &eye,const std::array<float,3> &right,
    const std::array<float,3> &up,const std::array<float,3> &look,
    float fovRadians,float aspect,float farPlane,float nearPlane=100);
// GXSetFog's truncated float registers and 24-bit perspective depth.
struct RoomFogRegisters {float a{},c{};std::uint32_t b{};unsigned shift{};bool orthographic{};};
RoomFogRegisters RoomFogParameters(const RoomFog &fog,float near=100,float far=1000000);
float RoomFogRasterAmount(const RoomFog &fog,const RoomFogRegisters &registers,std::uint32_t depth);
// cLightMgr::setEnv/move: source frames and byte-truncated colour Hokan.
struct RoomLightTransition {
  const RoomLighting *owner{};
  unsigned cut{}, remaining{};
  RoomFog fog{}, target{};
  double clock{};
  void select(const RoomLighting &lighting, unsigned next, double seconds,
              bool immediate=false);
};
} // namespace re4
