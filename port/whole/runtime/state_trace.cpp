// Zustands-Trace: one JSON line per game frame (RE4_TRACE=FILE), written when
// the game reads the controller (PADRead, once per frame). Compiled with the
// game's flags into libre4_main, because it reads the game's own state:
//
//   {"frame":N,"Rno0":3,"room":"0x100","pl":[x,y,z],"rsf":["0x80000000"],
//    "em":[{"slot":1,"id":"0x12","r":[1,11,0,0],"hp":500,"pos":[x,y,z]}, ...]}
//
// Rno0 is the game task step (2 room init, 3 main loop), "rsf" the room save
// flag word of the current room (flag_rsf.h), "em" every live enemy work of
// EmMgr except the player (id 0). Every pointer is checked against MEM1 first,
// so the trace can run from the first frame on.
#include "types.h"
#include "em.h"
#include "global.h"
#include "player.h"
#include "room_data.h"

#include <stdio.h>
#include <stdlib.h>

extern GLOBAL_WK* pG;
extern cPlayer* pPL;
extern cEmMgr EmMgr;

extern "C" void re4_set_frame_hook(void (*hook)(unsigned));

namespace {

FILE* g_trace;

bool live(const void* p)
{
    unsigned long a = (unsigned long) p;
    // the managers and the player can live in the libraries (above MEM1) or in MEM1
    return a >= 0x80000000ul;
}

void vec(FILE* f, const Vec& v)
{
    fprintf(f, "[%.3f,%.3f,%.3f]", (double) v.x, (double) v.y, (double) v.z);
}

void frame(unsigned n)
{
    FILE* f = g_trace;
    if (!f) {
        return;
    }
    GLOBAL_WK* g = pG;
    if (!live(g)) {
        return;
    }
    u16 room = g->room_id;
    fprintf(f, "{\"frame\":%u,\"Rno0\":%u,\"room\":\"0x%03x\"", n, (unsigned) g->Rno0, (unsigned) room);
    cPlayer* pl = pPL;
    if (live(pl)) {
        fprintf(f, ",\"pl\":");
        vec(f, pl->pos);
    }
    u8* save = g->Rno0 >= 2 ? (u8*) RoomData.getRoomSavePtr(room) : 0;
    if (live(save)) {
        fprintf(f, ",\"rsf\":[\"0x%08lx\"]", (unsigned long) *(u32*) (save + 4));
    }
    fprintf(f, ",\"em\":[");
    cEm* arr = EmMgr.pArray;
    u32 n2 = EmMgr.nArray;
    u32 stride = EmMgr.size;
    bool first = true;
    if (live(arr) && stride != 0 && n2 < 4096) {
        for (u32 i = 0; i < n2; i++) {
            cEm* e = (cEm*) ((u8*) arr + i * stride);
            if (!(e->be_flag & 1) || e->id == 0) {
                continue;
            }
            fprintf(f, "%s{\"slot\":%lu,\"id\":\"0x%02x\",\"r\":[%u,%u,%u,%u],\"hp\":%d,\"pos\":", first ? "" : ",",
                    (unsigned long) i, (unsigned) e->id, (unsigned) e->r_no_0, (unsigned) e->r_no_1,
                    (unsigned) e->r_no_2, (unsigned) e->r_no_3, (int) e->hp);
            vec(f, e->pos);
            fprintf(f, "}");
            first = false;
        }
    }
    fprintf(f, "]}\n");
    if ((n & 63) == 0) {
        fflush(f);
    }
}

struct Install {
    Install()
    {
        const char* path = getenv("RE4_TRACE");
        if (path && *path) {
            g_trace = fopen(path, "w");
            if (g_trace) {
                re4_set_frame_hook(frame);
            }
        }
    }
} g_install;

}  // namespace

// Starts (path) or ends (0) the trace at run time (runtime tests).
extern "C" int re4_state_trace_open(const char* path)
{
    if (g_trace) {
        fclose(g_trace);
        g_trace = 0;
    }
    re4_set_frame_hook(0);
    if (path) {
        g_trace = fopen(path, "w");
        if (g_trace) {
            re4_set_frame_hook(frame);
        }
    }
    return g_trace != 0;
}
