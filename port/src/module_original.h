#pragma once
#include "re4/port/modules.h"
#include "types.h"
#include <string>
namespace re4::port::modules_original {
// Native callback fields preserve signatures; never cast the 0xC PPC wire layout.
struct ROOM_DATA {
    u8 save_flg, pad_1;
    u16 rel_file;
    RoomFunction pInit, pMain;
};
struct StageTbl {
    ROOM_DATA *tbl;
    u16 num, pad_6;
};
inline void OSReport(const char *message) { ModuleContext::active().observe(message); }
inline void *mem_calloc(u32 bytes, const char *file, int line, int a, int b) {
    return ModuleContext::active().calloc(bytes, a, b, file, line);
}
} // namespace re4::port::modules_original
#define MEM_CALLOC(size, a, b) mem_calloc(size, __FILE__, __LINE__, a, b)
