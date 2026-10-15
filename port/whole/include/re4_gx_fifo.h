/* The GX FIFO of the port (runtime/gx_fifo.cpp).
 *
 * On the GameCube every GX command goes through one write address, the
 * write-gather pipe at 0xCC008000 (GXWGFifo): the GX library and the game's
 * inline vertex functions store bytes, halfwords, words and floats there.
 * whole_prepare.py rewrites each such store (`GXWGFifo.T = x`,
 * `GXWGFifo->T = x`) into a call of re4_gx_put_T(x), in the game's gx.h, the
 * SDK's GXVert.h and __gx.h. The port appends the value, big-endian as the
 * hardware sees it, to the command stream of the emulated graphics processor
 * (or, while a display list is recorded, to the list in memory).
 *
 * The hardware register blocks the GX library reads and writes (CP, PE, PI,
 * MEM) are port memory (re4_gx_register_block, patch 0030 in GXInit.c). */
#ifndef RE4_GX_FIFO_H
#define RE4_GX_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

void re4_gx_put_u8(unsigned char v);
void re4_gx_put_u16(unsigned short v);
void re4_gx_put_u32(unsigned int v);
void re4_gx_put_s8(signed char v);
void re4_gx_put_s16(short v);
void re4_gx_put_s32(int v);
void re4_gx_put_f32(float v);

/* The register block at physical address `paddr` (0x0C000000 CP, 0x0C001000
 * PE, 0x0C003000 PI, 0x0C004000 MEM). */
void* re4_gx_register_block(unsigned int paddr);

/* EFB access (GXPeekARGB, GXPeekZ, GXPoke*): the emulated frame buffer is
 * not readable from the game's side; a peek answers "nothing drawn" (colour
 * 0, depth far), a poke is ignored. */
unsigned int re4_gx_efb_peek(unsigned short x, unsigned short y, int z);

#ifdef __cplusplus
}
#endif

#endif
