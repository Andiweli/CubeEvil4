#pragma once
#include "object_actions.h"
#include "room_state.h"
#include <optional>
namespace re4 {
// Source inventory/state inputs to em_sub.cpp::RandomItemCk. The source
// count zero is kept for SceAtCreateItemAt's ordinary default-count lookup.
struct EnemyDropContext : ObjectDropContext {
  std::vector<unsigned> carriedWeapons;
  unsigned playerType{}, gameMode{};
  bool adaGame{};
};
struct EnemyDropState {
  std::uint16_t randomState{0x2300};
  unsigned noDropCount{}, noRecoveryDropCount{};
};
struct EnemyDropChoice { unsigned id{}, count{}; };
struct AuthoredEnemyDrop {
  unsigned listIndex{}, recordIndex{}, area{}, id{}, count{}, itemFlag{}, autoFlag{}, controlFlags{}, effect{};
  std::uint16_t room{};
  CollisionVec position{};
  bool fixedPosition{}, taken{};
  std::array<std::uint8_t,156> raw{};
};
std::optional<AuthoredEnemyDrop> FindAuthoredEnemyDrop(const RoomArchive &room,
    unsigned listIndex,const ScenarioState &scenario,
    std::uint16_t roomId=0x100,unsigned playerType=0);
bool ClaimAuthoredEnemyDrop(const AuthoredEnemyDrop &drop,ScenarioState &scenario);
std::optional<EnemyDropChoice> ResolveAuthoredEnemyDrop(const AuthoredEnemyDrop &drop,
    const EnemyDropContext &context,EnemyDropState &state);
std::optional<EnemyDropChoice> OriginalEnemyRandomDrop(unsigned enemyId,
    const EnemyDropContext &context, EnemyDropState &state,
    unsigned controlFlags=0);
} // namespace re4
