#ifndef TYPES_H
#define TYPES_H
/* Port replacement for include/types.h.
 *
 * On ILP32 targets (ARM32, x86-32) long is 32 bits, so the original
 * definitions are kept exactly: s32/u32 stay `long`. Headers that spell
 * `unsigned long` and units that spell `u32` then name the same function
 * (identical C++ mangling), as on the GameCube. LP64 (ARM64) needs fixed
 * widths and is handled in milestone 12. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
#if __SIZEOF_LONG__ == 4
typedef signed long s32;
typedef unsigned long u32;
#else
typedef __INT32_TYPE__ s32;
typedef __UINT32_TYPE__ u32;
#endif
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef int BOOL;
#ifndef NULL
#define NULL 0
#endif
#endif
