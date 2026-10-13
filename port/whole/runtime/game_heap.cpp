// The game's operator new/delete (src/game/main_mem.cpp: the current game
// heap) for the REL module libraries. The version script libre4_main.ver keeps
// them local to libre4_main, so they do not replace the process-wide operators
// used by the C++ runtime and the platform code. Module libraries are linked with
// --wrap=_Znwj (...): their `new` lands here, and from here in the game's
// operators, which this file sees because it is linked into libre4_main.
#include <stddef.h>

#include <new>

extern "C" {
void* __wrap__Znwj(size_t n) { return ::operator new(n); }
void* __wrap__Znaj(size_t n) { return ::operator new[](n); }
void __wrap__ZdlPv(void* p) { ::operator delete(p); }
void __wrap__ZdaPv(void* p) { ::operator delete[](p); }
}
