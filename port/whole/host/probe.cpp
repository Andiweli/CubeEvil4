// The game core without a picture (port/docs/ouya-machbarkeit.md, step O1):
// how fast the core runs on a device before a renderer is built for it.
//
//   re4_probe --data DIR [--seconds N] [--report N] [--no-copy] [--profile FILE]
//
// Loads the core as the core process does (host/coreproc.cpp), with every
// address range below 0x83000000 held first (host/low_memory.h): the game
// libraries then land above 0x80000000 also where the kernel hands out
// addresses from the bottom up (Android 4.1 on the OUYA). libre4_platform.so
// and libre4_main.so come by path from this program's folder, the module
// libraries from <folder>/modules or the folder itself. The game runs as in
// the app; this program stands in for the app's renderer and sound:
//
//   the GX thread     takes every chunk of the core as the renderer does and
//                     copies it as the core process copies it into shared
//                     memory (--no-copy: only taken and given back)
//   the sound thread  reads the sound at 32 kHz in real time, as AudioTrack
//   the main thread   every --report seconds (default 5) one line: game
//                     frames, frame times, CPU time of the game's thread per
//                     frame, its idle time (asleep for the next retrace), the
//                     process's CPU, GX, sound, memory, the processor clocks
//   the sampler       (--profile FILE) sends SIGPROF to the game's thread (and
//                     the GP thread, RE4_GX_THREAD) about 1000 times a second;
//                     the handler notes where it is, or that the thread was
//                     waiting (it hardly ran since the last sample). At the end
//                     each sampled place as "LIBRARY OFFSET COUNT" into FILE
//                     (the GP thread: FILE.gp; port/tools/probe_profile.py
//                     names the functions with the libraries of the build) and
//                     the share of each library in the output
//
// After --seconds (default 180) a summary over the stretches the game spent
// in its main loop ("probe-result: key=value ..." for
// port/scripts/ouya-probe.ps1), then the program ends. The test switches come
// from the environment (RE4_*, as port/tools/whole_r100_check.sh sets them:
// RE4_START_ROOM, RE4_PAD_SCRIPT ...); RE4_VI_FAST=1 runs the game without
// waiting for retraces (how fast it could go); RE4_GX_THREAD=1 puts the GX
// decoder on a thread of its own (runtime/gx_thread.cpp: its CPU time per
// frame as "gp", gp_cpu_ms). RE4_DEBUG_HUD defaults to 0 as
// in the app. On Linux RE4_CORE_TEST_ENTRY=remote runs the entry of the core
// process check (tests/core_remote_probe.cpp) instead of the game: the cloud
// check of this program without disc data.
//
// Exit codes: 0 measured, 2 the core did not load or start, 3 no game frame
// for 60 s, 4 the game ended or failed.
#include "low_memory.h"
#include "re4_core.h"
#include "re4_gx_frame.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <mutex>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <thread>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <vector>
#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace {

const unsigned long kAddrNoRandomize = 0x0040000;  // personality ADDR_NO_RANDOMIZE

long long monotonicNs()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

// --- the core's calls (dlsym, as the core process) -----------------------------

struct Core {
    void* main;
    void* platform;
    void (*log)(const char*, ...);
    int (*mem1Init)(void);
    int (*reserve)(const char*, const char*);
    int (*start)(void);
    int (*startEntry)(int (*)(void));
    int (*state)(void);
    const char* (*message)(void);
    int (*audio)(int16_t*, int);
    void (*audioStats)(unsigned*);
    const Re4CoreSnapshot* (*acquire)(void);
    void (*release)(const Re4CoreSnapshot*);
    int (*logTail)(char*, int);
    const Re4GxChunk* (*gxAcquire)(void);
    void (*gxRelease)(const Re4GxChunk*);
    void (*gxNotify)(void (*)(void));
    void (*addFrameHook)(void (*)(unsigned));
    int (*gameThread)(void);
    long long (*idleNs)(void);
    int (*gxThreaded)(int);   // the GP on a thread of its own (RE4_GX_THREAD, runtime/gx_thread.cpp)
    int (*gxThreadId)(void);
} core;

std::string gMissing;

// Android 4.1's dlsym looks only into the library it is given, not into the
// ones it needs: what libre4_platform.so defines is looked up there too.
template <class F> void bind(F& slot, const char* name, bool needed = true)
{
    void* p = core.main ? dlsym(core.main, name) : nullptr;
    if (!p && core.platform) {
        p = dlsym(core.platform, name);
    }
    slot = reinterpret_cast<F>(p);
    if (!p && needed && gMissing.empty()) {
        gMissing = name;
    }
}

std::string gExeDir;
int (*gTestEntry)(void);

bool loadCore(std::string& problem)
{
#if defined(__ANDROID__)
    // the C++ runtime first: before Android 6.0 the linker finds what a
    // library needs only in LD_LIBRARY_PATH or among the libraries loaded
    dlopen((gExeDir + "/libc++_shared.so").c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    core.platform = dlopen((gExeDir + "/libre4_platform.so").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (core.platform) {
        core.main = dlopen((gExeDir + "/libre4_main.so").c_str(), RTLD_NOW | RTLD_LOCAL);
    }
    if (!core.main) {
        const char* e = dlerror();
        problem = std::string("core libraries missing: ") + (e ? e : "?");
        return false;
    }
    bind(core.log, "re4_log");
    bind(core.mem1Init, "re4_mem1_init");
    bind(core.reserve, "re4_core_reserve");
    bind(core.start, "re4_core_start");
    bind(core.startEntry, "re4_core_start_entry");
    bind(core.state, "re4_core_state");
    bind(core.message, "re4_core_message");
    bind(core.audio, "re4_core_audio");
    bind(core.audioStats, "re4_core_audio_stats");
    bind(core.acquire, "re4_core_acquire");
    bind(core.release, "re4_core_release");
    bind(core.logTail, "re4_core_log_tail");
    bind(core.gxAcquire, "re4_core_gx_acquire");
    bind(core.gxRelease, "re4_core_gx_release");
    bind(core.gxNotify, "re4_core_gx_notify");
    bind(core.addFrameHook, "re4_add_frame_hook");
    bind(core.gameThread, "re4_core_game_thread");
    bind(core.idleNs, "re4_idle_ns", false);
    bind(core.gxThreaded, "re4_gx_threaded", false);
    bind(core.gxThreadId, "re4_gx_thread_id", false);
    if (!gMissing.empty()) {
        problem = "missing in libre4_main.so / libre4_platform.so: " + gMissing;
        return false;
    }
#if !defined(__ANDROID__)
    // the core process check's entry instead of the game (tests/core_remote_probe.cpp)
    if (const char* entry = getenv("RE4_CORE_TEST_ENTRY")) {
        if (*entry) {
            void* host = dlopen((gExeDir + "/libre4_host.so").c_str(), RTLD_NOW | RTLD_LOCAL);
            gTestEntry = host ? (int (*)(void)) dlsym(host, "re4_coreproc_test_entry") : nullptr;
            if (!gTestEntry) {
                problem = "RE4_CORE_TEST_ENTRY: no re4_coreproc_test_entry in libre4_host.so";
                return false;
            }
        }
    }
#endif
    // MEM1 before anything else maps memory there
    if (!core.mem1Init()) {
        problem = "MEM1 0x80000000..0x83000000 is taken";
        return false;
    }
    return true;
}

// --- crashes ----------------------------------------------------------------------

void where(const char* what, uintptr_t address)
{
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (address && dladdr((void*) address, &info) && info.dli_fname) {
        const char* file = strrchr(info.dli_fname, '/');
        fprintf(stderr, "probe:   %s 0x%08lx = %s+0x%lx (%s)\n", what, (unsigned long) address,
                file ? file + 1 : info.dli_fname, (unsigned long) (address - (uintptr_t) info.dli_fbase),
                info.dli_sname ? info.dli_sname : "?");
    } else {
        fprintf(stderr, "probe:   %s 0x%08lx\n", what, (unsigned long) address);
    }
}

void onCrash(int sig, siginfo_t* si, void* context)
{
    fprintf(stderr, "probe: CRASH signal %d, fault address %p (thread %d)\n", sig, si ? si->si_addr : nullptr,
            (int) syscall(SYS_gettid));
    const ucontext_t* uc = (const ucontext_t*) context;
#if defined(__arm__)
    where("pc", uc->uc_mcontext.arm_pc);
    where("lr", uc->uc_mcontext.arm_lr);
#elif defined(__i386__)
    where("pc", (uintptr_t) uc->uc_mcontext.gregs[REG_EIP]);
#else
    (void) uc;
#endif
    if (core.logTail) {
        static char tail[4096];
        core.logTail(tail, sizeof(tail));
        fprintf(stderr, "probe: last lines of the core's log:\n%s", tail);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void installCrashHandler()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = onCrash;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    for (int s : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT}) {
        sigaction(s, &action, nullptr);
    }
}

// --- the game's frames (the frame hook, on the game's thread) -----------------------

const unsigned kRing = 1u << 16;  // frame times kept (36 minutes at 30 frames/s)
float gFrameMs[kRing];
std::atomic<unsigned> gFrames{0};
std::atomic<long long> gLastFrameNs{0};
std::atomic<long long> gGameCpuNs{0};
long long gPreviousFrameNs;

void frameHook(unsigned)
{
    const long long now = monotonicNs();
    timespec cpu;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu);
    const unsigned n = gFrames.load(std::memory_order_relaxed);
    gFrameMs[n % kRing] = gPreviousFrameNs ? (float) ((now - gPreviousFrameNs) / 1e6) : 0.0f;
    gPreviousFrameNs = now;
    gGameCpuNs.store(cpu.tv_sec * 1000000000LL + cpu.tv_nsec, std::memory_order_relaxed);
    gLastFrameNs.store(now, std::memory_order_relaxed);
    gFrames.store(n + 1, std::memory_order_release);
}

// --- the renderer's part: GX chunks taken (and copied) --------------------------------

bool gCopy = true;
std::mutex gBellLock;
std::condition_variable gBell;
bool gRung;

struct GxCount {
    unsigned chunks;
    double bytes, draws, vertices, textureBytes, copyNs;
};
std::mutex gGxLock;
GxCount gGx;

// the core queued a chunk (its thread, its queue locked: only wake the GX thread)
void gxNotify(void)
{
    std::lock_guard<std::mutex> l(gBellLock);
    gRung = true;
    gBell.notify_one();
}

void gxThread()
{
    prctl(PR_SET_NAME, "probe-gx");
    std::vector<char> copy;
    for (;;) {
        {
            std::unique_lock<std::mutex> l(gBellLock);
            gBell.wait_for(l, std::chrono::milliseconds(20), [] { return gRung; });
            gRung = false;
        }
        while (const Re4GxChunk* k = core.gxAcquire()) {
            const size_t bytes = (size_t) k->opWords * 4 + k->vertexBytes + (size_t) k->indexCount * 2 + k->blobBytes;
            long long ns = 0;
            if (gCopy) {
                if (copy.size() < bytes) {
                    copy.resize(bytes + bytes / 4);
                }
                const long long from = monotonicNs();
                char* at = copy.data();
                memcpy(at, k->ops, (size_t) k->opWords * 4);
                at += (size_t) k->opWords * 4;
                memcpy(at, k->vertices, k->vertexBytes);
                at += k->vertexBytes;
                memcpy(at, k->indices, (size_t) k->indexCount * 2);
                at += (size_t) k->indexCount * 2;
                memcpy(at, k->blob, k->blobBytes);
                ns = monotonicNs() - from;
            }
            {
                std::lock_guard<std::mutex> l(gGxLock);
                gGx.chunks++;
                gGx.bytes += (double) bytes;
                gGx.draws += k->draws;
                gGx.vertices += k->vertexCount;
                gGx.textureBytes += k->textureBytes;
                gGx.copyNs += (double) ns;
            }
            core.gxRelease(k);
        }
    }
}

// --- where the game's thread spends its time (--profile) -------------------------------

const unsigned kSamples = 1u << 19;  // per thread: 8.7 minutes at 1000 a second
const long long kSampleNs = 1000000;

// One sampled thread: the game's, and the GP thread (RE4_GX_THREAD,
// runtime/gx_thread.cpp). A sample in which the thread hardly ran since the
// one before (its CPU time grew by less than half the interval) counts as
// waiting, not as a place: a thread asleep in a wait sits in the C library.
struct Sampled {
    std::atomic<int> tid;
    uintptr_t samples[kSamples];
    std::atomic<unsigned> count;
    std::atomic<unsigned> waiting;
    long long lastCpuNs;  // only the signal handler on that thread uses it
};
Sampled gGameSampled, gGpSampled;
std::atomic<bool> gSampling{true};

void onSample(int, siginfo_t*, void* context)
{
    if (!gSampling.load(std::memory_order_relaxed)) {
        return;
    }
    const int self = (int) syscall(SYS_gettid);
    Sampled* t = self == gGameSampled.tid.load(std::memory_order_relaxed) ? &gGameSampled
                 : self == gGpSampled.tid.load(std::memory_order_relaxed) ? &gGpSampled
                                                                           : nullptr;
    if (!t) {
        return;
    }
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    const long long cpu = (long long) ts.tv_sec * 1000000000LL + ts.tv_nsec;
    const long long last = t->lastCpuNs;
    t->lastCpuNs = cpu;
    if (last > 0 && cpu - last < kSampleNs / 2) {
        t->waiting.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const ucontext_t* uc = (const ucontext_t*) context;
    uintptr_t pc = 0;
#if defined(__arm__)
    pc = uc->uc_mcontext.arm_pc;
#elif defined(__i386__)
    pc = (uintptr_t) uc->uc_mcontext.gregs[REG_EIP];
#else
    (void) uc;
#endif
    const unsigned n = t->count.fetch_add(1, std::memory_order_relaxed);
    if (n < kSamples) {
        t->samples[n] = pc;
    }
}

void samplerThread()
{
    prctl(PR_SET_NAME, "probe-sampler");
    int tid = 0;
    while ((tid = core.gameThread()) <= 0) {
        usleep(10000);
    }
    gGameSampled.tid.store(tid);
    for (unsigned tick = 0; gSampling.load(); tick++) {
        usleep(kSampleNs / 1000);
        syscall(SYS_tgkill, getpid(), tid, SIGPROF);
        int gp = gGpSampled.tid.load();
        if (!gp && core.gxThreadId && tick % 100 == 0) {
            gp = core.gxThreadId();  // the GP thread starts with the game's first GX command
            gGpSampled.tid.store(gp);
        }
        if (gp) {
            syscall(SYS_tgkill, getpid(), gp, SIGPROF);
        }
    }
}

void startSampler()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = onSample;
    // the game's sleep for the next retrace (vi.cpp) only wakes early and sleeps again
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaction(SIGPROF, &action, nullptr);
    std::thread(samplerThread).detach();
}

// the samples of one thread by library and offset into FILE, the share of each
// library on stdout
void writeSamples(Sampled& t, const char* what, const std::string& path)
{
    const unsigned n = std::min(t.count.load(), kSamples), waiting = t.waiting.load();
    struct Place {
        std::string lib;
        uintptr_t offset;
        unsigned count;
    };
    std::vector<Place> places;
    std::vector<std::pair<std::string, unsigned>> libs;
    std::sort(t.samples, t.samples + n);
    for (unsigned i = 0; i < n;) {
        unsigned j = i;
        while (j < n && t.samples[j] == t.samples[i]) {
            j++;
        }
        Dl_info info;
        memset(&info, 0, sizeof(info));
        std::string lib = "?";
        uintptr_t offset = t.samples[i];
        if (t.samples[i] && dladdr((void*) t.samples[i], &info) && info.dli_fname) {
            const char* slash = strrchr(info.dli_fname, '/');
            lib = slash ? slash + 1 : info.dli_fname;
            offset = t.samples[i] - (uintptr_t) info.dli_fbase;
        }
        places.push_back({lib, offset, j - i});
        auto at = std::find_if(libs.begin(), libs.end(), [&](const std::pair<std::string, unsigned>& l) {
            return l.first == lib;
        });
        if (at == libs.end()) {
            libs.push_back({lib, j - i});
        } else {
            at->second += j - i;
        }
        i = j;
    }
    FILE* f = fopen(path.c_str(), "we");
    if (f) {
        fprintf(f, "# re4_probe samples of %s: %u\n# waiting: %u\n", what, n, waiting);
        for (const Place& p : places) {
            fprintf(f, "%s 0x%lx %u\n", p.lib.c_str(), (unsigned long) p.offset, p.count);
        }
        fclose(f);
    }
    std::sort(libs.begin(), libs.end(), [](const std::pair<std::string, unsigned>& a,
                                           const std::pair<std::string, unsigned>& b) { return a.second > b.second; });
    printf("probe: profile of %s: %u samples at work, %u waiting, %zu places%s%s\n", what, n, waiting, places.size(),
           f ? " -> " : ", NOT written to ", path.c_str());
    for (size_t i = 0; i < libs.size() && i < 8; i++) {
        printf("probe-profile-lib: %s %.1f %%\n", libs[i].first.c_str(), n ? 100.0 * libs[i].second / n : 0.0);
    }
}

// FILE: the game's thread; FILE.gp: the GP thread (when it ran)
void writeProfile(const char* path)
{
    gSampling.store(false);
    usleep(20000);  // a signal still on its way
    writeSamples(gGameSampled, "the game's thread", path);
    if (gGpSampled.tid.load()) {
        writeSamples(gGpSampled, "the GP thread", std::string(path) + ".gp");
    }
}

// --- the sound: read at 32 kHz in real time --------------------------------------------

std::atomic<unsigned long long> gSoundFrames{0}, gSoundFromGame{0};

void soundThread()
{
    prctl(PR_SET_NAME, "probe-sound");
    std::vector<int16_t> block(2 * 2048);
    const long long start = monotonicNs();
    unsigned long long taken = 0;
    for (;;) {
        const unsigned long long due = (unsigned long long) ((monotonicNs() - start) / 1000 * 32 / 1000);
        while (taken < due) {
            const int n = (int) std::min<unsigned long long>(due - taken, 2048);
            const int got = core.audio(block.data(), n);
            taken += (unsigned long long) n;
            gSoundFrames.fetch_add((unsigned long long) n);
            gSoundFromGame.fetch_add(got > 0 ? (unsigned long long) got : 0);
        }
        usleep(10000);
    }
}

// --- the device ------------------------------------------------------------------------

std::string readFile(const char* path, size_t max = 1 << 16)
{
    std::string text;
    if (FILE* f = fopen(path, "re")) {
        char buffer[4096];
        size_t n;
        while (text.size() < max && (n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
            text.append(buffer, n);
        }
        fclose(f);
    }
    return text;
}

std::string firstLine(const std::string& text, const char* key)
{
    const size_t keyLen = strlen(key);
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (text.compare(at, keyLen, key) == 0) {
            return text.substr(at, end - at);
        }
        at = end + 1;
    }
    return "";
}

void trimEnd(std::string& s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r')) {
        s.pop_back();
    }
}

void describeDevice()
{
#if defined(__ANDROID__)
    for (const char* key : {"ro.product.manufacturer", "ro.product.model", "ro.hardware", "ro.build.version.release",
                            "ro.build.version.sdk"}) {
        char value[PROP_VALUE_MAX] = "";
        __system_property_get(key, value);
        printf("probe: device %s = %s\n", key, value);
    }
#endif
    utsname u;
    if (uname(&u) == 0) {
        printf("probe: kernel %s %s %s\n", u.sysname, u.release, u.machine);
    }
    const std::string cpu = readFile("/proc/cpuinfo");
    for (const char* key : {"Processor", "model name", "Hardware", "Features", "CPU part"}) {
        const std::string line = firstLine(cpu, key);
        if (!line.empty()) {
            printf("probe: cpuinfo %s\n", line.c_str());
        }
    }
    std::string online = readFile("/sys/devices/system/cpu/online");
    std::string maxFreq = readFile("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    std::string aslr = readFile("/proc/sys/kernel/randomize_va_space");
    trimEnd(online);
    trimEnd(maxFreq);
    trimEnd(aslr);
    printf("probe: cpus configured %ld, online %s, cpu0 max %s kHz, randomize_va_space %s\n",
           sysconf(_SC_NPROCESSORS_CONF), online.c_str(), maxFreq.c_str(), aslr.c_str());
    const std::string mem = readFile("/proc/meminfo");
    printf("probe: %s; %s\n", firstLine(mem, "MemTotal").c_str(), firstLine(mem, "MemFree").c_str());
}

// "1700/1700/off/off": the clock of each processor core in MHz
std::string clocks()
{
    std::string out;
    const long n = sysconf(_SC_NPROCESSORS_CONF);
    for (long i = 0; i < n && i < 16; i++) {
        char path[96];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_cur_freq", i);
        const std::string v = readFile(path, 64);
        out += (i ? "/" : "") + (v.empty() ? std::string("off") : std::to_string(atol(v.c_str()) / 1000));
    }
    return out;
}

// utime + stime of a stat file (/proc/self/stat, /proc/self/task/N/stat), in clock ticks
long long statTicks(const char* path)
{
    const std::string s = readFile(path, 2048);
    const size_t close = s.rfind(')');
    if (close == std::string::npos) {
        return 0;
    }
    // after the name: state is field 3, utime 14, stime 15
    std::vector<std::string> fields;
    size_t at = close + 2;
    while (at < s.size() && fields.size() < 13) {
        size_t end = s.find(' ', at);
        if (end == std::string::npos) {
            end = s.size();
        }
        fields.push_back(s.substr(at, end - at));
        at = end + 1;
    }
    return fields.size() >= 13 ? atoll(fields[11].c_str()) + atoll(fields[12].c_str()) : 0;
}

long rssKb()
{
    const std::string line = firstLine(readFile("/proc/self/status"), "VmRSS:");
    return line.empty() ? 0 : atol(line.c_str() + 6);
}

void printMaps(const char* when)
{
    const std::string maps = readFile("/proc/self/maps", 1 << 20);
    printf("probe: address map %s (MEM1 0x80000000..0x83000000; the game's libraries must lie above it):\n", when);
    for (size_t at = 0; at < maps.size();) {
        size_t end = maps.find('\n', at);
        if (end == std::string::npos) {
            end = maps.size();
        }
        printf("map: %s\n", maps.substr(at, end - at).c_str());
        at = end + 1;
    }
}

void threadTimes()
{
    const long tick = sysconf(_SC_CLK_TCK);
    DIR* d = opendir("/proc/self/task");
    if (!d) {
        return;
    }
    struct Thread {
        long long ticks;
        std::string name;
        int tid;
    };
    std::vector<Thread> threads;
    while (dirent* e = readdir(d)) {
        const int tid = atoi(e->d_name);
        if (tid <= 0) {
            continue;
        }
        char path[64];
        snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
        const long long t = statTicks(path);
        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
        std::string name = readFile(path, 64);
        trimEnd(name);
        threads.push_back({t, name, tid});
    }
    closedir(d);
    std::sort(threads.begin(), threads.end(), [](const Thread& a, const Thread& b) { return a.ticks > b.ticks; });
    const int game = core.gameThread ? core.gameThread() : 0;
    for (size_t i = 0; i < threads.size() && i < 8; i++) {
        printf("probe: thread %d %-16s %.1f s CPU%s\n", threads[i].tid, threads[i].name.c_str(),
               (double) threads[i].ticks / tick, threads[i].tid == game ? " (the game's thread)" : "");
    }
}

// --- reports and the summary ---------------------------------------------------------------

struct Mark {
    long long ns;
    unsigned frames;
    long long gameCpuNs, idleNs, processTicks;
    long long gpTicks;  // CPU time of the GP thread (clock ticks; 0: none)
    unsigned long long sound, soundFromGame;
    bool snapshot;
    unsigned room, step, screen;
};

struct Stretch {
    Mark from, to;
    GxCount gx;
    long rss;
    bool loop, play;
};

Mark mark()
{
    Mark m;
    memset(&m, 0, sizeof(m));
    m.ns = monotonicNs();
    m.frames = gFrames.load(std::memory_order_acquire);
    m.gameCpuNs = gGameCpuNs.load();
    m.idleNs = core.idleNs ? core.idleNs() : 0;
    m.processTicks = statTicks("/proc/self/stat");
    m.gpTicks = 0;
    if (const int gp = core.gxThreadId ? core.gxThreadId() : 0) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/self/task/%d/stat", gp);
        m.gpTicks = statTicks(path);
    }
    m.sound = gSoundFrames.load();
    m.soundFromGame = gSoundFromGame.load();
    if (const Re4CoreSnapshot* s = core.acquire()) {
        m.snapshot = true;
        m.room = s->room;
        m.step = s->rno[0];
        m.screen = s->screen;
        core.release(s);
    }
    return m;
}

std::string place(const Mark& m)
{
    if (!m.snapshot) {
        return "no frame yet";
    }
    char text[64];
    snprintf(text, sizeof(text), "r%03x step %u%s%s", m.room, m.step,
             (m.screen & RE4_CORE_SCREEN_EVENT) ? " event" : "", (m.screen & RE4_CORE_SCREEN_SUB) ? " sub screen" : "");
    return text;
}

struct Figures {
    double seconds = 0, frames = 0, fps = 0, p50 = 0, p95 = 0, p99 = 0, max = 0, late = 0, cpuMs = 0, idleMs = 0,
           processPct = 0, gxKb = 0, copyMs = 0, vertices = 0, soundShare = 0, rssMb = 0, gpMs = 0;
};

// the frame times of frames [from, to) still in the ring
void frameTimes(unsigned from, unsigned to, std::vector<float>& out)
{
    if (to - from > kRing) {
        from = to - kRing;
    }
    for (unsigned i = from; i < to; i++) {
        if (gFrameMs[i % kRing] > 0) {
            out.push_back(gFrameMs[i % kRing]);
        }
    }
}

Figures figures(const std::vector<Stretch>& stretches)
{
    Figures f;
    std::vector<float> times;
    double cpuNs = 0, idleNs = 0, ticks = 0, chunks = 0, bytes = 0, copyNs = 0, vertices = 0, sound = 0, fromGame = 0,
           rss = 0, gpTicks = 0;
    for (const Stretch& s : stretches) {
        f.seconds += (s.to.ns - s.from.ns) / 1e9;
        f.frames += s.to.frames - s.from.frames;
        cpuNs += (double) (s.to.gameCpuNs - s.from.gameCpuNs);
        idleNs += (double) (s.to.idleNs - s.from.idleNs);
        ticks += (double) (s.to.processTicks - s.from.processTicks);
        gpTicks += s.from.gpTicks ? (double) (s.to.gpTicks - s.from.gpTicks) : 0;
        chunks += s.gx.chunks;
        bytes += s.gx.bytes;
        copyNs += s.gx.copyNs;
        vertices += s.gx.vertices;
        sound += (double) (s.to.sound - s.from.sound);
        fromGame += (double) (s.to.soundFromGame - s.from.soundFromGame);
        rss = std::max(rss, (double) s.rss);
        frameTimes(s.from.frames, s.to.frames, times);
    }
    if (f.frames <= 0 || f.seconds <= 0) {
        return f;
    }
    std::sort(times.begin(), times.end());
    auto at = [&](double q) { return times.empty() ? 0.0 : (double) times[(size_t) (q * (times.size() - 1))]; };
    f.fps = f.frames / f.seconds;
    f.p50 = at(0.5);
    f.p95 = at(0.95);
    f.p99 = at(0.99);
    f.max = times.empty() ? 0 : times.back();
    for (float t : times) {
        f.late += t > 45.0f;  // a third retrace or more: the 30 Hz pace missed
    }
    f.cpuMs = cpuNs / 1e6 / f.frames;
    f.idleMs = idleNs / 1e6 / f.frames;
    f.gpMs = gpTicks / sysconf(_SC_CLK_TCK) * 1000.0 / f.frames;
    f.processPct = ticks / sysconf(_SC_CLK_TCK) / f.seconds * 100.0;
    f.gxKb = chunks ? bytes / 1024.0 / chunks : 0;
    f.copyMs = chunks ? copyNs / 1e6 / chunks : 0;
    f.vertices = chunks ? vertices / chunks : 0;
    f.soundShare = sound ? fromGame / sound * 100.0 : 0;
    f.rssMb = rss / 1024.0;
    return f;
}

void printResult(const char* set, const Figures& f, const Mark& last)
{
    const char* fast = getenv("RE4_VI_FAST");
    printf("probe-result: set=%s mode=%s room=r%03x seconds=%.0f frames=%.0f fps=%.1f frame_ms_p50=%.1f "
           "frame_ms_p95=%.1f frame_ms_p99=%.1f frame_ms_max=%.1f late=%.0f core_cpu_ms=%.1f idle_ms=%.1f "
           "process_cpu_pct=%.0f gx_kb=%.0f copy_ms=%.2f vertices=%.0f sound_from_game_pct=%.0f rss_mb=%.0f "
           "copy=%d gx_thread=%d gp_cpu_ms=%.1f\n",
           set, fast && strcmp(fast, "1") == 0 ? "fast" : "realtime", last.room, f.seconds, f.frames, f.fps, f.p50,
           f.p95, f.p99, f.max, f.late, f.cpuMs, f.idleMs, f.processPct, f.gxKb, f.copyMs, f.vertices, f.soundShare,
           f.rssMb, gCopy ? 1 : 0, core.gxThreaded && core.gxThreaded(-1) ? 1 : 0, f.gpMs);
}

void printLogTail()
{
    if (core.logTail) {
        static char tail[4096];
        core.logTail(tail, sizeof(tail));
        printf("probe: last lines of the core's log:\n%s", tail);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    // the C library's heap begins right behind this program instead of at a random place up to
    // 32 MB further, which can be inside MEM1 (a kernel like the OUYA's puts a position-independent
    // program just below 0x80000000): once more without address randomisation
    if (!getenv("RE4_PROBE_FIXED")) {
        const long persona = syscall(SYS_personality, 0xffffffffUL);
        if (persona != -1 && !(persona & kAddrNoRandomize) &&
            syscall(SYS_personality, (unsigned long) persona | kAddrNoRandomize) != -1) {
            setenv("RE4_PROBE_FIXED", "1", 1);
            execv("/proc/self/exe", argv);
        }
        setenv("RE4_PROBE_FIXED", "randomized", 1);
    }
    // everything below 0x83000000 held, MEM1 first: the core's libraries load above it
    unsetenv("RE4_MEM1_PLACEHOLDER");
    reserveLowMemory();
    const char* placeholder = getenv("RE4_MEM1_PLACEHOLDER") ? "reserved" : "NOT reserved";
    setvbuf(stdout, nullptr, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);
    // the name ps shows (the PC script ends a probe that is left over by it); the game's thread inherits it
    prctl(PR_SET_NAME, "re4-probe");

    std::string data;
    const char* profile = nullptr;
    int seconds = 180, report = 5;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            report = std::max(1, atoi(argv[++i]));
        } else if (strcmp(argv[i], "--no-copy") == 0) {
            gCopy = false;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profile = argv[++i];
        } else {
            fprintf(stderr, "usage: %s --data DIR [--seconds N] [--report N] [--no-copy] [--profile FILE]\n", argv[0]);
            return 2;
        }
    }
    char exe[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = 0;
        gExeDir = exe;
        gExeDir = gExeDir.substr(0, gExeDir.rfind('/'));
    }
    if (data.empty()) {
        data = gExeDir + "/data";
    }
    // as the app: without the debug build's own displays
    setenv("RE4_DEBUG_HUD", "0", 0);

    printf("probe: the game core without a picture (%d-bit, address randomisation %s, MEM1 placeholder %s)\n",
           (int) (sizeof(void*) * 8),
           strcmp(getenv("RE4_PROBE_FIXED"), "1") == 0 ? "off for this program" : "on", placeholder);
    describeDevice();
    extern char** environ;
    for (char** e = environ; *e; e++) {
        if (strncmp(*e, "RE4_", 4) == 0) {
            printf("probe: setting %s\n", *e);
        }
    }
    printf("probe: data %s, %d s, a report every %d s, GX chunks %s\n", data.c_str(), seconds, report,
           gCopy ? "copied as by the core process" : "only taken");

    std::string problem;
    const long long loadFrom = monotonicNs();
    if (!loadCore(problem)) {
        printf("probe: FAILED to load: %s\n", problem.c_str());
        printMaps("after the failed load");
        return 2;
    }
    printf("probe: core loaded in %.1f s\n", (monotonicNs() - loadFrom) / 1e9);
    printMaps("after loading the core");
    installCrashHandler();
    struct stat st;
    std::string modules = gExeDir + "/modules";
    if (stat(modules.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        modules = gExeDir;
    }
    if (!core.reserve(data.c_str(), modules.c_str())) {
        printf("probe: FAILED to reserve: %s\n", core.message());
        return 2;
    }
    core.addFrameHook(frameHook);
    core.gxNotify(gxNotify);
    if (!(gTestEntry ? core.startEntry(gTestEntry) : core.start())) {
        printf("probe: FAILED to start: %s\n", core.message());
        return 2;
    }
    std::thread(gxThread).detach();
    std::thread(soundThread).detach();
    if (profile) {
        startSampler();
    }
    printf("probe: the game runs (modules from %s)\n", modules.c_str());

    std::vector<Stretch> stretches;
    Mark previous = mark();
    const long long startNs = previous.ns;
    unsigned lastFrames = 0;
    long long lastNewFrameNs = startNs;
    int code = 0;
    for (;;) {
        const long long next = previous.ns + (long long) report * 1000000000LL;
        for (long long now = monotonicNs(); now < next; now = monotonicNs()) {
            usleep((useconds_t) std::min<long long>((next - now) / 1000, 200000));
        }
        Stretch s;
        s.from = previous;
        s.to = mark();
        {
            std::lock_guard<std::mutex> l(gGxLock);
            s.gx = gGx;
            memset(&gGx, 0, sizeof(gGx));
        }
        s.rss = rssKb();
        s.loop = s.from.snapshot && s.to.snapshot && s.from.step == 3 && s.to.step == 3 && s.from.room == s.to.room &&
                 s.to.frames > s.from.frames;
        s.play = s.loop && !((s.from.screen | s.to.screen) & (RE4_CORE_SCREEN_EVENT | RE4_CORE_SCREEN_SUB));
        stretches.push_back(s);
        previous = s.to;
        const double dt = (s.to.ns - s.from.ns) / 1e9;
        const unsigned frames = s.to.frames - s.from.frames;
        std::vector<float> times;
        frameTimes(s.from.frames, s.to.frames, times);
        float worst = 0;
        unsigned late = 0;
        double sum = 0;
        for (float t : times) {
            worst = std::max(worst, t);
            late += t > 45.0f;
            sum += t;
        }
        printf("probe: %5.0f s %-22s frames %4u (%4.1f/s) frame ms avg %5.1f max %6.1f late %3u | core %5.1f ms/frame, "
               "idle %5.1f, gp %5.1f | process cpu %3.0f %% | gx %3u chunks %5.0f KB copy %5.2f ms | sound %3.0f %% game | "
               "rss %ld MB | MHz %s\n",
               (s.to.ns - startNs) / 1e9, place(s.to).c_str(), frames, frames / dt, times.empty() ? 0.0 : sum / times.size(),
               worst, late, frames ? (s.to.gameCpuNs - s.from.gameCpuNs) / 1e6 / frames : 0.0,
               frames && core.idleNs ? (s.to.idleNs - s.from.idleNs) / 1e6 / frames : 0.0,
               frames && s.from.gpTicks ? (double) (s.to.gpTicks - s.from.gpTicks) / sysconf(_SC_CLK_TCK) * 1000.0 / frames
                                        : 0.0,
               (double) (s.to.processTicks - s.from.processTicks) / sysconf(_SC_CLK_TCK) / dt * 100.0, s.gx.chunks,
               s.gx.chunks ? s.gx.bytes / 1024.0 / s.gx.chunks : 0.0, s.gx.chunks ? s.gx.copyNs / 1e6 / s.gx.chunks : 0.0,
               s.to.sound > s.from.sound
                   ? (double) (s.to.soundFromGame - s.from.soundFromGame) / (s.to.sound - s.from.sound) * 100.0
                   : 0.0,
               s.rss / 1024, clocks().c_str());
        const int state = core.state();
        if (state != RE4_CORE_RUNNING) {
            printf("probe: the game %s: %s\n", state == RE4_CORE_ENDED ? "ended" : "failed", core.message());
            printLogTail();
            code = 4;
            break;
        }
        if (s.to.frames != lastFrames) {
            lastFrames = s.to.frames;
            lastNewFrameNs = s.to.ns;
        } else if (s.to.ns - lastNewFrameNs > 60000000000LL) {
            printf("probe: HANG: no game frame for %.0f s (frame %u)\n", (s.to.ns - lastNewFrameNs) / 1e9, s.to.frames);
            printLogTail();
            code = 3;
            break;
        }
        if (s.to.ns - startNs >= (long long) seconds * 1000000000LL) {
            break;
        }
    }

    // the summary: while the player was free (no event, no sub screen), else the whole main loop
    std::vector<Stretch> play, loop;
    for (const Stretch& s : stretches) {
        if (s.play) {
            play.push_back(s);
        }
        if (s.loop) {
            loop.push_back(s);
        }
    }
    const Figures all = figures(stretches), inLoop = figures(loop), inPlay = figures(play);
    printf("probe: summary: %.0f s, %.0f frames; main loop %.0f s; free to play %.0f s\n", all.seconds, all.frames,
           inLoop.seconds, inPlay.seconds);
    threadTimes();
    if (profile) {
        writeProfile(profile);
    }
    if (inPlay.seconds > 0) {
        printResult("play", inPlay, previous);
    }
    if (inLoop.seconds > 0) {
        printResult("loop", inLoop, previous);
    }
    printResult("all", all, previous);
    {
        unsigned a[5] = {0, 0, 0, 0, 0};
        core.audioStats(a);
        printf("probe: sound of the core: %u blocks, %u waiting, %u lost, %u reads ran dry, %u skips\n", a[0], a[1], a[2],
               a[3], a[4]);
    }
    fflush(stdout);
    fflush(stderr);
    // as in the core process: the game is never shut down, the process just ends
    _exit(code);
}
