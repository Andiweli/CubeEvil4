// Memory card (CARD): until saving is ported (milestone 10) both slots are
// empty. Every call answers CARD_RESULT_NOCARD, exactly what the SDK reports
// for an empty slot, so the game takes its "no memory card" paths (boot check:
// card.cpp cCard::firstCheck00 -> MES_NO_CARD_START). Asynchronous calls fail
// immediately and, as in the SDK, do not call their callback.
#include "re4_runtime.h"

namespace {

typedef long s32;
typedef unsigned long u32;
typedef void (*CARDCallback)(s32 chan, s32 result);

const s32 kNoCard = -3;  // CARD_RESULT_NOCARD

}  // namespace

extern "C" {

void CARDInit(void) {}

s32 CARDProbeEx(s32, s32*, s32*) { return kNoCard; }
s32 CARDMountAsync(s32, void*, CARDCallback, CARDCallback) { return kNoCard; }
s32 CARDUnmount(s32) { return kNoCard; }
s32 CARDGetResultCode(s32) { return kNoCard; }
s32 CARDCheckAsync(s32, CARDCallback) { return kNoCard; }
s32 CARDFreeBlocks(s32, s32*, s32*) { return kNoCard; }
s32 CARDGetSerialNo(s32, unsigned long long*) { return kNoCard; }
s32 CARDFormatAsync(s32, CARDCallback) { return kNoCard; }
s32 CARDCreateAsync(s32, const char*, u32, void*, CARDCallback) { return kNoCard; }
s32 CARDDeleteAsync(s32, const char*, CARDCallback) { return kNoCard; }
s32 CARDOpen(s32, const char*, void*) { return kNoCard; }
s32 CARDClose(void*) { return kNoCard; }
s32 CARDReadAsync(void*, void*, s32, s32, CARDCallback) { return kNoCard; }
s32 CARDWriteAsync(void*, void*, s32, s32, CARDCallback) { return kNoCard; }
s32 CARDGetStatus(s32, s32, void*) { return kNoCard; }
s32 CARDSetStatusAsync(s32, s32, void*, CARDCallback) { return kNoCard; }

}  // extern "C"
