#pragma once
#include "collision_query.h"
#include <optional>
#include <vector>
namespace re4 {
struct GanadoAwarenessInput {
 unsigned type{},flags{},gotoMode{};
 bool found{},lineVisible{},playerFootstep{},playerFired{},somebodyDamaged{},damaged{};
 float distanceSquared{},viewAngle{},heightDelta{},routeDistance{},burstDistanceSquared{};
 bool burst{};
};
// Literal em10FindCk decision, with external engine state supplied explicitly.
bool OriginalGanadoFind(const GanadoAwarenessInput &input,unsigned mode=0);
struct GanadoPeerState {
 CollisionVec position{};
 unsigned id=0x12,routine{},subroutine{};
 int hp{};
 bool allocated=true,active=true;
};
struct GanadoPeerAwareness {bool somebodyDamaged{},otherFinding{};};
// Original peer checks; the caller excludes the observing actor itself.
GanadoPeerAwareness OriginalGanadoPeers(const CollisionVec &observer,float heading,
    const std::vector<GanadoPeerState> &others,unsigned room=0x100);
} // namespace re4
