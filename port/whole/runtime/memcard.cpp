// The memory card in slot A (M10): a GameCube Memory Card 251 (16 Mbit,
// 2 MB, 8 KB sectors, 251 user blocks) as a file, driven by the card
// library of the SDK (src/lib/CARD*.c, compiled unchanged but for patch 0057)
// through the EXI functions of runtime/exi.cpp.
//
// The card speaks the command protocol of the real one (as Dolphin's
// EXI_DeviceMemoryCard does): 0x52 read array (5 command bytes, latency
// bytes, then the data), 0xF2 page program (5 bytes, 128 data bytes), 0xF1
// sector erase (3 bytes), 0xF4 chip erase, 0x81 interrupt enable, 0x83 read
// status, 0x85 vendor ID, 0x89 clear status. Address bytes: AD1 << 17 | AD2
// << 9 | AD3 << 7 | BA. A program or erase ends with the card's EXI
// interrupt (runtime/exi.cpp delivers it after the library has let go of
// the channel). The status says "unlocked" from the start, so the library
// skips the DSP challenge of CARDUnlock.c (Dolphin does the same).
//
// The file holds the card as the port's library writes it: directory, FAT
// and the game's save data in host byte order (decision of the project
// owner, 16.10.2026: a card for the port, not GameCube/Dolphin compatible).
// Every program and erase is written through to the file at once;
// a second after the last one it is synced (re4_memcard_flush).
//
//   RE4_MEMCARD unset   <data dir>/memcard/card-a.bin (made when missing: formatted and
//                       empty like a new card; RE4_MEMCARD_NEW=blank: unformatted)
//   RE4_MEMCARD=none    no card in slot A (the game's "no memory card" screens)
//   RE4_MEMCARD=FILE    that file
// Slot B stays empty.
//
// The SRAM (OSSram / OSSramEx, battery memory of the console; OSRtc.c is not
// compiled) is kept here as well: CARD stores the card's flash ID in it at
// mount and checks it against the card header (CARDCheck.c VerifyID). The
// port derives the flash ID from the header of the card file when the card
// comes in (as Dolphin's SetCardFlashID), so a card stays valid without a
// saved SRAM; an unformatted card gets a fixed one.
#include "dolphin/os.h"
#include "dolphin/os/OSRtc.h"
#include "re4_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// (no C++ containers: this file is built with the game's flags and headers,
// re4_formats_obj, where libc++'s allocator does not compile. There the
// prelude's empty __asm__ also drops the C library's renames of the file
// functions (64-bit offsets on Linux, the fortified open/pread/pwrite of
// Android), so the card file is reached through runtime/host_file.cpp.)

extern "C" {
// CARDFormat.c stores __VIRegs[55] (DTV status) in the header (re4_card_port.h)
volatile unsigned short re4_card_vi_regs[64];
void re4_exi_raise(int chan);  // runtime/exi.cpp: the card's EXI interrupt
}

namespace {

const u32 kCardId = 0x00000010;  // EXI_MEMORY_CARD_251: 16 Mbit, 8 KB sectors, latency 4
const u32 kCardBytes = 16u * 1024 * 1024 / 8;
const u32 kSectorBytes = 0x2000;
const u8 kStatusReady = 0x01, kStatusUnlocked = 0x40, kStatusErrors = 0x18;

struct Card {
    bool ready = false;    // init() ran
    bool present = false;  // a card is in the slot
    char path[512] = {};
    int fd = -1;
    u8* mem = 0;
    u8 status = kStatusReady | kStatusUnlocked;
    bool interrupts = false;  // 0x81 01
    // the command of the current selection
    u8 cmd[5] = {};
    int cmdLen = 0, cmdNeed = 0;
    u32 address = 0, readPos = 0;
    u8 page[128] = {};
    int pageLen = 0;
    // write-through bookkeeping
    bool dirty = false;
    long long lastWrite = 0;
    unsigned writes = 0, erases = 0;
};

Card gCard;
OSSram gSram;
OSSramEx gSramEx;

long long nowNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void persist(u32 offset, u32 length)
{
    Card& c = gCard;
    if (c.fd < 0) {
        return;
    }
    if (re4_host_file_write(c.fd, &c.mem[offset], length, offset) != (long) length) {
        static bool logged;
        if (!logged) {
            logged = true;
            re4_log("memcard: writing %s failed (%s)", c.path, re4_host_file_error());
        }
        return;
    }
    c.dirty = true;
    c.lastWrite = nowNs();
}

// The flash ID CARDCheck.c VerifyID expects for this header: serial[i] =
// flashID[i] + rand_i, rand from the format time in serial[12..19] (host
// order, as CARDFormat.c stored it). A header without a valid checksum (an
// unformatted card) gets a fixed ID.
// The flash ID of a card without a valid header (and of a new card)
const u8 kFixedFlashId[12] = {'R', 'E', '4', 'P', 'O', 'R', 'T', 'C', 'A', 'R', 'D', 'A'};

void deriveFlashId()
{
    static const u8 kErased[0x200] = {0};
    const u8* id = gCard.mem ? gCard.mem : kErased;
    u16 sum = 0, inv = 0;
    for (int i = 0; i < 0x1FC; i += 2) {
        u16 w;
        memcpy(&w, id + i, 2);
        sum += w;
        inv += (u16) ~w;
    }
    if (sum == 0xFFFF) {
        sum = 0;
    }
    if (inv == 0xFFFF) {
        inv = 0;
    }
    u16 checkSum, checkSumInv, deviceId;
    memcpy(&checkSum, id + 0x1FC, 2);
    memcpy(&checkSumInv, id + 0x1FE, 2);
    memcpy(&deviceId, id + 0x20, 2);
    bool formatted = gCard.present && deviceId == 0 && checkSum == sum && checkSumInv == inv;
    if (formatted) {
        OSTime rand;
        memcpy(&rand, id + 12, sizeof(rand));
        for (int i = 0; i < 12; i++) {
            rand = (rand * 1103515245 + 12345) >> 16;
            gSramEx.flashID[0][i] = (u8) (id[i] - (u8) rand);
            rand = ((rand * 1103515245 + 12345) >> 16) & 0x7FFF;
        }
    } else {
        memcpy(gSramEx.flashID[0], kFixedFlashId, 12);
    }
    u8 check = 0;
    for (int i = 0; i < 12; i++) {
        check += gSramEx.flashID[0][i];
    }
    gSramEx.flashIDCheckSum[0] = (u8) ~check;
}

// __CARDCheckSum (CARDCheck.c): the sum of the u16 words and of their
// complements (0xFFFF counts as 0), stored at `store`.
void storeCheckSum(const u8* p, int bytes, u8* store)
{
    u16 sum = 0, inv = 0;
    for (int i = 0; i < bytes; i += 2) {
        u16 w;
        memcpy(&w, p + i, 2);
        sum += w;
        inv += (u16) ~w;
    }
    if (sum == 0xFFFF) {
        sum = 0;
    }
    if (inv == 0xFFFF) {
        inv = 0;
    }
    memcpy(store, &sum, 2);
    memcpy(store + 2, &inv, 2);
}

// A new card comes formatted and empty, as one from the shop (decision of the
// project owner, 17.10.2026: the game's format question only for a broken
// card). The five system blocks exactly as CARDFormat.c writes them
// (__CARDFormatRegionAsync): the header with the serial from the flash ID of
// deriveFlashId and the format time (now), ANSI encoding (OSGetFontEncode of
// a US console), counter bias, language and DTV status 0 (the port's SRAM and
// VI); two empty directories (check codes 0 and 1) and two FATs with 251 free
// blocks. The user blocks stay erased. RE4_MEMCARD_NEW=blank: an
// unformatted card instead (the game then offers to format it).
void formatNew(u8* mem)
{
    memset(mem, 0xFF, 5 * kSectorBytes);
    u8* id = mem;
    const u16 encode = 0, deviceId = 0, sizeMbit = (u16) (kCardBytes * 8 / (1024 * 1024));
    const u32 zero = 0;
    memcpy(id + 0x24, &encode, 2);
    memcpy(id + 20, &zero, 4);  // serial[20]: SRAM counter bias
    memcpy(id + 24, &zero, 4);  // serial[24]: SRAM language
    OSTime time = OSGetTime(), rand = time;
    for (int i = 0; i < 12; i++) {
        rand = (rand * 1103515245 + 12345) >> 16;
        id[i] = (u8) (kFixedFlashId[i] + rand);
        rand = ((rand * 1103515245 + 12345) >> 16) & 0x7FFF;
    }
    memcpy(id + 28, &zero, 4);  // serial[28]: the VI DTV status
    memcpy(id + 12, &time, sizeof(time));
    memcpy(id + 0x20, &deviceId, 2);
    memcpy(id + 0x22, &sizeMbit, 2);
    storeCheckSum(id, 0x1FC, id + 0x1FC);
    for (int i = 0; i < 2; i++) {
        u8* dir = mem + (1 + i) * kSectorBytes;
        const s16 code = (s16) i;
        memcpy(dir + 0x1FFA, &code, 2);  // CARDDirCheck: checkCode, then the sums
        storeCheckSum(dir, 0x1FFC, dir + 0x1FFC);
    }
    for (int i = 0; i < 2; i++) {
        u8* fat = mem + (3 + i) * kSectorBytes;
        memset(fat, 0, kSectorBytes);
        const u16 head[3] = { (u16) i, (u16) (kCardBytes / kSectorBytes - 5), 4 };  // check code, free, last slot
        memcpy(fat + 4, head, sizeof(head));
        storeCheckSum(fat + 4, 0x1FFC, fat);
    }
}

void logDirectory();

// The card of `path` comes into slot A (0 or "none": the slot stays empty).
void insert(const char* path)
{
    Card& c = gCard;
    c.ready = true;
    if (!path || !*path || strcmp(path, "none") == 0) {
        re4_log("memcard: slot A empty%s", path ? " (RE4_MEMCARD=none)" : "");
        deriveFlashId();
        return;
    }
    snprintf(c.path, sizeof(c.path), "%s", path);
    c.mem = (u8*) malloc(kCardBytes);
    memset(c.mem, 0xFF, kCardBytes);
    c.fd = re4_host_file_open(c.path);
    if (c.fd < 0) {
        re4_log("memcard: %s cannot be opened (%s), slot A empty", c.path, re4_host_file_error());
        deriveFlashId();
        return;
    }
    long got = re4_host_file_read(c.fd, c.mem, kCardBytes, 0);
    if (got != (long) kCardBytes) {
        // new (or short) file: the rest is erased flash, as a new card
        if (got < 0) {
            got = 0;
        }
        memset(c.mem + got, 0xFF, kCardBytes - (size_t) got);
        const char* kind = getenv("RE4_MEMCARD_NEW");
        bool blank = got != 0 || (kind && strcmp(kind, "blank") == 0);
        if (!blank) {
            formatNew(c.mem);
        }
        persist(0, kCardBytes);
        re4_log("memcard: %s %s (2 MB, Memory Card 251, %s)", c.path, got == 0 ? "made" : "completed",
                blank ? "unformatted" : "formatted, empty");
    } else {
        re4_log("memcard: %s (2 MB, Memory Card 251)", c.path);
    }
    c.present = true;
    deriveFlashId();
    logDirectory();
}

// __CARDCheckSum (CARDCheck.c) of `bytes` at p against the stored pair.
bool sumOk(const u8* p, int bytes, const u8* stored)
{
    u16 sum = 0, inv = 0;
    for (int i = 0; i < bytes; i += 2) {
        u16 w;
        memcpy(&w, p + i, 2);
        sum += w;
        inv += (u16) ~w;
    }
    if (sum == 0xFFFF) {
        sum = 0;
    }
    if (inv == 0xFFFF) {
        inv = 0;
    }
    u16 s, i;
    memcpy(&s, stored, 2);
    memcpy(&i, stored + 2, 2);
    return s == sum && i == inv;
}

// The current one of two copies (directory or FAT, CARDCheck.c): both valid,
// the higher check code; else the valid one; -1 when none is.
int currentCopy(const u8* first, int codeAt, int sumAt, int from, int bytes)
{
    bool ok[2];
    s16 code[2];
    for (int i = 0; i < 2; i++) {
        const u8* b = first + i * kSectorBytes;
        ok[i] = sumOk(b + from, bytes, b + sumAt);
        memcpy(&code[i], b + codeAt, 2);
    }
    if (ok[0] && ok[1]) {
        return (s16) (code[0] - code[1]) < 0 ? 1 : 0;
    }
    return ok[0] ? 0 : ok[1] ? 1 : -1;
}

// "memcard: files: ..." when the card's file list changed since the last
// line (at insertion and a second after writes): what is on the card, in
// the log of every run.
void logDirectory()
{
    static char last[1024];
    Card& c = gCard;
    if (!c.present || !c.mem) {
        return;
    }
    char line[1024];
    int n = 0;
    int dir = currentCopy(c.mem + kSectorBytes, 0x1FFA, 0x1FFC, 0, 0x1FFC);
    int fat = currentCopy(c.mem + 3 * kSectorBytes, 4, 0, 4, 0x1FFC);
    u16 header;
    memcpy(&header, c.mem + 0x20, 2);
    if (dir < 0 || fat < 0) {
        n = snprintf(line, sizeof(line), "not formatted (or broken: directory %d, FAT %d)", dir, fat);
    } else {
        const u8* d = c.mem + (1 + dir) * kSectorBytes;
        int files = 0;
        for (int i = 0; i < 127 && n < (int) sizeof(line) - 64; i++) {
            const u8* e = d + i * 64;
            if (e[0] == 0xFF && e[1] == 0xFF && e[2] == 0xFF && e[3] == 0xFF) {
                continue;
            }
            u16 blocks;
            memcpy(&blocks, e + 56, 2);
            n += snprintf(line + n, sizeof(line) - (size_t) n, "%s%.32s (%u)", files ? ", " : "", (const char*) e + 8,
                          (unsigned) blocks);
            files++;
        }
        u16 free;
        memcpy(&free, c.mem + (3 + fat) * kSectorBytes + 6, 2);
        snprintf(line + n, sizeof(line) - (size_t) n, "%s; %u of 251 blocks free", files ? "" : "none", (unsigned) free);
    }
    if (strcmp(line, last) != 0) {
        snprintf(last, sizeof(last), "%s", line);
        re4_log("memcard: files: %s", line);
    }
}

void init()
{
    if (gCard.ready) {
        return;
    }
    const char* env = getenv("RE4_MEMCARD");
    if (env && *env) {
        insert(env);
        return;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/memcard/card-a.bin", re4_data_dir());
    insert(path);
}

void eject()
{
    Card& c = gCard;
    if (c.fd >= 0) {
        if (c.dirty) {
            re4_host_file_sync(c.fd);
        }
        re4_host_file_close(c.fd);
    }
    free(c.mem);
    c = Card();
}

int needFor(u8 command)
{
    switch (command) {
    case 0x52:
    case 0xF2:
        return 5;
    case 0xF1:
    case 0xF4:
        return 3;
    case 0x00:
    case 0x81:
    case 0x83:
    case 0x85:
        return 2;
    default:
        return 1;
    }
}

}  // namespace

extern "C" {

int re4_memcard_present(int chan)
{
    init();
    return chan == 0 && gCard.present;
}

u32 re4_memcard_id(int chan)
{
    return re4_memcard_present(chan) ? kCardId : 0;
}

void re4_memcard_select(int chan)
{
    (void) chan;
    Card& c = gCard;
    c.cmdLen = c.cmdNeed = 0;
    c.readPos = 0;
    c.pageLen = 0;
}

void re4_memcard_write(int chan, const void* data, int length)
{
    (void) chan;
    Card& c = gCard;
    const u8* p = (const u8*) data;
    for (int i = 0; i < length; i++) {
        u8 b = p[i];
        if (c.cmdLen == 0) {
            c.cmd[0] = b;
            c.cmdLen = 1;
            c.cmdNeed = needFor(b);
        } else if (c.cmdLen < c.cmdNeed) {
            c.cmd[c.cmdLen++] = b;
        } else if (c.cmd[0] == 0xF2 && c.pageLen < (int) sizeof(c.page)) {
            c.page[c.pageLen++] = b;  // page program data
        }
        // 0x52: the latency bytes after the address are ignored
        if (c.cmdLen == c.cmdNeed && (c.cmd[0] == 0x52 || c.cmd[0] == 0xF2 || c.cmd[0] == 0xF1)) {
            c.address = ((u32) (c.cmd[1] & 0x7F) << 17) | ((u32) c.cmd[2] << 9);
            if (c.cmdNeed == 5) {
                c.address |= ((u32) (c.cmd[3] & 3) << 7) | (c.cmd[4] & 0x7F);
            }
        }
    }
}

void re4_memcard_read(int chan, void* data, int length)
{
    (void) chan;
    Card& c = gCard;
    u8* p = (u8*) data;
    for (int i = 0; i < length; i++) {
        u8 v = 0xFF;
        switch (c.cmdLen == c.cmdNeed ? c.cmd[0] : 0xFF) {
        case 0x52:
            v = c.mem[(c.address + c.readPos) & (kCardBytes - 1)];
            break;
        case 0x83:
            v = c.status;
            break;
        case 0x85:
            v = c.readPos == 0 ? 0xC2 : 0x21;  // vendor ID 0xC221 (as Dolphin's card)
            break;
        case 0x00:
            v = (u8) (kCardId >> (24 - 8 * (c.readPos & 3)));
            break;
        default:
            break;
        }
        c.readPos++;
        p[i] = v;
    }
}

// The end of a command: a page program or erase takes effect, then the card
// raises its interrupt (when enabled).
void re4_memcard_deselect(int chan)
{
    Card& c = gCard;
    if (c.cmdLen != c.cmdNeed || c.cmdLen == 0) {
        c.cmdLen = 0;
        return;
    }
    bool done = false;
    switch (c.cmd[0]) {
    case 0xF2: {
        u32 base = c.address & ~0x7Fu;
        for (int i = 0; i < c.pageLen; i++) {
            u32 at = base | ((c.address + (u32) i) & 0x7F);
            c.mem[at & (kCardBytes - 1)] = c.page[i];
        }
        persist(base & (kCardBytes - 1), 0x80);
        c.writes++;
        done = true;
        break;
    }
    case 0xF1: {
        u32 base = (c.address & (kCardBytes - 1)) & ~(kSectorBytes - 1);
        memset(&c.mem[base], 0xFF, kSectorBytes);
        persist(base, kSectorBytes);
        c.erases++;
        done = true;
        break;
    }
    case 0xF4:
        memset(c.mem, 0xFF, kCardBytes);
        persist(0, kCardBytes);
        done = true;
        break;
    case 0x81:
        c.interrupts = (c.cmd[1] & 1) != 0;
        break;
    case 0x89:
        c.status &= (u8) ~kStatusErrors;
        break;
    default:
        break;
    }
    c.cmdLen = 0;
    if (done && c.interrupts) {
        re4_exi_raise(chan);
    }
}

// A second after the last write the file is synced (from the interrupt
// points, runtime/exi.cpp); `force` syncs now.
void re4_memcard_flush(int force)
{
    Card& c = gCard;
    if (!c.dirty || c.fd < 0) {
        return;
    }
    if (!force && nowNs() - c.lastWrite < 1000000000LL) {
        return;
    }
    re4_host_file_sync(c.fd);
    c.dirty = false;
    re4_log("memcard: %s saved (%u pages written, %u sectors erased so far)", c.path, c.writes, c.erases);
    logDirectory();
}

// Whether the boot check shows "loading the system file" (patch 0060): only
// with RE4_CARD_BOOT_MESSAGE=1, as the original does.
int re4_card_boot_message(void)
{
    const char* v = getenv("RE4_CARD_BOOT_MESSAGE");
    return v && *v == '1';
}

// The game's card screen, every frame (patch 0058, cCard::MainLoop): its
// steps into the log when they change. mode 0 load, 1 save, 2 the boot
// check; rno0 the row of the MainLoop table, rno1 the step within it.
void re4_card_screen(int mode, int rno0, int rno1, int err, int saveNo, unsigned status)
{
    static const char* const kModes[3] = { "load", "save", "boot check" };
    static const char* const kStates[9][3] = {
        { "slot select", "slot select", "card check" },
        { "card check", "card check", "system file read" },
        { "file list", "file list", "result" },
        { "loading", "saving", "done" },
        { "exit", "exit", "exit" },
        { "message", "message", "message" },
        { "format", "format", "format" },
        { "delete", "delete", "delete" },
        { "-", "saving the system file", "making the system file" },
    };
    static int lastMode = -1, lastRno0 = -1, lastRno1 = -1;
    static long long lastCall;
    static unsigned lines;
    if (mode < 0 || mode > 2 || rno0 < 0 || rno0 > 8) {
        return;
    }
    long long now = nowNs();
    bool open = mode != lastMode || now - lastCall > 1000000000LL;
    lastCall = now;
    if (lines > 600) {
        return;
    }
    if (open) {
        lines++;
        re4_log("card: %s screen open", kModes[mode]);
        lastMode = mode;
        lastRno0 = lastRno1 = -1;
    }
    if (rno0 == lastRno0 && (rno1 == lastRno1 || (rno0 != 3 && rno0 != 6 && rno0 != 8))) {
        return;
    }
    char extra[160] = "";
    if (rno0 == 5) {
        const char* what = "";
        switch (err) {
        case -2: what = "wrong device"; break;
        case -3: what = "no memory card"; break;
        case -5: case -0x80: what = "card damaged"; break;
        case -6: what = "card broken / not formatted"; break;
        case -0xD: what = "other region, needs format"; break;
        case -0x200: what = "card not supported"; break;
        case -0x201: what = "not enough space"; break;
        case -0x202: what = "save data corrupted"; break;
        case -0x203: what = "saving failed"; break;
        case -0x204: what = "loading failed"; break;
        case -0x205: what = "saving the system file failed"; break;
        case -0x206: what = "reading the system file failed"; break;
        case -0x207: what = "no system file"; break;
        case -0x208: what = "system file not made"; break;
        case -0x209: what = "format cancelled"; break;
        case -0x20A: what = "no space for the system file"; break;
        case -0x20B: what = "no space for the save file"; break;
        default: break;
        }
        snprintf(extra, sizeof(extra), " (code %d%s%s)", err, *what ? ": " : "", what);
    } else if (rno0 == 3 && mode != 2) {
        snprintf(extra, sizeof(extra), " (file %d, step %d)", saveNo + 1, rno1);
    } else if (rno0 == 6 || rno0 == 8) {
        snprintf(extra, sizeof(extra), " (step %d)", rno1);
    } else if (rno0 == 4 || (mode == 2 && rno0 == 2)) {
        snprintf(extra, sizeof(extra), " (slot A: 0x%x%s%s%s%s)", status, (status & 2) ? " no card" : "",
                 (status & 0x10) ? " broken" : "", (status & 0x100) ? " saves" : "",
                 (status & 0x200) ? " system file" : "");
    }
    lines++;
    re4_log("card: %s: %s%s", kModes[mode], kStates[rno0][mode], extra);
    lastRno0 = rno0;
    lastRno1 = rno1;
}

// Takes the card out of slot A and puts the one of `path` in (0: none), as a
// new start of the app does: the file is read again, the flash ID derived
// anew (runtime tests, tests/card_probe.cpp). The CARD library must have
// unmounted the slot.
void re4_memcard_open(const char* path)
{
    eject();
    insert(path);
}

// --- SRAM ---------------------------------------------------------------------

OSSram* __OSLockSram(void)
{
    init();
    return &gSram;
}

OSSramEx* __OSLockSramEx(void)
{
    init();
    return &gSramEx;
}

int __OSUnlockSram(BOOL commit)
{
    (void) commit;
    return 1;
}

int __OSUnlockSramEx(BOOL commit)
{
    (void) commit;
    return 1;
}

}  // extern "C"
