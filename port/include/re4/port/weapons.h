#pragma once
#include "re4/port/engine_loader.h"
#include <memory>

namespace re4::port {
class GameRuntime;
struct PlayerDomain;
struct WeaponDomain;
struct WeaponReadSnapshot {
    bool loaded{},initBound{},moveBound{},initialized{};
    std::uint32_t module{},mappedWeapon{},bodyBytes{},soundBytes{},relOffset{},relBytes{};
    std::string path,sourceHash,bodyHash;
};
// Original ReadWepData uses this shared resource/module owner. This is not
// WeaponPreview or an independent weapon/gameplay state machine.
class WeaponRuntime final {
public:
    WeaponRuntime(GameRuntime &,PlayerDomain &);
    ~WeaponRuntime();
    WeaponReadSnapshot snapshot() const;
    void clear();
    std::shared_ptr<WeaponDomain> domain() const;
private:
    std::shared_ptr<WeaponDomain> domain_;
};
} // namespace re4::port
