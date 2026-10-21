// Between the GX FIFO front end (gx_fifo.cpp) and the command decoder
// (gx_decode.cpp) of the emulated graphics processor. Port-internal.
#ifndef RE4_GX_INTERNAL_H
#define RE4_GX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace re4gx {

// Only for gx_fifo.cpp and gx_decode.cpp (both in libre4_platform.so): hidden,
// so that the calls between them are direct (not through the PLT; each value
// the CPU puts into the FIFO passes feed())
#pragma GCC visibility push(hidden)

// gx_fifo.cpp -------------------------------------------------------------

// Game memory at a physical address as the graphics processor sees it: MEM1
// (0 .. RE4_MEM1_SIZE) or a window of port memory outside MEM1 (static data of
// the libraries, re4_gx_physical). NULL when `size` bytes from `physical` are
// not memory.
uint8_t* memory(uint32_t physical, uint32_t size);
// The same for a run of bytes: the host address of `physical` and in
// *available how many bytes from there on are memory (0 and NULL: none).
uint8_t* memorySpan(uint32_t physical, uint32_t* available);

// The address word of a display list call: the SDK writes the list pointer as
// it is (GXCallDisplayList); on the GameCube the GP masks it to physical.
// Here it may also be a pointer to port memory outside MEM1.
uint8_t* listMemory(uint32_t address, uint32_t size);

// The PE raises its interrupts (delivered by re4_gx_deliver_interrupts); a
// token without interrupt (BP 0x47) only sets the token register. On the GP
// thread (gx_thread.cpp) they wake the dispatcher's idle (raised()).
void peFinish();
void peToken(uint16_t token, bool interrupt);
bool interruptPending();

// gx_decode.cpp -----------------------------------------------------------

// Bytes of the command stream, in order (the CPU FIFO linked to the GP).
void feed(const uint8_t* bytes, size_t n);
// Works through what has arrived (a command cut off waits for its bytes).
void drain();

// The port's calls between the bytes of the stream (VI, marks, host-order
// lists), applied in their place: at once, or by the GP thread
struct Event {
    enum Kind { kViNext, kViBlack, kViFlush, kMark, kHostList } kind;
    uint64_t at;        // after this many bytes of the stream (post())
    uintptr_t pointer;  // kViNext: the XFB; kHostList: the list
    uint32_t value;     // kViBlack: black; kHostList: its size
    uint64_t hash;      // kHostList: its bytes when the game registered it
};
void apply(const Event& e);

// gx_thread.cpp -----------------------------------------------------------

// The GP on a thread of its own (RE4_GX_THREAD=1, re4_gx_threaded): the
// FIFO's bytes and the events go through a ring to that thread
extern bool gThreaded;
// The game's thread puts its bytes at gRingAt while they fit below gRingStop
// (the room the GP thread left, the next hand-over, the ring's end): no call
// for most values; the rest goes through ringPutSlow
extern uint8_t* gRingAt;
extern uint8_t* gRingStop;
void ringPutSlow(const uint8_t* bytes, size_t n);
inline void ringPut(const uint8_t* bytes, size_t n)
{
    if ((size_t) (gRingStop - gRingAt) >= n) {
        memcpy(gRingAt, bytes, n);
        gRingAt += n;
    } else {
        ringPutSlow(bytes, n);
    }
}
void post(Event e);                            // an event after the bytes put so far
void sync();  // waits until the GP thread has worked through all of it
void raised();  // the GP thread raised a PE interrupt: an idle game thread takes it
void flush();   // the bytes put so far to the GP thread, now (as GXFlush on the GameCube)

#pragma GCC visibility pop

}  // namespace re4gx

#endif
