// Per-frame hook of the game (PADRead calls it once per frame), compiled with
// the game's flags into libre4_main because it reads the game's own state.
//
// Zustands-Trace: one JSON line per game frame (RE4_TRACE=FILE):
//
//   {"frame":N,"Rno0":3,"room":"0x100","pl":[x,y,z],"rsf":["0x80000000"],
//    "em":[{"slot":1,"id":"0x12","r":[1,11,0,0],"hp":500,"pos":[x,y,z]}, ...]}
//
// Rno0 is the game task step (2 room init, 3 main loop), "rsf" the room save
// flag word of the current room (flag_rsf.h), "em" every live enemy work of
// EmMgr except the player (id 0). Every pointer is checked against MEM1 first,
// so the trace can run from the first frame on.
//
// Run control of the local checks (port/tools/whole_r100_check.sh):
//
//   RE4_PAD_UNTIL_ROOM=1   the test script (RE4_PAD_SCRIPT) ends when the game
//                          starts loading its first room (step 2): its presses
//                          drive the title screen only
//   RE4_STOP_AFTER=N       the run ends (exit code 0) after N frames counted
//                          from the first frame of the game's main loop
//                          (step 3), with a summary line in the log
#include "types.h"
#include "em.h"
#include "global.h"
#include "player.h"
#include "cam_ctrl.h"
#include "room_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern GLOBAL_WK* pG;
extern cPlayer* pPL;
extern cEmMgr EmMgr;

extern "C" void re4_set_frame_hook(void (*hook)(unsigned));
extern "C" void re4_pad_script_stop(void);
extern "C" void re4_log(const char* fmt, ...);

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

void trace(unsigned n)
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
    // the camera the game draws with (pG->Camera, which CameraMove fills from
    // CamCtrl.camera when STA_CAMERA is on and the debug camera is off) and
    // the switches that decide it: C STA_CAMERA, D DBG_DBG_CAM, S SPF_CAMERA,
    // E STA_EVENT, U STA_SUB_SCRN, X extra (boss/event) camera
    {
        const CAMERA_POINT& c = g->Camera.param;
        fprintf(f, ",\"cam\":{\"eye\":");
        vec(f, c.Campos);
        fprintf(f, ",\"at\":");
        vec(f, c.Target);
        fprintf(f, ",\"fov\":%.2f,\"sw\":\"%s%s%s%s%s%s\",\"r0\":%u,\"ctrl\":", (double) c.Fovy,
                StaFlagChk(g, STA_CAMERA) ? "C" : "", DbgFlagChk(g, DBG_DBG_CAM) ? "D" : "",
                SpfFlagChk(g, SPF_CAMERA) ? "S" : "", StaFlagChk(g, STA_EVENT) ? "E" : "",
                StaFlagChk(g, STA_SUB_SCRN) ? "U" : "", CamCtrl.m_pExtraCamera ? "X" : "", (unsigned) CamCtrl.r0);
        vec(f, CamCtrl.camera.param.Campos);
        fprintf(f, "}");
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
    fflush(f);  // a crash must not lose the last frames
}

bool g_padUntilRoom;
unsigned long g_stopAfter;
unsigned long g_roomFrames;  // frames since the first main loop frame
unsigned long g_loopFrames;  // of them in step 3

void frame(unsigned n)
{
    trace(n);
    GLOBAL_WK* g = pG;
    if (!live(g)) {
        return;
    }
    if (g_padUntilRoom && g->Rno0 >= 2) {
        re4_pad_script_stop();
        g_padUntilRoom = false;
    }
    if (g_stopAfter == 0 || (g_roomFrames == 0 && g->Rno0 != 3)) {
        return;
    }
    if (g_roomFrames == 0) {
        re4_log("run: main loop reached at frame %u in room r%03x", n, (unsigned) g->room_id);
    }
    g_roomFrames++;
    g_loopFrames += g->Rno0 == 3;
    if (g_roomFrames >= g_stopAfter) {
        re4_log("run: stopped after %lu frames from the main loop on (%lu in step 3), room r%03x, step %u "
                "(RE4_STOP_AFTER)",
                g_roomFrames, g_loopFrames, (unsigned) g->room_id, (unsigned) g->Rno0);
        if (g_trace) {
            fflush(g_trace);
        }
        fflush(stdout);
        fflush(stderr);
        _exit(0);
    }
}

void configure()
{
    const char* path = getenv("RE4_TRACE");
    if (path && *path && !g_trace) {
        g_trace = fopen(path, "w");
    }
    const char* until = getenv("RE4_PAD_UNTIL_ROOM");
    g_padUntilRoom = until && *until && *until != '0';
    const char* stop = getenv("RE4_STOP_AFTER");
    g_stopAfter = stop ? strtoul(stop, 0, 10) : 0;
    if (g_trace || g_padUntilRoom || g_stopAfter) {
        re4_set_frame_hook(frame);
    }
}

struct Install {
    Install() { configure(); }
} g_install;

}  // namespace

// Reads the switches again (the app sets them after the library is loaded,
// runtime/core_api.cpp re4_core_start).
extern "C" void re4_state_trace_reconfigure(void)
{
    configure();
}

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
