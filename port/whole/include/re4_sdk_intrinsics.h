/* CodeWarrior PowerPC intrinsics the Dolphin SDK sources use (src/lib), in C.
 * Force-included into the SDK units of the port (CMakeLists.txt re4_sdk_obj).
 *
 *   __rlwimi(a, b, sh, mb, me)  rotate b left by sh, insert the bits mb..me
 *                               (IBM numbering, bit 0 = most significant) into a
 *                               (the GX register field macro SET_REG_FIELD)
 *   __cntlzw(x)                 count leading zeros of a word (32 for 0) */
#ifndef RE4_SDK_INTRINSICS_H
#define RE4_SDK_INTRINSICS_H

static inline unsigned int re4_ppc_mask(int mb, int me)
{
    unsigned int from = 0xFFFFFFFFu >> mb;
    unsigned int to = me == 31 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> (me + 1));
    return mb <= me ? (from & to) : (from | to);
}

static inline unsigned int __rlwimi(unsigned int a, unsigned int b, int sh, int mb, int me)
{
    unsigned int rotated = sh ? (b << sh) | (b >> (32 - sh)) : b;
    unsigned int mask = re4_ppc_mask(mb, me);
    return (rotated & mask) | (a & ~mask);
}

static inline unsigned int __cntlzw(unsigned int x)
{
    return x ? (unsigned int) __builtin_clz(x) : 32u;
}

#endif
