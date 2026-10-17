// Compiled with the game's flags and headers (re4_game_flags) for the runtime
// test "math": checks of what only exists inside game units -- the inline
// fabsf of math_sub.h, the EmRoutineSetW macro of em.h (patch 0023), the
// big-endian views of re4_whole_prelude.h and the length of __FILE__.
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
