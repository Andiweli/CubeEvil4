// What a movie does on the device, for the log (M9): game/sofdec.cpp calls
// re4_movie_status after every playback step (cSofdec::appMain, patch 0054)
// and re4_movie_end when the movie is finished (finishMovie). Every 5 s of
// playback and at the end one line "movie: ..." with the player's time, the
// frames handed out, and the frames the CRI player skipped because decoding
// or the display fell behind (mwPlyGetNumSkipDec / GetNumSkipDisp: on a fast
// enough device both stay 0, as on the GameCube). Compiled with the game's
// flags (re4_gamehooks_obj).
#include "mwply.h"

#include <string.h>

extern "C" void re4_log(const char* fmt, ...);

namespace {

int gSteps;
int gFrames;
int gLastFno = -1;
char gName[0x40];
int gLastSkipDec, gLastSkipDisp;

void report(MWPLY hn, const char* what, int stat)
{
    int count = 0, scale = 1;
    mwPlyGetTime(hn, &count, &scale);
    gLastSkipDec = mwPlyGetNumSkipDec(hn);
    gLastSkipDisp = mwPlyGetNumSkipDisp(hn);
    re4_log("movie: %s %s, %d steps, %d frames, time %d.%02d s, skipped %d decode / %d display, status %d", gName,
            what, gSteps, gFrames, scale > 0 ? count / scale : 0, scale > 0 ? (int) ((long long) count * 100 / scale % 100) : 0,
            gLastSkipDec, gLastSkipDisp, stat);
}

}  // namespace

extern "C" void re4_movie_status(MWPLY hn, const char* fname, const MWS_FRM* frm, int stat)
{
    if (gSteps == 0) {
        strncpy(gName, fname ? fname : "?", sizeof(gName) - 1);
        gFrames = 0;
        gLastFno = -1;
        re4_log("movie: %s started", gName);
    }
    gSteps++;
    if (frm && frm->bufadr != NULL && frm->fno != gLastFno) {
        gLastFno = frm->fno;
        gFrames++;
    }
    if (gSteps % 300 == 0) {  // 5 s at 60 steps a second
        report(hn, "playing", stat);
    }
}

extern "C" void re4_movie_end(MWPLY hn, int skipped)
{
    if (gSteps > 0 && hn) {
        report(hn, skipped ? "skipped (START)" : "ended", mwPlyGetStat(hn));
    }
    gSteps = 0;
}
