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
    setMatrix(rigid->mat, 0.0f, 5.0f, 0.0f);
    // seven more skinned infos like Leon's (nine in all: body, costume, knife,
    // face, head, hair, hands); the last one with the hair's be_flag 0x40
    cModelInfo* last = rigid;
    for (int i = 0; i < 7; i++) {
        cModelInfo* more = (cModelInfo*) (base + 0x4000 + i * 0x400);
        more->model_addr = (cModelData*) bin;
        more->tpl_addr = tpl;
        more->be_flag = i == 6 ? 0x48 : 8;
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
    for (int i = 0; i < 3; i++) {
        cObj* o = (cObj*) (base + i * stride);
        o->be_flag = 1 | 2 | 4;
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

static_assert(sizeof(cObj) <= 0x500, "the fake scenery works overlap");
static_assert(sizeof(cEm) <= 0x2000, "the fake enemy work overlaps its parts");
static_assert(2 * sizeof(cParts) <= 0x1000, "the fake parts overlap the model info");
static_assert(sizeof(cModelInfo) <= 0x400 && sizeof(cModelData) <= 0x100, "the fake infos overlap");
