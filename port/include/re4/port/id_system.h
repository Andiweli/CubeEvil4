#pragma once
#include "re4_id_save_types.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace re4::port {
class GameplayHeaps;
class NativeScheduler;
struct IdHandle { u32 slot{}; u64 generation{}, runtime{}; };
struct IdSystemSnapshot {
    int capacity{}, alive{}, levelMax{};
    std::vector<u32> setFlags, displayOff;
    bool initialized{};
};
// Original CPU pool and Cockpit::gameInit. File-table instantiation, animated
// paths, texture registration and OT/GX drawing remain explicit later consumers.
class IdRuntime final {
  public:
    IdRuntime(GameplayHeaps &, NativeScheduler &);
    ~IdRuntime();
    void initialize(); // Original main.cpp's IdSys.gameInit(0x80), system heap.
    void cockpitGameInit();
    void roomInit();
    void clear(); // Requires quiescent task stacks; original IDSystem::free.
    IdSystemSnapshot snapshot() const;
    std::optional<IdHandle> pull();
    const ID_UNIT &unit(IdHandle) const; // Owner-thread borrow until reset/free.
    ID_UNIT &unit(IdHandle); // Original widgets write these named CPU fields.
    void parent(IdHandle parent, IdHandle child);
    void push(IdHandle);
    void kill(u8 mark, ID_CLASS);
    void display(ID_CLASS, int);
    int isSet(ID_CLASS) const;
    void time(IdHandle, s16);
    void moving(IdHandle, bool);
    const std::vector<std::string> &trace() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
