#pragma once
#include "re4_merchant_types.h"
#include "re4_runtime_types.h"
#include <memory>
#include <vector>
#include <string>

namespace re4::port {
struct MerchantSnapshot {
    MERCHANT_DATA data{};
    bool initialized{}, characterBound{};
};
// Actual original merchant boot tables and save/load bodies. Shop UI and actors
// remain separate dependencies. Pointers never overlay a GC save image.
class MerchantRuntime final {
public:
    MerchantRuntime(NativeGameState &, u8 &language);
    ~MerchantRuntime();
    void gameInit();
    void secondRoundInit();
    void clear();
    MerchantSnapshot snapshot() const;
    void save(MERCHANT_DATA &);
    void load(const MERCHANT_DATA &);
    bool usesState(const NativeGameState &) const;
    const std::vector<std::string> &trace() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
