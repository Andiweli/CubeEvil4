// Formatumwandler: debug data the retail game reads too.
#include "formats.h"

#include "re4_runtime.h"
#include "vec.h"
#include "room_jmp.h"

using namespace re4fmt;

static_assert(sizeof(CRoomInfo) == 0x20, "CRoomInfo layout");

namespace re4fmt {

// debug/roomInfo.dat (include/room_jmp.h): u32 stage count, u32 offset per
// stage (0 = none), each stage block a u32 count and CRoomInfo records; the
// strings they point at are bytes.
void convertRoomInfo(Blob& b, const Part&)
{
    u32 stages = be32(b, 0);
    if (!fits(b, 4, stages * 4, "stage offsets")) {
        return;
    }
    u32 offs[64];
    for (u32 i = 0; i < stages && i < 64; i++) {
        offs[i] = be32(b, 4 + i * 4);
    }
    swap32(b, 0, stages + 1);
    for (u32 s = 0; s < stages && s < 64 && b.ok; s++) {
        u32 o = offs[s];
        if (o == 0) {
            continue;
        }
        u32 n = be32(b, o);
        swap32(b, o);
        if (!fits(b, o + 4, n * sizeof(CRoomInfo), "CRoomInfo records")) {
            return;
        }
        for (u32 i = 0; i < n; i++) {
            u32 r = o + 4 + i * sizeof(CRoomInfo);
            RE4_SWAP(b, r, CRoomInfo, flag);
            RE4_SWAP(b, r, CRoomInfo, roomNo);
            swap32(b, r + offsetof(CRoomInfo, pos), 3);
            RE4_SWAP(b, r, CRoomInfo, angle);
            RE4_SWAP(b, r, CRoomInfo, name);
            RE4_SWAP(b, r, CRoomInfo, person);
            RE4_SWAP(b, r, CRoomInfo, person2);
        }
    }
}

}  // namespace re4fmt
