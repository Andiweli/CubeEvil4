// Formatumwandler: light data (LIT, include/light.h cLit / cLightEnv /
// cLightWork; game/light.cpp cLightMgr::roomInit, cLit::versionUp).
//
// File: u16 cut count, u8 version, u8 max lights, u32 cut offsets (0 = none);
// a cut is a cLightEnv (0x104 bytes) followed by nLight cLightWork records
// (0x12C). The per-light work area (cLightWork::sub) is interpreted by the
// move handler (LightFuncTbl[Id], light01..light10.cpp). Field catalogue as
// the earlier port's checked reader (port/src/lights.cpp resolveLightBlock).
#include "formats.h"

#include "light.h"
#include "re4_runtime.h"

using namespace re4fmt;

static_assert(sizeof(cLightEnv) == 0x104, "cLightEnv layout");
static_assert(sizeof(cLightWork) == 0x12C, "cLightWork layout");
static_assert(offsetof(cLightWork, sub) == 0x6C, "cLightWork sub");
static_assert(sizeof(LIT04_MOVE_FREE) == 0x1C, "LIT04_MOVE_FREE layout");

namespace re4fmt {

namespace {

void convertEnv(Blob& b, u32 o)
{
    typedef cLightEnv E;
    RE4_SWAP(b, o, E, nLight);
    swap32(b, o + offsetof(E, Fog), 3);        // Type, Start, End (Color: bytes)
    swap32(b, o + offsetof(E, MirrorFog), 3);
    RE4_SWAP(b, o, E, FocusZ);
    RE4_SWAP(b, o, E, far_play_ratio);
    RE4_SWAP(b, o, E, lod_bias);
}

void convertWork(Blob& b, u32 o)
{
    typedef cLightWork W;
    u8 id = b.data[o + offsetof(W, Id)];
    swap32(b, o + offsetof(W, Pos), 3);
    RE4_SWAP(b, o, W, Radius);
    RE4_SWAP(b, o, W, Intensity);
    RE4_SWAP(b, o, W, ParentNo);
    RE4_SWAP(b, o, W, HitRadius);
    RE4_SWAP(b, o, W, Dummy82);
    RE4_SWAP(b, o, W, Dummy9);
    // spot / cone / parallel block by Type: words up to K2 (LIT_TYPE04_FREE
    // 0x00..0x24), the rest of the block is unused padding
    swap32(b, o + offsetof(W, spot), 9);
    u32 s = o + offsetof(W, sub);
    switch (id) {
    case 2:  // LIT02_MOVE_FREE: four floats
        swap32(b, s, 4);
        break;
    case 3:  // LIT03_MOVE_FREE: Vec
    case 6:  // LIT06_MOVE_FREE: three floats
        swap32(b, s, 3);
        break;
    case 7:  // LIT07_MOVE_FREE: two Vecs
        swap32(b, s, 6);
        break;
    case 4:  // LIT04_MOVE_FREE: Flag, Kind/Tex_no (bytes), Ang_x/Ang_y, bytes, Lit_pos
        RE4_SWAP(b, s, LIT04_MOVE_FREE, Flag);
        RE4_SWAP(b, s, LIT04_MOVE_FREE, Ang_x);
        RE4_SWAP(b, s, LIT04_MOVE_FREE, Ang_y);
        swap32(b, s + offsetof(LIT04_MOVE_FREE, Lit_pos), 3);
        break;
    default:  // 0, 1 (colour, flicker byte), 5 (path id / flag bytes; the
              // path follower is runtime state), 8 and 0x10 (bytes)
        break;
    }
}

}  // namespace

void convertLitAt(Blob& b, u32 base)
{
    if (!fits(b, base, 4, "cLit")) {
        return;
    }
    u32 n = be16(b, base);
    if (!fits(b, base + 4, n * 4, "LIT cut table")) {
        return;
    }
    u32 offs[1024];
    if (n > 1024) {
        fits(b, base, 0xFFFFFFFFu, "LIT cut count");
        return;
    }
    for (u32 i = 0; i < n; i++) {
        offs[i] = be32(b, base + 4 + i * 4);
    }
    swap16(b, base);
    swap32(b, base + 4, n);
    Seen seen;
    for (u32 i = 0; i < n && b.ok; i++) {
        u32 c = base + offs[i];
        if (offs[i] == 0 || !seen.first(c) || !fits(b, c, sizeof(cLightEnv), "cLightEnv")) {
            continue;
        }
        u32 lights = be32(b, c + offsetof(cLightEnv, nLight));
        if (!fits(b, c + sizeof(cLightEnv), lights * sizeof(cLightWork), "cLightWork records")) {
            break;
        }
        convertEnv(b, c);
        for (u32 k = 0; k < lights; k++) {
            convertWork(b, c + sizeof(cLightEnv) + k * sizeof(cLightWork));
        }
    }
}

void convertLit(Blob& b, const Part&)
{
    convertLitAt(b, 0);
}

}  // namespace re4fmt
