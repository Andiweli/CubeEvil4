// Compiled with the game's flags and headers (re4_game_flags) for the runtime
// test "math": checks of what only exists inside game units -- the inline
// fabsf of math_sub.h, the EmRoutineSetW macro of em.h (patch 0023), the
// big-endian views of re4_whole_prelude.h and the length of __FILE__; for the
// test "partner" SetSubDamage on Luis (patch 0087); for the test "route"
// getNearPoint from a position far outside the room (patch 0088); for the test
// "enemylog" the enemy and menu lines of the error log (state_trace.cpp).
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
#include "em10.h"
#include "joy.h"
#include "player.h"
#include "sce_at.h"
#include "area.h"

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

extern "C" void re4_state_trace_enemy_frame(unsigned n);

// The error log's enemy and menu lines (state_trace.cpp enemyLog / menuLog) over two fake works
// of EmMgr in MEM1: slot 1 becomes a Ganado (0x1a type 3) of room r209, is hit on the head (part
// 5, weapon 0x10), changes its routine, loses its head, is hidden, jumps and goes; then the C
// stick moves the cursor of a sub screen. The test reads the lines from the log's tail.
extern "C" void re4probe_enemy_log(void)
{
    u8* works = (u8*) (unsigned long) 0x82E20000ul;
    const u32 stride = (u32) sizeof(cEm10);
    cEm* keepArray = EmMgr.pArray;
    u32 keepCount = EmMgr.nArray, keepSize = EmMgr.size;
    u8 keepStep = pG->Rno0;
    u16 keepRoom = pG->room_id;
    JOY keepJoy = Joy[0];
    int keepSub = StaFlagChk(pG, STA_SUB_SCRN) != 0;

    memset(works, 0, 2 * stride);
    EmMgr.pArray = (cEm*) works;
    EmMgr.nArray = 2;
    EmMgr.size = stride;
    pG->Rno0 = 3;
    pG->room_id = 0x209;
    StaFlagOff(pG, STA_SUB_SCRN);
    cEm* e = (cEm*) (works + stride);
    re4_state_trace_enemy_frame(1000);  // the new array: no line
    e->be_flag = 1 | 2;
    e->id = 0x1A;
    e->type = 3;
    e->emset_no = 5;
    e->hp = 300;
    e->pos.x = 100.0f;
    e->pos.z = 200.0f;
    e->r_no_0 = 1;
    re4_state_trace_enemy_frame(1001);  // em: + s1
    e->hp = 100;
    e->hitInfo.parts_no = 5;
    e->dmg.m_pDamageYarare = &e->hitInfo;
    e->dmg.m_Wep = 0x10;
    e->r_no_0 = 2;
    e->r_no_1 = 4;
    EM10_WK(e)->Be_flg = 0x4000;
    re4_state_trace_enemy_frame(1002);  // em: hit s1
    e->r_no_1 = 9;
    re4_state_trace_enemy_frame(1003);  // em: s1 r 2/9/0/0
    EM10_WK(e)->Be_flg |= 0x80;
    re4_state_trace_enemy_frame(1004);  // em: s1 head lost
    e->be_flag &= ~2u;
    re4_state_trace_enemy_frame(1005);  // em: s1 hidden
    e->pos.x = 5000.0f;
    re4_state_trace_enemy_frame(1006);  // em: s1 jumps
    e->be_flag = 0;
    re4_state_trace_enemy_frame(1007);  // em: - s1
    StaFlagOn(pG, STA_SUB_SCRN);
    Joy[0].rep = 0x00100000;
    Joy[0].substickX = 12;
    re4_state_trace_enemy_frame(1008);  // pad: menu frame 1008
    Joy[0] = keepJoy;
    if (!keepSub) {
        StaFlagOff(pG, STA_SUB_SCRN);
    }
    pG->Rno0 = keepStep;
    pG->room_id = keepRoom;
    EmMgr.pArray = keepArray;
    EmMgr.nArray = keepCount;
    EmMgr.size = keepSize;
    re4_state_trace_enemy_frame(1009);  // the old array again: starts over without a line
}

extern cPlayer* pPL;

// The head of an AEV file as SceAtInit reads it (sce_at.cpp SceAtFileHead).
struct SceAtProbeHead {
    char magic[4];
    u16 version;
    u16 num;
    u8 pad[8];
};
static_assert(sizeof(cPlayer) <= 0x10000, "the fake player must fit below the fake scene areas");

// Leon's side of a death in the error log (state_trace.cpp playerLog) with a fake player and
// one scene area in MEM1: r21d starts running, the death demo starts with the areas 0x05 (a
// quadrilateral for the player) and 0x13 hit, Leon's position stops being a number, the room
// runs again (the continue); then an enemy of a fake EmMgr array goes to NaN (enemyLog).
extern "C" void re4probe_death_log(void)
{
    cPlayer* keepPl = pPL;
    cEm* keepSub = pSUB;
    u8 keepStep = pG->Rno0;
    u16 keepRoom = pG->room_id;
    u16 keepLife = pG->pl_life, keepMax = pG->pl_life_max;
    u8 keepWeapon = pG->weapon_no;
    int keepDying = StaFlagChk(pG, STA_DIEDEMO) != 0;
    cEm* keepArray = EmMgr.pArray;
    u32 keepCount = EmMgr.nArray, keepSize = EmMgr.size;

    u8* aev = (u8*) (unsigned long) 0x82E50000ul;
    memset(aev, 0, 0x10 + sizeof(SCE_AT_DATA));
    memcpy(aev, "AEV", 4);
    ((SceAtProbeHead*) aev)->version = 0x104;
    ((SceAtProbeHead*) aev)->num = 1;
    SCE_AT_DATA* w = (SCE_AT_DATA*) (aev + 0x10);
    w->no = 5;
    w->be_flg = 1;
    w->trg_type = 1;
    w->target_type = 1;
    w->parts_no = -1;
    w->area.type = AREA_TYPE_XZ4;
    w->area.xz4.height = 2000.0f;
    w->area.xz4.p[1].x = 1000.0f;
    w->area.xz4.p[2].x = 1000.0f;
    w->area.xz4.p[2].z = 1000.0f;
    w->area.xz4.p[3].z = 1000.0f;
    SceAtInit(aev, 0);

    cPlayer* pl = (cPlayer*) (unsigned long) 0x82E40000ul;
    memset((void*) pl, 0, sizeof(cPlayer));
    pPL = pl;
    pSUB = 0;
    pl->pos.x = 100.0f;
    pl->pos.z = 200.0f;
    pl->ang.y = 0.5f;
    pG->room_id = 0x21D;
    pG->pl_life = 1200;
    pG->pl_life_max = 1200;
    pG->weapon_no = 2;
    StaFlagOff(pG, STA_DIEDEMO);
    EmMgr.pArray = 0;  // no enemy lines yet
    pG->Rno0 = 4;
    re4_state_trace_enemy_frame(2000);
    pG->Rno0 = 3;
    re4_state_trace_enemy_frame(2001);  // room: r21d runs
    SceAtSetHitFlg(5);
    SceAtSetHitFlg(0x13);
    StaFlagOn(pG, STA_DIEDEMO);
    re4_state_trace_enemy_frame(2003);  // die: r21d after 2 frames, die: area 0x05
    pl->pos.x = __builtin_nanf("");
    re4_state_trace_enemy_frame(2004);  // pl: not a number
    pG->Rno0 = 4;
    re4_state_trace_enemy_frame(2005);
    StaFlagOff(pG, STA_DIEDEMO);
    pl->pos.x = 100.0f;
    pG->Rno0 = 3;
    re4_state_trace_enemy_frame(2006);  // room: r21d runs ..., continue after a death

    u8* works = (u8*) (unsigned long) 0x82E20000ul;
    const u32 stride = (u32) sizeof(cEm10);
    memset(works, 0, 2 * stride);
    EmMgr.pArray = (cEm*) works;
    EmMgr.nArray = 2;
    EmMgr.size = stride;
    cEm* e = (cEm*) (works + stride);
    e->be_flag = 1 | 2;
    e->id = 0x2D;
    e->hp = 100;
    re4_state_trace_enemy_frame(2007);  // em: + s1 0x2d/0
    e->pos.y = __builtin_nanf("");
    re4_state_trace_enemy_frame(2008);  // em: s1 position is not a number

    EmMgr.pArray = keepArray;
    EmMgr.nArray = keepCount;
    EmMgr.size = keepSize;
    SceAtInit(0, 0);
    pPL = keepPl;
    pSUB = keepSub;
    pG->Rno0 = keepStep;
    pG->room_id = keepRoom;
    pG->pl_life = keepLife;
    pG->pl_life_max = keepMax;
    pG->weapon_no = keepWeapon;
    if (keepDying) {
        StaFlagOn(pG, STA_DIEDEMO);
    }
}

extern "C" unsigned re4_arc_clamp_count(void);

// acosf / asinf of the game code (re4_whole_prelude.h re4_acosf, whole_prepare.py ARC_CALL): the
// insects' wall step (em2d/em2c em2dSetWallMatrix2: sinf(acosf(dot(nrm, wallNrm))) * 700 added
// to the position) on a slope with the normal (0.1, 1, -0.05), all four probes on that face, with
// the port's own PSVECNormalize / PSVECAdd / PSVECDotProduct: the dot product comes out above 1,
// which made the insect's position NaN. Bits: 1 the dot product is not above 1 here (the case
// is not reproduced), 2 the step is not a number, 4 a rounding step beyond 1 is not 0 / pi /
// pi/2, 8 further out is not NaN (as on the GameCube), 16 an ordinary value changed, 32 the
// clamps were not counted.
extern "C" int re4probe_arc(void)
{
    int bad = 0;
    unsigned before = re4_arc_clamp_count();
    Vec raw = {0.1f, 1.0f, -0.05f};
    Vec nrm, sum, wall;
    PSVECNormalize(&raw, &nrm);
    PSVECAdd(&nrm, &nrm, &sum);
    PSVECAdd(&nrm, &sum, &sum);
    PSVECAdd(&nrm, &sum, &sum);
    PSVECNormalize(&sum, &wall);
    f32 dot = PSVECDotProduct(&nrm, &wall);
    if (!(dot > 1.0f)) bad |= 1;
    f32 step = sinf(re4_acosf(dot)) * 700.0f;
    if (step - step != 0.0f) bad |= 2;
    volatile f32 up = 1.00000012f, down = -1.00000012f, far = 1.001f, half = 0.5f;
    if (re4_acosf(up) != 0.0f || re4_acosf(down) != acosf(-1.0f) || re4_asinf(up) != asinf(1.0f)) bad |= 4;
    f32 out = re4_acosf(far);
    if (out == out) bad |= 8;
    if (re4_acosf(half) != acosf(half) || re4_asinf(half) != asinf(half)) bad |= 16;
    if (re4_arc_clamp_count() - before != (dot > 1.0f ? 4u : 3u)) bad |= 32;
    return bad;
}
