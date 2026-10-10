#pragma once
#include "re4/port/consumers.h"
#include "re4_light_types.h"
#include <memory>

namespace re4::port {
class GameplayHeaps;
struct LightBlockView {
    ResourceLease source;
    std::vector<u32> nativeWords;
    std::vector<u32> cutOffsets;
    u8 sourceVersion{};
    cLit *header();
    const cLit *header() const;
};
// A checked native copy. Immutable GC wire data is never patched in place.
std::shared_ptr<LightBlockView> resolveLightBlock(ResourceLease);
struct LightPathView {
    ResourceLease source;
    std::vector<u32> nativeWords;
    std::vector<u32> offsets, lengths;
    LightPathHeader *header();
};
std::shared_ptr<LightPathView> resolveLightPaths(ResourceLease);
struct LightHandle { u64 runtime{},epoch{};u32 slot{};u64 generation{}; };
struct LightSnapshot {
    bool initialized{},pathsBound{};
    u32 capacity{},alive{};
    f32 electricPower{};
    std::array<u32,8> kindFlags{};
    cLightEnv environment{};
    std::array<GXColor,3> tune{};
};
struct LightStateView {
    u32 flags{},parentNo{};
    u8 type{},id{},kind{},enableMask{},parentType{},routine{};
    Vec position{},world{},direction{};
    f32 radius{},intensity{};
    GXColor color{},displayColor{};
    u16 index{};
};
// Shared CPU state on the frame owner. Snapshots are copied, never GC layouts.
class LightRuntime final {
public:
    LightRuntime(GameplayHeaps &,NativeScheduler &,EngineConsumers &,NativeGameState &);
    ~LightRuntime();
    LightRuntime(const LightRuntime &)=delete;
    LightRuntime &operator=(const LightRuntime &)=delete;
    void initialize(); // Actual original LightMgr.init + original function table.
    void initializePaths(ResourceLease);
    void bindRoom(ResourceLease core,ResourceLease room,ResourceLease third,u32 capacity);
    void bindArchives(ResourceLease coreBody,ResourceLease roomBody); // Original CORE_ARC/GetDataExt/CNS indices.
    void clear(); // Requires quiescent callback stacks; invalidates all handles.
    LightSnapshot snapshot() const;
    const std::vector<std::string> &trace() const;
    std::shared_ptr<const LightPathView> paths() const;
    std::vector<int> pathSteps(u8 id,u8 flags,u32 steps); // Real path bytes and original setPath/movePath.
    std::shared_ptr<const LightBlockView> block(int) const;
    cLightEnv cut(int block,int cut) const;
    std::vector<cLightWork> records(int block,int cut) const;
    std::optional<LightHandle> create(int block,int cut,int light,bool back=false);
    LightStateView state(LightHandle) const;
    void moveType(LightHandle); // Original type handler, not the full lightMove/frame pipeline.
    void destroy(LightHandle);
    void event(bool begin);
    void kind(u8,bool enabled);
    bool kind(u8) const;
    f32 power(f32 delta);
    void selectBlock(int); // -1 restores original room block.
    bool roomSelected() const;
    void tune(int block,int cut);
    void fogDistances(f32 start,f32 end);
    // Model lookup must be supplied by real manager registrations. A missing
    // provider throws rather than reporting an absent enemy/object as success.
    using ModelResolver=std::function<std::optional<ConsumerHandle>(u8,u16)>;
    void setModelResolver(ModelResolver);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
