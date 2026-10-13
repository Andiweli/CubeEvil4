// re4_host --runtime-test: checks of the platform runtime that need no disc
// data. Each test prints one line; the return value counts failures.
//
//   link   OSLink/OSUnlink with a synthetic big-endian REL header and the test
//          module re4test_rel (tests/rel_module.cpp)
//   dvd    the DVD API over a synthetic two-disc folder written to a temp dir
//   yz2    the game's yz2 set-up (yz2code.cpp) plus yz2Decode_Decode on the
//          synthetic fixtures of port/tests/yz2_fixtures.h
//   math   PS*/C_* matrix and vector functions (SDK C versions)
//   tasks  the game's own task scheduler (scheduler.cpp: TaskExec, TaskSleep,
//          TaskExit, iTaskExec) on the OS thread emulation, plus VI retraces
#include "re4_runtime.h"
#include "re4/port/sha256.h"
#include "yz2_fixtures.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int taskTestMain()
{
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
    CHECK(retraces == 3 && gRetraceCalls == 3 && gLastRetrace == VIGetRetraceCount());
    report("tasks", ok);
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
    report("math", ok);
}

int re4_runtime_tests()
{
    gFailures = 0;
    testLink();
    testDvd();
    testYz2();
    testMath();
    testTasks();
    re4_log("runtime-test: %d failed", gFailures);
    return gFailures;
}
