/* CodeWarrior PowerPC intrinsics the Dolphin SDK sources use (src/lib), in C.
 * Force-included into the SDK units of the port (CMakeLists.txt re4_sdk_obj).
 *
 *   __rlwimi(a, b, sh, mb, me)  rotate b left by sh, insert the bits mb..me
 *                               (IBM numbering, bit 0 = most significant) into a
 *                               (the GX register field macro SET_REG_FIELD)
 *   __cntlzw(x)                 count leading zeros of a word (32 for 0)
 *
 * The instructions hold sh, mb and me in 5-bit fields, so only the low 5 bits of each count
 * (GXBump.c __GXSetIndirectMask passes ~0xFF as the shift: sh 0, mb 24, me 31 on the GameCube,
 * the low byte of bpMask). Unmasked, C shifts by 32 or more are undefined: x86 took the count
 * modulo 32 and made the mask empty (bpMask kept 0xFF), ARM takes it modulo 256 (the sanitizer
 * build found it, bug hunt part 2, 22.10.2026). */
#ifndef RE4_SDK_INTRINSICS_H
#define RE4_SDK_INTRINSICS_H

static inline unsigned int re4_ppc_mask(int mb, int me)
{
    mb &= 31;
    me &= 31;
    unsigned int from = 0xFFFFFFFFu >> mb;
    unsigned int to = me == 31 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> (me + 1));
    return mb <= me ? (from & to) : (from | to);
}

static inline unsigned int __rlwimi(unsigned int a, unsigned int b, int sh, int mb, int me)
{
    sh &= 31;
    unsigned int rotated = sh ? (b << sh) | (b >> (32 - sh)) : b;
    unsigned int mask = re4_ppc_mask(mb, me);
    return (rotated & mask) | (a & ~mask);
}

static inline unsigned int __cntlzw(unsigned int x)
{
    return x ? (unsigned int) __builtin_clz(x) : 32u;
}

#endif
