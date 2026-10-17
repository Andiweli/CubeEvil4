// Depth of field of the core mode (patch 0065, src/game/filter01.cpp
// Filter01Trans), compiled with the game's flags into libre4_main:
//
//  - The log says which blur the game's own filter queues each frame (the
//    focus of a room's light cut, the near / far focus curves of an event)
//    and when that changes ("dof: ..."), so a device log shows whether a
//    scene has the game's depth of field at all.
//  - The app's gameplay autofocus (wish of the project owner, 17.10.2026:
//    "as in RE4-Decomp", the first Android port, depth_of_field_gl.cpp there):
//    while the player is free and the game asks for no focus, the background
//    behind what the centre of the picture shows gets soft. Not a GameCube
//    effect; the menu's depth of field switch turns it off with the game's
//    own blur. The core marks its place in the GX command stream with a write
//    of BP register 0x5A, which the GameCube does not use (re4_gx_frame.h);
//    the app's GX renderer (gx_gl.cpp, Autofocus) draws the blur there: in
//    the filter table (OT 0x12) like the game's own depth of field, after
//    the scene and before the tables of the interface (0x13..0x15).
//    Leon and his weapon are no focus (Retroid, 17.10.2026: standing, the
//    centre of the picture is on Leon, and all behind him went soft): the
//    marker's value is how deep Leon reaches into the picture (camera depth
//    of his and the weapon's joints plus a margin); the renderer takes the
//    focus only from what lies deeper.
//    RE4_DOF_AUTOFOCUS=0 leaves it out (the game's own picture only).
#include "types.h"
#include "global.h"
#include "em.h"
#include "model.h"
#include "player.h"
#include "pl_wep.h"
#include "re4_gx_fifo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern GLOBAL_WK* pG;
extern cPlayer* pPL;

extern "C" void re4_log(const char* fmt, ...);

namespace {

// one queued pass: kind 1 light cut, 2 event near, 3 event far, 4 autofocus
struct Pass {
    int kind, mode, level, type, z;
};

const char* const kKind[] = {"", "light cut", "event near", "event far", "autofocus (app)"};
const int kMaxPasses = 4;
const unsigned kMaxLines = 200;

Pass gNow[kMaxPasses];  // this frame's passes
int gNowCount;
Pass gShown[kMaxPasses];  // as last logged
int gShownCount = -1;
unsigned gFrames, gLines;
int gAutofocus = -1;  // RE4_DOF_AUTOFOCUS, read once

bool samePasses()
{
    if (gNowCount != gShownCount) {
        return false;
    }
    for (int i = 0; i < gNowCount; i++) {
        const Pass& a = gNow[i];
        const Pass& b = gShown[i];
        // the focus depth follows the event's curve every frame: not a change
        if (a.kind != b.kind || a.mode != b.mode || a.level != b.level || a.type != b.type) {
            return false;
        }
    }
    return true;
}

// The previous frame's passes into the log when they differ from the last logged.
void flush()
{
    if (samePasses()) {
        return;
    }
    memcpy(gShown, gNow, sizeof(gShown));
    gShownCount = gNowCount;
    if (gLines >= kMaxLines) {
        return;
    }
    if (++gLines == kMaxLines) {
        re4_log("dof: further changes not logged");
        return;
    }
    char line[256];
    int at = snprintf(line, sizeof(line), "dof: from frame %u: ", gFrames - 1);
    if (gNowCount == 0) {
        snprintf(line + at, sizeof(line) - at, "none");
    }
    for (int i = 0; i < gNowCount && at < (int) sizeof(line); i++) {
        const Pass& p = gNow[i];
        if (p.kind == 4) {
            at += snprintf(line + at, sizeof(line) - at, "%s%s", i ? "; " : "", kKind[p.kind]);
        } else {
            at += snprintf(line + at, sizeof(line) - at, "%s%s, %s, level %d, type %d, focus z %d",
                           i ? "; " : "", kKind[p.kind], p.mode ? "behind the focus" : "in front of the focus",
                           p.level, p.type, p.z);
        }
    }
    re4_log("%s", line);
}

}  // namespace

// Patch 0065: kind 0 at the start of each frame's Filter01Trans (with blur
// permission), then one call per queued pass.
extern "C" void re4_dof_note(int kind, int mode, f32 level, int z, int type)
{
    if (kind == 0) {
        if (gFrames++ > 0) {
            flush();
        }
        gNowCount = 0;
        return;
    }
    if (gNowCount < kMaxPasses && kind < 5) {
        Pass& p = gNow[gNowCount++];
        p.kind = kind;
        p.mode = mode;
        p.level = (int) level;
        p.type = type;
        p.z = z;
    }
}

// Patch 0065: 1 when the gameplay autofocus is wanted this frame (the game
// queued no focus of its own): the player is free, as state_trace.cpp has
// it: main loop, no event, no sub screen.
extern "C" int re4_dof_autofocus(void)
{
    if (gAutofocus < 0) {
        const char* e = getenv("RE4_DOF_AUTOFOCUS");
        gAutofocus = !(e && e[0] == '0');
        if (!gAutofocus) {
            re4_log("dof: the app's gameplay autofocus is off (RE4_DOF_AUTOFOCUS=0)");
        }
    }
    GLOBAL_WK* g = pG;
    return gAutofocus && g && g->Rno0 == 3 && !StaFlagChk(g, STA_EVENT) && !StaFlagChk(g, STA_SUB_SCRN);
}

namespace {

bool inMem1(const void* p)
{
    unsigned long a = (unsigned long) p;
    return a >= 0x80000000ul && a < 0x83000000ul;
}

// The deepest camera depth of the joints of model m (cParts::mat, the world
// matrices of this frame) along the view direction `look` from `eye`.
f32 deepestJoint(const cModel* m, const Vec& eye, const Vec& look, f32 deepest)
{
    if (!inMem1(m)) {
        return deepest;
    }
    const cParts* p = m->pList;
    for (u32 i = 0; i < m->nParts && inMem1(p); i++, p = p->pList) {
        const f32 d = (p->mat[0][3] - eye.x) * look.x + (p->mat[1][3] - eye.y) * look.y +
                      (p->mat[2][3] - eye.z) * look.z;
        if (d > deepest) {
            deepest = d;
        }
    }
    return deepest;
}

}  // namespace

// How deep Leon and his weapon reach into the picture (game units along the
// camera's view), 0 when there is no player.
extern "C" unsigned re4_dof_player_depth(void)
{
    GLOBAL_WK* g = pG;
    cPlayer* pl = pPL;
    if (!g || !inMem1(pl)) {
        return 0;
    }
    const Vec& eye = g->Camera.param.Campos;
    const Vec& at = g->Camera.param.Target;
    Vec look = { at.x - eye.x, at.y - eye.y, at.z - eye.z };
    const f32 len = sqrtf(look.x * look.x + look.y * look.y + look.z * look.z);
    if (!(len > 1.0f)) {
        return 0;
    }
    look.x /= len;
    look.y /= len;
    look.z /= len;
    f32 deepest = deepestJoint(pl, eye, look, 0.0f);
    if (inMem1(pl->Wep) && inMem1(pl->Wep->m_pWep)) {
        deepest = deepestJoint(pl->Wep->m_pWep, eye, look, deepest);
    }
    if (!(deepest > 0.0f)) {
        return 0;
    }
    // joints are inside the body: the chest, the hand around the grip and a
    // barrel in front of it reach about this much further
    deepest += 600.0f;
    return deepest < 16777215.0f ? (unsigned) deepest : 16777215u;
}

// The OT callback (filter table 0x12): the marker in the GX command stream,
// its value Leon's depth (at least 1).
extern "C" void re4_dof_autofocus_mark(void* w)
{
    (void) w;
    unsigned depth = re4_dof_player_depth();
    re4_gx_put_u8(0x61);
    re4_gx_put_u32(0x5Au << 24 | (depth ? depth : 1));
}
