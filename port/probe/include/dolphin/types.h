#ifndef _DOLPHIN_TYPES_H_
#define _DOLPHIN_TYPES_H_
/* Probe replacement for include/dolphin/types.h with exact widths. */
#include <types.h>
typedef char *Ptr;
#define FALSE 0
#define TRUE 1
#define AT_ADDRESS(addr)
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#define INT_MAX 2147483647
#ifndef NULL
#define NULL 0
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#define fabsf fabsf_libm
#include <math.h>
#undef fabsf
#endif
