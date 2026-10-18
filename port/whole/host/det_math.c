/* libre4_detmath.so: the float functions of the C library the game calls (sinf, cosf, tanf,
 * atan2f, atanf, acosf, asinf, expf, logf, powf), each through its double version rounded once
 * to float. Only for the 32/64-bit comparison of the PC check (whole_r100_check.sh --compare,
 * LD_PRELOAD): glibc has other float code for i686 (partly x87 assembly) than for x86_64, and
 * their last bits differ; through the double functions both builds get the same float results
 * (a double result differs in its last bits at most, which the rounding to float hides), so the
 * two runs part only where the game itself differs. Not part of the game, not on Android. */
#include <math.h>

float sinf(float x) { return (float) sin((double) x); }
float cosf(float x) { return (float) cos((double) x); }
float tanf(float x) { return (float) tan((double) x); }
float atanf(float x) { return (float) atan((double) x); }
float atan2f(float y, float x) { return (float) atan2((double) y, (double) x); }
float acosf(float x) { return (float) acos((double) x); }
float asinf(float x) { return (float) asin((double) x); }
float expf(float x) { return (float) exp((double) x); }
float logf(float x) { return (float) log((double) x); }
float powf(float x, float y) { return (float) pow((double) x, (double) y); }
