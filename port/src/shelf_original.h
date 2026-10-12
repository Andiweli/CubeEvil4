#pragma once
#include "scroll_original.h"
#include "re4_scheduler_original.h"
#include <functional>
namespace re4::port::shelf {
using cObj=ModelCpu;
using cModel=ModelPart;
struct SHELF_SMD { u8 door[2]; };
struct SoundRequest { u16 block;u32 number; };
struct Domain {
    scroll::Domain &models;
    std::vector<SoundRequest> soundRequests;
    std::vector<std::array<f32,2>> sleeps;
    std::array<u8,2> ids{};
    cObj *SmdGetObjPtr(u32 n) { return &models.SmdGetObjPtr(n)->cpu(); }
    // An explicit diagnostic request sink, not SndCall or audible playback.
    void SndCall(u16 b,u32 n,Vec *p,u32 flags,int pan,u32 value) {
        if(p || flags || pan || value)throw MissingGameConsumer("shelf diagnostic sound request signature");
        soundRequests.push_back({b,n});
    }
    void SceSleep(int n) {
        sleeps.push_back({SmdGetObjPtr(ids[0])->pList->ang.y,SmdGetObjPtr(ids[1])->pList->ang.y});
        original::SceSleep(n);
    }
    void r103_openShelf_main(SHELF_SMD *,int);
    void r106_openShelf_main(int,int);
};
// Parameters are original source records, never an int-cast native pointer.
SHELF_SMD originalShelf(u16 room,u32 selection);
}
