// Files on the port's memory card for the app's menu (M11, .gci import and
// export; re4_card_files.h). Works on the card image of runtime/memcard.cpp
// the way the SDK's card library does (src/lib/CARDBlock.c, CARDDir.c,
// CARDCreate.c, CARDDelete.c): the directory and the FAT are kept twice
// (blocks 1/2 and 3/4), the copy with the higher check code counts; a change
// goes, with the check code one higher and new checksums, into the other
// copy, so the current one stays whole until the new one is written. Blocks
// are taken from the FAT as __CARDAllocBlock takes them (after the last slot,
// wrapping round to block 5).
//
// Order of a write: the file's blocks (free ones, nothing points at them
// yet), then the FAT, then the directory.
#include "re4_card_files.h"
#include "re4_runtime.h"

#include <stdio.h>
#include <string.h>

extern "C" {
unsigned char* re4_memcard_mem(void);                       // runtime/memcard.cpp
void re4_memcard_store(unsigned offset, unsigned length);
void re4_memcard_flush(int force);
void re4_gci_describe(Re4CardFile* file, const unsigned char* data, unsigned bytes);  // runtime/gci.cpp
}

namespace {

typedef unsigned char u8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned u32;

const u32 kBlock = RE4_CARD_BLOCK;
const int kEntries = 127;
const u32 kDirCode = 0x1FFA, kDirSum = 0x1FFC;  // CARDDirCheck: checkCode, checkSum, checkSumInv

// __CARDCheckSum (CARDCheck.c): the sum of the u16 words and of their
// complements, 0xFFFF counting as 0.
void checkSum(const u8* p, u32 bytes, u16* sum, u16* inv)
{
    u16 s = 0, n = 0;
    for (u32 i = 0; i < bytes; i += 2) {
        u16 w;
        memcpy(&w, p + i, 2);
        s += w;
        n += (u16) ~w;
    }
    *sum = s == 0xFFFF ? 0 : s;
    *inv = n == 0xFFFF ? 0 : n;
}

bool sumHolds(const u8* p, u32 bytes, const u8* stored)
{
    u16 s, n, ws, wn;
    checkSum(p, bytes, &s, &n);
    memcpy(&ws, stored, 2);
    memcpy(&wn, stored + 2, 2);
    return s == ws && n == wn;
}

u16 get16(const u8* p)
{
    u16 v;
    memcpy(&v, p, 2);
    return v;
}

u32 get32(const u8* p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

void put16(u8* p, u16 v)
{
    memcpy(p, &v, 2);
}

// The card as the library would mount it: header formatted, the current
// directory and FAT copy (CARDCheck.c __CARDVerify: both valid, the higher
// check code; one valid, that one).
struct Card {
    u8* mem = 0;
    int dir = -1, fat = -1;
    u32 blocks = 0;  // blocks of the card, system blocks included

    bool open()
    {
        mem = re4_memcard_mem();
        if (!mem || !sumHolds(mem, 0x1FC, mem + 0x1FC) || get16(mem + 0x20) != 0) {
            return false;
        }
        u32 mbit = get16(mem + 0x22);
        blocks = mbit * 1024 * 1024 / 8 / kBlock;
        if (blocks < 6 || blocks > 0x1000) {
            return false;
        }
        dir = current(1, kDirCode, kDirSum, 0);
        fat = current(3, 4, 0, 4);
        return dir >= 0 && fat >= 0;
    }

    int current(int first, u32 codeAt, u32 sumAt, u32 from)
    {
        bool ok[2];
        s16 code[2];
        for (int i = 0; i < 2; i++) {
            const u8* b = mem + (first + i) * kBlock;
            ok[i] = sumHolds(b + from, 0x1FFC, b + sumAt);
            memcpy(&code[i], b + codeAt, 2);
        }
        if (ok[0] && ok[1]) {
            return (s16) (code[0] - code[1]) < 0 ? 1 : 0;
        }
        return ok[0] ? 0 : ok[1] ? 1 : -1;
    }

    const u8* dirBlock() const { return mem + (1 + dir) * kBlock; }
    const u8* fatBlock() const { return mem + (3 + fat) * kBlock; }
    bool validBlock(u32 n) const { return n >= 5 && n < blocks; }
};

bool sameFile(const u8* a, const u8* b)
{
    return memcmp(a, b, 6) == 0 && strncmp((const char*) a + 8, (const char*) b + 8, 32) == 0;
}

void copyText(char* out, const u8* in, int bytes)
{
    memcpy(out, in, (size_t) bytes);
    out[bytes] = 0;
}

}  // namespace

extern "C" {

int re4_card_free_blocks(void)
{
    Card c;
    return c.open() ? get16(c.fatBlock() + 6) : -1;
}

int re4_card_list(Re4CardFile* out, int max)
{
    Card c;
    if (!c.open()) {
        return RE4_CARD_NOCARD;
    }
    static u8 data[128 * kBlock];
    int n = 0;
    for (int i = 0; i < kEntries && n < max; i++) {
        const u8* e = c.dirBlock() + i * 64;
        if (e[0] == 0xFF) {
            continue;
        }
        Re4CardFile& f = out[n++];
        memset(&f, 0, sizeof(f));
        f.index = i;
        copyText(f.game, e, 4);
        copyText(f.company, e + 4, 2);
        copyText(f.name, e + 8, 32);
        f.blocks = get16(e + 0x38);
        f.time = get32(e + 0x28);
        u8 dir[64];
        int bytes = re4_card_read(i, dir, data, sizeof(data));
        u32 comment = get32(e + 0x3C);
        if (bytes > 0 && comment != 0xFFFFFFFFu && comment + 64 <= (u32) bytes) {
            copyText(f.comment[0], data + comment, 32);
            copyText(f.comment[1], data + comment + 32, 32);
        }
        if (bytes > 0) {
            re4_gci_describe(&f, data, (unsigned) bytes);
        }
    }
    return n;
}

int re4_card_read(int index, unsigned char* dir, unsigned char* data, unsigned capacity)
{
    Card c;
    if (!c.open()) {
        return RE4_CARD_NOCARD;
    }
    if (index < 0 || index >= kEntries || c.dirBlock()[index * 64] == 0xFF) {
        return RE4_CARD_NOFILE;
    }
    const u8* e = c.dirBlock() + index * 64;
    memcpy(dir, e, 64);
    if (!data) {
        return 0;  // the entry only
    }
    u32 length = get16(e + 0x38), block = get16(e + 0x36);
    if (length * kBlock > capacity) {
        return RE4_CARD_NOSPACE;
    }
    const u8* fat = c.fatBlock();
    for (u32 i = 0; i < length; i++) {
        if (!c.validBlock(block)) {
            re4_log("card files: %.32s: block chain broken after %u of %u blocks", (const char*) e + 8, i, length);
            return RE4_CARD_BAD;
        }
        memcpy(data + i * kBlock, c.mem + block * kBlock, kBlock);
        block = get16(fat + 2 * block);
    }
    return (int) (length * kBlock);
}

int re4_card_write(const unsigned char* entry, const unsigned char* data, int replace)
{
    Card c;
    if (!c.open()) {
        return RE4_CARD_NOCARD;
    }
    static u8 dir[0x2000], fat[0x2000];
    memcpy(dir, c.dirBlock(), kBlock);
    memcpy(fat, c.fatBlock(), kBlock);
    u16* table = (u16*) fat;
    u16 length = get16(entry + 0x38);
    if (length == 0 || length > c.blocks - 5) {
        return RE4_CARD_BAD;
    }
    int freeNo = -1;
    for (int i = 0; i < kEntries; i++) {
        u8* e = dir + i * 64;
        if (e[0] == 0xFF) {
            continue;
        }
        if (!sameFile(e, entry)) {
            continue;
        }
        if (!replace) {
            return RE4_CARD_EXISTS;
        }
        // the old file goes (CARDDelete.c / __CARDFreeBlock)
        u32 n = get16(e + 0x36);
        u32 freed = 0;
        while (n != 0xFFFF) {
            if (!c.validBlock(n) || freed++ > c.blocks) {
                return RE4_CARD_BAD;
            }
            u32 next = table[n];
            table[n] = 0;
            table[3]++;
            n = next;
        }
        memset(e, 0xFF, 64);
    }
    for (int i = 0; i < kEntries && freeNo < 0; i++) {
        if (dir[i * 64] == 0xFF) {
            freeNo = i;
        }
    }
    if (freeNo < 0 || table[3] < length) {
        return RE4_CARD_NOSPACE;
    }
    // __CARDAllocBlock
    u16 chain[0x1000];
    table[3] -= length;
    u16 start = 0xFFFF, prev = 0xFFFF, block = table[4];
    u32 count = 0, left = length;
    while (left > 0) {
        if (c.blocks - 5 < ++count) {
            return RE4_CARD_BAD;
        }
        block++;
        if (!c.validBlock(block)) {
            block = 5;
        }
        if (table[block] == 0) {
            if (start == 0xFFFF) {
                start = block;
            } else {
                table[prev] = block;
            }
            chain[length - left] = block;
            prev = block;
            table[block] = 0xFFFF;
            left--;
        }
    }
    table[4] = block;
    // the file's blocks
    for (u32 i = 0; i < length; i++) {
        memcpy(c.mem + chain[i] * kBlock, data + i * kBlock, kBlock);
        re4_memcard_store(chain[i] * kBlock, kBlock);
    }
    // the FAT into its other copy (__CARDUpdateFatBlock)
    table[2]++;
    checkSum(fat + 4, 0x1FFC, &table[0], &table[1]);
    u32 fatAt = (3 + (1 - c.fat)) * kBlock;
    memcpy(c.mem + fatAt, fat, kBlock);
    re4_memcard_store(fatAt, kBlock);
    // the directory into its other copy (__CARDUpdateDir)
    u8* e = dir + freeNo * 64;
    memcpy(e, entry, 64);
    put16(e + 0x36, start);
    s16 code;
    memcpy(&code, dir + kDirCode, 2);
    code++;
    memcpy(dir + kDirCode, &code, 2);
    u16 sum, inv;
    checkSum(dir, 0x1FFC, &sum, &inv);
    put16(dir + kDirSum, sum);
    put16(dir + kDirSum + 2, inv);
    u32 dirAt = (1 + (1 - c.dir)) * kBlock;
    memcpy(c.mem + dirAt, dir, kBlock);
    re4_memcard_store(dirAt, kBlock);
    re4_memcard_flush(1);
    re4_log("card files: %.32s written (%u blocks from block %u, directory entry %d)", (const char*) entry + 8,
            (unsigned) length, (unsigned) start, freeNo);
    return RE4_CARD_OK;
}

}  // extern "C"
