// Compiled with the game's flags and headers (re4_game_flags) for the runtime
// test "math": checks of what only exists inside game units -- the inline
// fabsf of math_sub.h, the EmRoutineSetW macro of em.h (patch 0023), the
// big-endian views of re4_whole_prelude.h and the length of __FILE__; for the
// test "partner" SetSubDamage on Luis (patch 0087); for the test "route"
// getNearPoint from a position far outside the room (patch 0088).
#include "types.h"
#include "math_sub.h"
#include "em.h"
#include "dolphin/gx/GXStruct.h"
#include "dolphin/gx/GXFifo.h"
#include "global.h"
#include "main_mem.h"
#include "esp.h"
#include "at_sub.h"
#include "cam_extra.h"
#include "obj.h"
#include "light.h"
#include "pl14.h"
#include "pl_sub.h"
#include "route_ck.h"

#include <new>
#include <stdlib.h>
#include <string.h>

// The SDK casts GXFifoObj (a byte array) to a struct of words (patch 0038).
static_assert(__alignof__(GXFifoObj) >= 4, "GXFifoObj must be word aligned for ARM");
// A subclass's work starts at the end of cEsp, 0xF8 as on the GameCube, with
// the vptr first here and last there (patch 0040 finds it by this size).
static_assert(sizeof(void*) != 4 || sizeof(cEsp) == 0xF8, "cEsp must keep the GameCube size (ILP32)");

struct ProbeRoutine {  // the four routine bytes of cModel (r_no_0..3)
    u8 r_no_0;
    u8 r_no_1;
    u8 r_no_2;
    u8 r_no_3;
};

struct ProbeAxAddr {  // the AX address pair the sound driver reads as one word
    u16 currentAddressHi;
    u16 currentAddressLo;
};

extern "C" float re4probe_fabsf(float x)
{
    return fabsf(x);
}

extern "C" void re4probe_routine(unsigned char out[4])
{
    ProbeRoutine r = {9, 9, 9, 9};
    ProbeRoutine* em = &r;
    EmRoutineSetW(em, 1, 0x2E, 3, 4);
    out[0] = r.r_no_0;
    out[1] = r.r_no_1;
    out[2] = r.r_no_2;
    out[3] = r.r_no_3;
}

// Returns a bit mask of failed checks (0 = all passed).
// The attribute word of a collision polygon as the hit checks see it (patch
// 0041): the stored word, in either byte order of the host.
extern "C" unsigned re4probe_poly_attr(unsigned status)
{
    SAT_POLY poly = {};
    poly.m_Status = status;
    return Get_poly_attr(&poly);
}

// Float to a small integer type in game code (-fno-strict-float-cast-overflow):
// clamped to the type's range as ProDG's -mfast-cast psq_st conversion does
// (the title logo's alpha went 300 -> 44 when it wrapped).
extern "C" unsigned re4probe_cast_u8(float f)
{
    return (u8) f;
}

extern "C" int re4probe_cast_s16(float f)
{
    return (s16) f;
}

// A finished special camera deleted twice, as CameraControl does (m_pProc stays
// set after `delete m_pProc`): the subclass destructor poisons the object with
// 9s, cCamera's destructor must leave a usable vtable pointer (patch 0046).
// 1 when the pointer is the poison (the second delete would crash).
extern "C" int re4probe_camera_double_delete(void)
{
    static u64 store[0x200 / 8];
    cCamera* c = new (store) CameraPushObject();
    delete c;
    u32 first;
    memcpy(&first, store, 4);  // the vtable pointer of the port's layout
    if (first == 0x09090909u) {
        return 1;
    }
    delete c;
    return 0;
}

// The debug page (state_trace.cpp: RE4_DEBUG_HUD=0, patch 0043); set < 0 only reads.
extern "C" int re4probe_debug_mode(int set)
{
    if (set >= 0) {
        pG->debug_mode = (s8) set;
    }
    return pG->debug_mode;
}

// The state trace after a soft reset (state_trace.cpp inArray): ObjMgr's alive list as the
// title found it there, two works of the manager's array and then a next pointer into memory the
// title used again (PC check 18.10.2026: the trace followed it and crashed). set 1 puts the list
// in place (step 3, so the trace counts the works), 0 puts ObjMgr and the step back.
extern "C" void re4probe_stale_list(int set)
{
    static u8 works[2 * 0x400] __attribute__((aligned(16)));
    static cObj* array;
    static u32 n, size;
    static cObj* alive;
    static u8 step;
    static_assert(sizeof(cUnit) <= 0x400, "the fake works hold a cUnit each");
    if (set) {
        array = ObjMgr.pArray;
        n = ObjMgr.nArray;
        size = ObjMgr.size;
        alive = ObjMgr.pAlive;
        step = pG->Rno0;
        memset(works, 0, sizeof(works));
        ((cUnit*) works)->setNext((cUnit*) (works + 0x400));
        ((cUnit*) (works + 0x400))->setNext((cUnit*) (unsigned long) 0xC7D14C2Bul);
        ObjMgr.pArray = (cObj*) works;
        ObjMgr.nArray = 2;
        ObjMgr.size = 0x400;
        ObjMgr.pAlive = (cObj*) works;
        pG->Rno0 = 3;
    } else {
        ObjMgr.pArray = array;
        ObjMgr.nArray = n;
        ObjMgr.size = size;
        ObjMgr.pAlive = alive;
        pG->Rno0 = step;
    }
}

// Light type 5 (light05.cpp) over the work bytes of a room light record, which keep the GameCube
// form: the path number Id at 0xC and Flag at 0xD, the path follower cLightPath before them (fixed
// form since 19.10.2026; on LP64 its two pointers had moved Id to 0x18, compare run r107). A light
// path file in MEM1 with two paths, 0: 200 and 1: 50, 100; the light asks for path 1. Returns
// DispCol.r after two frames with Col.r 200: 100 (0.5 * 200) when path 1 plays, 200 for path 0.
extern "C" int re4probe_light05(void)
{
    static_assert(__builtin_offsetof(LIT05_MOVE_FREE, Id) == 0xC, "LIT05 path number at its GameCube offset");
    static u8 buf[sizeof(cLight)] __attribute__((aligned(16)));
    u8* file = (u8*) (unsigned long) 0x82E00000ul;
    memset(file, 0, 0x20);
    file[0] = 2;
    *(u32*) (file + 4) = 0x10;
    *(u32*) (file + 8) = 0x14;
    file[0x10] = 200;
    file[0x11] = 0xFF;
    file[0x14] = 50;
    file[0x15] = 100;
    file[0x16] = 0xFF;
    if (!LightMgr.initPath((LightPathHeader*) file)) {
        return -1;
    }
    cLight* l = new (buf) cLight;
    l->Rno0 = 0;
    l->Col.r = l->Col.g = l->Col.b = 200;
    l->Col.a = 255;
    memset(l->work, 0, sizeof(l->work));
    l->work[0xC] = 1;
    l->work[0xD] = 0;
    Light05_Move(l);
    Light05_Move(l);
    return l->DispCol.r;
}

extern "C" int re4probe_byte_views(void)
{
    int failed = 0;
    GXColor c = {0, 0, 0, 0};
    // fade.h: `*(u32*) &col.start = 0xFF` is black with full alpha on the GameCube
    RE4_BE(u32, 1, &c) = 0xFF;
    if (c.r != 0 || c.g != 0 || c.b != 0 || c.a != 0xFF) {
        failed |= 1;
    }
    RE4_BE(u32, 1, &c) = 0x11223344;
    if (c.r != 0x11 || c.g != 0x22 || c.b != 0x33 || c.a != 0x44 || RE4_BE(u32, 1, &c) != 0x11223344) {
        failed |= 2;
    }
    // light.cpp: `(*(u32*) &l->DispCol & 0xFFFFFF00) == 0` tests r, g, b
    c.r = c.g = c.b = 0;
    if ((RE4_BE(u32, 1, &c) & 0xFFFFFF00) != 0) {
        failed |= 4;
    }
    // event.cpp: the EVD tag "event" read as a word
    char tag[8] = {'e', 'v', 'e', 'n', 't', 0, 0, 0};
    if (RE4_BE(u32, 1, tag) != 0x6576656E) {
        failed |= 8;
    }
    // snd_iss2.cpp: the AX Hi/Lo halfwords as one address
    ProbeAxAddr a = {0x0012, 0x3456};
    if (RE4_BE(u32, 2, &a.currentAddressHi) != 0x00123456) {
        failed |= 16;
    }
    // copy through two views keeps the bytes; compound assignment
    GXColor d = {1, 2, 3, 4};
    RE4_BE(u32, 1, &c) = RE4_BE(u32, 1, &d);
    RE4_BE(u32, 1, &c) |= 0xFF;
    if (c.r != 1 || c.g != 2 || c.b != 3 || c.a != 0xFF) {
        failed |= 32;
    }
    // motion.cpp: an s16 in a big-endian byte stream
    u8 stream[2] = {0xFF, 0x38};
    if ((s16) RE4_BE(s16, 1, stream) != -200) {
        failed |= 64;
    }
    // parts counted from the most significant end of a host value
    s16 parts = 0x0107;
    int fno = 0x00020005;
    f32 one = 1.0f;
    if (RE4_BE_AT(u8, &parts, 1) != 7 || RE4_BE_AT(u8, &parts, 0) != 1 || RE4_BE_AT(u16, &fno, 1) != 5 ||
        RE4_BE_AT(s8, &fno, 3) != 5 || RE4_BE_AT(u8, &one, 0) != 0x3F || RE4_BE_AT(u8, &one, 1) != 0x80) {
        failed |= 128;
    }
    return failed;
}

extern "C" const char* re4probe_file(void)
{
    return __FILE__;
}

// VALID_PTR (patch 0037): a stack variable of an OS thread (onThread: called
// on one), the game's static data, MEM1 and what stays invalid. Returns a bit
// mask of failed checks (0 = all passed).
extern "C" int re4probe_valid_ptr(int onThread)
{
    int failed = 0;
    volatile float local[3] = {1.0f, 2.0f, 3.0f};
    if (onThread && !VALID_PTR(&local[1])) {
        failed |= 0x1;
    }
    if (!VALID_PTR(&Global)) {
        failed |= 0x2;
    }
    if (!VALID_PTR((void*) 0x80001000)) {
        failed |= 0x4;
    }
    // host heap memory belongs to no game library and no stack
    void* heap = malloc(64);
    if (VALID_PTR((void*) 0) || VALID_PTR((void*) 0x1000) || VALID_PTR(heap)) {
        failed |= 0x8;
    }
    free(heap);
    return failed;
}

// The partner damage routine on Luis (pl_sub SetSubDamage, patch 0087): Luis (cSubLuis) is a cEm,
// not a cSubChar; the original stores the routine 4 function and his F_KARAMI status through
// cSubChar members that only on the GameCube lie on his m_pFunc and status. On zeroed Luis bytes
// (no constructor: SetSubDamage touches only plain members) as pSUB, id 4. Returns 0 when his
// m_pFunc, status, routine and catcher are set and his analysis work is untouched, else a bit
// for each fault.
static void probeLuisFunc() {}

extern "C" int re4probe_luis_damage(void)
{
    static u8 buf[sizeof(cSubLuis)] __attribute__((aligned(16)));
    static u8 catcher[sizeof(cEm)] __attribute__((aligned(16)));
    u8 analysis[sizeof(cAnalysis)];
    cSubLuis* luis = (cSubLuis*) buf;
    cEm* keep = pSUB;
    int bad = 0;

    memset(buf, 0, sizeof(buf));
    luis->id = 4;
    memset(&luis->analysis, 0x5A, sizeof(cAnalysis));
    memcpy(analysis, &luis->analysis, sizeof(cAnalysis));
    pSUB = (cEm*) luis;
    SetSubDamage((cEm*) catcher, probeLuisFunc);
    pSUB = keep;
    if (luis->m_pFunc != probeLuisFunc) bad |= 1;
    if (!luis->status.check(cSubLuis::F_KARAMI)) bad |= 2;
    if (luis->r_no_0 != 4) bad |= 4;
    if (luis->pEmCatch != (cEm*) catcher) bad |= 8;
    if (memcmp(analysis, &luis->analysis, sizeof(cAnalysis)) != 0) bad |= 0x10;
    return bad;
}

// Stack garbage where the next call keeps its locals (route test): a float-like pattern, as the
// tester's report had in getNearPoint's idx[].
__attribute__((noinline)) static void probeDirtyStack(void)
{
    volatile int fill[256];
    for (int i = 0; i < 256; i++) {
        fill[i] = 0x46464646;
    }
}

// getNearPoint (patch 0088) over route data with three points in MEM1 (host order, as the port
// loads it): a position near point 1 finds it; a position 2e8 away is out of reach of all three,
// which leaves the sorted list empty (the original then reads pt[garbage]). Returns a bit mask of
// faults: 1 near (mode 1) not point 1, 2 far (mode 1) not -1, 4 far (mode 0, the clear-line
// search) not -1.
extern "C" int re4probe_route_far(void)
{
    RTP* rtp = (RTP*) (unsigned long) 0x82E10000ul;
    RTP_POINT* pt = (RTP_POINT*) ((u8*) rtp + 0x20);
    auto keep = pG->Rtp;
    Vec near = {900.0f, 0.0f, 0.0f};
    Vec far = {0.0f, 2.0e8f, 0.0f};
    int bad = 0;

    memset(rtp, 0, 0x80);
    rtp->ID = 0x32525450;
    rtp->nPoint = 3;
    rtp->pPoint = 0x20;
    pt[1].pos.x = 1000.0f;
    pt[2].pos.x = 5000.0f;
    pG->Rtp = rtp;
    if (getNearPoint(&near, 1, 0) != 1) bad |= 1;
    probeDirtyStack();
    if (getNearPoint(&far, 1, 0) != -1) bad |= 2;
    probeDirtyStack();
    if (getNearPoint(&far, 0, 0) != -1) bad |= 4;
    pG->Rtp = keep;
    return bad;
}
