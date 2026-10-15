// re4_host --runtime-test: checks of the platform runtime that need no disc
// data. Each test prints one line; the return value counts failures.
//
//   link   OSLink/OSUnlink with a synthetic big-endian REL header and the test
//          module re4test_rel (tests/rel_module.cpp)
//   dvd    the DVD API over a synthetic two-disc folder written to a temp dir
//   yz2    the game's yz2 set-up (yz2code.cpp) plus yz2Decode_Decode on the
//          synthetic fixtures of port/tests/yz2_fixtures.h
//   dvdq   the game's own DVD queue (dvd.cpp) reading a synthetic headered file:
//          big-endian part table (patch 0013), parts into the game heap
//   gx     the SDK's GX library feeds the port's GX FIFO: register encodings,
//          vertex sizes, display lists, PE interrupts (tests/gx_probe.cpp)
//   stream the sound driver's stream code (snd_str*.cpp) plays two synthetic
//          stream files with its refill in the audio frame (an interrupt):
//          DVD reads and ARAM DMAs started there complete after the handler
//   math   PS*/C_* matrix and vector functions (SDK C versions); the C
//          versions of the game's Gekko asm (patches 0020-0022: fabsf, SQRTF,
//          SINF, COSF, LIMIT_ANGLE bit-exact against an exact oracle,
//          quantised loads/stores, the CalcSk1 skinning loops with GQR6);
//          EmRoutineSetW (patch 0023), the big-endian views and the length
//          of __FILE__ (tests/game_probe.cpp, compiled as game code)
//   trace  the per-frame state trace (RE4_TRACE, runtime/state_trace.cpp)
//          through PADRead, and the coverage log of GetDataExt (patch 0025)
//   tasks  the game's own task scheduler (scheduler.cpp: TaskExec, TaskSleep,
//          TaskExit, iTaskExec) on the OS thread emulation, plus VI retraces
#include "re4_gx_frame.h"
#include "re4_runtime.h"
#include "re4_ppc.h"
#include "re4/port/sha256.h"
#include "yz2_fixtures.h"

#include <dlfcn.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

namespace {

int gFailures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            re4_log("  check failed: %s (%s:%d)", #cond, __FILE__, __LINE__); \
            ok = false;                                                   \
        }                                                                 \
    } while (0)

void report(const char* name, bool ok)
{
    re4_log("runtime-test %s: %s", name, ok ? "ok" : "FAILED");
    if (!ok) {
        gFailures++;
    }
}

void putBe32(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char) (v >> 24);
    p[1] = (unsigned char) (v >> 16);
    p[2] = (unsigned char) (v >> 8);
    p[3] = (unsigned char) v;
}

}  // namespace

// --- OSLink -------------------------------------------------------------------

struct TestModuleHeader {  // OSModuleHeader (OSModule.h), host view after OSLink
    unsigned id, next, prev, numSections, sectionInfoOffset, nameOffset, nameSize, version;
    unsigned bssSize, relOffset, impOffset, impSize;
    unsigned char prologSection, epilogSection, unresolvedSection, bssSection;
    unsigned prolog, epilog, unresolved, align, bssAlign, fixSize;
};

extern "C" int OSLink(void* info, void* bss);
extern "C" int OSUnlink(void* info);

static void writeDiscRel(unsigned char* p, unsigned id)
{
    memset(p, 0, sizeof(TestModuleHeader));
    putBe32(p + 0x00, id);
    putBe32(p + 0x0C, 4);       // numSections
    putBe32(p + 0x10, 0x4C);    // sectionInfoOffset
    putBe32(p + 0x1C, 3);       // version
    putBe32(p + 0x20, 0x40);    // bssSize
    p[0x30] = 1;                // prologSection
    p[0x31] = 1;                // epilogSection
    putBe32(p + 0x34, 0x10);    // prolog: offset in its section, as on disc
    putBe32(p + 0x38, 0x20);    // epilog
    putBe32(p + 0x3C, 0x30);    // unresolved
    putBe32(p + 0x40, 0x20);    // align
}

static void testLink()
{
    bool ok = true;
    const unsigned kId = 0xF001;
    re4_module_register(kId, "re4test_rel");
    CHECK(re4_mem1_init());
    // Two REL images in MEM1, as the game reads them from disc.
    unsigned char* rel = (unsigned char*) (unsigned long) (RE4_MEM1_BASE + 0x01000000);
    unsigned char* rel2 = rel + 0x1000;
    unsigned char* bss = rel + 0x2000;
    TestModuleHeader* h = (TestModuleHeader*) rel;
    writeDiscRel(rel, kId);
    memset(bss, 0xAA, 0x40);

    CHECK(re4_rel_header_to_host(rel) == 1);
    CHECK(h->id == kId && h->version == 3 && h->bssSize == 0x40 && h->prologSection == 1);
    CHECK(re4_rel_header_to_host(rel) == 0);  // idempotent

    CHECK(OSLink(rel, bss) == 1);
    void* handle = re4_module_load("re4test_rel");
    CHECK(handle != 0);
    if (!handle) {
        report("link", false);
        return;
    }
    int (*state)() = (int (*)()) dlsym(handle, "re4test_rel_state");
    void (*bump)() = (void (*)()) dlsym(handle, "re4test_rel_bump");
    void (*setCounter)(int*) = (void (*)(int*)) dlsym(handle, "re4test_rel_set_dtor_counter");
    CHECK(state && bump && setCounter);
    CHECK(h->prolog == (unsigned) (unsigned long) dlsym(handle, "_prolog"));
    CHECK(h->epilog == (unsigned) (unsigned long) dlsym(handle, "_epilog"));
    CHECK(h->unresolved == (unsigned) (unsigned long) dlsym(handle, "_unresolved"));
    CHECK(bss[0] == 0 && bss[0x3F] == 0);
    CHECK(*(unsigned*) (unsigned long) (RE4_MEM1_BASE + 0x30C8) == (unsigned) (unsigned long) rel);

    int dtors = 0;
    setCounter(&dtors);
    ((void (*)()) (unsigned long) h->prolog)();  // DLL_PROLOG
    bump();
    CHECK(state() == 108);

    // Pause and resume (cRoomData::stopRelData / restartRelData): same header,
    // state kept; the GameCube epilog ran the static destructors.
    ((void (*)()) (unsigned long) h->epilog)();
    CHECK(OSUnlink(rel) == 1);
    CHECK(dtors == 1);
    CHECK(*(unsigned*) (unsigned long) (RE4_MEM1_BASE + 0x30C8) == 0);
    CHECK(OSUnlink(rel) == 0);  // not linked any more
    CHECK(OSLink(rel, bss) == 1);
    CHECK(re4_module_load("re4test_rel") == handle);
    CHECK(state() == 108);
    CHECK(OSUnlink(rel) == 1);

    // The REL read from disc again: the library starts from its initial data,
    // and the destructors that already ran do not run again on unload.
    writeDiscRel(rel2, kId);
    CHECK(OSLink(rel2, 0) == 1);
    handle = re4_module_load("re4test_rel");
    state = (int (*)()) dlsym(handle, "re4test_rel_state");
    CHECK(state && state() == 7);
    CHECK(dtors == 1);
    CHECK(OSUnlink(rel2) == 1);

    // Not a module of this game / not a REL header.
    writeDiscRel(rel2, 0xBEEF);
    CHECK(OSLink(rel2, 0) == 0);
    memset(rel2, 0x5A, 0x4C);
    CHECK(OSLink(rel2, 0) == 0);
    report("link", ok);
}

// --- DVD ----------------------------------------------------------------------

struct TestFileInfo {  // DVDFileInfo (dvd.h)
    unsigned cb[12];
    unsigned startAddr;
    unsigned length;
    void* callback;
};
struct TestDiskID {
    char gameName[4];
    char company[2];
    unsigned char diskNumber, gameVersion, streaming, streamingBufSize, padding[22];
};
struct TestDir {
    unsigned entryNum, location, next;
};
struct TestDirEntry {
    unsigned entryNum;
    int isDir;
    char* name;
};
struct TestCommandBlock {
    unsigned words[12];
};

extern "C" {
int DVDConvertPathToEntrynum(const char* path);
int DVDFastOpen(int entrynum, TestFileInfo* fi);
int DVDOpen(const char* path, TestFileInfo* fi);
int DVDReadAsyncPrio(TestFileInfo* fi, void* addr, int length, int offset,
                     void (*cb)(int, TestFileInfo*), int prio);
void* DVDGetFSTLocation(void);
TestDiskID* DVDGetCurrentDiskID(void);
int DVDOpenDir(const char* path, TestDir* dir);
int DVDReadDir(TestDir* dir, TestDirEntry* entry);
int DVDChangeDiskAsync(TestCommandBlock* block, TestDiskID* id, void (*cb)(int, TestCommandBlock*));
int DVDGetCommandBlockStatus(const void* block);
int DVDCancelAsync(void* block, void (*cb)(int, TestCommandBlock*));
}

namespace {

struct FstEntry {
    bool dir;
    std::string name;
    unsigned a, b;  // file: data, size / dir: parent, end
};

// Big-endian FST as on the disc: entries, then the name table.
std::vector<unsigned char> buildFst(const std::vector<FstEntry>& entries)
{
    std::string names;
    std::vector<unsigned char> out((entries.size() + 1) * 12);
    out[0] = 1;
    putBe32(&out[8], (unsigned) entries.size() + 1);
    for (size_t i = 0; i < entries.size(); i++) {
        unsigned char* e = &out[(i + 1) * 12];
        putBe32(e, (unsigned) names.size());
        e[0] = entries[i].dir ? 1 : 0;
        putBe32(e + 4, entries[i].a);
        putBe32(e + 8, entries[i].b);
        names += entries[i].name;
        names += '\0';
    }
    out.insert(out.end(), names.begin(), names.end());
    return out;
}

bool writeFile(const std::string& path, const void* data, size_t size)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    bool ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

bool makeDirs(const std::string& path)
{
    std::string cmd = "mkdir -p '" + path + "'";
    return system(cmd.c_str()) == 0;
}

int gReadResult = -99;
void onRead(int result, TestFileInfo*)
{
    gReadResult = result;
}
int gChangeResult = -99;
void onChange(int result, TestCommandBlock*)
{
    gChangeResult = result;
}

}  // namespace

static void testDvd()
{
    bool ok = true;
    char tmpl[] = "/tmp/re4-dvd-test-XXXXXX";
    const char* root = mkdtemp(tmpl);
    CHECK(root != 0);
    if (!root) {
        report("dvd", false);
        return;
    }
    std::string base = root;
    // disc 0: /bgm/Bio4Str.hed, /bgm/sub/x.dat, /etc.dat ; disc 1: /disc2.txt
    const char hed[] = "HED0123456789abcdef";  // 19 bytes
    std::vector<FstEntry> fst0 = {
        {true, "bgm", 0, 5},
        {false, "Bio4Str.hed", 0x1000, sizeof(hed) - 1},
        {true, "sub", 1, 5},
        {false, "x.dat", 0x2000, 4},
        {false, "etc.dat", 0x3000, 3},
    };
    std::vector<FstEntry> fst1 = {{false, "disc2.txt", 0x1000, 5}};
    std::vector<unsigned char> f0 = buildFst(fst0), f1 = buildFst(fst1);
    unsigned char boot0[0x40] = "G4BE08", boot1[0x40] = "G4BE08";
    boot1[6] = 1;
    CHECK(makeDirs(base + "/disc0/sys") && makeDirs(base + "/disc0/files/bgm/sub") &&
          makeDirs(base + "/disc1/sys") && makeDirs(base + "/disc1/files"));
    CHECK(writeFile(base + "/disc0/sys/fst.bin", f0.data(), f0.size()));
    CHECK(writeFile(base + "/disc0/sys/boot.bin", boot0, sizeof(boot0)));
    CHECK(writeFile(base + "/disc0/files/bgm/Bio4Str.hed", hed, sizeof(hed) - 1));
    CHECK(writeFile(base + "/disc0/files/bgm/sub/x.dat", "XDAT", 4));
    CHECK(writeFile(base + "/disc0/files/etc.dat", "ETC", 3));
    CHECK(writeFile(base + "/disc1/sys/fst.bin", f1.data(), f1.size()));
    CHECK(writeFile(base + "/disc1/sys/boot.bin", boot1, sizeof(boot1)));
    CHECK(writeFile(base + "/disc1/files/disc2.txt", "disc2", 5));

    char saved[1024];
    snprintf(saved, sizeof(saved), "%s", re4_data_dir());
    re4_set_data_dir(root);

    CHECK(DVDConvertPathToEntrynum("bgm/Bio4Str.hed") == 2);
    CHECK(DVDConvertPathToEntrynum("BGM/BIO4STR.HED") == 2);
    CHECK(DVDConvertPathToEntrynum("/bgm/sub/../../etc.dat") == 5);
    CHECK(DVDConvertPathToEntrynum("bgm/sub/x.dat") == 4);
    CHECK(DVDConvertPathToEntrynum("bgm") == 1);
    CHECK(DVDConvertPathToEntrynum("bgm/missing.dat") == -1);
    CHECK(DVDConvertPathToEntrynum("etc.dat/x") == -1);
    CHECK(re4_dvd_inserted_disc() == 0);
    CHECK(memcmp(DVDGetCurrentDiskID()->gameName, "G4BE", 4) == 0 && DVDGetCurrentDiskID()->diskNumber == 0);
    void* fstAddr = DVDGetFSTLocation();
    CHECK(re4_in_mem1(fstAddr) && memcmp(fstAddr, f0.data(), f0.size()) == 0);

    TestFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    CHECK(DVDFastOpen(2, &fi) == 1 && fi.length == sizeof(hed) - 1 && fi.startAddr == 0x1000);
    CHECK(DVDFastOpen(1, &fi) == 0);  // a directory
    CHECK(DVDOpen("bgm/Bio4Str.hed", &fi) == 1);
    // Reads are 32-byte multiples; the part beyond the file end reads as zero.
    unsigned char buf[64];
    memset(buf, 0xEE, sizeof(buf));
    CHECK(DVDReadAsyncPrio(&fi, buf, 32, 0, onRead, 2) == 1);
    CHECK(gReadResult == 32 && memcmp(buf, hed, sizeof(hed) - 1) == 0 && buf[19] == 0 && buf[31] == 0 &&
          buf[32] == 0xEE);
    CHECK(DVDReadAsyncPrio(&fi, buf, 32, 3, onRead, 2) == 1 && gReadResult == 32 && buf[0] == '0');
    // Started inside an interrupt handler (the sound driver's stream refill in
    // the audio frame), a read completes when the handler has returned: the
    // data is there, the block stays busy and the callback waits.
    gReadResult = -99;
    re4_os_interrupt_begin();
    CHECK(DVDReadAsyncPrio(&fi, buf, 32, 0, onRead, 2) == 1 && gReadResult == -99 &&
          DVDGetCommandBlockStatus(&fi) == 1 && memcmp(buf, hed, 8) == 0);
    re4_os_interrupt_end();
    CHECK(gReadResult == 32 && DVDGetCommandBlockStatus(&fi) == 0);
    // cancelled before the handler returned: the callback gets -3
    gReadResult = -99;
    re4_os_interrupt_begin();
    CHECK(DVDReadAsyncPrio(&fi, buf, 32, 0, onRead, 2) == 1);
    CHECK(DVDCancelAsync(&fi, 0) == 1 && gReadResult == -99);
    re4_os_interrupt_end();
    CHECK(gReadResult == -3 && DVDGetCommandBlockStatus(&fi) == 10);

    TestDir dir;
    TestDirEntry ent;
    CHECK(DVDOpenDir("bgm", &dir) == 1);
    CHECK(DVDReadDir(&dir, &ent) == 1 && ent.entryNum == 2 && !ent.isDir && strcmp(ent.name, "Bio4Str.hed") == 0);
    CHECK(DVDReadDir(&dir, &ent) == 1 && ent.entryNum == 3 && ent.isDir && strcmp(ent.name, "sub") == 0);
    CHECK(DVDReadDir(&dir, &ent) == 0);

    // Disc change to disc 2 (the game's cDvd::DiscChange).
    TestDiskID id;
    memset(&id, 0, sizeof(id));
    memcpy(id.gameName, "G4BE", 4);
    memcpy(id.company, "08", 2);
    id.diskNumber = 1;
    TestCommandBlock block;
    CHECK(DVDChangeDiskAsync(&block, &id, onChange) == 1 && gChangeResult == 0);
    CHECK(re4_dvd_inserted_disc() == 1 && DVDGetCurrentDiskID()->diskNumber == 1);
    CHECK(DVDConvertPathToEntrynum("disc2.txt") == 1 && DVDConvertPathToEntrynum("etc.dat") == -1);
    CHECK(DVDOpen("disc2.txt", &fi) == 1);
    CHECK(DVDReadAsyncPrio(&fi, buf, 32, 0, onRead, 2) == 1 && gReadResult == 32 && memcmp(buf, "disc2", 5) == 0);

    re4_set_data_dir(saved);
    std::string cmd = "rm -rf '" + base + "'";
    CHECK(system(cmd.c_str()) == 0);
    report("dvd", ok);
}

// --- yz2 ----------------------------------------------------------------------

// The game's set-up half (src/game/yz2code.cpp, in libre4_main).
unsigned long Yz2DecodeSet(char* str, void* buf);
void Yz2DecodeExec(void* dst);

static bool decodeFixture(const unsigned char* data, size_t size, size_t outSize, const char* sha)
{
    // The stream starts at the next 32-byte boundary after the header text,
    // counted from the absolute address: the input must be 32-byte aligned.
    std::vector<unsigned char> input(size + 64), work(0x142800 + 32), out(outSize + 1, 0xCD);
    unsigned char* in = (unsigned char*) (((unsigned long) input.data() + 31) & ~31ul);
    unsigned char* w = (unsigned char*) (((unsigned long) work.data() + 31) & ~31ul);
    memcpy(in, data, size);
    if (Yz2DecodeSet((char*) in, w) != outSize) {
        return false;
    }
    Yz2DecodeExec(out.data());
    return re4::port::sha256(out.data(), outSize) == sha && out[outSize] == 0xCD;
}

static void testYz2()
{
    namespace f = re4::port::fixtures;
    bool ok = true;
    // Output digests from port/tests/yz2_fixtures.json (tools/motion/yz2.py).
    CHECK(decodeFixture(f::literals, sizeof(f::literals), f::literalsOutputBytes,
                        "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880"));
    CHECK(decodeFixture(f::matches, sizeof(f::matches), f::matchesOutputBytes,
                        "06f469c97c14e84c74853bb96aa79305eb4f6635291bf1202c4fdadb82706204"));
    CHECK(decodeFixture(f::adaptive, sizeof(f::adaptive), f::adaptiveOutputBytes,
                        "9b4b7f1dbab3c2b32e8761be77968d129e9859f5b38c08c524e4850bf2ee228b"));
    report("yz2", ok);
}

// --- task scheduler and VI ----------------------------------------------------

// The game's scheduler (src/game/scheduler.cpp) and heap set-up, in libre4_main.
struct TaskFunc {  // port/whole/include/re4_task_func.h: one pointer
    void* p;
};
struct TASK;
void SystemMemInit();
void TaskSchedulerInit();
TASK* TaskExec(int level, TaskFunc func, void* arg);
TASK* iTaskExec(TaskFunc func);
void TaskScheduler();
void iTaskScheduler();
void TaskSleep(int frames);
void TaskExit();
void iTaskExit();
unsigned char TaskStatus(int level);

extern "C" {
void OSInit(void);
typedef void (*VIRetraceCallback)(unsigned);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);
void VIWaitForRetrace(void);
unsigned VIGetRetraceCount(void);
int re4probe_valid_ptr(int onThread);  // tests/game_probe.cpp
}

namespace {

char gTrace[128];
int gTraceLen;
unsigned gLastRetrace;
int gRetraceCalls;

void trace(char c)
{
    if (gTraceLen < (int) sizeof(gTrace) - 1) {
        gTrace[gTraceLen++] = c;
    }
}

void taskA(int)
{
    for (;;) {
        trace('A');
        TaskSleep(1);
    }
}

void taskB(int)
{
    for (int i = 0; i < 3; i++) {
        trace('B');
        TaskSleep(2);
    }
    TaskExit();
}

void taskIsr(int)
{
    trace('C');
    iTaskExit();
}

void onRetrace(unsigned count)
{
    gLastRetrace = count;
    gRetraceCalls++;
}

int gValidPtrOnThread = -1;

int taskTestMain()
{
    gValidPtrOnThread = re4probe_valid_ptr(1);
    OSInit();
    SystemMemInit();
    TaskSchedulerInit();
    TaskExec(1, TaskFunc{(void*) taskA}, 0);
    TaskExec(3, TaskFunc{(void*) taskB}, 0);
    iTaskExec(TaskFunc{(void*) taskIsr});
    for (int frame = 0; frame < 8; frame++) {
        trace('|');
        TaskScheduler();
        iTaskScheduler();
    }
    // the video clock may have run since an earlier test (stream: SceSleep
    // outside a scenario task waits for retraces): catch up first, so the
    // three waits below see one retrace each
    VIWaitForRetrace();
    VISetPostRetraceCallback(onRetrace);
    unsigned before = VIGetRetraceCount();
    for (int i = 0; i < 3; i++) {
        VIWaitForRetrace();
    }
    return (int) (VIGetRetraceCount() - before);
}

}  // namespace

static void testTasks()
{
    bool ok = true;
    int retraces = re4_os_run(taskTestMain);
    // Task A sleeps one frame, B two frames and exits after its third turn,
    // the background (ISR) task runs once, in the frame it was started.
    CHECK(strcmp(gTrace, "|ABC|A|AB|A|AB|A|A|A") == 0);
    if (strcmp(gTrace, "|ABC|A|AB|A|AB|A|A|A") != 0) {
        re4_log("  trace: %s", gTrace);
    }
    CHECK(TaskStatus(3) == 0 && TaskStatus(1) != 0);
    // VALID_PTR (patch 0037) on an OS thread's stack and outside one
    CHECK(gValidPtrOnThread == 0);
    CHECK(re4probe_valid_ptr(0) == 0);
    if (gValidPtrOnThread != 0) {
        re4_log("  VALID_PTR failures: 0x%x", gValidPtrOnThread);
    }
    CHECK(retraces == 3 && gRetraceCalls == 3 && gLastRetrace == VIGetRetraceCount());
    // hang search: every OS thread with the place it waits; task A sleeps in
    // TaskSleep (recorded when RE4_WAIT_TRACE=1, as whole_build.sh runs it)
    char path[] = "/tmp/re4-threads-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd >= 0) {
        re4_os_dump_threads(fd);
        lseek(fd, 0, SEEK_SET);
        static char dump[32768];
        ssize_t n = read(fd, dump, sizeof(dump) - 1);
        dump[n > 0 ? n : 0] = 0;
        close(fd);
        unlink(path);
        const char* waitTrace = getenv("RE4_WAIT_TRACE");
        CHECK(strstr(dump, "OS thread") != 0);
        CHECK(!(waitTrace && *waitTrace == '1') || strstr(dump, "TaskSleep") != 0);
        if (waitTrace && *waitTrace == '1' && !strstr(dump, "TaskSleep")) {
            re4_log("  thread dump:\n%s", dump);
        }
    }
    report("tasks", ok);
}

// --- the game's DVD queue ------------------------------------------------------

extern "C" void OSInit(void);
void SystemMemInit();

struct DVD_READINFO {  // include/dvd.h; the name is part of the C++ symbols
    unsigned start_addr[2][32];
    unsigned read_size[2][32];
    unsigned mram_readsize;
    unsigned aram_readsize;
};
struct cDvd {  // the members used here (include/dvd.h); the object is the game's
    void Init();
    int ReadCheck(int id, DVD_READINFO* info);
};
extern cDvd Dvd;
int DvdReadN(const char* name, void* dst, int a, int b, int c, int mode, const char* file, int line);

namespace {

std::string gQueueDir;
bool gQueueOk;

int dvdQueueMain()
{
    bool ok = true;
    OSInit();
    SystemMemInit();
    Dvd.Init();
    // mode: 0x8000 part table at the file start, 0x4 destination from the
    // current heap, 0x1 synchronous (as the boot code reads its tables)
    int req = DvdReadN("hdr.dat", 0, 0, 0, 0, 0x8000 | 0x4 | 0x1, __FILE__, __LINE__);
    DVD_READINFO info;
    memset(&info, 0, sizeof(info));
    CHECK(req >= 0 && Dvd.ReadCheck(req, &info) == 1);
    const unsigned char* a = (const unsigned char*) (unsigned long) info.start_addr[0][0];
    const unsigned char* b = (const unsigned char*) (unsigned long) info.start_addr[0][1];
    CHECK(info.read_size[0][0] == 0x40 && info.read_size[0][1] == 0x20 && info.mram_readsize == 0x60);
    CHECK(re4_in_mem1(a) && b == a + 0x40);
    bool data = re4_in_mem1(a);
    for (int i = 0; data && i < 0x40; i++) {
        data = a[i] == (unsigned char) (0xA0 + i);
    }
    for (int i = 0; data && i < 0x20; i++) {
        data = b[i] == (unsigned char) (0x10 + i);
    }
    CHECK(data);
    gQueueOk = ok;
    return 0;
}

}  // namespace

static void testDvdQueue()
{
    bool ok = true;
    char tmpl[] = "/tmp/re4-dvdq-test-XXXXXX";
    const char* root = mkdtemp(tmpl);
    CHECK(root != 0);
    if (!root) {
        report("dvdq", false);
        return;
    }
    gQueueDir = root;
    // hdr.dat: 0x400-byte part table (entry 0 unused, two MRAM parts, end),
    // then the parts; every table field a big-endian word as on the disc.
    std::vector<unsigned char> file(0x460, 0);
    unsigned char* t = &file[0];
    putBe32(t + 0x20 + 0x00, 0);       // part 1: TRANS_MRAM
    putBe32(t + 0x20 + 0x04, 0x40);    // size
    putBe32(t + 0x20 + 0x0C, 0x400);   // offset in the file
    putBe32(t + 0x40 + 0x00, 0);       // part 2: TRANS_MRAM
    putBe32(t + 0x40 + 0x04, 0x20);
    putBe32(t + 0x40 + 0x0C, 0x440);
    putBe32(t + 0x60, 0xFFFFFFFF);     // TRANS_EOF
    for (int i = 0; i < 0x40; i++) {
        file[0x400 + i] = (unsigned char) (0xA0 + i);
    }
    for (int i = 0; i < 0x20; i++) {
        file[0x440 + i] = (unsigned char) (0x10 + i);
    }
    std::vector<FstEntry> fst = {{false, "hdr.dat", 0x1000, (unsigned) file.size()}};
    std::vector<unsigned char> f = buildFst(fst);
    unsigned char boot[0x40] = "G4BE08";
    CHECK(makeDirs(gQueueDir + "/disc0/sys") && makeDirs(gQueueDir + "/disc0/files"));
    CHECK(writeFile(gQueueDir + "/disc0/sys/fst.bin", f.data(), f.size()));
    CHECK(writeFile(gQueueDir + "/disc0/sys/boot.bin", boot, sizeof(boot)));
    CHECK(writeFile(gQueueDir + "/disc0/files/hdr.dat", file.data(), file.size()));
    char saved[1024];
    snprintf(saved, sizeof(saved), "%s", re4_data_dir());
    re4_set_data_dir(root);
    re4_os_run(dvdQueueMain);
    re4_set_data_dir(saved);
    std::string cmd = "rm -rf '" + gQueueDir + "'";
    CHECK(system(cmd.c_str()) == 0);
    report("dvdq", ok && gQueueOk);
}

// --- the sound driver's streams -------------------------------------------------

extern "C" int re4probe_stream_test(const char* name, int blocks, int mode, int* readyField, int* endField);

// The game's stream code (snd_str*.cpp, tests/stream_probe.cpp) plays two
// files of a synthetic disc to their end: one that fits the ARAM ring
// (2 blocks) and one that wraps it (11 blocks); then the long one is stopped
// while playing and while buffering. Before the interrupt
// rule of dvd.cpp/aram.cpp the first read left the stream busy for good and
// it never got ready (the radio call of r100 waited in SndStrPlayBlock).
static void testStream()
{
    bool ok = true;
    char tmpl[] = "/tmp/re4-stream-test-XXXXXX";
    const char* root = mkdtemp(tmpl);
    CHECK(root != 0);
    if (!root) {
        report("stream", false);
        return;
    }
    std::string base = root;
    const unsigned kShort = 2, kLong = 11;
    std::vector<unsigned char> data(kLong * 0x8000);
    for (unsigned i = 0; i < data.size(); i++) {
        data[i] = (unsigned char) (i * 7 + (i >> 15) * 13 + 3);
    }
    std::vector<FstEntry> fst = {{false, "short.str", 0x10000, kShort * 0x8000},
                                 {false, "long.str", 0x100000, kLong * 0x8000}};
    std::vector<unsigned char> f = buildFst(fst);
    unsigned char boot[0x40] = "G4BE08";
    CHECK(makeDirs(base + "/disc0/sys") && makeDirs(base + "/disc0/files"));
    CHECK(writeFile(base + "/disc0/sys/fst.bin", f.data(), f.size()));
    CHECK(writeFile(base + "/disc0/sys/boot.bin", boot, sizeof(boot)));
    CHECK(writeFile(base + "/disc0/files/short.str", data.data(), kShort * 0x8000));
    CHECK(writeFile(base + "/disc0/files/long.str", data.data(), data.size()));
    char saved[1024];
    snprintf(saved, sizeof(saved), "%s", re4_data_dir());
    re4_set_data_dir(root);
    struct {
        const char* file;
        unsigned blocks;
        int mode;
        const char* what;
    } runs[] = {{"short.str", kShort, 0, "short stream"},
                {"long.str", kLong, 0, "long stream"},
                {"long.str", kLong, 1, "stopped while playing"},
                {"long.str", kLong, 2, "stopped while buffering"},
                {"long.str", kLong, 3, "stopped and waited for like the radio screen"}};
    for (const auto& r : runs) {
        int ready = -1, end = -1;
        int failed = re4probe_stream_test(r.file, (int) r.blocks, r.mode, &ready, &end);
        re4_log("  %s: checks 0x%x, ready after %d fields, gone after %d", r.what, failed, ready, end);
        CHECK(failed == 0);
    }
    re4_set_data_dir(saved);
    std::string cmd = "rm -rf '" + base + "'";
    CHECK(system(cmd.c_str()) == 0);
    report("stream", ok);
}

// --- GX: the FIFO of the emulated graphics processor -------------------------------

extern "C" int re4probe_gx_test(void* fifo, void* list);
extern "C" int re4probe_gx_frame_test(void* fifo, void* mem);
extern "C" void re4probe_gx_produce(void* fifo, void* xfb, int frames, int init);

// The SDK's GX library and the game's inline vertex functions feed the port's
// GX FIFO (runtime/gx_fifo.cpp, tests/gx_probe.cpp). The FIFO and the display
// list live in MEM1 as on the GameCube (the GP addresses memory physically).
static void testGx()
{
    bool ok = true;
    void* fifo = (void*) (unsigned long) 0x82800000u;
    void* list = (void*) (unsigned long) 0x82810000u;
    int failed = re4probe_gx_test(fifo, list);
    if (failed) {
        re4_log("  gx checks failed: 0x%x", failed);
    }
    CHECK(failed == 0);
    // G2: the decoder turns a synthetic frame into the expected chunk
    failed = re4probe_gx_frame_test(fifo, (void*) (unsigned long) 0x82820000u);
    if (failed) {
        re4_log("  gx frame checks failed: 0x%x", failed);
    }
    CHECK(failed == 0);
    // the queue between the game thread and a renderer thread: 60 frames
    // arrive in order (the first starts over), none dropped; a renderer that
    // stops taking chunks does not hold the game up for long (the queue is
    // dropped after a second), and the next chunk it takes starts over
    {
        void* xfb = (void*) (unsigned long) 0x82830000u;
        std::atomic<bool> stop(false);
        std::atomic<int> received(0), outOfOrder(0), resets(0);
        re4_core_gx_acquire();  // attach
        re4probe_gx_produce(fifo, xfb, 1, 1);  // the boundary where recording starts
        std::thread renderer([&] {
            unsigned last = 0;
            while (!stop.load() && received.load() < 60) {
                const Re4GxChunk* c = re4_core_gx_acquire();
                if (!c) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                if (last && c->sequence != last + 1) {
                    outOfOrder++;
                }
                if (c->flags & RE4_GX_CHUNK_RESET) {
                    resets++;
                }
                last = c->sequence;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));  // a slow renderer
                re4_core_gx_release(c);
                received++;
            }
        });
        re4probe_gx_produce(fifo, xfb, 60, 0);
        renderer.join();
        CHECK(received.load() == 60 && outOfOrder.load() == 0 && resets.load() == 1);
        // nobody takes chunks now: the producer must not wait for ever
        auto t0 = std::chrono::steady_clock::now();
        re4probe_gx_produce(fifo, xfb, 12, 0);
        double waited = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        CHECK(waited < 3.0);
        const Re4GxChunk* first = 0;
        for (int i = 0; i < 4 && !first; i++) {  // attach again; the next chunk starts over
            first = re4_core_gx_acquire();
            if (!first) {
                re4probe_gx_produce(fifo, xfb, 1, 0);
            }
        }
        CHECK(first && (first->flags & RE4_GX_CHUNK_RESET));
        if (first) {
            re4_core_gx_release(first);
        }
        re4_core_gx_detach();
        if (received.load() != 60 || outOfOrder.load() || resets.load() != 1 || waited >= 3.0) {
            re4_log("  gx queue: %d received, %d out of order, %d resets, %.2f s waited", received.load(),
                    outOfOrder.load(), resets.load(), waited);
        }
    }
    report("gx", ok);
}

// --- matrix math ----------------------------------------------------------------

typedef float TMtx[3][4];
struct TVec {
    float x, y, z;
};
extern "C" {
void PSMTXIdentity(TMtx m);
void PSMTXRotRad(TMtx m, char axis, float rad);
void PSMTXTrans(TMtx m, float x, float y, float z);
void PSMTXConcat(const TMtx a, const TMtx b, TMtx ab);
unsigned long PSMTXInverse(const TMtx src, TMtx inv);
void PSMTXMultVec(const TMtx m, const TVec* src, TVec* dst);
void PSMTXReorder(const TMtx src, float dest[4][3]);
void PSMTXROMultVecArray(const float m[4][3], const TVec* src, TVec* dst, unsigned long count);
void PSVECNormalize(const TVec* src, TVec* dst);
float PSVECDotProduct(const TVec* a, const TVec* b);
void PSVECCrossProduct(const TVec* a, const TVec* b, TVec* axb);
}

static bool near(float a, float b)
{
    return a - b < 1e-5f && b - a < 1e-5f;
}

// --- C versions of the Gekko asm (patches 0020-0024) -------------------------

float SQRTF(float x);
float SINF(float x);
float COSF(float x);
float LIMIT_ANGLE(float x);
void CalcSk1_x(void* dst, void* src, unsigned long n);
void CalcSk1_x2(void* dst, void* src, unsigned long n);
void setupGQR6(unsigned long v);
extern "C" {
float re4probe_fabsf(float x);
void re4probe_routine(unsigned char out[4]);
int re4probe_byte_views(void);
const char* re4probe_file(void);
}

static unsigned bitsOf(float f)
{
    unsigned u;
    memcpy(&u, &f, sizeof u);
    return u;
}

static float floatOf(unsigned u)
{
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

// x, SINF(x), COSF(x), LIMIT_ANGLE(x) as f32 bits. Computed by an exact
// oracle (Python fractions: every ps_muls/fsubs/fadds rounded once to single,
// every ps_madd fused, as on Gekko), independent of the C code under test.
static const unsigned kTrig[][4] = {
    {0x00000000, 0x00000000, 0x3F80011F, 0x00000000},
    {0x3F000000, 0x3EF57765, 0x3F60B4FF, 0x3F000000},
    {0xBF000000, 0xBEF57747, 0x3F60A971, 0xBF000000},
    {0x3F800000, 0x3F576AC6, 0x3F0A772E, 0x3F800000},
    {0xBF800000, 0xBF576AAA, 0x3F0A5153, 0xBF800000},
    {0x3FC90FDB, 0x3F80011F, 0xB89D63A0, 0x3FC90FDB},
    {0xBFC90FDB, 0xBF800009, 0x00000000, 0xBFC90FDB},
    {0x40000000, 0x3F68D169, 0xBED510F2, 0x40000000},
    {0x40400000, 0x3E123DCD, 0xBF7D7049, 0x40400000},
    {0x40490FDB, 0xB89D63A0, 0xBF800009, 0xC0490FDB},
    {0xC0490FDB, 0xB89D63A0, 0xBF800009, 0xC0490FDB},
    {0x404CCCCD, 0xBD6F41D6, 0xBF7F904F, 0xC04552E9},
    {0xC04CCCCD, 0x3D77F588, 0xBF7F9058, 0x404552E9},
    {0x40800000, 0xBF41BE73, 0xBF275532, 0xC0121FB6},
    {0x40C90FDB, 0x00000000, 0x3F80011F, 0x00000000},
    {0x40F00000, 0x3F70212A, 0x3EB1F83E, 0x3F9BC094},
    {0xC0F00000, 0xBF7020CA, 0x3EB17A50, 0xBF9BC094},
    {0x41200000, 0xBF0B4548, 0xBF56CD6F, 0xC0243F6C},
    {0xC1440000, 0x3E9F4B29, 0x3F735275, 0x3EA1FB60},
    {0x42C80000, 0xBF01A1B9, 0x3F5CC0BA, 0xBF07EDF0},
    {0x3A83126F, 0x3A831278, 0x3F80011C, 0x3A83126F},
    {0xC0300000, 0xBEC36897, 0xBF6C9F1C, 0xC0300000},
    {0x3AA3D70A, 0x3AA3D714, 0x3F800119, 0x3AA3D70A},
    {0x41F80000, 0xBECEDE51, 0x3F6A2CCD, 0xBED4F4A0},
};

static bool testGekkoMath()
{
    bool ok = true;
    // fabsf (math_sub.h inline, compiled as game code): sign bit only
    CHECK(bitsOf(re4probe_fabsf(-2.5f)) == bitsOf(2.5f) && bitsOf(re4probe_fabsf(-0.0f)) == 0);
    CHECK(re4probe_fabsf(3.0f) == 3.0f);
    // SQRTF: 0 up to 0.00001, then the square root
    CHECK(SQRTF(0.0f) == 0.0f && SQRTF(0.00001f) == 0.0f && SQRTF(-4.0f) == 0.0f);
    CHECK(SQRTF(16.0f) == 4.0f && SQRTF(2.0f) == sqrtf(2.0f) && SQRTF(12345.678f) == sqrtf(12345.678f));
    int mismatches = 0;
    for (const auto& row : kTrig) {
        float x = floatOf(row[0]);
        unsigned s = bitsOf(SINF(x)), c = bitsOf(COSF(x)), l = bitsOf(LIMIT_ANGLE(x));
        if (s != row[1] || c != row[2] || l != row[3]) {
            if (mismatches++ < 4) {
                re4_log("  trig x=%08x: SINF %08x (want %08x) COSF %08x (want %08x) LIMIT_ANGLE %08x (want %08x)",
                        row[0], s, row[1], c, row[2], l, row[3]);
            }
        }
    }
    CHECK(mismatches == 0);
    // accuracy of the original series (about 0.0026 at most) and the range
    float worst = 0;
    for (int k = -2000; k <= 2000; k++) {
        float x = k * 0.00157f;
        float e1 = fabsf(SINF(x) - sinf(x)), e2 = fabsf(COSF(x) - cosf(x));
        worst = e1 > worst ? e1 : worst;
        worst = e2 > worst ? e2 : worst;
        float w = LIMIT_ANGLE(x * 7.0f);
        CHECK(w >= -3.1415927f && w < 3.1415927f);
    }
    CHECK(worst < 0.003f);
    // a NaN angle comes back (the GameCube loop would never end)
    float nan = floatOf(0x7FC00000);
    CHECK(LIMIT_ANGLE(nan) != LIMIT_ANGLE(nan));
    // quantised loads and stores (GQR2 u8, GQR5 s16)
    unsigned char u8v = 200;
    short s16v = -1234, out = 0;
    CHECK(re4_psq_l_u8(&u8v) == 200.0f && re4_psq_l_s16(&s16v) == -1234.0f);
    re4_psq_st_s16(-4.75f, &out);
    CHECK(out == -4);  // truncation towards zero
    re4_psq_st_s16(40000.0f, &out);
    CHECK(out == 32767);
    re4_psq_st_s16(-40000.0f, &out);
    CHECK(out == -32768);
    re4_psq_st_s16(nan, &out);
    CHECK(out == 0);
    CHECK(re4_gqr_scale(0x32) == -14 && re4_gqr_scale(0x20) == -32 && re4_gqr_scale(5) == 5);
    return ok;
}

// CalcSk1_x / CalcSk1_x2 (trans.cpp, patch 0021): palette ROMtx in the locked
// cache, GQR6 scales, fused multiply-adds, clamp and truncate.
static bool testSkinning()
{
    bool ok = true;
    float* lc = (float*) re4_locked_cache;
    // matrix 0: scale 1.5 and translation (10, -20, 0.5); matrix 1: swap x/y
    TMtx a = {{1.5f, 0, 0, 10}, {0, 1.5f, 0, -20}, {0, 0, 1.5f, 0.5f}};
    TMtx b = {{0, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}};
    PSMTXReorder(a, (float(*)[3]) (lc + 0));
    PSMTXReorder(b, (float(*)[3]) (lc + 12));
    // positions: s16 x/y/z + index, shift 2 (GQR6 = shift << 24 | shift << 8 | 0x70007)
    short src[4 * 4] = {
        4, -8, 3, 0,             // (1, -2, 0.75) * 1.5 + t = (11.5, -23, 1.625) -> *4: 46, -92, 6.5 -> 6
        -3, 100, -7, 0,          // (-0.75, 25, -1.75) -> (8.875, 17.5, -2.125) -> 35.5, 70, -8.5 -> 35, 70, -8
        32000, -32000, 0, 0,     // (8000, -8000) * 1.5 + t -> 48040, -48080 -> clamp
        12, 34, 56, 1,           // matrix 1: (3, 8.5, 14) -> (8.5, 3, 14) -> 34, 12, 56
    };
    short dst[4 * 3 + 1];
    memset(dst, 0x55, sizeof dst);
    unsigned long shift = 2;
    setupGQR6((shift << 24) | (shift << 8) | 0x00070007);
    CalcSk1_x(dst, src, 4);
    const short want[12] = {46, -92, 6, 35, 70, -8, 32767, -32768, 2, 34, 12, 56};
    for (int i = 0; i < 12; i++) {
        if (dst[i] != want[i]) {
            re4_log("  CalcSk1_x[%d] = %d, want %d", i, dst[i], want[i]);
            ok = false;
        }
    }
    CHECK(dst[12] == 0x5555);  // nothing written past the last vertex
    // normals: s8 x/y/z + u8 index, GQR6 0x20062006 (load * 2^32, store * 2^-32:
    // the translation vanishes next to the scaled normal)
    signed char nsrc[2 * 4] = {64, -64, 10, 0, 100, 20, -128, 1};
    signed char ndst[2 * 3];
    setupGQR6(0x20062006);
    CalcSk1_x2(ndst, nsrc, 2);
    // matrix 0: 1.5 * (64, -64, 10) = (96, -96, 15); matrix 1: (20, 100, -128)
    CHECK(ndst[0] == 96 && ndst[1] == -96 && ndst[2] == 15);
    CHECK(ndst[3] == 20 && ndst[4] == 100 && ndst[5] == -128);
    // n == 0 writes nothing (the hardware loop would run 2^32 times)
    ndst[0] = 7;
    CalcSk1_x2(ndst, nsrc, 0);
    CHECK(ndst[0] == 7);
    return ok;
}

// What exists only inside game units (tests/game_probe.cpp).
static bool testGameProbe()
{
    bool ok = true;
    unsigned char r[4];
    re4probe_routine(r);
    CHECK(r[0] == 1 && r[1] == 0x2E && r[2] == 3 && r[3] == 4);  // EmRoutineSetW, patch 0023
    int views = re4probe_byte_views();
    if (views != 0) {
        re4_log("  byte views failed: mask 0x%x", views);
    }
    CHECK(views == 0);
    // __FILE__ fits DVD_REQ_WORK::reqfile (0x30 bytes) whatever the build path
    const char* f = re4probe_file();
    CHECK(strlen(f) < 0x30 && strcmp(f, "port/whole/tests/game_probe.cpp") == 0);
    CHECK(strlen(__FILE__) < 0x30);
    return ok;
}

static void testMath()
{
    bool ok = true;
    TMtx rot, trans, m, inv, id;
    PSMTXRotRad(rot, 'y', 3.14159265f / 2);
    PSMTXTrans(trans, 10, 20, 30);
    PSMTXConcat(trans, rot, m);  // rotate, then translate
    TVec x = {1, 0, 0}, r;
    PSMTXMultVec(m, &x, &r);
    CHECK(near(r.x, 10) && near(r.y, 20) && near(r.z, 29));  // +x turns to -z about y
    CHECK(PSMTXInverse(m, inv) == 1);
    PSMTXConcat(m, inv, id);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            CHECK(near(id[i][j], i == j ? 1.0f : 0.0f));
        }
    }
    float ro[4][3];
    PSMTXReorder(m, ro);
    TVec v[2] = {{1, 2, 3}, {-4, 5, 0.5f}}, a[2], b;
    PSMTXROMultVecArray(ro, v, a, 2);
    for (int i = 0; i < 2; i++) {
        PSMTXMultVec(m, &v[i], &b);
        CHECK(near(a[i].x, b.x) && near(a[i].y, b.y) && near(a[i].z, b.z));
    }
    TVec n, y = {0, 3, 4}, c;
    PSVECNormalize(&y, &n);
    CHECK(near(n.y, 0.6f) && near(n.z, 0.8f));
    TVec e1 = {1, 0, 0}, e2 = {0, 1, 0};
    PSVECCrossProduct(&e1, &e2, &c);
    CHECK(near(c.z, 1) && near(PSVECDotProduct(&e1, &e2), 0));
    ok = testGekkoMath() && ok;
    ok = testSkinning() && ok;
    ok = testGameProbe() && ok;
    report("math", ok);
}

// --- state trace and coverage log ------------------------------------------------

extern "C" int re4_state_trace_open(const char* path);
struct TestPadStatus {  // dolphin/pad.h PADStatus
    unsigned short button;
    signed char stickX, stickY, substickX, substickY;
    unsigned char triggerL, triggerR, analogA, analogB;
    signed char err;
};
extern "C" unsigned long PADRead(TestPadStatus* status);
extern "C" int PADInit(void);
void* GetDataExt(void* arc, const char* tag, int no);

static void testTrace()
{
    bool ok = true;
    char path[] = "/tmp/re4-trace-test-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd >= 0) {
        close(fd);
    }
    CHECK(re4_state_trace_open(path) == 1);
    TestPadStatus pads[4];
    PADRead(pads);
    PADRead(pads);
    re4_state_trace_open(0);
    FILE* f = fopen(path, "r");
    char line[4096] = "";
    int lines = 0;
    while (f && fgets(line, sizeof(line), f)) {
        lines++;
        CHECK(strncmp(line, "{\"frame\":", 9) == 0 && strstr(line, "\"Rno0\":") && strstr(line, "\"room\":\"0x") &&
              strstr(line, "\"em\":[") && line[strlen(line) - 2] == '}');
    }
    if (f) {
        fclose(f);
    }
    CHECK(lines == 2);
    unlink(path);
    // the test script ends on request (run control: RE4_PAD_UNTIL_ROOM)
    setenv("RE4_PAD_SCRIPT", "A@0-100000/1", 1);
    PADInit();
    PADRead(pads);
    CHECK(pads[0].button == 0x0100 && pads[1].err == -1);
    re4_pad_script_stop();
    PADRead(pads);
    CHECK(pads[0].button == 0);
    unsetenv("RE4_PAD_SCRIPT");
    // then the room script, its frames counted from the stop; the main stick
    setenv("RE4_PAD_SCRIPT", "A@0-100000/1", 1);
    setenv("RE4_PAD_ROOM_SCRIPT", "START@2 STICKUP+B@0-10/1", 1);
    re4_pad_script_stop();  // nothing runs: no new script parsed yet
    PADInit();
    PADRead(pads);
    CHECK(pads[0].button == 0x0100 && pads[0].stickY == 0);
    re4_pad_script_stop();
    PADRead(pads);
    CHECK(pads[0].button == 0x0200 && pads[0].stickY == 100 && pads[0].stickX == 0);
    PADRead(pads);
    PADRead(pads);
    CHECK(pads[0].button == (0x1000 | 0x0200) && pads[0].stickY == 100);
    // once the player is free (state_trace.cpp), the free script replaces the
    // rest of the room script, its frames counted from then
    setenv("RE4_PAD_FREE_SCRIPT", "STICKLEFT@1-3/1", 1);
    re4_pad_script_free();
    PADRead(pads);
    CHECK(pads[0].button == 0 && pads[0].stickX == 0 && pads[0].stickY == 0);
    PADRead(pads);
    CHECK(pads[0].button == 0 && pads[0].stickX == -100 && pads[0].stickY == 0);
    unsetenv("RE4_PAD_FREE_SCRIPT");
    unsetenv("RE4_PAD_SCRIPT");
    unsetenv("RE4_PAD_ROOM_SCRIPT");
    re4_pad_script_stop();
    PADRead(pads);
    PADRead(pads);
    PADRead(pads);
    CHECK(pads[0].button == 0 && pads[0].stickY == 0 && pads[0].stickX == 0);
    // GetDataExt on a host-order archive: one entry "CNS", one "SMD"
    unsigned arc[8] = {2, 0, 0, 0, 0x18, 0x1C, 0, 0};
    memcpy(&arc[6], "CNS\0SMD\0", 8);
    CHECK(GetDataExt(arc, "SMD", 0) == (char*) arc + 0x1C && GetDataExt(arc, "LIT", 0) == 0);
    report("trace", ok);
}

int re4_runtime_tests()
{
    gFailures = 0;
    testLink();
    testDvd();
    testYz2();
    testDvdQueue();
    testStream();
    testGx();
    testMath();
    testTrace();
    testTasks();
    re4_log("runtime-test: %d failed", gFailures);
    return gFailures;
}
