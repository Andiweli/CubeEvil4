#pragma once
#include "re4/port/game_runtime.h"
#include "re4/port/gx_scene.h"

namespace re4::port {
// Isolated execution of original room callbacks, not ScenarioRoomInit/GameTask.
// The session owns the SMD pool and all callback borrows on the frame owner.
class ShelfCallbackSession {
public:
    ShelfCallbackSession(GameRuntime &,u16 room,u32 selection=0);
    ~ShelfCallbackSession();
    ShelfCallbackSession(const ShelfCallbackSession &)=delete;
    ShelfCallbackSession &operator=(const ShelfCallbackSession &)=delete;
    void start(bool alreadyOpened=false);
    void tick();
    bool suspend();
    bool resume();
    void cancel();
    bool finished() const;
    u32 visits() const;
    GxScene snapshot();
    std::string report() const;
    void verifyReleaseBlocked();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Real-data verification: animated/opened paths, suspend/resume, cancellation,
// fresh callback generations and repeated complete native model retirement.
std::string verifyShelfCallbacks(GameRuntime &,u16 room,u32 selection);
}
