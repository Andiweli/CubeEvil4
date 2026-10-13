/* Forced include for every game unit of the whole-program port build. */

/* The platform C library is declared before any game header. Game headers
 * redeclare a few C functions themselves (memcpy in joy.h, sprintf in xml.h);
 * after the system declaration those redeclarations are compatible.
 * math_sub.h defines its own inline fabsf, so libm's is renamed here exactly
 * as include/dolphin/types.h does it. */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#define fabsf fabsf_libm
#include <math.h>
#undef fabsf

/* GameCube inline asm (register pinning, PPC instructions, section
 * directives) has no meaning on ARM or x86 and is dropped. Defined after the
 * system headers, which use __asm__ for symbol redirection. Symbol aliases
 * (`decl asm("name")`) are rewritten to __asm by whole_prepare.py and kept. */
#define asm(...)
#define __asm__(...)

/* C++ declares const-correct overloads of the C search functions; the game
 * assigns their results to char* as C allows. Route the calls to wrappers with
 * the C signatures. */
inline char* re4_strstr(const char* s, const char* n) { return (char*)strstr(s, n); }
inline char* re4_strchr(const char* s, int c) { return (char*)strchr(s, c); }
inline char* re4_strrchr(const char* s, int c) { return (char*)strrchr(s, c); }
inline char* re4_strpbrk(const char* s, const char* a) { return (char*)strpbrk(s, a); }
inline void* re4_memchr(const void* s, int c, size_t n) { return (void*)memchr(s, c, n); }
#define strstr re4_strstr
#define strchr re4_strchr
#define strrchr re4_strrchr
#define strpbrk re4_strpbrk
#define memchr re4_memchr

#ifdef __cplusplus
/* Two bytes of a big-endian halfword as an array view (u8 v[2] in a union
 * with the halfword, patch 0016): index 0 is the high byte, 1 the low byte,
 * as the original code expects, whatever the host byte order. */
struct Re4BeBytes2 {
    unsigned char v[2];
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    unsigned char& operator[](int i) { return v[1 - i]; }
    const unsigned char& operator[](int i) const { return v[1 - i]; }
#else
    unsigned char& operator[](int i) { return v[i]; }
    const unsigned char& operator[](int i) const { return v[i]; }
#endif
};
#endif
