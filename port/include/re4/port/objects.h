#pragma once
#include "re4/port/consumers.h"
#include <functional>
#include <memory>

namespace re4::port {
class GameplayHeaps;
struct NativeObject;
struct WeaponDomain;
struct NativeObjectManager;
struct ObjectHandle { u64 runtime{},epoch{},generation{};u32 slot{}; };
struct ObjectSnapshot { u32 capacity{},active{},occupied{},peak{},guid{},pending{};u64 epoch{}; };
struct ObjectWorkSnapshot { u32 flags{},guid{};u8 id{},kind{};s32 block{};bool model{}; };

// Shared native storage for the original cObjMgr/cManager algorithms. The
// largest supported native class sets the stride; no GC object is overlaid.
class ObjectRuntime final {
public:
    ObjectRuntime(GameplayHeaps &,EngineConsumers &,NativeScheduler &);
    ~ObjectRuntime();
    ObjectRuntime(const ObjectRuntime &)=delete;
    ObjectRuntime &operator=(const ObjectRuntime &)=delete;
    void configure(u32 count);
    void clear();
    bool configured() const;
    std::optional<ObjectHandle> create(u32 id,bool back=false);
    std::optional<ObjectHandle> createAt(u32 id,u32 slot);
    void destroy(ObjectHandle,bool immediately=false);
    void move(ObjectHandle);
    void dieCheck();
    void destroyAll();
    ObjectSnapshot snapshot() const;
    ObjectWorkSnapshot snapshot(ObjectHandle) const;
    std::vector<ObjectHandle> active() const;
    void initializeModel(ObjectHandle,ResourceLease bin,ResourceLease tpl);
    // Internal original-consumer boundary: pointers are checked on each call.
    // Handles, rather than these temporary pointers, cross native lifetimes.
    NativeObject *getActiveWork();
    NativeObject *getNext(NativeObject *);
    void destroy(NativeObject *);
    void destroyNow(NativeObject *);
    int id(NativeObject *) const;
    void applyFunc(const std::function<void(NativeObject*)> &);
    const std::vector<std::string> &trace() const;
private:
    friend class EffectRuntime;
    friend struct WeaponDomain;
    NativeObject *checkedObject(ObjectHandle);
    NativeObjectManager &originalManager();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
