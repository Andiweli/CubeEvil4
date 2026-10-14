// Movies (CRI Sofdec, game/sofdec.cpp) until milestone 9: a player that has
// nothing to show. Every movie ends at once, as if it had played to its end
// (status PLAYEND), and the game goes on with what follows it: the opening
// movie of r120 leads straight into the car ride (events r120s00/s01), the
// title's demo movies back to the title.
//
// The generated stubs did not do that: mwPlyCreateSofdec returned no handle,
// yet cSofdec::ThreadMove plays on and calls mwPlyGetStat through the handle's
// interface table (a crash), and mwPlyGetCurFrm left the frame record
// unwritten (a random frame buffer address for loadMvFrmFx).
//
// The decoder itself (MPEG-1 video and ADX sound, src/lib mwsfd*/mpv*/sfd*)
// comes with milestone 9.
#include "mwply.h"

#include "re4_runtime.h"

#include <string.h>

namespace {

void* queryInterface(MWPLY, void*)
{
    return 0;
}

u32 addRef(MWPLY)
{
    return 1;
}

u32 release(MWPLY)
{
    return 1;
}

void handler(MWPLY) {}

void destroy(MWPLY) {}

void startFname(MWPLY, const char* fname)
{
    re4_log("movie: %s skipped (no movie decoder yet, milestone 9)", fname ? fname : "?");
}

void stop(MWPLY) {}

int getStat(MWPLY)
{
    return MWE_PLY_STAT_PLAYEND;
}

void getTime(MWPLY, int* ncount, int* tscale)
{
    if (ncount) {
        *ncount = 0;
    }
    if (tscale) {
        *tscale = 1;
    }
}

void pause(MWPLY, int) {}

MWPLY_IF gInterface = {queryInterface, addRef, release, handler, handler, destroy,
                       startFname,     stop,   getStat, getTime, pause};
MWPLY_OBJ gPlayer = {&gInterface};

}  // namespace

extern "C" {

void mwPlyInitSfdFx(MWS_PLY_INIT_SFD*) {}

int mwPlyCalcWorkCprmSfd(MWS_PLY_CPRM_SFD*)
{
    return 32;
}

MWPLY mwPlyCreateSofdec(MWS_PLY_CPRM_SFD*)
{
    return &gPlayer;
}

// The size of a full-screen movie; cSofdec sizes its texture from it.
void mwPlyGetHdrInf(void*, int, MWS_SFD_HDRINF* info)
{
    memset(info, 0, sizeof(*info));
    info->width = 512;
    info->height = 448;
}

void mwPlyGetCurFrm(MWPLY, MWS_FRM* frm)
{
    memset(frm, 0, sizeof(*frm));  // no frame (bufadr 0)
}

void mwPlyRelCurFrm(MWPLY) {}

int mwPlyGetNumSkipDec(MWPLY)
{
    return 0;
}

int mwPlyGetNumSkipDisp(MWPLY)
{
    return 0;
}

void mwPlyFxSetOutBufSize(MWPLY, int, int) {}

void mwPlyFxSetOutBufPitchHeight(MWPLY, int, int) {}

void mwPlyFxCnvFrmY84C44(MWPLY, MWS_FRM*, void*, void*) {}

void mwPlyFxCnvFrmARGB8888(MWPLY, MWS_FRM*, void*) {}

}  // extern "C"
