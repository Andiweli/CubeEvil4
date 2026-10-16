// Compiled with the game's flags and headers (re4_game_flags) for the core
// test (re4_host --core-test, "card"): the memory card library of the SDK
// (src/lib/CARD*.c) on the port's EXI functions and card (runtime/exi.cpp,
// memcard.cpp), without disc data, on a card file of its own:
//
//   - a new card file is an unformatted Memory Card 251 (16 Mbit, 8 KB
//     sectors); slot B is empty
//   - mount says "broken" (no valid header), the format makes it usable,
//     CARDCheck finds directory and FAT in order, 251 blocks and 127 files free
//   - the game's two files (tests/../src/game/card.cpp): bh4_system (1 block)
//     and bh4_data00 (8 blocks) are made, written, given their status
//     (banner, icon, comment as cCard::makeCardStatus) and read back
//   - a file is deleted (its blocks are free again) and made anew
//   - a new start of the app (the card file read again, the SRAM flash ID
//     derived anew): mount, check and the files' contents as written
//   - no card in slot A: probe and mount say "no card"
//   - the synchronous calls (CARDMount, CARDFormat, CARDCreate, CARDWrite,
//     CARDRead, CARDDelete) on an OS thread of the port's scheduler
//     (re4_os_run): they sleep in __CARDSync until the card's interrupts,
//     delivered when the thread goes to sleep, wake them
//
// The asynchronous calls are waited for as the game's card task does: the
// result code polled, the pending EXI interrupts delivered in between (in
// the game the interrupt points of the OS emulation do that).
#define new newName  // a parameter of CARDRename(Async) in this C header
#include <dolphin/card.h>
#undef new
#include <dolphin/os.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void CRCInit();  // src/game/card.cpp
u32 CRCCalc(u8* bufPtr, u32 nByte);

extern "C" {
int re4_os_run(int (*entry)());  // runtime/os_thread.cpp
s32 CARDFormatAsync(s32 chan, CARDCallback callback);
void re4_memcard_open(const char* path);
void re4_exi_deliver(void);
void re4_log(const char* fmt, ...);
int re4_mem1_init(void);
}

namespace {

int gFailures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            re4_log("card-test: %s:%d: %s", __FILE__, __LINE__, #cond); \
            gFailures++;                                                \
        }                                                               \
    } while (0)

const u32 kSystemSize = 0x2000, kDataSize = 0x10000;  // card.cpp: 1 and 8 blocks
u8 gWork[CARD_WORKAREA_SIZE] __attribute__((aligned(32)));
u8 gBuf[kDataSize] __attribute__((aligned(32)));
u8 gRead[kDataSize] __attribute__((aligned(32)));
s32 gCallbackResult;
int gCallbacks;

void callback(s32, s32 result)
{
    gCallbackResult = result;
    gCallbacks++;
}

// An asynchronous call started with result `started`: its end.
s32 finish(s32 started)
{
    if (started < 0) {
        return started;
    }
    for (int i = 0; i < 5000 && CARDGetResultCode(0) == CARD_RESULT_BUSY; i++) {
        re4_exi_deliver();
        if (i > 50) {
            usleep(1000);  // a card timeout alarm (100 ms / 2 s) would come from here
        }
    }
    s32 result = CARDGetResultCode(0);
    if (result != CARD_RESULT_BUSY && result != gCallbackResult) {
        re4_log("card-test: result %d, callback %d", (int) result, (int) gCallbackResult);
    }
    return result;
}

void put32(u8* p, u32 v)
{
    memcpy(p, &v, 4);  // the game stores its words in host order (card.cpp)
}

// The content of a test file: noise, with the game's own CRCs (card.cpp
// CRCCalc) where makeSaveData / makeSystemSaveData put them, so that
// port/tools/memcard_list.py can be checked against the game's code
// (whole_build.sh, RE4_CARD_TEST_FILE).
void fill(u8* p, u32 n, u32 seed)
{
    for (u32 i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        p[i] = (u8) (seed >> 16);
    }
    CRCInit();
    if (n == kSystemSize) {
        put32(p + 0x1E78, CRCCalc(p, 0x1E78));  // SYS_CRC
    } else if (n == kDataSize) {
        put32(p + 0x2004, 0x116);  // SaveInfo: magic, 2026-10-16 12:34:56, room r100
        const int when[10] = { 56, 34, 12, 16, 9, 2026, 5, 288, 0, 0 };
        memcpy(p + 0x2008, when, sizeof(when));
        u16 room = 0x100;
        memcpy(p + 0x2050, &room, 2);
        put32(p + 0x2000, CRCCalc(p + 0x2004, 0x1FC));
        put32(p + 0xEAF8, CRCCalc(p, 0xEAF8));  // SAVE_CRC
    }
}

s32 mount()
{
    return finish(CARDMountAsync(0, gWork, 0, callback));
}

s32 check()
{
    s32 xfer = 0;
    return finish(CARDCheckExAsync(0, &xfer, callback));
}

// Makes and writes file `name` as the game does (cCard::fileCreate, fileWrite,
// makeCardStatus): banner RGB5A3 at 0x40, one C8 icon at 0x1840, comment at 0.
bool makeFile(const char* name, u32 size, u32 seed)
{
    CARDFileInfo fi;
    s32 r = finish(CARDCreateAsync(0, name, size, &fi, callback));
    CHECK(r == CARD_RESULT_READY);
    if (r != CARD_RESULT_READY) {
        return false;
    }
    fill(gBuf, size, seed);
    r = finish(CARDWriteAsync(&fi, gBuf, (s32) size, 0, callback));
    CHECK(r == CARD_RESULT_READY);
    CARDStat stat;
    CHECK(CARDGetStatus(0, fi.fileNo, &stat) == CARD_RESULT_READY);
    CHECK(strcmp(stat.fileName, name) == 0 && stat.length == size);
    CHECK(memcmp(stat.gameName, "G4BE", 4) == 0 && memcmp(stat.company, "08", 2) == 0);
    CARDSetCommentAddress(&stat, 0);
    CARDSetBannerFormat(&stat, CARD_STAT_BANNER_RGB5A3);
    CARDSetIconAddress(&stat, 0x40);
    CARDSetIconFormat(&stat, 0, CARD_STAT_ICON_C8);
    CARDSetIconSpeed(&stat, 0, CARD_STAT_SPEED_SLOW);
    CARDSetIconFormat(&stat, 1, CARD_STAT_ICON_NONE);
    CARDSetIconSpeed(&stat, 1, CARD_STAT_SPEED_END);
    r = finish(CARDSetStatusAsync(0, fi.fileNo, &stat, callback));
    CHECK(r == CARD_RESULT_READY);
    CHECK(CARDClose(&fi) == CARD_RESULT_READY);
    return r == CARD_RESULT_READY;
}

// File `name` holds what makeFile(seed) wrote, with its status.
void readFile(const char* name, u32 size, u32 seed)
{
    CARDFileInfo fi;
    s32 r = CARDOpen(0, name, &fi);
    CHECK(r == CARD_RESULT_READY);
    if (r != CARD_RESULT_READY) {
        re4_log("card-test: %s does not open (%d)", name, (int) r);
        return;
    }
    memset(gRead, 0, size);
    r = finish(CARDReadAsync(&fi, gRead, (s32) size, 0, callback));
    CHECK(r == CARD_RESULT_READY);
    fill(gBuf, size, seed);
    CHECK(memcmp(gRead, gBuf, size) == 0);
    CARDStat stat;
    CHECK(CARDGetStatus(0, fi.fileNo, &stat) == CARD_RESULT_READY);
    CHECK(stat.length == size && CARDGetBannerFormat(&stat) == CARD_STAT_BANNER_RGB5A3);
    CHECK(stat.iconAddr == 0x40 && stat.commentAddr == 0 && CARDGetIconFormat(&stat, 0) == CARD_STAT_ICON_C8);
    // banner 96x32 RGB5A3, then the C8 icon and its palette (CARDStat.c)
    CHECK(stat.offsetBanner == 0x40 && stat.offsetIcon[0] == 0x40 + 0x1800 && stat.offsetIconTlut == 0x1C40);
    CHECK(stat.offsetData == 0x1E40);
    CHECK(CARDClose(&fi) == CARD_RESULT_READY);
}

s32 freeBlocks()
{
    s32 bytes = -1, files = -1;
    CHECK(CARDFreeBlocks(0, &bytes, &files) == CARD_RESULT_READY);
    return bytes / 0x2000;
}

char gSyncPath[256];

// The synchronous API on the boot thread of the OS emulation.
int syncMain()
{
    int before = gFailures;
    re4_memcard_open(gSyncPath);
    CHECK(CARDMount(0, gWork, 0) == CARD_RESULT_BROKEN);
    CHECK(CARDFormat(0) == CARD_RESULT_READY);
    CHECK(CARDCheck(0) == CARD_RESULT_READY);
    CARDFileInfo fi;
    CHECK(CARDCreate(0, "bh4_data05", kDataSize, &fi) == CARD_RESULT_READY);
    fill(gBuf, kDataSize, 7);
    CHECK(CARDWrite(&fi, gBuf, (s32) kDataSize, 0) == CARD_RESULT_READY);
    CHECK(CARDClose(&fi) == CARD_RESULT_READY);
    CHECK(CARDOpen(0, "bh4_data05", &fi) == CARD_RESULT_READY);
    memset(gRead, 0, kDataSize);
    CHECK(CARDRead(&fi, gRead, (s32) kDataSize, 0) == CARD_RESULT_READY);
    CHECK(memcmp(gRead, gBuf, kDataSize) == 0);
    CHECK(CARDClose(&fi) == CARD_RESULT_READY);
    CHECK(freeBlocks() == 251 - 8);
    CHECK(CARDDelete(0, "bh4_data05") == CARD_RESULT_READY);
    CHECK(freeBlocks() == 251);
    CHECK(CARDUnmount(0) == CARD_RESULT_READY);
    return gFailures - before;
}

}  // namespace

// The card checks; the number of failed checks.
extern "C" int re4probe_card_test(void)
{
    gFailures = 0;
    CHECK(re4_mem1_init());  // the disc ID in low memory (CARDInit, CARDCreate)
    // RE4_CARD_TEST_FILE: the card stays there afterwards (whole_build.sh lists it)
    char path[256];
    const char* keep = getenv("RE4_CARD_TEST_FILE");
    if (keep && *keep) {
        snprintf(path, sizeof(path), "%s", keep);
    } else {
        snprintf(path, sizeof(path), "/tmp/re4-card-test-%d.bin", (int) getpid());
    }
    unlink(path);
    re4_memcard_open(path);
    CARDInit();

    // a new card: present, unformatted
    s32 memSize = 0, sectorSize = 0;
    CHECK(CARDProbeEx(0, &memSize, &sectorSize) == CARD_RESULT_READY);
    CHECK(memSize == 16 && sectorSize == 0x2000);
    CHECK(CARDProbeEx(1, 0, 0) == CARD_RESULT_NOCARD);
    // (the game's headers have a struct stat of their own; lseek64 by name, as
    // runtime/memcard.cpp says)
    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0 && lseek64(fd, 0, SEEK_END) == 2 * 1024 * 1024);
    if (fd >= 0) {
        close(fd);
    }
    s32 r = mount();
    CHECK(r == CARD_RESULT_BROKEN);
    CHECK(gCallbacks > 0);

    // format: an empty card
    r = finish(CARDFormatAsync(0, callback));
    CHECK(r == CARD_RESULT_READY);
    CHECK(check() == CARD_RESULT_READY);
    CHECK(freeBlocks() == 251);
    u16 encode = 0xFFFF;
    CHECK(CARDGetEncoding(0, &encode) == CARD_RESULT_READY && encode == CARD_ENCODE_ANSI);
    u64 serial = 0;
    CHECK(CARDGetSerialNo(0, &serial) == CARD_RESULT_READY);

    // the game's files
    CHECK(makeFile("bh4_system", kSystemSize, 1));
    CHECK(makeFile("bh4_data00", kDataSize, 2));
    CHECK(freeBlocks() == 251 - 1 - 8);
    CARDFileInfo fi;
    CHECK(finish(CARDCreateAsync(0, "bh4_system", kSystemSize, &fi, callback)) == CARD_RESULT_EXIST);
    readFile("bh4_system", kSystemSize, 1);
    readFile("bh4_data00", kDataSize, 2);
    CHECK(CARDOpen(0, "bh4_data01", &fi) == CARD_RESULT_NOFILE);

    // delete and make anew
    CHECK(finish(CARDDeleteAsync(0, "bh4_data00", callback)) == CARD_RESULT_READY);
    CHECK(freeBlocks() == 251 - 1);
    CHECK(CARDOpen(0, "bh4_data00", &fi) == CARD_RESULT_NOFILE);
    CHECK(makeFile("bh4_data00", kDataSize, 3));
    CHECK(check() == CARD_RESULT_READY);
    CHECK(CARDUnmount(0) == CARD_RESULT_READY);

    // a new start of the app: the card from its file
    re4_memcard_open(path);
    r = mount();
    CHECK(r == CARD_RESULT_READY);
    CHECK(check() == CARD_RESULT_READY);
    u64 serialAgain = 1;
    CHECK(CARDGetSerialNo(0, &serialAgain) == CARD_RESULT_READY && serialAgain == serial);
    CHECK(freeBlocks() == 251 - 1 - 8);
    readFile("bh4_system", kSystemSize, 1);
    readFile("bh4_data00", kDataSize, 3);
    CHECK(CARDUnmount(0) == CARD_RESULT_READY);

    // the synchronous calls on an OS thread, on a card of their own
    snprintf(gSyncPath, sizeof(gSyncPath), "%s.sync", path);
    unlink(gSyncPath);
    re4_os_run(syncMain);
    unlink(gSyncPath);

    // no card in slot A
    re4_memcard_open(0);
    CHECK(CARDProbeEx(0, 0, 0) == CARD_RESULT_NOCARD);
    CHECK(mount() == CARD_RESULT_NOCARD);
    if (!keep || !*keep) {
        unlink(path);
    }
    re4_log("card-test: %d failed", gFailures);
    return gFailures;
}

// re4_host --memcard-format FILE: FILE becomes a formatted, empty card (made
// when missing; what was on it is gone). 1 on success.
extern "C" int re4probe_card_format(const char* path)
{
    re4_mem1_init();
    re4_memcard_open(path);
    CARDInit();
    s32 r = mount();
    if (r == CARD_RESULT_READY || r == CARD_RESULT_BROKEN || r == CARD_RESULT_ENCODING) {
        r = finish(CARDFormatAsync(0, callback));
    }
    if (r == CARD_RESULT_READY) {
        r = check();
    }
    s32 blocks = r == CARD_RESULT_READY ? freeBlocks() : 0;
    CARDUnmount(0);
    re4_memcard_open(0);  // the file synced and closed
    re4_log("memcard-format: %s %s (%d blocks free)", path, r == CARD_RESULT_READY ? "formatted" : "FAILED",
            (int) blocks);
    return r == CARD_RESULT_READY && blocks == 251;
}
