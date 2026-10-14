/* C equivalents of the Gekko (GameCube CPU) instructions the game's inline asm
 * used. whole_prepare.py drops every inline asm; the reviewed patches 0020-0023
 * replace the asm with real instructions by these helpers, and
 * port/tools/whole_asm_check.py stops the build if one is dropped without a
 * replacement (port/docs/whole-program-status.md, "Assembler-Ersatz").
 * Included by re4_whole_prelude.h, so plain C types only. */
#ifndef RE4_PPC_H
#define RE4_PPC_H

#ifdef __cplusplus
extern "C" {
#endif
void re4_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/* The 16 KB locked data cache, at 0xE0000000 on the GameCube (LCEnable in
 * main.cpp). The game keeps the skinning matrix palette (trans.cpp) and the
 * cloth collision list (pendulum.cpp) there. The address is in use on the
 * port's hosts (32-bit Android maps libraries there), so the cache is a
 * buffer of the platform library (runtime/mem1.cpp). */
#define RE4_LOCKED_CACHE_SIZE 0x4000
extern unsigned char re4_locked_cache[RE4_LOCKED_CACHE_SIZE];
#ifdef __cplusplus
}
#endif
#define RE4_LC_BASE ((unsigned long) re4_locked_cache)

/* fabs: clears the sign bit (of a NaN too), no rounding. */
static inline float re4_fabsf(float x)
{
    return __builtin_fabsf(x);
}

/* GQR scale field (6 bits, signed): loads multiply by 2^-scale, stores by
 * 2^scale. */
static inline int re4_gqr_scale(unsigned field)
{
    field &= 63;
    return field >= 32 ? (int) field - 64 : (int) field;
}

/* Quantised store of one value (psq_st with W=1): clamp to the integer range,
 * then truncate towards zero, as Gekko does (Dolphin: ScaleAndClamp; the
 * earlier port's skinning, port/src/skinning.cpp). A NaN, which has no
 * defined result on the hardware, stores 0. */
static inline int re4_quantize(float f, float lo, float hi)
{
    if (f != f) {
        return 0;
    }
    if (f < lo) {
        return (int) lo;
    }
    if (f > hi) {
        return (int) hi;
    }
    return (int) f;
}

/* The fixed quantisation registers: main.cpp and scheduler.cpp (every task
 * thread) set GQR2 = u8, GQR3 = u16, GQR4 = s8, GQR5 = s16, all with scale 0,
 * and nothing changes them afterwards (GQR6 belongs to the skinning,
 * trans.cpp). Loads convert exactly. The game's data must already be in host
 * byte order (disc data: runtime/formats). */
static inline float re4_psq_l_u8(const void* p) /* psq_l f,0(p),1,2 */
{
    return (float) *(const unsigned char*) p;
}

static inline float re4_psq_l_s16(const void* p) /* psq_l f,0(p),1,5 */
{
    return (float) *(const short*) p;
}

static inline void re4_psq_st_s16(float f, void* p) /* psq_st f,0(p),1,5 */
{
    *(short*) p = (short) re4_quantize(f, -32768.0f, 32767.0f);
}

#endif
