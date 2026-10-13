// Replacements for the SN ProDG / GCC 2.95 toolchain runtime the game links
// against: linker-script constructor lists, the old operator new/delete names
// and the hand-written asm helpers whose bodies the prelude drops.
#include "re4_runtime.h"

#include <new>
#include <stddef.h>
#include <string.h>

extern "C" {

// main.dol's own lists; constructors of libre4_main already ran at load time.
void (*_ctors[1])(void) = {0};
void (*_dtors[1])(void) = {0};

// GCC 2.95 names of the global allocation operators.
void* __builtin_new(size_t n) { return ::operator new(n); }
void* __builtin_vec_new(size_t n) { return ::operator new[](n); }
void __builtin_delete(void* p) { ::operator delete(p); }
void __builtin_vec_delete(void* p) { ::operator delete[](p); }

// src/game/memset_2.cpp (whole-function asm): memset that returns nothing and
// a word-wise clear.
void memset_asm(void* dst, int c, size_t n) { memset(dst, c, n); }
void memclr_asm(void* dst, size_t n) { memset(dst, 0, n); }

}
