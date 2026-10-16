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

// src/lib/tealeaf.c: float -> unsigned 32-bit (CodeWarrior's runtime name, called
// by the CRI libraries, whose prototype takes a Float32). As the Gekko code:
// below 2^31 fctiwz (truncation, saturating, negative values keep their
// two's-complement pattern), from 2^31 on (and NaN) the value minus 2^31 with
// the top bit put back.
static int re4_fctiwz(double x)
{
    if (x != x || x <= -2147483648.0) {
        return (int) 0x80000000u;
    }
    if (x >= 2147483647.0) {
        return 0x7FFFFFFF;
    }
    return (int) x;
}

unsigned int __cvt_fp2unsigned(float f)
{
    double d = f;
    if (d >= 2147483648.0 || d != d) {
        return (unsigned int) re4_fctiwz(d - 2147483648.0) ^ 0x80000000u;
    }
    return (unsigned int) re4_fctiwz(d);
}

}
