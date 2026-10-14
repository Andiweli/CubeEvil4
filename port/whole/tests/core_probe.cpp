// Compiled with the game's flags and headers: builds a fake game state for the
// core bridge test (tests/core_tests.cpp) -- one live enemy work in EmMgr with
// two parts and one model info, a camera and a room in pG -- and restores the
// original manager state afterwards. Only the fields the bridge reads are
// set; no game code runs on the fake objects.
#include "types.h"
#include "em.h"
#include "global.h"
#include "model.h"
#include "obj.h"

#include <string.h>

extern GLOBAL_WK* pG;
extern cEmMgr EmMgr;
extern cObjMgr ObjMgr;
extern "C" void re4_scroll_placed(void* obj, int blk, int index);

namespace {

cEm* gSavedArray;
u32 gSavedCount;
u32 gSavedSize;
u16 gSavedRoom;
u8 gSavedRno0;
CAMERA_POINT gSavedCamera;

void setMatrix(Mtx m, float tx, float ty, float tz)
{
    memset(m, 0, sizeof(Mtx));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
    m[0][3] = tx;
    m[1][3] = ty;
    m[2][3] = tz;
}

// what the cModel / cModelInfo constructors set: fully visible, white
void visible(cModel* m)
{
    m->invisible_factor = 1.0f;
    m->invisible_factor2 = 1.0f;
}

void visible(cModelInfo* info)
{
    info->invisible_factor = 1.0f;
    memset(info->color, 0xFF, 4);
}

}  // namespace

// `mem`: zeroed scratch memory in MEM1 (at least 0x6000 bytes), `bin` and `tpl`
// the addresses of two converted archive entries.
extern "C" void re4probe_core_setup(void* mem, void* bin, void* tpl)
{
    u8* base = (u8*) mem;
    cEm* em = (cEm*) base;
    cParts* parts = (cParts*) (base + 0x2000);
    cModelInfo* info = (cModelInfo*) (base + 0x3000);
    // a second, rigid info hung on parts 1 (cModelData with one part, no
    // weights) and moved up by its own matrix
    cModelInfo* rigid = (cModelInfo*) (base + 0x3400);
    cModelData* data = (cModelData*) (base + 0x3800);
    ModelDataHead* head = (ModelDataHead*) (base + 0x3900);
    em->be_flag = 1 | 2 | 4;
    em->id = 0x12;
    em->type = 3;
    visible(em);
    em->z_mode = 1;  // as the officers' heads in r100's events: depth test only
    em->nParts = 2;
    setMatrix(em->mat, 100.0f, 0.0f, -50.0f);
    em->pList = &parts[0];
    for (int i = 0; i < 2; i++) {
        setMatrix(parts[i].mat, 100.0f, 10.0f * (i + 1), -50.0f);
        setMatrix(parts[i].lt_inv_mat, 0.0f, -10.0f * (i + 1), 0.0f);
        parts[i].pList = i == 0 ? &parts[1] : 0;
    }
    info->model_addr = (cModelData*) bin;
    info->tpl_addr = tpl;
    info->be_flag = 8;
    visible(info);
    info->blend_mode = 2;  // added on (a glow)
    info->color[3] = 0x80;
    setMatrix(info->mat, 0.0f, 0.0f, 0.0f);
    info->pList = rigid;
    head->partsNo = 1;
    data->pHead = head;
    data->nParts = 1;
    data->weight_palette_num = 1;
    data->weight_ext_num = 0;
    rigid->model_addr = data;
    rigid->tpl_addr = tpl;
    rigid->be_flag = 8;
    visible(rigid);
    setMatrix(rigid->mat, 0.0f, 5.0f, 0.0f);
    // seven more skinned infos like Leon's (nine in all: body, costume, knife,
    // face, head, hair, hands); the last one with the hair's be_flag 0x40
    cModelInfo* last = rigid;
    for (int i = 0; i < 7; i++) {
        cModelInfo* more = (cModelInfo*) (base + 0x4000 + i * 0x400);
        more->model_addr = (cModelData*) bin;
        more->tpl_addr = tpl;
        more->be_flag = i == 6 ? 0x48 : 8;
        visible(more);
        setMatrix(more->mat, 0.0f, 0.0f, 0.0f);
        more->pList = 0;
        last->pList = more;
        last = more;
    }
    em->pModelInfo = info;

    gSavedArray = EmMgr.pArray;
    gSavedCount = EmMgr.nArray;
    gSavedSize = EmMgr.size;
    EmMgr.pArray = em;
    EmMgr.nArray = 1;
    EmMgr.size = 0x2000;

    gSavedRoom = pG->room_id;
    gSavedRno0 = pG->Rno0;
    gSavedCamera = pG->Camera.param;
    pG->room_id = 0x100;
    pG->Rno0 = 3;
    pG->Camera.param.Campos.x = 1.0f;
    pG->Camera.param.Campos.y = 2.0f;
    pG->Camera.param.Campos.z = 3.0f;
    pG->Camera.param.Target.x = 4.0f;
    pG->Camera.param.Target.y = 5.0f;
    pG->Camera.param.Target.z = 6.0f;
    pG->Camera.param.Roll = 0.25f;
    pG->Camera.param.Fovy = 50.0f;
}

extern "C" void re4probe_core_restore(void)
{
    EmMgr.pArray = gSavedArray;
    EmMgr.nArray = gSavedCount;
    EmMgr.size = gSavedSize;
    pG->room_id = gSavedRoom;
    pG->Rno0 = gSavedRno0;
    pG->Camera.param = gSavedCamera;
}

// The display switches the bridge follows (trans.cpp emTrans / ModelTrans),
// on the fake enemy of re4probe_core_setup (`mem` as given there): what 1 = an
// event holds the world (STA_SUSPEND), 2 = the same with the model's be_flag
// 0x800 (setNoSuspend), 3 = the model is the player and DPF_PL is on, 4 =
// DPF_EM is on (the model is an enemy), 5 = faded out completely
// (invisible_factor 0), 0 = everything back.
static u32 gSavedDisp;
static u32 gSavedStatus[4];
static cPlayer* gSavedPl;

extern "C" void re4probe_core_display(void* mem, int what)
{
    cEm* em = (cEm*) mem;
    if (what == 1) {
        gSavedDisp = pG->Disp_flg;
        memcpy(gSavedStatus, pG->Status_flg, sizeof(gSavedStatus));
        gSavedPl = pPL;
    }
    pG->Disp_flg = gSavedDisp;
    memcpy(pG->Status_flg, gSavedStatus, sizeof(gSavedStatus));
    pPL = gSavedPl;
    em->be_flag &= ~0x800;
    em->invisible_factor = 1.0f;
    switch (what) {
    case 1:
        StaFlagOn(pG, STA_SUSPEND);
        break;
    case 2:
        StaFlagOn(pG, STA_SUSPEND);
        em->be_flag |= 0x800;
        break;
    case 3:
        pPL = (cPlayer*) em;
        DpfFlagOn(pG, DPF_PL);
        break;
    case 4:
        DpfFlagOn(pG, DPF_EM);
        break;
    case 5:
        em->invisible_factor = 0.0f;
        break;
    }
}

// Scenery in ObjMgr (cObjScr, id 2), for the bridge's SMD works (patch 0029):
// slot 0 is work 5 of the room's SMD, slot 1 a room script's scenery (never
// placed from an SMD), slot 2 was work 7 of scroll block 3 but now holds other
// model data (the slot was reused). `mem`: zeroed MEM1 memory, 0x2000 bytes.
static cObj* gSavedObjArray;
static u32 gSavedObjCount;
static u32 gSavedObjSize;
static u32 gSavedEmCount;

extern "C" void re4probe_scroll_setup(void* mem, void* bin, void* other)
{
    u8* base = (u8*) mem;
    const u32 stride = 0x500;
    cParts* parts = (cParts*) (base + 3 * stride);
    cModelInfo* info = (cModelInfo*) (base + 3 * stride + 0x200);
    gSavedObjArray = ObjMgr.pArray;
    gSavedObjCount = ObjMgr.nArray;
    gSavedObjSize = ObjMgr.size;
    gSavedEmCount = EmMgr.nArray;
    ObjMgr.pArray = (cObj*) base;
    ObjMgr.nArray = 3;
    ObjMgr.size = stride;
    EmMgr.nArray = 0;
    setMatrix(parts->mat, 0.0f, 0.0f, 0.0f);
    info->model_addr = (cModelData*) bin;
    info->tpl_addr = bin;
    info->be_flag = 8;
    visible(info);
    for (int i = 0; i < 3; i++) {
        cObj* o = (cObj*) (base + i * stride);
        o->be_flag = 1 | 2 | 4;
        visible(o);
        o->id = cObjMgr::ID_SCROLL;
        o->nParts = 1;
        o->pList = parts;
        o->pModelInfo = info;
        o->blk = i == 2 ? 3 : -1;
        setMatrix(o->mat, 10.0f * i, 0.0f, 0.0f);
    }
    re4_scroll_placed(base, -1, 5);
    re4_scroll_placed(base + 2 * stride, 3, 7);
    // slot 2 reused: another model there now
    cModelInfo* info2 = (cModelInfo*) (base + 3 * stride + 0x600);
    *info2 = *info;
    info2->model_addr = (cModelData*) other;
    ((cObj*) (base + 2 * stride))->pModelInfo = info2;
}

extern "C" void re4probe_scroll_restore(void)
{
    ObjMgr.pArray = gSavedObjArray;
    ObjMgr.nArray = gSavedObjCount;
    ObjMgr.size = gSavedObjSize;
    EmMgr.nArray = gSavedEmCount;
}

// A face with a running shape (shape.cpp): two vertices, one shape channel
// whose curve holds weight 100 % and moves vertex 1 by (4, -8, 12). `mem`:
// zeroed MEM1 memory, 0x4000 bytes.
static u32 gSavedShapeCount;
static cEm* gSavedShapeArray;
static u32 gSavedShapeSize;

static void host16(u8* p, u16 v)
{
    memcpy(p, &v, 2);  // counts and frames: host order, as the converter leaves them
}

extern "C" void re4probe_shape_setup(void* mem)
{
    u8* base = (u8*) mem;
    cEm* em = (cEm*) base;
    cParts* parts = (cParts*) (base + 0x2000);
    cModelInfo* info = (cModelInfo*) (base + 0x3000);
    cModelData* d = (cModelData*) (base + 0x3400);
    s16* vtx = (s16*) (base + 0x3500);
    u8* tbl = base + 0x3600;
    u8* sd = base + 0x3800;
    em->be_flag = 1 | 2 | 4;
    em->id = 0x18;
    visible(em);
    em->nParts = 1;
    em->pList = parts;
    em->pModelInfo = info;
    setMatrix(em->mat, 0.0f, 0.0f, 0.0f);
    setMatrix(parts->mat, 0.0f, 0.0f, 0.0f);
    const s16 v[8] = { 10, 20, 30, 0, 40, 50, 60, 0 };
    memcpy(vtx, v, sizeof(v));
    d->vtxOrig = vtx;
    d->nVtx = 2;
    d->shapeOfs = (u32) (tbl - (u8*) d) - 4;
    // shape table: entry 0 -> one delta (vertex 1)
    const u32 entry[2] = { 0x10, 1 };
    memcpy(tbl, entry, sizeof(entry));
    const s16 delta[4] = { 1, 4, -8, 12 };
    memcpy(tbl + 0x10, delta, sizeof(delta));
    info->model_addr = d;
    info->be_flag = 8 | 2;
    visible(info);
    // ShapeData: 20 frames, one channel (kind 4, float keys), shape entry 0,
    // its curve at +0x20: three axes of two keys, the weight axis (y) 100
    host16(sd, 20);
    sd[2] = 1;
    host16(sd + 3, 0x0004);
    sd[5] = 0;
    const s32 curve = 0x20;
    memcpy(sd + 0xC, &curve, 4);
    u8* c = sd + 0x20;
    for (int axis = 0; axis < 3; axis++) {
        host16(c, 2);
        host16(c + 2, 0);
        host16(c + 4, 10);
        // the key values stay big-endian (motion.cpp FCC_F32 reads them bytewise)
        for (int k = 0; k < 2; k++) {
            const float key[3] = { axis == 1 ? 100.0f : 0.0f, 0.0f, 0.0f };
            for (int j = 0; j < 3; j++) {
                u32 bits;
                memcpy(&bits, &key[j], 4);
                u8* q = c + 6 + k * 12 + j * 4;
                q[0] = (u8) (bits >> 24);
                q[1] = (u8) (bits >> 16);
                q[2] = (u8) (bits >> 8);
                q[3] = (u8) bits;
            }
        }
        c += 6 + 2 * 12;
    }
    info->shape[0].rate = 0.0f;
    info->shape[0].data = (ShapeData*) sd;
    gSavedShapeArray = EmMgr.pArray;
    gSavedShapeCount = EmMgr.nArray;
    gSavedShapeSize = EmMgr.size;
    EmMgr.pArray = em;
    EmMgr.nArray = 1;
    EmMgr.size = 0x2000;
}

extern "C" void re4probe_shape_restore(void)
{
    EmMgr.pArray = gSavedShapeArray;
    EmMgr.nArray = gSavedShapeCount;
    EmMgr.size = gSavedShapeSize;
}

static_assert(sizeof(cObj) <= 0x500, "the fake scenery works overlap");
static_assert(sizeof(cEm) <= 0x2000, "the fake enemy work overlaps its parts");
static_assert(2 * sizeof(cParts) <= 0x1000, "the fake parts overlap the model info");
static_assert(sizeof(cModelInfo) <= 0x400 && sizeof(cModelData) <= 0x100, "the fake infos overlap");


// AX without sound (runtime/ax.cpp): voices, play positions, the audio frame.
// Returns the number of failed checks (each logged).
#include "dolphin/ax.h"

extern "C" void re4_log(const char* fmt, ...);
extern "C" void re4_ax_field(void);
extern "C" void re4_ax_frame(void);

static int gAxDrops;
static int gAxFrames;
static void axDrop(void*)
{
    gAxDrops++;
}
static void axFrame()
{
    gAxFrames++;
}

#define AX_CHECK(cond)                                           \
    do {                                                         \
        if (!(cond)) {                                           \
            re4_log("core-test ax: %s:%d: %s", __FILE__, __LINE__, #cond); \
            failed++;                                            \
        }                                                        \
    } while (0)

static u32 axCurrent(AXVPB* v)
{
    return ((u32) v->pb.addr.currentAddressHi << 16) | v->pb.addr.currentAddressLo;
}

extern "C" int re4probe_ax_test(void)
{
    int failed = 0;
    AXInitEx(0);
    AXCallback old = AXRegisterCallback(axFrame);
    // 64 voices; the 65th of a higher priority takes the oldest of the lowest
    AXVPB* v[64];
    for (int i = 0; i < 64; i++) {
        v[i] = AXAcquireVoice(i < 2 ? 1 : 5, axDrop, i);
        AX_CHECK(v[i] != 0);
    }
    AX_CHECK(AXAcquireVoice(1, axDrop, 99) == 0);  // nothing below priority 1
    AXVPB* taken = AXAcquireVoice(10, axDrop, 100);
    AX_CHECK(taken == v[0] && gAxDrops == 1 && taken->userContext == 100 && taken->pb.state == 0);
    for (int i = 1; i < 64; i++) {
        AXFreeVoice(v[i]);
    }
    // ADPCM: 14 samples per 16 nibbles; 160 samples at ratio 1.0 per frame
    AXPBADDR addr;
    memset(&addr, 0, sizeof(addr));
    addr.format = 0;
    addr.currentAddressLo = 2;          // sample 0
    addr.endAddressHi = 0;
    addr.endAddressLo = 0x10 * 99 + 15;  // last sample of frame 99 (1400 samples)
    AXSetVoiceAddr(taken, &addr);
    AXSetVoiceSrcType(taken, AX_SRC_TYPE_LINEAR);
    AXSetVoiceSrcRatio(taken, 1.0f);
    AXSetVoiceState(taken, 1);
    re4_ax_frame();
    // 160 samples: frame 11 (154), sample 6 -> nibble 8
    AX_CHECK(axCurrent(taken) == 11 * 16 + 2 + 6);
    AX_CHECK(gAxFrames == 1);
    // ratio 0.5: 80 samples
    AXSetVoiceSrcRatio(taken, 0.5f);
    re4_ax_frame();
    AX_CHECK(axCurrent(taken) == (240 / 14) * 16 + 2 + 240 % 14);
    // no loop: stops at the end address
    AXSetVoiceSrcRatio(taken, 4.0f);  // 640 samples a frame
    for (int i = 0; i < 4; i++) {
        re4_ax_frame();
    }
    AX_CHECK(taken->pb.state == 0 && axCurrent(taken) == 0x10 * 99 + 15);
    // loop: wraps to the loop address
    AXSetVoiceCurrentAddr(taken, 2);
    AXSetVoiceLoopAddr(taken, 0x10 * 50 + 2);  // sample 700
    AXSetVoiceLoop(taken, 1);
    AXSetVoiceState(taken, 1);
    AXSetVoiceSrcRatio(taken, 4.0f);
    re4_ax_frame();  // 640
    re4_ax_frame();  // 1280
    re4_ax_frame();  // 1920 -> 700 + (1920 - 1400) = 1220 (the loop is 700 samples)
    AX_CHECK(taken->pb.state == 1 && axCurrent(taken) == (1220 / 14) * 16 + 2 + 1220 % 14);
    // PCM16: sample addresses
    AXVPB* pcm = AXAcquireVoice(3, 0, 0);
    memset(&addr, 0, sizeof(addr));
    addr.format = 10;
    addr.endAddressLo = 1000;
    AXSetVoiceAddr(pcm, &addr);
    AXSetVoiceSrcType(pcm, AX_SRC_TYPE_NONE);  // no resampling: 1.0
    AXSetVoiceState(pcm, 1);
    re4_ax_frame();
    AX_CHECK(axCurrent(pcm) == 160 && pcm->pb.adpcm.gain == 0x0800);
    // the audio frames of video fields: 200 per second, 3 or 4 per field
    int before = gAxFrames;
    for (int i = 0; i < 60; i++) {
        re4_ax_field();
    }
    AX_CHECK(gAxFrames - before == 200 || gAxFrames - before == 201);
    AXFreeVoice(pcm);
    AXFreeVoice(taken);
    AXRegisterCallback(old);
    return failed;
}
