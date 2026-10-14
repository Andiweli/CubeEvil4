// re4_host --core-test: the embedding interface of the core (runtime/re4_core.h)
// without disc data, the way the Android app uses it. A process of its own:
// the game thread runs the Dolphin OS emulation, which the other runtime tests
// also start.
//
//   bridge  the format converter's notes give the disc source of an archive
//           entry; one snapshot of a fake game state (tests/core_probe.cpp):
//           room, step, camera, the enemy work with its parts' world matrices,
//           bind positions and its BIN/TPL sources
//   thread  re4_core_reserve / re4_core_start_entry: the boot thread runs on a
//           thread of its own, the controller set with re4_core_pad arrives in
//           PADRead, one snapshot per frame, one reader at a time, the state
//           ends with the entry's return value, the log tail is readable
#include "re4_core.h"
#include "re4_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern "C" void re4probe_core_setup(void* mem, void* bin, void* tpl);
extern "C" void re4probe_core_restore(void);

namespace {

int gFailures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            re4_log("core-test: %s:%d: %s", __FILE__, __LINE__, #cond); \
            gFailures++;                                               \
            ok = false;                                                \
        }                                                              \
    } while (0)

void report(const char* name, bool ok)
{
    re4_log("core-test %s: %s", name, ok ? "ok" : "FAILED");
}

struct Header {  // DvdHeader (include/dvd.h)
    unsigned type, size, dest, ofs, sndType, sndArg, sndNo, x1C;
};

void put32be(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char) (v >> 24);
    p[1] = (unsigned char) (v >> 16);
    p[2] = (unsigned char) (v >> 8);
    p[3] = (unsigned char) v;
}

bool near(float a, float b)
{
    return fabsf(a - b) < 1e-4f;
}

extern "C" int re4_formats_describe(const void* addr, char* out, int size);

extern "C" void ARStartDMA(unsigned type, unsigned mainAddr, unsigned aramAddr, unsigned length);
extern "C" unsigned char* re4_aram(void);

Re4CoreSnapshot gSnap;

void testBridge()
{
    bool ok = true;
    CHECK(re4_mem1_init());
    // a big-endian archive from "em/em99.drs" (part at file offset 0x400):
    // two entries with tags no converter knows, so their bytes stay as they are
    unsigned char* arc = (unsigned char*) (unsigned long) 0x82000000u;
    memset(arc, 0, 0x8000);
    put32be(arc + 0x00, 2);
    put32be(arc + 0x10, 0x20);
    put32be(arc + 0x14, 0x40);
    memcpy(arc + 0x18, "XBN\0XTP\0", 8);
    Header h = { 0, 0x60, 0, 0x400, 0, 0, 0, 0 };
    re4_formats_part_loaded("em/em99.drs", 0, 0, &h, 0x82000000u);
    Re4CoreSource src;
    CHECK(re4_formats_locate(arc + 0x40, &src));
    CHECK(strcmp(src.file, "em/em99.drs") == 0 && src.fileOffset == 0x400 && src.archiveSize == 0x60 &&
          src.entry == 1);
    // nested data (a BIN inside an SMD entry): the entry around it and the offset in it
    CHECK(re4_formats_locate(arc + 0x44, &src) && src.entry == 1 && src.innerOffset == 4 &&
          strcmp(src.file, "em/em99.drs") == 0);
    CHECK(re4_formats_locate(arc + 0x40, &src) && src.entry == 1 && src.innerOffset == 0);
    CHECK(!re4_formats_locate(arc + 0x9000, &src) && src.entry == -1 && src.file[0] == 0);
    // r100 loads far more archives than the first note table held (64): the
    // first one must still be found after 100 more
    for (unsigned k = 0; k < 100; k++) {
        unsigned char* more = (unsigned char*) (unsigned long) (0x82100000u + k * 0x100);
        memset(more, 0, 0x100);
        put32be(more + 0x00, 1);
        put32be(more + 0x10, 0x20);
        memcpy(more + 0x14, "XBN\0", 4);
        char name[32];
        snprintf(name, sizeof(name), "em/t%02u.drs", k);
        Header hk = { 0, 0x40, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded(name, 0, 0, &hk, 0x82100000u + k * 0x100);
    }
    CHECK(re4_formats_locate(arc + 0x40, &src) && strcmp(src.file, "em/em99.drs") == 0 && src.entry == 1);
    CHECK(re4_formats_locate((unsigned char*) (unsigned long) 0x82106320u, &src) &&
          strcmp(src.file, "em/t99.drs") == 0 && src.entry == 0);
    char where[160];
    re4_formats_describe(arc + 0x44, where, sizeof(where));
    CHECK(strstr(where, "inside") && strstr(where, "+0x44"));
    re4_formats_describe((void*) (unsigned long) 0x82F00000u, where, sizeof(where));
    CHECK(strstr(where, "not in a noted archive") != 0);
    // memory reused: another archive loaded over em/t98.drs makes its notes stale
    {
        unsigned char* reuse = (unsigned char*) (unsigned long) 0x82106200u;
        memset(reuse, 0, 0x100);
        put32be(reuse + 0x00, 1);
        put32be(reuse + 0x10, 0x40);
        memcpy(reuse + 0x14, "XBN\0", 4);
        Header hr = { 0, 0x60, 0, 0x800, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/new.drs", 0, 0, &hr, 0x82106200u);
        CHECK(!re4_formats_locate(reuse + 0x20, &src));  // the old entry start
        CHECK(re4_formats_locate(reuse + 0x40, &src) && strcmp(src.file, "em/new.drs") == 0 && src.entry == 0);
        // and data loaded over an archive without a note of its own (its tags change)
        unsigned char* plain = (unsigned char*) (unsigned long) 0x82106000u;  // em/t96.drs
        CHECK(re4_formats_locate(plain + 0x24, &src) && strcmp(src.file, "em/t96.drs") == 0 &&
              src.innerOffset == 4);
        memset(plain, 0x5A, 0x40);
        // any new note makes the lookups run again
        unsigned char* other = (unsigned char*) (unsigned long) 0x82107000u;
        memset(other, 0, 0x40);
        put32be(other + 0x00, 1);
        put32be(other + 0x10, 0x20);
        memcpy(other + 0x14, "XBN\0", 4);
        Header hx = { 0, 0x40, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/other.drs", 0, 0, &hx, 0x82107000u);
        CHECK(!re4_formats_locate(plain + 0x24, &src));
        // a later load over the middle of an archive: only that part is stale
        unsigned char* big = (unsigned char*) (unsigned long) 0x82108000u;
        memset(big, 0, 0x80);
        put32be(big + 0x00, 3);
        put32be(big + 0x10, 0x40);
        put32be(big + 0x14, 0x50);
        put32be(big + 0x18, 0x60);
        memcpy(big + 0x1C, "XBN\0XTP\0XBN\0", 12);
        Header hb = { 0, 0x80, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/big.drs", 0, 0, &hb, 0x82108000u);
        CHECK(re4_formats_locate(big + 0x54, &src) && strcmp(src.file, "em/big.drs") == 0 && src.entry == 1 &&
              src.innerOffset == 4);
        unsigned char* middle = big + 0x50;
        memset(middle, 0, 0x40);  // reaches past the end of em/big.drs
        put32be(middle + 0x00, 1);
        put32be(middle + 0x10, 0x20);
        memcpy(middle + 0x14, "XBN\0", 4);
        Header hm = { 0, 0x40, 0, 0x800, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/middle.drs", 0, 0, &hm, 0x82108050u);
        CHECK(re4_formats_locate(big + 0x40, &src) && strcmp(src.file, "em/big.drs") == 0 && src.entry == 0);
        CHECK(!re4_formats_locate(big + 0x54, &src));
        CHECK(re4_formats_locate(big + 0x70, &src) && strcmp(src.file, "em/middle.drs") == 0 && src.entry == 0);
        re4_formats_describe(big + 0x48, where, sizeof(where));
        CHECK(strstr(where, "no entry starts there") != 0);
        re4_formats_describe(big + 0x54, where, sizeof(where));
        CHECK(strstr(where, "overwritten by a later load") != 0);
        // an event file (EVD): its models are found by their offset in the file
        // (entry -2); a sub screen swapping the memory out suspends the note
        // until the data is back
        unsigned char* evd = (unsigned char*) (unsigned long) 0x8210A000u;
        memset(evd, 0, 0x200);
        memcpy(evd, "event", 5);
        memcpy(evd + 0x20, "r100", 4);
        memcpy(evd + 0x28, "s00", 3);
        put32be(evd + 0x40, 0x50);  // packets (none)
        put32be(evd + 0x4C, 0x50);  // bin table (no entries)
        Header he = { 0, 0x200, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded("Evd/r100s00.evd", 0, 0, &he, 0x8210A000u);
        CHECK(re4_formats_locate(evd + 0x123, &src) && strcmp(src.file, "evd/r100s00.evd") == 0 &&
              src.entry == -2 && src.innerOffset == 0x123 && src.fileOffset == 0x400 && src.archiveSize == 0x200);
        unsigned char saved[0x40];
        memcpy(saved, evd, sizeof(saved));
        memset(evd, 0x77, sizeof(saved));  // swapped out
        unsigned char* trigger = (unsigned char*) (unsigned long) 0x8210C000u;
        memset(trigger, 0, 0x40);
        put32be(trigger + 0x00, 1);
        put32be(trigger + 0x10, 0x20);
        memcpy(trigger + 0x14, "XBN\0", 4);
        Header ht = { 0, 0x40, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/trigger.drs", 0, 0, &ht, 0x8210C000u);
        CHECK(!re4_formats_locate(evd + 0x123, &src));
        memcpy(evd, saved, sizeof(saved));  // back again
        memset(trigger, 0, 0x40);
        put32be(trigger + 0x00, 1);
        put32be(trigger + 0x10, 0x20);
        memcpy(trigger + 0x14, "XBN\0", 4);
        re4_formats_part_loaded("em/trigger2.drs", 0, 0, &ht, 0x8210C000u);
        CHECK(re4_formats_locate(evd + 0x123, &src) && src.entry == -2 && src.innerOffset == 0x123);
        // an event file loaded to ARAM (converted and noted there) and copied
        // to MEM1 when the event plays: found through the copy
        {
            unsigned aramAt = 0x00F00000u;  // high in ARAM, away from the game's stack
            unsigned char* inAram = re4_aram() + aramAt;
            memset(inAram, 0, 0x200);
            memcpy(inAram, "event", 5);
            memcpy(inAram + 0x20, "r100", 4);
            memcpy(inAram + 0x28, "s03", 3);
            put32be(inAram + 0x40, 0x50);
            put32be(inAram + 0x4C, 0x50);
            Header ha = { 3 /* TRANS_ARAM */, 0x200, 0, 0x400, 0, 0, 0, 0 };
            re4_formats_part_loaded("Evd/r100s03.evd", 0, 0, &ha, aramAt);
            unsigned char* inMem1 = (unsigned char*) (unsigned long) 0x8210E000u;
            ARStartDMA(1, 0x8210E000u, aramAt, 0x200);
            CHECK(memcmp(inMem1, "event", 5) == 0);
            CHECK(re4_formats_locate(inMem1 + 0x180, &src) && strcmp(src.file, "evd/r100s03.evd") == 0 &&
                  src.entry == -2 && src.innerOffset == 0x180);
            // an archive loaded there later wins over the copy
            memset(inMem1, 0, 0x60);
            put32be(inMem1 + 0x00, 1);
            put32be(inMem1 + 0x10, 0x20);
            memcpy(inMem1 + 0x14, "XBN\0", 4);
            Header hl = { 0, 0x60, 0, 0x400, 0, 0, 0, 0 };
            re4_formats_part_loaded("em/later.drs", 0, 0, &hl, 0x8210E000u);
            CHECK(re4_formats_locate(inMem1 + 0x20, &src) && strcmp(src.file, "em/later.drs") == 0 && src.entry == 0);
        }
        // an event borrows an enemy's archive memory (MemorySwap, patch 0028):
        // the event file goes from ARAM to the enemy's MRAM, the enemy's
        // archive to ARAM; the notes follow both ways and back
        {
            unsigned aramAt = 0x00E00000u;
            unsigned char* ev = re4_aram() + aramAt;
            memset(ev, 0, 0x200);
            memcpy(ev, "event", 5);
            memcpy(ev + 0x20, "r100", 4);
            memcpy(ev + 0x28, "s00", 3);
            put32be(ev + 0x40, 0x50);
            put32be(ev + 0x4C, 0x50);
            Header hv = { 3 /* TRANS_ARAM */, 0x200, 0, 0x400, 0, 0, 0, 0 };
            re4_formats_part_loaded("Evd/r100s00.evd", 0, 0, &hv, aramAt);
            unsigned char* enemy = (unsigned char*) (unsigned long) 0x82110000u;
            memset(enemy, 0, 0x200);
            put32be(enemy + 0x00, 1);
            put32be(enemy + 0x10, 0x40);
            memcpy(enemy + 0x14, "XBN\0", 4);
            Header hn = { 0, 0x200, 0, 0x400, 0, 0, 0, 0 };
            re4_formats_part_loaded("em/em12.drs", 0, 0, &hn, 0x82110000u);
            CHECK(re4_formats_locate(enemy + 0x40, &src) && strcmp(src.file, "em/em12.drs") == 0);
            unsigned char tmp[0x200];
            memcpy(tmp, enemy, 0x200);  // what MemorySwap does
            memcpy(enemy, ev, 0x200);
            memcpy(ev, tmp, 0x200);
            re4_formats_memory_swapped(enemy, aramAt, 0x200);
            CHECK(re4_formats_locate(enemy + 0x120, &src) && strcmp(src.file, "evd/r100s00.evd") == 0 &&
                  src.entry == -2 && src.innerOffset == 0x120);
            memcpy(tmp, enemy, 0x200);  // and back after the event
            memcpy(enemy, ev, 0x200);
            memcpy(ev, tmp, 0x200);
            re4_formats_memory_swapped(enemy, aramAt, 0x200);
            CHECK(re4_formats_locate(enemy + 0x40, &src) && strcmp(src.file, "em/em12.drs") == 0 && src.entry == 0);
        }
        // motion curves (FCV): a joint the game reads is converted, an unused
        // joint whose offset points into the header is left alone (weapon
        // motions have such joints) and does not stop the conversion
        unsigned char* fa = (unsigned char*) (unsigned long) 0x82109000u;
        memset(fa, 0, 0x60);
        put32be(fa + 0x00, 1);
        put32be(fa + 0x10, 0x20);
        memcpy(fa + 0x14, "FCV\0", 4);
        unsigned char* m = fa + 0x20;
        m[0] = 0; m[1] = 10;                 // maxFrame
        m[2] = 2;                            // joints
        m[3] = 0xF0; m[4] = 0x02;            // joint 0: key layout 15 (4 bytes), rotation
        m[5] = 0xF0; m[6] = 0x00;            // joint 1: kind 0, never read
        put32be(m + 12, 48);                 // size; offsets follow
        put32be(m + 16, 24);
        put32be(m + 20, 4);                  // points into the header
        for (int axis = 0; axis < 3; axis++) {
            unsigned char* a = m + 24 + axis * 8;
            a[1] = 1;                        // one key
            a[3] = (unsigned char) (5 + axis);  // at frame 5 + axis
            a[4] = 0x3F; a[5] = 0x80;        // key bytes stay big-endian
        }
        Header hf = { 0, 0x50, 0, 0x400, 0, 0, 0, 0 };
        re4_formats_part_loaded("em/fcv.drs", 0, 0, &hf, 0x82109000u);
        unsigned short v;
        memcpy(&v, m + 0, 2);
        CHECK(v == 10);
        memcpy(&v, m + 3, 2);
        CHECK(v == 0xF002);  // the kind words once in host order (joint 1 did not swap them back)
        memcpy(&v, m + 5, 2);
        CHECK(v == 0xF000);
        for (int axis = 0; axis < 3; axis++) {
            memcpy(&v, m + 24 + axis * 8, 2);
            CHECK(v == 1);
            memcpy(&v, m + 26 + axis * 8, 2);
            CHECK(v == 5 + axis);
            CHECK(m[28 + axis * 8] == 0x3F && m[29 + axis * 8] == 0x80);
        }
    }

    re4probe_core_setup(arc + 0x1000, arc + 0x20, arc + 0x40);
    re4_bridge_capture(&gSnap);
    re4probe_core_restore();
    CHECK(gSnap.apiVersion == RE4_CORE_API_VERSION);
    CHECK(gSnap.room == 0x100 && gSnap.rno[0] == 3);
    CHECK(gSnap.cameraValid && near(gSnap.cameraEye[2], 3.0f) && near(gSnap.cameraTarget[0], 4.0f) &&
          near(gSnap.cameraRoll, 0.25f) && near(gSnap.cameraFovy, 50.0f));
    CHECK(gSnap.nModels == 1 && gSnap.nParts == 2 && gSnap.droppedModels == 0);
    const Re4CoreModel& m = gSnap.models[0];
    CHECK(m.address == 0x82001000u && m.manager == 0 && m.id == 0x12 && m.type == 3 && (m.beFlag & 6) == 6);
    CHECK(m.firstMatrix == 0 && m.nParts == 2 && near(m.matrix[3], 100.0f) && near(m.matrix[11], -50.0f));
    for (int i = 0; i < 2; i++) {
        const Re4CorePart& p = gSnap.parts[i];
        CHECK(near(p.matrix[0], 1.0f) && near(p.matrix[7], 10.0f * (i + 1)));
        CHECK(near(p.bind[0], 0.0f) && near(p.bind[1], 10.0f * (i + 1)) && near(p.bind[2], 0.0f));
    }
    // all nine infos (Leon has nine; the hair is the sixth)
    CHECK(m.firstInfo == 0 && m.nInfos == 9 && gSnap.nInfos == 9 && gSnap.droppedInfos == 0);
    const Re4CoreModelInfo* mi = gSnap.infos + m.firstInfo;
    CHECK(mi[0].binAddress == 0x82000020u && mi[0].tplAddress == 0x82000040u);
    // how the game draws the infos: the first skinned, the second rigid on parts 1
    CHECK(mi[0].flags == 8 && mi[0].rigidPart == -1 && near(mi[0].matrix[0], 1.0f));
    CHECK(mi[1].flags == 8 && mi[1].rigidPart == 1 && near(mi[1].matrix[7], 5.0f));
    CHECK(strcmp(mi[0].bin.file, "em/em99.drs") == 0 && mi[0].bin.entry == 0 &&
          mi[0].tpl.entry == 1 && mi[0].tpl.fileOffset == 0x400);
    CHECK(mi[8].flags == 0x48 && mi[8].rigidPart == -1 && mi[8].binAddress == 0x82000020u &&
          strcmp(mi[8].tpl.file, "em/em99.drs") == 0);
    report("bridge", ok);
}

struct PadStatus {  // PADStatus
    unsigned short button;
    signed char stickX, stickY, substickX, substickY;
    unsigned char triggerLeft, triggerRight, analogA, analogB;
    signed char err;
};
extern "C" unsigned PADRead(PadStatus* status);

PadStatus gSeen[8];
volatile int gEntryFrames;

int entry()
{
    PadStatus pads[4];
    for (int i = 0; i < 6; i++) {
        PADRead(pads);
        gSeen[i] = pads[0];
        gEntryFrames = i + 1;
        usleep(2000);
    }
    return 7;
}

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void testThread()
{
    bool ok = true;
    CHECK(re4_core_state() == RE4_CORE_IDLE);
    CHECK(re4_core_acquire() == 0);  // nothing published yet
    CHECK(re4_core_reserve("/nonexistent-re4-data", ""));
    Re4CorePad pad = { 0x0100, 50, -20, 0, 0, 0, 200 };
    re4_core_pad(&pad);
    CHECK(re4_core_start_entry(entry));
    double end = now() + 10.0;
    while (re4_core_state() == RE4_CORE_RUNNING && now() < end) {
        usleep(1000);
    }
    CHECK(re4_core_state() == RE4_CORE_ENDED);
    CHECK(strcmp(re4_core_message(), "re4_game_main returned 7") == 0);
    CHECK(gEntryFrames == 6);
    for (int i = 0; i < 6; i++) {
        CHECK(gSeen[i].button == 0x0100 && gSeen[i].stickX == 50 && gSeen[i].stickY == -20 &&
              gSeen[i].triggerRight == 200 && gSeen[i].analogA == 0xFF && gSeen[i].err == 0);
    }
    const Re4CoreSnapshot* s = re4_core_acquire();
    CHECK(s != 0);
    if (s) {
        CHECK(s->apiVersion == RE4_CORE_API_VERSION && s->serial == 6 && s->frame == 5);
        CHECK(re4_core_acquire() == 0);  // one reader at a time
        re4_core_release(s);
        const Re4CoreSnapshot* again = re4_core_acquire();
        CHECK(again == s);
        re4_core_release(again);
    }
    static char tail[4096];
    re4_core_log_tail(tail, sizeof(tail));
    CHECK(strstr(tail, "core: running") != 0 && strstr(tail, "returned 7") != 0);
    report("thread", ok);
}

}  // namespace

int re4_core_tests()
{
    gFailures = 0;
    testBridge();
    testThread();
    re4_log("core-test: %d failed", gFailures);
    return gFailures;
}
