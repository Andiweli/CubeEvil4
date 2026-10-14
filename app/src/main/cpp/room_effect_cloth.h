#pragma once
#include "environment.h"
namespace re4 {
struct RoomEffectClothVertex {
  std::array<float,3> position{},normal{};
  std::array<float,2> uv{};
  bool operator==(const RoomEffectClothVertex &o) const {return position==o.position && normal==o.normal && uv==o.uv;}
};
struct RoomEffectClothMesh {
  std::vector<RoomEffectClothVertex> vertices; // Original row strips expanded to triangles.
  std::array<std::uint8_t,4> color{};
  std::array<float,3> lightCenter{};
  unsigned target{},record{},animation{};
  std::uint64_t serial{};
  bool additive{};
  float lightSize=10000; // clothTrans temporary LightInfo Size, not sheet radius.
};
struct RoomEffectClothState {
  unsigned columns{},rows{};
  float widthGap{},heightGap{},scale{},time{},time2{},windTime{};
  bool pinFourRows{},alive=true,additive{};
  EnvironmentMatrix placement{};
  std::array<float,3> center{};
  std::array<std::uint8_t,4> color{255,255,255,255};
  std::vector<std::array<float,3>> positions,normals,speeds;
};
// Original math_sub.cpp::SINF paired-single polynomial, including its f32
// fused multiply/add lanes and original wrapping. Not a std::sin substitute.
float OriginalClothSin(float angle);
class RoomEffectCloth {
public:
  RoomEffectCloth(WaterSprite &sprite,const WaterRecord &record);
  // cEsp4e owns CommonMove and deliberately never advances texture ANM.
  // Every wave float draw consumes the room's global byte-RNG stream.
  bool move(WaterSprite &sprite,const WaterRecord &record,std::uint16_t &visualRandom);
  RoomEffectClothMesh mesh() const;
  const RoomEffectClothState &state() const {return state_;}
private:
  RoomEffectClothState state_;
  unsigned target_{},record_{},animation_{};
  std::uint64_t serial_{};
  int timePlus_{},timePlus2_{},power_{},power2_{};
  float range_{},range2_{},offset_{},windTimePlus_{},windRangePower_{},randomRatio_{};
  void calcSpeed(float damping);
  void integrate();
  void calcNormal();
};
} // namespace re4
