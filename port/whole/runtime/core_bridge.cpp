// Core bridge, game side: one snapshot of what the GameCube would draw this
// frame, for the renderer of the Android app (runtime/re4_core.h). Compiled
// with the game's flags into libre4_main because it reads the game's own
// structures (model.h, em.h, obj.h, global.h, camera.h).
//
// Per frame: the game step and room, the camera (pG->Camera.param, which
// CameraMove turns into the projection; CamCtrl's camera while a sub screen
// is open) and every live work of EmMgr (player,
// partner, enemies) and ObjMgr (weapons, objects) with
//   - cModel::mat (world placement) and be_flag (drawn when bits 1 and 2 are
//     set, trans.cpp ModelTrans),
//   - the world matrix of each cParts (after partsWorldCalc) and its bind
//     position (cParts::lt_inv_mat is the translation by minus it; the skin
//     palette is mat * lt_inv_mat),
//   - each cModelInfo's BIN and TPL with their disc source from the format
//     converter's notes, so the app loads the same data from the disc.
// Every pointer is checked against MEM1 / the game libraries first: the
// snapshot may be taken in any state, also while a room is being built.
#include "types.h"
#include "em.h"
#include "global.h"
#include "model.h"
#include "cam_ctrl.h"
#include "obj.h"

#include "re4_core.h"

#include <string.h>

extern GLOBAL_WK* pG;
extern cEmMgr EmMgr;
extern cObjMgr ObjMgr;

extern "C" int re4_dvd_inserted_disc(void);

namespace {

bool live(const void* p)
{
    // MEM1 (0x80000000..0x83000000) or a game library above it
    return (unsigned long) p >= 0x80000000ul;
}

bool inMem1(const void* p)
{
    unsigned long a = (unsigned long) p;
    return a >= 0x80000000ul && a < 0x83000000ul;
}

void copy34(float* out, const Mtx m)
{
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            out[r * 4 + c] = m[r][c];
        }
    }
}

void captureModel(Re4CoreSnapshot* s, cModel* m, u8 manager)
{
    if (s->nModels >= RE4_CORE_MAX_MODELS) {
        s->droppedModels++;
        return;
    }
    Re4CoreModel& out = s->models[s->nModels];
    memset(&out, 0, sizeof(out));
    out.address = (u32) (unsigned long) m;
    out.beFlag = m->be_flag;
    out.manager = manager;
    out.id = m->id;
    out.type = m->type;
    copy34(out.matrix, m->mat);
    out.firstMatrix = s->nParts;
    u32 n = m->nParts;
    cParts* p = m->pList;
    for (u32 i = 0; i < n && inMem1(p); i++) {
        if (s->nParts >= RE4_CORE_MAX_PARTS) {
            break;
        }
        Re4CorePart& part = s->parts[s->nParts++];
        copy34(part.matrix, p->mat);
        part.bind[0] = -p->lt_inv_mat[0][3];
        part.bind[1] = -p->lt_inv_mat[1][3];
        part.bind[2] = -p->lt_inv_mat[2][3];
        out.nParts++;
        p = p->pList;
    }
    cModelInfo* info = m->pModelInfo;
    while (inMem1(info) && out.nInfos < RE4_CORE_MAX_INFOS) {
        Re4CoreModelInfo& mi = out.infos[out.nInfos++];
        mi.binAddress = (u32) (unsigned long) info->model_addr;
        mi.tplAddress = (u32) (unsigned long) info->tpl_addr;
        mi.flags = info->be_flag;
        copy34(mi.matrix, info->mat);
        // trans.cpp commonModelTrans: which matrix the info is drawn with
        cModelData* d = info->model_addr;
        mi.rigidPart = -1;
        if (m->be_flag & 0x4000) {
            mi.rigidPart = -2;
        } else if (live(d) && d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) &&
                   d->nParts == 1 && live(d->pHead)) {
            mi.rigidPart = d->pHead->partsNo;
        }
        re4_formats_locate(info->model_addr, &mi.bin);
        re4_formats_locate(info->tpl_addr, &mi.tpl);
        info = info->pList;
    }
    s->nModels++;
}

template <class Mgr>
void captureManager(Re4CoreSnapshot* s, Mgr& mgr, u8 manager)
{
    u8* arr = (u8*) mgr.pArray;
    u32 n = mgr.nArray;
    u32 stride = mgr.size;
    if (!live(arr) || stride == 0 || n > 4096) {
        return;
    }
    for (u32 i = 0; i < n; i++) {
        cModel* m = (cModel*) (arr + i * stride);
        // alive (bit0) and not being destroyed (bit9)
        if ((m->be_flag & 0x201) != 1 || m->nParts == 0) {
            continue;
        }
        captureModel(s, m, manager);
    }
}

}  // namespace

extern "C" void re4_bridge_capture(Re4CoreSnapshot* s)
{
    s->apiVersion = RE4_CORE_API_VERSION;
    s->nModels = 0;
    s->nParts = 0;
    s->droppedModels = 0;
    s->cameraValid = 0;
    s->screen = 0;
    s->disc = (int8_t) re4_dvd_inserted_disc();
    GLOBAL_WK* g = pG;
    if (!live(g)) {
        return;
    }
    s->rno[0] = g->Rno0;
    s->rno[1] = g->Rno1;
    s->rno[2] = g->Rno2;
    s->rno[3] = g->Rno3;
    s->room = g->room_id;
    s->screen = (StaFlagChk(g, STA_SUB_SCRN) ? RE4_CORE_SCREEN_SUB : 0) |
                (StaFlagChk(g, STA_EVENT) ? RE4_CORE_SCREEN_EVENT : 0) |
                (DbgFlagChk(g, DBG_DBG_CAM) ? RE4_CORE_SCREEN_DEBUGCAM : 0);
    // a sub screen draws its own items with its own camera in pG->Camera
    // (sscrn.cpp keeps the world camera aside); the world behind it is the
    // controller's camera
    const CAMERA_POINT& cam = (s->screen & RE4_CORE_SCREEN_SUB) ? CamCtrl.camera.param : g->Camera.param;
    for (int k = 0; k < 3; k++) {
        s->cameraEye[k] = (&cam.Campos.x)[k];
        s->cameraTarget[k] = (&cam.Target.x)[k];
    }
    s->cameraRoll = cam.Roll;
    s->cameraFovy = cam.Fovy;
    s->cameraValid = cam.Fovy > 0.0f && cam.Fovy < 180.0f;
    captureManager(s, EmMgr, 0);
    captureManager(s, ObjMgr, 1);
}
