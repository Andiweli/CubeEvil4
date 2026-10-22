// Touch in the game's sub screens (wish of the project owner, 22.10.2026: pick the tabs and the
// items of the attache case with a finger), compiled with the game's flags into libre4_main.
//
// The app hands a tap on the picture over (re4_core_touch: u across the game's picture, v down
// it, 0..1; CoreActivity.watchTap only passes taps beside the touch controls). Patch 0106 calls
// re4_touch_sscrn once per frame of the sub screen task, before the current screen reads Key; a
// tap younger than 400 ms becomes the cursor and keys of that frame:
//
//  - a tab of the top row (key items, attache case, map, files, exit; only in the normal
//    inventory, open_flag 1), or a swipe across the picture (re4_core_swipe: the next tab or the
//    one before, the four screens in a ring, never the exit; wish of the project owner after the
//    first device test, 22.10.2026): with the row focused, menu_next and A (sscrnMainMenu then
//    switches like the cross and A); else B until the row has the focus, at most three times
//    12 frames apart (the case only from its cursor with nothing on the spare board and nothing in
//    hand, as B does there; the key items and the files; the map from its zoomed view, its whole
//    view zooms in on the first B), and the tab at the frame the row has the focus (45 at most);
//  - the attache case, its cursor (PieceSelect state 0) or the combine target (PieceCombine): a
//    tap on a cell moves the cursor there (sound 6, like the cross); a tap on the piece under
//    the cursor is A (the command menu, or the combination);
//  - the command menu (PieceCommand mode 0): a tap on an entry selects it (sound 0xA), a second
//    tap is A; a tap on the case closes the menu (B). Its yes/no or Leon/Ashley question (mode
//    2): a tap selects the option (left/right), a second tap is A. So nothing the game cannot
//    undo (combine, discard, use) happens on a single tap.
//
// Positions: the sub screen draws its id units through IDSystem::m_scrn_mat and the 3D case
// through the camera of sscrnCameraInit, both with pG->Camera.ProjMat (4:3) over the whole
// picture (640 x 448 while the sub screen is open; u and v do not depend on the width). A piece
// in hand, the item examine, the key items, map and file screens themselves and the merchant are
// not touched (their buttons as before).
#include "types.h"
#include "global.h"
#include "main.h"
#include "camera.h"
#include "id_sys.h"
#include "puzzle.h"
#include "item.h"
#include "snd.h"
#include "sscrn.h"
#include "ss_main.h"
#include "ss_pzzl.h"

#include <time.h>

extern GLOBAL_WK* pG;

extern "C" void re4_log(const char* fmt, ...);

namespace {

const u64 kA = 0x80000000ull, kB = 0x40000000ull, kRight = 0x04000000ull, kLeft = 0x08000000ull;
const f32 kCell = 100.0f;  // pzlGrid::size (ss_pzzl.cpp caseModelMove)
const u32 kTapAgeMs = 400;
const unsigned kTabFrames = 45;     // frames a tab waits for the row to get the focus
const unsigned kTabPresses = 3;     // B at most this often for it (the map: zoom in, then to the row)
const unsigned kTabPressGap = 12;   // frames between two of them (the map's zoom takes 10)

// the app's thread writes, the game's reads: the tap as float bits, when, counted
volatile u32 gTapU, gTapV, gTapWhen, gTapSeq;
u32 gTapSeen;
bool gAnnounced;

// a tab waiting for the row's focus (after B)
struct Tab {
    int no;  // -1: none
    unsigned frames, presses, lastPress;
};
Tab gTab = {-1, 0, 0, 0};

unsigned gLines;

u32 nowMs()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (u32) (t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

u32 bits(float f)
{
    u32 b;
    __builtin_memcpy(&b, &f, 4);
    return b;
}

float value(u32 b)
{
    float f;
    __builtin_memcpy(&f, &b, 4);
    return f;
}

bool line()
{
    gLines++;
    return gLines <= 80 || gLines % 50 == 0;
}

// m (model to camera) then the camera's projection: the place in the picture, 0..1
bool project(const Mtx m, f32 x, f32 y, f32 z, f32* u, f32* v)
{
    Vec p = {x, y, z}, q;
    PSMTXMultVec(m, &p, &q);
    const Mtx44& P = pG->Camera.ProjMat;
    const f32 cx = P[0][0] * q.x + P[0][1] * q.y + P[0][2] * q.z + P[0][3];
    const f32 cy = P[1][0] * q.x + P[1][1] * q.y + P[1][2] * q.z + P[1][3];
    const f32 cw = P[3][0] * q.x + P[3][1] * q.y + P[3][2] * q.z + P[3][3];
    if (!(cw > 1e-6f)) {
        return false;
    }
    *u = 0.5f + 0.5f * cx / cw;
    *v = 0.5f - 0.5f * cy / cw;
    return true;
}

// inside the convex quad (either winding)
bool inQuad(const f32* qu, const f32* qv, f32 u, f32 v)
{
    int pos = 0, neg = 0;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) & 3;
        const f32 c = (qu[j] - qu[i]) * (v - qv[i]) - (qv[j] - qv[i]) * (u - qu[i]);
        if (c > 0.0f) {
            pos++;
        } else if (c < 0.0f) {
            neg++;
        }
    }
    return pos == 0 || neg == 0;
}

} // namespace

// The quad an id unit is drawn as (IdCommonTrans: m_scrn_mat * mat, its four ver) holds (u, v).
extern "C" int re4_touch_unit_hit(const ID_UNIT* unit, f32 u, f32 v)
{
    if (!unit || unit->be_flag == 0xFF) {
        return 0;
    }
    Mtx m;
    PSMTXConcat(IDSystem::m_scrn_mat, unit->mat, m);
    f32 qu[4], qv[4];
    for (int i = 0; i < 4; i++) {
        if (!project(m, unit->ver[i].x, unit->ver[i].y, unit->ver[i].z, &qu[i], &qv[i])) {
            return 0;
        }
    }
    return inQuad(qu, qv, u, v) ? 1 : 0;
}

// The cell of board b under (u, v): cells of kCell, x right, y down (local y negative), z 0, in
// the board's matrix (caseModelMove) and the camera's view. 0 when none.
extern "C" int re4_touch_board_cell(pzlBoard* b, f32 u, f32 v, int* cellX, int* cellY)
{
    if (!b) {
        return 0;
    }
    Mtx m;
    PSMTXConcat(pG->Camera.v_mat, *b->orientation(), m);
    for (int y = 0; y < b->size_y(); y++) {
        for (int x = 0; x < b->size_x(); x++) {
            const f32 x0 = kCell * x, x1 = kCell * (x + 1), y0 = -kCell * y, y1 = -kCell * (y + 1);
            f32 qu[4], qv[4];
            if (!project(m, x0, y0, 0.0f, &qu[0], &qv[0]) || !project(m, x1, y0, 0.0f, &qu[1], &qv[1]) ||
                !project(m, x1, y1, 0.0f, &qu[2], &qv[2]) || !project(m, x0, y1, 0.0f, &qu[3], &qv[3])) {
                return 0;
            }
            if (inQuad(qu, qv, u, v)) {
                *cellX = x;
                *cellY = y;
                return 1;
            }
        }
    }
    return 0;
}

namespace {

int tabAt(f32 u, f32 v)
{
    for (int k = 0; k < 5; k++) {
        if (re4_touch_unit_hit(IdSub.unitPtr(k, IDC_SSCRN_MAIN_MENU), u, v)) {
            return k;
        }
    }
    return -1;
}

// the case model rests (its unit 0xFE finished the slide in); unknown counts as resting
bool caseResting()
{
    if (!IdSub.setCk(IDC_SSCRN_BACK_GROUND)) {
        return true;
    }
    const ID_UNIT* u = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    return (u->rev_flag & 1) == 0 && (u->anima_state & 1) != 0;
}

unsigned itemId(const pzlPiece* p)
{
    return p && p->item ? p->item->id : 0;
}

// a cell of the case, or of the spare board while it holds pieces
pzlBoard* cellAt(pzlPlayer* pl, f32 u, f32 v, int* x, int* y)
{
    if (re4_touch_board_cell(pl->boardPtr(), u, v, x, y)) {
        return pl->boardPtr();
    }
    pzlBoard* space = pl->spacePtr();
    if (space && space->getPieceNum() > 0 && re4_touch_board_cell(space, u, v, x, y)) {
        return space;
    }
    return 0;
}

// the cursor on a cell (PieceSelect / PieceCombine): A when the cell's piece is under the cursor
// already, else the cursor moves there like with the cross
void pickCell(SUB_SCREEN* wk, pzlPlayer* pl, f32 u, f32 v, const char* what)
{
    int x, y;
    pzlBoard* b = cellAt(pl, u, v, &x, &y);
    if (!b) {
        if (line()) {
            re4_log("touch: %s: not on a cell (%.2f %.2f)", what, (double) u, (double) v);
        }
        return;
    }
    pzlBoard* active = pl->m_p_active_board;
    pzlPiece* tapped = b->getPiece(x, y);
    pzlPiece* under = active ? active->getPiece(active->m_cur_x, active->m_cur_y) : 0;
    if (tapped && tapped == under) {
        Key.trg |= kA;
        if (line()) {
            re4_log("touch: %s: A on item 0x%02x at (%d %d)", what, itemId(tapped), x, y);
        }
        return;
    }
    if (b == active && x == active->m_cur_x && y == active->m_cur_y) {
        return;
    }
    pl->m_p_active_board = b;
    b->m_cur_x = (s8) x;
    b->m_cur_y = (s8) y;
    wk->cursor_flag |= 1;
    SndCall(0, 6, 0, 0, 0, 0);
    if (line()) {
        re4_log("touch: %s: cursor to (%d %d) %s, item 0x%02x", what, x, y,
                b == pl->boardPtr() ? "in the case" : "on the spare board", itemId(tapped));
    }
}

void commandMenu(SUB_SCREEN* wk, pzlPlayer* pl, PieceCommand* c, f32 u, f32 v)
{
    if (c->mode == 0) {
        for (int i = 0; i < c->num && 7 + 2 * i < 16; i++) {
            if (re4_touch_unit_hit(c->id[6 + 2 * i], u, v) || re4_touch_unit_hit(c->id[7 + 2 * i], u, v)) {
                const bool press = wk->cmd_menu_no == i;
                if (press) {
                    Key.trg |= kA;
                } else {
                    wk->cmd_menu_no = (s8) i;
                    SndCall(0, 0xA, 0, 0, 0, 0);
                }
                if (line()) {
                    re4_log("touch: command %d of %d%s", i, (int) c->num, press ? ": A" : "");
                }
                return;
            }
        }
        int x, y;
        if (cellAt(pl, u, v, &x, &y)) {
            Key.trg |= kB;
            if (line()) {
                re4_log("touch: command menu closed (a tap on the case)");
            }
        }
        return;
    }
    if (c->mode == 2) {
        const int option = re4_touch_unit_hit(c->sub[5], u, v) ? 0 : re4_touch_unit_hit(c->sub[7], u, v) ? 1 : -1;
        if (option < 0) {
            return;
        }
        const bool press = c->subSel == option;
        Key.trg |= press ? kA : option == 0 ? kLeft : kRight;
        if (line()) {
            re4_log("touch: question option %d%s", option, press ? ": A" : "");
        }
    }
}

// the attache case's own taps (its main widget in state 0)
void caseScreen(SUB_SCREEN* wk, SsPzzlMain* main, f32 u, f32 v)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    if (!pl || wk->board_size != wk->board_next || !caseResting()) {
        return;
    }
    Widget<SUB_SCREEN>* child = main->cur;
    if (child == main->select && main->select->state == 0) {
        if (pl->pieceInHand()) {
            return;
        }
        pickCell(wk, pl, u, v, "case");
    } else if (child == main->combine && main->combine->state == 0) {
        pickCell(wk, pl, u, v, "combine");
    } else if (child == main->command) {
        commandMenu(wk, pl, main->command, u, v);
    }
}


// The tab of the screen the player is in (0 key items, 1 attache case, 2 map, 3 files), -1 none.
int screenTab(Widget<SUB_SCREEN>* cur, SsPzzlMain* pzzl, SsItemMain* item, SsMapMain* map, SsFileMain* file)
{
    if (pzzl && cur == pzzl) {
        return 1;
    }
    if (item && cur == item) {
        return 0;
    }
    if (map && cur == map) {
        return 2;
    }
    if (file && cur == file) {
        return 3;
    }
    return -1;
}

// B now gives the tab row the focus, as the player's B does: the case only from its cursor with
// nothing in hand and nothing on the spare board (B else asks about the spare board); the key
// items and the files from their main view; the map from its zoomed view (MapRead: B leaves to
// the row) or the whole view (B zooms in first).
bool backToRow(SUB_SCREEN* wk, Widget<SUB_SCREEN>* cur, SsPzzlMain* pzzl, SsItemMain* item, SsMapMain* map,
               SsFileMain* file)
{
    if (pzzl && cur == pzzl) {
        pzlPlayer* pl = wk->puzzlePlayer;
        return pzzl->state == 0 && pzzl->cur == pzzl->select && pzzl->select->state == 0 && pl &&
               !pl->pieceInHand() && pl->spacePtr()->getPieceNum() == 0;
    }
    if (map && cur == map) {
        // MapRead / MapEntire are only declared here; their Widget base is at offset 0
        const void* view = map->cur;
        return map->state == 0 && (view == (const void*) map->read || view == (const void*) map->entire);
    }
    return (item && cur == item && item->state == 0) || (file && cur == file && file->state == 0);
}

// the swipe of the app (re4_core_swipe): -1 the tab before, 1 the next one
volatile u32 gSwipeDir, gSwipeWhen, gSwipeSeq;
u32 gSwipeSeen;

// a new tab to go to (a tap on it, a swipe)
void wantTab(int k, const char* why)
{
    gTab.no = k;
    gTab.frames = 0;
    gTab.presses = 0;
    gTab.lastPress = 0;
    if (line()) {
        re4_log("touch: tab %d (%s)", k, why);
    }
}

} // namespace

// Patch 0106 (src/Sscrn/ss_main.cpp SubScreenTask), every frame before cur->move: the screens'
// main widgets (0 outside the normal inventory) to tell where the player is.
extern "C" void re4_touch_sscrn(SUB_SCREEN* wk, Widget<SUB_SCREEN>* cur, SsPzzlMain* pzzl, SsItemMain* item,
                                SsMapMain* map, SsFileMain* file)
{
    bool tap = false;
    f32 u = 0.0f, v = 0.0f;
    const u32 seq = __atomic_load_n(&gTapSeq, __ATOMIC_ACQUIRE);
    if (seq != gTapSeen) {
        gTapSeen = seq;
        u = value(__atomic_load_n(&gTapU, __ATOMIC_RELAXED));
        v = value(__atomic_load_n(&gTapV, __ATOMIC_RELAXED));
        tap = nowMs() - __atomic_load_n(&gTapWhen, __ATOMIC_RELAXED) <= kTapAgeMs;
    }
    int swipe = 0;
    const u32 swipeSeq = __atomic_load_n(&gSwipeSeq, __ATOMIC_ACQUIRE);
    if (swipeSeq != gSwipeSeen) {
        gSwipeSeen = swipeSeq;
        if (nowMs() - __atomic_load_n(&gSwipeWhen, __ATOMIC_RELAXED) <= kTapAgeMs) {
            swipe = (int) __atomic_load_n(&gSwipeDir, __ATOMIC_RELAXED) == 1 ? 1 : -1;
        }
    }
    if (!wk || !cur || wk->Loop != 1 || wk->pzzl_debug_open || wk->item_make_open) {
        gTab.no = -1;
        return;
    }
    const bool row = (pzzl && cur == pzzl && pzzl->state == 1) || (item && cur == item && item->state == 1) ||
                     (map && cur == map && map->state == 1) || (file && cur == file && file->state == 1);
    const int here = screenTab(cur, pzzl, item, map, file);
    if (wk->open_flag == SS_OPEN_NORMAL) {
        if (tap && IdSub.setCk(IDC_SSCRN_MAIN_MENU)) {
            const int k = tabAt(u, v);
            if (k >= 0) {
                tap = false;
                if (k == here && !row) {
                    if (line()) {
                        re4_log("touch: tab %d is the screen already", k);
                    }
                } else {
                    wantTab(k, "tap");
                }
            }
        }
        if (swipe && here >= 0) {
            // the four screens in a ring, never the exit
            wantTab((here + 4 + swipe) % 4, swipe > 0 ? "swipe to the next" : "swipe to the one before");
            tap = false;
        }
    }
    if (gTab.no >= 0) {
        if (row) {
            wk->menu_next = (u8) gTab.no;
            Key.trg |= kA;
            gTab.no = -1;
            return;
        }
        // B until the row has the focus (the map's whole view zooms in first), a few frames
        // apart so that each is a new press
        if (gTab.presses < kTabPresses && (gTab.presses == 0 || gTab.frames - gTab.lastPress >= kTabPressGap) &&
            backToRow(wk, cur, pzzl, item, map, file)) {
            Key.trg |= kB;
            gTab.presses++;
            gTab.lastPress = gTab.frames;
        }
        if (++gTab.frames > kTabFrames) {
            if (line()) {
                re4_log("touch: tab %d dropped (the row did not get the focus)", gTab.no);
            }
            gTab.no = -1;
        }
        return;
    }
    if (tap && pzzl && cur == pzzl && pzzl->state == 0 && (wk->open_flag == SS_OPEN_NORMAL || wk->open_flag == SS_OPEN_PZZL)) {
        caseScreen(wk, pzzl, u, v);
    }
}

// The app (CoreBridge.swipe): a swipe across the picture, -1 to the right (the tab before), 1 to
// the left (the next tab); any thread.
extern "C" void re4_core_swipe(int dir)
{
    static bool announced;
    if (!announced) {
        announced = true;
        re4_log("touch: swipes change the tab of the sub screen");
    }
    __atomic_store_n(&gSwipeDir, (u32) (dir > 0 ? 1 : 0), __ATOMIC_RELAXED);
    __atomic_store_n(&gSwipeWhen, nowMs(), __ATOMIC_RELAXED);
    __atomic_fetch_add(&gSwipeSeq, 1, __ATOMIC_RELEASE);
}

// The app (CoreBridge.tap): a tap on the picture, any thread.
extern "C" void re4_core_touch(float u, float v)
{
    if (!gAnnounced) {
        gAnnounced = true;
        re4_log("touch: taps on the picture reach the sub screens");
    }
    if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
        return;
    }
    __atomic_store_n(&gTapU, bits(u), __ATOMIC_RELAXED);
    __atomic_store_n(&gTapV, bits(v), __ATOMIC_RELAXED);
    __atomic_store_n(&gTapWhen, nowMs(), __ATOMIC_RELAXED);
    __atomic_fetch_add(&gTapSeq, 1, __ATOMIC_RELEASE);
}
