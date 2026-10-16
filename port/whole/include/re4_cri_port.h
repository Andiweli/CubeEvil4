/* Port helpers for the CRI middleware (ADX, Sofdec, CVFS) of src/lib, compiled
 * unchanged apart from the reviewed patches (CMakeLists.txt re4_cri_obj,
 * port/docs/whole-program-status.md "M9 Videos"). Force-included into every
 * CRI unit after the SDK intrinsics.
 *
 * CodeWarrior PowerPC intrinsics the CRI sources call:
 *   __dcbt(p, o)            cache prefetch: nothing
 *   __dcbz(p, o)            zero the 32-byte cache line holding p + o: the
 *                           memory is zeroed, as on the GameCube
 *   __lwbrx(p, o)           byte-reversed word load: the four bytes at p + o
 *                           with the first in the lowest bits
 *   __rlwinm(x, sh, mb, me) rotate left, keep the bits mb..me (IBM numbering)
 *   __frsqrte(x)            reciprocal square root estimate; the Gekko's
 *                           table estimate (5 bits) is refined by three Newton
 *                           steps in the only caller (adx_dcd.c sqrtf), so the
 *                           exact value is used as the estimate
 *
 * Byte order: the CRI code reads stream data (MPEG bit stream, Sofdec and ADX
 * headers, ADX frames) with word loads that assume the GameCube's order. The
 * patches read such data through these helpers instead. */
#ifndef RE4_CRI_PORT_H
#define RE4_CRI_PORT_H

#include "re4_sdk_intrinsics.h"

#define __dcbt(p, o) ((void) (p), (void) (o))
#define __dcbz(p, o) \
    __builtin_memset((void*) (((unsigned long) (p) + (unsigned long) (o)) & ~31UL), 0, 32)

static inline unsigned int re4_be16(const void* p)
{
    const unsigned char* b = (const unsigned char*) p;
    return (unsigned int) b[0] << 8 | b[1];
}

static inline unsigned int re4_be32(const void* p)
{
    const unsigned char* b = (const unsigned char*) p;
    return (unsigned int) b[0] << 24 | (unsigned int) b[1] << 16 | (unsigned int) b[2] << 8 | b[3];
}

static inline unsigned int re4_le16(const void* p)
{
    const unsigned char* b = (const unsigned char*) p;
    return (unsigned int) b[1] << 8 | b[0];
}

static inline unsigned int re4_le32(const void* p)
{
    const unsigned char* b = (const unsigned char*) p;
    return (unsigned int) b[3] << 24 | (unsigned int) b[2] << 16 | (unsigned int) b[1] << 8 | b[0];
}

static inline void re4_set_be16(void* p, unsigned int v)
{
    unsigned char* b = (unsigned char*) p;
    b[0] = (unsigned char) (v >> 8);
    b[1] = (unsigned char) v;
}

static inline void re4_set_be32(void* p, unsigned int v)
{
    unsigned char* b = (unsigned char*) p;
    b[0] = (unsigned char) (v >> 24);
    b[1] = (unsigned char) (v >> 16);
    b[2] = (unsigned char) (v >> 8);
    b[3] = (unsigned char) v;
}

static inline unsigned int __lwbrx(const void* p, int o)
{
    return re4_le32((const unsigned char*) p + o);
}

static inline unsigned int __rlwinm(unsigned int x, int sh, int mb, int me)
{
    unsigned int rotated = sh ? (x << sh) | (x >> (32 - sh)) : x;
    return rotated & re4_ppc_mask(mb, me);
}

static inline double __frsqrte(double x)
{
    return 1.0 / __builtin_sqrt(x);
}

/* Paired singles of the translated CRI kernels (port/tools/whole_ppc_to_c.py):
 * an FPR is two floats (ps0, ps1) and, for lfd/stfd copies, 8 raw bytes.
 * CodeWarrior's __vec2x32float__ register variables (dct_fsri.c) are such
 * pairs. The graphics quantisation registers the CRI code programs itself
 * (MPVUMC_SetGqr, DCT_FsriSetGqr) live in re4_cri_gqr (uty_ppc.c, patch 0048),
 * apart from the game's own fixed GQR2..5 (re4_ppc.h): CRI saved and restored
 * the game's around each decode (UTY_PushGqr/PopGqr), the port needs neither.
 * Quantised loads and stores as on the Gekko (and in Dolphin): load
 * value * 2^-scale, store value * 2^scale clamped to the type and truncated;
 * 16-bit types in the host's byte order (only translated kernels read them). */
typedef struct {
    float ps0, ps1;
    unsigned long long raw;
} re4_ps;
typedef re4_ps __vec2x32float__;
extern unsigned int re4_cri_gqr[8];

/* 2^scale for the 6-bit signed scale field (-32..31), built from the exponent bits */
static inline float re4_gqr_power(unsigned int scale)
{
    int s = (int) (scale & 0x3F);
    unsigned int bits = (unsigned int) (127 + (s >= 32 ? s - 64 : s)) << 23;
    float f;
    __builtin_memcpy(&f, &bits, 4);
    return f;
}

static inline unsigned int re4_gqr_size(unsigned int type)
{
    return type == 4 || type == 6 ? 1u : type == 5 || type == 7 ? 2u : 4u;
}

static inline float re4_psq_load1(const unsigned char* p, unsigned int type, float dequant)
{
    switch (type) {
    case 4:
        return (float) p[0] * dequant;
    case 5:
        return (float) *(const unsigned short*) p * dequant;
    case 6:
        return (float) (signed char) p[0] * dequant;
    case 7:
        return (float) *(const short*) p * dequant;
    default: {
        float f;
        __builtin_memcpy(&f, p, 4);
        return f;
    }
    }
}

static inline void re4_psq_store1(unsigned char* p, float v, unsigned int type, float quant)
{
    float q = v * quant;
    switch (type) {
    case 4:
        p[0] = (unsigned char) (q < 0.0f ? 0.0f : q > 255.0f ? 255.0f : q);
        break;
    case 5:
        *(unsigned short*) p = (unsigned short) (q < 0.0f ? 0.0f : q > 65535.0f ? 65535.0f : q);
        break;
    case 6:
        p[0] = (unsigned char) (signed char) (q < -128.0f ? -128.0f : q > 127.0f ? 127.0f : q);
        break;
    case 7:
        *(short*) p = (short) (q < -32768.0f ? -32768.0f : q > 32767.0f ? 32767.0f : q);
        break;
    default:
        __builtin_memcpy(p, &v, 4);
        break;
    }
}

static inline void re4_psq_l(re4_ps* f, const void* addr, int w, unsigned int gqr)
{
    const unsigned char* p = (const unsigned char*) addr;
    unsigned int type = (gqr >> 16) & 7;
    float dequant = 1.0f / re4_gqr_power(gqr >> 24);
    f->ps0 = re4_psq_load1(p, type, dequant);
    f->ps1 = w ? 1.0f : re4_psq_load1(p + re4_gqr_size(type), type, dequant);
}

static inline void re4_psq_st(const re4_ps* f, void* addr, int w, unsigned int gqr)
{
    unsigned char* p = (unsigned char*) addr;
    unsigned int type = gqr & 7;
    float quant = re4_gqr_power(gqr >> 8);
    re4_psq_store1(p, f->ps0, type, quant);
    if (!w) {
        re4_psq_store1(p + re4_gqr_size(type), f->ps1, type, quant);
    }
}

#endif
