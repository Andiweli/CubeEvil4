#ifndef FAKE_TGMATH_H
#define FAKE_TGMATH_H
/* Port replacement for include/fake_tgmath.h, MWCC inline versions of
 * sqrtf/fabsf/floor/cos (with __frsqrte); the platform libm provides them.
 * <dolphin/types.h> hides libm's fabsf (math_sub.h of the game defines its
 * own), so the SDK units get the compiler builtin. Only src/lib includes this. */
#include <math.h>
#ifndef fabsf
#define fabsf(x) __builtin_fabsf(x)
#endif
#endif
