// Frozen upstream em_sub.cpp73-205, em10.cpp1937-2180 / em10SetSeTbl.
#include "enemy_feedback.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include "weapon_action_source.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace re4 {
std::uint32_t GanadoEffectTriggerSerial(unsigned owner,std::uint64_t event) {
  if(owner>255 || !event || event>0xffffff)
    throw std::runtime_error("Ganado: invalid source blood event owner/counter");
  return (owner<<24)|std::uint32_t(event);
}
EnemyDamagePose GanadoDamagePose(const AuthoredHitShape &shape,
                                const HitVector &cross,
                                const Matrix34 &posedPart,
                                const CollisionVec &source) {
  using namespace motion64;
  for (float component : posedPart)
    if (!std::isfinite(component) || std::abs(component) > 1e7f)
      throw std::runtime_error("Ganado: invalid blood part matrix");
  for (double component : source)
    if (!std::isfinite(component) || std::abs(component) > 1e7)
      throw std::runtime_error("Ganado: invalid blood hit source");
  for (float component : shape.offset)
    if (!std::isfinite(component) || std::abs(component) > 1e7f)
      throw std::runtime_error("Ganado: invalid blood capsule offset");
  if (!std::isfinite(shape.radius) || shape.radius < 0 ||
      shape.radius > 1e6f || !std::isfinite(shape.height) ||
      shape.height < 0 || shape.height > 1e6f)
    throw std::runtime_error("Ganado: invalid blood capsule size");
  EnemyDamagePose result;
  if (shape.flags & HitDamagePosition) {
    for (float component : cross)
      if (!std::isfinite(component) || std::abs(component) > 1e7f)
        throw std::runtime_error("Ganado: invalid blood intersection");
    result.position = cross;
    result.rotation[1] = std::atan2(float(source[0]) - cross[0],
                                    float(source[2]) - cross[2]);
    return result;
  }
  const unsigned axis = shape.flags & HitXAxis ? 0
                        : shape.flags & HitZAxis ? 2 : 1;
  Mtx matrix, inverse;
  std::memcpy(matrix, posedPart.data(), sizeof(matrix));
  Vec bottom{shape.offset[0], shape.offset[1], shape.offset[2]};
  auto top = bottom;
  if (axis == 0) top.x += shape.height;
  else if (axis == 2) top.z += shape.height;
  else top.y += shape.height;
  PSMTXMultVec(matrix, &top, &top);
  PSMTXMultVec(matrix, &bottom, &bottom);
  Vec center;
  PSVECAdd(&top, &bottom, &center);
  PSVECScale(&center, &center, .5f);
  Vec radius{0, 0, shape.radius};
  PSMTXMultVecSR(matrix, &radius, &radius);
  const float magnitude = PSVECMag(&radius);
  // Original TransMatrix replaces translation with the capsule bottom.
  matrix[0][3] = bottom.x;
  matrix[1][3] = bottom.y;
  matrix[2][3] = bottom.z;
  if (!PSMTXInverse(matrix, inverse)) PSMTXIdentity(inverse);
  Vec from{float(source[0]), float(source[1]), float(source[2])}, local;
  PSMTXMultVec(inverse, &from, &local);
  HitVector localVector{local.x, local.y, local.z}, radial = localVector;
  radial[axis] = 0;
  if (radial[0] == 0 && radial[1] == 0 && radial[2] == 0)
    radial[axis == 0 ? 1 : 0] = 1;
  Vec direction{radial[0], radial[1], radial[2]};
  PSVECNormalize(&direction, &direction);
  PSVECScale(&direction, &direction, magnitude);
  radial = {direction.x, direction.y, direction.z};
  radial[axis] = std::clamp(localVector[axis], 0.f, shape.height);
  direction = {radial[0], radial[1], radial[2]};
  Vec position;
  PSMTXMultVec(matrix, &direction, &position);
  result.position = {position.x, position.y, position.z};
  result.rotation[1] = std::atan2(from.x - center.x, from.z - center.z);
  return result;
}
std::optional<unsigned> GanadoBloodSequence(unsigned weapon, bool near,
                                            bool cooling, float cameraDistance) {
  if (!std::isfinite(cameraDistance) || cameraDistance < 0)
    throw std::runtime_error("Ganado: invalid blood camera distance");
  switch (weapon) {
  case 0: case 0x14: case 0x22: case 0x23: case 0x24: case 0x25: case 0x2a:
    return {};
  case 1: case 2: case 3: case 4: case 0x11: case 0x19: case 0x1c:
  case 0x1f: case 0x20: case 0x26: case 0x2b:
    return 1;
  case 0x10: case 0x1a:
    return 0x29;
  case 0xb: case 0xc: case 0x1b: case 0x1d: case 0x27:
    return 0xc;
  case 7: case 8: case 0x21:
    if (!near) return 1;
    break;
  case 9: case 0xa: case 0x28:
    return 0x87;
  default:
    break;
  }
  if (cooling) return cameraDistance < 16000000.f ? 0x37 : 0x35;
  return cameraDistance < 16000000.f ? 0x36 : 2;
}
unsigned GanadoVoiceCue(unsigned slot,unsigned modelType) {
  constexpr std::array<std::array<unsigned,19>,4> table{{{
      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x25,
      0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
      0x5b, 0xb5, 0x15},{
      0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x27,
      0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x5c,0xb6,0x1d},{
      0x2e,0x2f,0x30,0x31,0x32,0x33,0x34,0x29,
      0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x5d,0xb7,0x2d},{
      0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x2b,
      0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x5e,0xb8,0x35}}};
  // EM12 uses type1 and EM15 uses type11 for voice set1.
  // Frozen em12_set.cpp/em15_set.cpp::Em10SetSeTbl; model type is
  // deliberately different from the four-row sound table index.
  const unsigned voiceSet=(modelType==1 || modelType==11)?1:modelType==3?2:(modelType==4 || modelType==12)?3:0;
  if (slot >= table[0].size() || (modelType!=0 && modelType!=1 && modelType!=11 && modelType!=3 && modelType!=4 && modelType!=12))
    throw std::runtime_error("Ganado: invalid original voice slot");
  return table[voiceSet][slot];
}
unsigned GanadoFirearmBaseDamage(unsigned weapon,unsigned level,bool near) {
  if(level>6 || weapon>0x2d)
    throw std::runtime_error("Ganado: unsupported original weapon/power rank");
  const float rate=!near && OriginalShotgun(weapon)
      ?OriginalShotgunFarPower[weapon==7?0:weapon==8?1:2][level]
      :OriginalWeaponPower[weapon][level];
  return unsigned(float(OriginalGanadoBaseDamage[weapon])*rate);
}
GanadoFirearmDamage GanadoFirearmHitDamage(unsigned weapon,unsigned level,float lenSq,bool head) {
  if(!std::isfinite(lenSq) || lenSq<0)throw std::runtime_error("Ganado: invalid hit distance");
  const bool near=lenSq<36000000.f;
  const auto base=GanadoFirearmBaseDamage(weapon,level,near);
  bool critical=false;
  float rate=1;
  if(head) {
    rate=(weapon==9 || weapon==10)?5.f:1.2f;
    // em10SetDmVal excludes these effects/melee from critical RNG.
    switch(weapon) {
    case 0:case 14:case 16:case 20:case 21:case 22:case 23:case 24:case 25:
    case 26:case 31:case 32:case 34:case 35:case 36:case 37:case 42:break;
    default:critical=Rnd()%12==6;break;
    }
    if(weapon==2 && level>5 && Rnd()%10>4)critical=true;
    if(OriginalShotgun(weapon) && near && Rnd()%3==1)critical=true;
    if(critical)rate=10;
  }
  return {std::min(unsigned(float(base)*rate),9999u),critical};
}
unsigned GanadoAxeDamageType(const CollisionVec &feet,float heading,
                             const CollisionVec &point,const CollisionVec &base) {
  for(const auto *p : {&feet,&point,&base})for(double v:*p)
    if(!std::isfinite(v)||std::abs(v)>1e7)
      throw std::runtime_error("Axe: invalid hit position");
  if(!std::isfinite(heading))throw std::runtime_error("Axe: invalid player rotation");
  float dx=float(point[0])-float(base[0]),dz=float(point[2])-float(base[2]);
  if(dx*dx+dz*dz<10000) {dx=float(feet[0])-float(point[0]);dz=float(feet[2])-float(point[2]);}
  unsigned type=std::sin(heading)*dx+std::cos(heading)*dz>=0;
  const float height=float(point[1])-float(feet[1]);
  if(height<800)type+=4;else if(height<1300)type+=2;
  return type;
}
} // namespace re4
