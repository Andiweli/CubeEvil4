// Per-frame hook of the game (PADRead calls it once per frame), compiled with
// the game's flags into libre4_main because it reads the game's own state.
//
// Zustands-Trace: one JSON line per game frame (RE4_TRACE=FILE):
//
//   {"frame":N,"Rno0":3,"room":"0x100","pl":[x,y,z],"rsf":["0x80000000"],
//    "str":[{"w":0,"id":"0x..","be":"0x0013","rno":2,"req":"0x0","vol":..,"dvd":[busy,reqs,status],"dma":0,"err":0}],
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
//   RE4_TEST_PLACE="F:x,y,z,a"  F frames after the room started loading (the
//                          frame RE4_PAD_UNTIL_ROOM ends the title script), the
//                          player is put at x,y,z facing a (radians), as a room
//                          script does (setPos / setAng): the checks reach a
//                          place without steering there (r100: the first
//                          house, where the s03 event starts);
//                          "free+F:x,y,z,a" counts from the moment the player
//                          is free (below)
//   RE4_PAD_FREE_SCRIPT    pad.cpp: a test script started once the player is
//                          free in the room: main loop, no event and no sub
//                          screen for 300 frames in a row ("run: player free"
//                          in the log)
#include "types.h"
#include "em.h"
#include "global.h"
#include "player.h"
#include "cam_ctrl.h"
#include "room_data.h"
#include "sce_at.h"
#include "snd_drv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern GLOBAL_WK* pG;
extern cPlayer* pPL;
extern cEmMgr EmMgr;

extern "C" void re4_set_frame_hook(void (*hook)(unsigned));
extern "C" void re4_pad_script_stop(void);
extern "C" void re4_pad_script_free(void);
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

// The room's scene areas (AEV, sce_at.cpp) once its main loop runs: where the
// room scripts' triggers are (r100: 0x0A starts the s03 event at the first
// house), for placing the player in the checks (RE4_TEST_PLACE).
void logAreas()
{
    unsigned count = 0;
    for (SCE_AT_DATA* w = sceAtSetOtStart(); (w = sceAtGetOtAddr(w)) != 0 && count < 256; count++) {
        const AREA_HIT_DATA& a = w->area;
        if (a.type == AREA_TYPE_XZ4) {
            const AREA_XZ4& q = a.xz4;
            re4_log("run: area 0x%02x id %u on %u trg %u: floor %.0f height %.0f, (%.0f %.0f) (%.0f %.0f) (%.0f %.0f) "
                    "(%.0f %.0f)", (unsigned) w->no, (unsigned) w->id, (unsigned) (w->be_flg & 1),
                    (unsigned) w->trg_type, (double) q.floor, (double) q.height, (double) q.p[0].x, (double) q.p[0].z,
                    (double) q.p[1].x, (double) q.p[1].z, (double) q.p[2].x, (double) q.p[2].z, (double) q.p[3].x,
                    (double) q.p[3].z);
        } else if (a.type == AREA_TYPE_CYLINDER) {
            const AREA_CYLINDER& c = a.cylinder;
            re4_log("run: area 0x%02x id %u on %u trg %u: floor %.0f height %.0f, circle (%.0f %.0f) radius %.0f",
                    (unsigned) w->no, (unsigned) w->id, (unsigned) (w->be_flg & 1), (unsigned) w->trg_type,
                    (double) c.floor, (double) c.height, (double) c.x, (double) c.z, (double) c.radius);
        } else {
            const AREA_EYE_TRIGGER& e = a.eye_trigger;
            re4_log("run: area 0x%02x id %u on %u trg %u: type %u floor %.0f height %.0f, at (%.0f %.0f) radius %.0f, "
                    "view %.2f %.2f open %.2f", (unsigned) w->no, (unsigned) w->id, (unsigned) (w->be_flg & 1),
                    (unsigned) w->trg_type, (unsigned) a.type, (double) e.floor, (double) e.height, (double) e.xz,
                    (double) e.z, (double) e.radius, (double) e.ang_x, (double) e.ang_y, (double) e.open_ang);
        }
    }
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
    // the sound streams (radio voice, music): status, state, requests, volume,
    // DVD and DMA work in flight (a stream that never fades out holds the
    // game in SndStrStopBlock)
    {
        bool any = false;
        for (int i = 0; i < SND_STR_MAX; i++) {
            const SND_STR& st = Snd_str_work[i];
            if (st.be_flag == 0) {
                continue;
            }
            fprintf(f, "%s{\"w\":%d,\"id\":\"0x%lx\",\"be\":\"0x%04x\",\"rno\":%d,\"req\":\"0x%lx\",\"vol\":%d,"
                       "\"dvd\":[%d,%d,%lu],\"dma\":%d,\"err\":%u}",
                    any ? "," : ",\"str\":[", i, (unsigned long) st.snd_id, (unsigned) st.be_flag, (int) st.rno,
                    (unsigned long) st.req_flag, (int) st.now_vol, (int) st.dvd_busy, (int) st.dvd_req_num,
                    (unsigned long) st.dvd_status, (int) st.dma_busy, (unsigned) st.err_flag);
            any = true;
        }
        if (any) {
            fprintf(f, "]");
        }
    }
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
// RE4_TEST_PLACE: frames from the room start, or with "free+" from the moment
// the player is free
bool g_place;
bool g_placeFree;
unsigned long g_placeFrame;
float g_placePos[3], g_placeAng;
long g_roomStart = -1;  // frame the room started loading
// The player is free: main loop, no event (STA_EVENT), no sub screen
// (STA_SUB_SCRN) for kFreeFrames frames in a row (the opening event and the
// radio call follow each other within a few frames).
const unsigned kFreeFrames = 300;
unsigned g_freeRun;
long g_freeAt = -1;
unsigned long g_stopAfter;
// RE4_DEBUG_HUD=0: the debug build's own displays off (debug_mode 0, as the
// publicity build sets it in main.cpp): the event and score lines, the
// process bars, the heap numbers, the debug text (EprintfDrawing) and the
// green far fog of DBG_FOG_FAR_GREEN (game.cpp). The app sets it by default.
bool g_hideDebugHud;
unsigned long g_roomFrames;  // frames since the first main loop frame
unsigned long g_loopFrames;  // of them in step 3

void frame(unsigned n)
{
    trace(n);
    GLOBAL_WK* g = pG;
    if (!live(g)) {
        return;
    }
    if (g_hideDebugHud && g->debug_mode != 0) {
        g->debug_mode = 0;
    }
    if (g_roomStart < 0 && g->Rno0 >= 2) {
        g_roomStart = (long) n;
    }
    if (g_roomStart >= 0 && g_freeAt < 0) {
        bool free = g->Rno0 == 3 && !StaFlagChk(g, STA_EVENT) && !StaFlagChk(g, STA_SUB_SCRN);
        g_freeRun = free ? g_freeRun + 1 : 0;
        if (g_freeRun == kFreeFrames) {
            g_freeAt = (long) n;
            re4_log("run: player free (no event, no sub screen for %u frames) at frame %u, %ld after the room start",
                    kFreeFrames, n, (long) n - g_roomStart);
            re4_pad_script_free();
        }
    }
    long placeBase = g_placeFree ? g_freeAt : g_roomStart;
    if (g_place && placeBase >= 0 && n == placeBase + g_placeFrame && live(pPL) && g->Rno0 == 3) {
        g_place = false;
        Vec pos = { g_placePos[0], g_placePos[1], g_placePos[2] };
        Vec ang = { 0.0f, g_placeAng, 0.0f };
        pPL->setPos(&pos);
        pPL->setAng(&ang);
        re4_log("run: player placed at (%.0f %.0f %.0f), facing %.2f, frame %u", (double) pos.x, (double) pos.y,
                (double) pos.z, (double) g_placeAng, n);
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
        logAreas();
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
    const char* place = getenv("RE4_TEST_PLACE");
    if (place && *place) {
        g_placeFree = strncmp(place, "free+", 5) == 0;
        g_place = sscanf(place + (g_placeFree ? 5 : 0), "%lu:%f,%f,%f,%f", &g_placeFrame, &g_placePos[0],
                         &g_placePos[1], &g_placePos[2], &g_placeAng) == 5;
        if (!g_place) {
            re4_log("run: RE4_TEST_PLACE \"%s\" not understood ([free+]F:x,y,z,a)", place);
        }
    }
    const char* stop = getenv("RE4_STOP_AFTER");
    g_stopAfter = stop ? strtoul(stop, 0, 10) : 0;
    const char* freeScript = getenv("RE4_PAD_FREE_SCRIPT");
    const char* hud = getenv("RE4_DEBUG_HUD");
    g_hideDebugHud = hud && *hud == '0';
    if (g_trace || g_padUntilRoom || g_stopAfter || g_place || g_hideDebugHud || (freeScript && *freeScript)) {
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
