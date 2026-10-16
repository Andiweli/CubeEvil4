/* The memory card library of the SDK (src/lib/CARD*.c, M10) on the port:
 * included before every CARD unit (CMakeLists.txt, re4_card_obj).
 *
 * - The OS low-memory bytes CARD reads (0x800030E3: card disabled by the
 *   IPL, 0x800030C0: probe times) are declared only for the CodeWarrior
 *   build (include/dolphin/os.h); MEM1 is at its GameCube address in the port
 *   (runtime/mem1.cpp), so they are read there.
 * - CARDFormat stores the VI DTV status (__VIRegs[55], component cable) in
 *   the card header; the port has no VI registers: a zero block stands in
 *   (no component cable, as VIGetDTVStatus answers).
 * - re4_card_cmd / re4_card_be16: the byte order of the one- and two-byte
 *   card commands (patch 0057). */
#ifndef RE4_CARD_PORT_H
#define RE4_CARD_PORT_H

#include <dolphin/hw_regs.h>

#define __gUnknown800030E3 (*(volatile unsigned char*) 0x800030E3)
#define __gUnknown800030C0 ((volatile int*) 0x800030C0)

#undef __VIRegs
extern volatile unsigned short re4_card_vi_regs[64];  /* runtime/memcard.cpp */
#define __VIRegs re4_card_vi_regs

/* A command word whose first byte in memory is its top byte (the command). */
static inline unsigned int re4_card_cmd(unsigned int word)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

/* Two bytes the card sent, high byte first, as a value. */
static inline unsigned short re4_card_be16(unsigned short bytes)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(bytes);
#else
    return bytes;
#endif
}

#endif
