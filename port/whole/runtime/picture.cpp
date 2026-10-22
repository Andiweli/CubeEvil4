// The app's picture format "4:3 without bars" (Anbernic RG 477V report, 22.10.2026), compiled with
// the game's flags into libre4_main: patch 0105 asks re4_letterbox_open every frame (Render: the
// scissor to the letterbox rows, main loop: Draw_cinesco). Open while the app's switch is on
// (re4_core_set_letterbox, runtime/core_api.cpp) and the player plays: the game's main loop, no
// sub screen, no event (events keep their letterbox, SceEventStart sets the scissor for them).
#include "types.h"
#include "global.h"

extern GLOBAL_WK* pG;

extern "C" int re4_letterbox_off(void);
extern "C" void re4_log(const char* fmt, ...);

extern "C" int re4_letterbox_open(void)
{
    const int open = re4_letterbox_off() && pG && pG->Rno0 == 3 && !StaFlagChk(pG, STA_SUB_SCRN) &&
                     !StaFlagChk(pG, STA_EVENT);
    // each change into the error log (at most 40): whether the game drew without its bars
    static int last = -1;
    static unsigned changes;
    if (open != last && (open || last == 1) && changes < 40) {
        changes++;
        re4_log("picture: the game draws %s its letterbox (r%03x)", open ? "without" : "with", pG ? pG->room_id : 0);
    }
    last = open;
    return open;
}
