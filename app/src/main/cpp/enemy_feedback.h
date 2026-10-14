#pragma once
#include "enemy_preview.h"
#include "hit_geometry.h"
#include <optional>

namespace re4 {
enum class EnemySoundChannel { Impact, Voice, Breath, Contact };
struct EnemySoundEvent {
  unsigned block = 8, number{};
  CollisionVec position{};
  EnemySoundChannel channel = EnemySoundChannel::Impact;
  bool stop{}, replace{};
};
struct EnemyEffectEvent {
  unsigned sequence{}; // EFF_EM10 EST number, not a synthetic particle id.
  std::array<float, 3> position{}, rotation{};
  std::uint64_t eventSerial{};
};
struct EnemyDamagePose {
  HitVector position{}, rotation{};
};
// WorldEffects stores a source32-bit instance id before appending its sprite
// id. Reserve the high byte for the ESL owner, the low24 for its event count.
std::uint32_t GanadoEffectTriggerSerial(unsigned owner,std::uint64_t event);
// em_sub.cpp::EmGetDmPos: a posed hit capsule's surface, clamped along its
// authored axis, looking towards m_PosFrom. Cross is used only for DMPOS.
EnemyDamagePose GanadoDamagePose(const AuthoredHitShape &shape,
                                const HitVector &cross,
                                const Matrix34 &posedPart,
                                const CollisionVec &source);
// Ordinary unarmoured type0 branches from cEm10::em10BloodSet. The CTRL12
// BIGEFF timeout is owned by the caller, in original30-Hz simulation ticks.
std::optional<unsigned> GanadoBloodSequence(unsigned weaponNo, bool near,
                                            bool bigEffectCooling,
                                            float cameraDistanceSquared);
// Original village Se_tbl0..3 from em10SetSeTbl. Keeping named slots prevents
// generic grunt substitutions and allows fixture checks without disc bytes.
unsigned GanadoVoiceCue(unsigned tableSlot, unsigned originalModelType = 0);
// GetWepDmVal for the current original WEP01/WEP02/WEP07 profiles. Distance
// uses the authored hit's strict lenSq<36000000 boundary, not pellet count.
unsigned GanadoFirearmBaseDamage(unsigned weaponNo,unsigned powerLevel,bool near);
struct GanadoFirearmDamage { unsigned damage{};bool critical{}; };
GanadoFirearmDamage GanadoFirearmHitDamage(unsigned weaponNo,unsigned powerLevel,
                                           float lenSq,bool head);
// EmAtkHitCk2's source motion direction/height selection, after the posed
// sphere/capsule collision and EAT checks have succeeded. Returns 0..5.
unsigned GanadoAxeDamageType(const CollisionVec &playerFeet,
                             float playerHeading, const CollisionVec &point,
                             const CollisionVec &sweepBase);
} // namespace re4
