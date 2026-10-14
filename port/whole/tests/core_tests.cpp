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
    CHECK(!re4_formats_locate(arc + 0x44, &src) && src.entry == -1 && src.file[0] == 0);

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
    CHECK(m.nInfos == 1 && m.infos[0].binAddress == 0x82000020u && m.infos[0].tplAddress == 0x82000040u);
    CHECK(strcmp(m.infos[0].bin.file, "em/em99.drs") == 0 && m.infos[0].bin.entry == 0 &&
          m.infos[0].tpl.entry == 1 && m.infos[0].tpl.fileOffset == 0x400);
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
