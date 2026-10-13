// TEXGet of the SDK's character pipeline (src/lib/texPalette.c, whose loader
// half needs the SDK file cache the game never uses): descriptor `id` of a
// palette the game has relocated (cTexSys::CalcTplAddr, MessageFont::create).
#include "tpl.h"

extern "C" TEXDescriptor* TEXGet(TEXPalette* pal, u32 id)
{
    return &pal->descriptorArray[id];
}
