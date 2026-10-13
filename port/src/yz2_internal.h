#pragma once
#include "types.h"
#include "re4/port/yz2.h"
#include <cstring>
#include <stdexcept>

namespace re4::port::detail {
struct Yz2Checkpoint { void (*callback)(void*, u32); void* argument; };
#include "yz2_structs.inc"
static_assert(sizeof(Yz2DicEnt) == 0x1004);
constexpr std::size_t yz2WorkBytes = 0x142800;
inline void yz2Pulse(Yz2InEv& ev, u32 units) {
    if (ev.checkpoint.callback) ev.checkpoint.callback(ev.checkpoint.argument, units);
}
inline u8 yz2ReadByte(Yz2InEv& ev) {
    if (ev.src >= ev.srcEnd) throw std::runtime_error("Truncated Yz2 coded stream");
    return *ev.src++;
}
inline void* yz2Bump(Yz2InEv& ev, u32 bytes) {
    if (static_cast<std::size_t>(ev.workEnd - ev.free) < bytes)
        throw std::runtime_error("Yz2 work buffer overflow");
    void* result = ev.free;
    ev.free += bytes;
    return result;
}
u32 Yz2DecodeSet(char* str, void* buf, Yz2InEv& ev);
void Yz2DecodeExec(void* dst, Yz2InEv& ev);
void yz2Decode_Decode(void* ctx, void* dst, u32 bytes, void* ev);
} // namespace re4::port::detail
