// Core mode (armeabi-v7a): JNI of com.ast.re4decomp.CoreBridge. Loads the
// game core (libre4_main.so with libre4_platform.so and the module libraries,
// built by port/tools/whole_build_android.sh), reserves the GameCube main
// memory at 0x80000000 as early as the process allows (JNI_OnLoad, before any
// GL work), starts the game in its own thread, passes the controller and draws
// what the game draws: the GX chunks of the core's emulated graphics processor
// with the GX renderer (gx_gl.cpp, port/docs/gx-plan.md step G4), and until
// the first display copy (or with the setting RE4_VIEW=bridge) the published
// state frames with CoreScene.
#include "core_scene.h"
#include "gx_gl.h"
#include "re4_card_files.h"
#include "re4_gx_frame.h"
#include "re4_gx_frame_io.h"

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <unistd.h>
#include <jni.h>
#if defined(__aarch64__)
#include <android/dlext.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "RE4Core", __VA_ARGS__)

struct CoreApi {
  void *main{};
  std::string error;
  int (*mem1Init)(void){};
  int (*reserve)(const char *, const char *){};
  int (*start)(void){};
  int (*state)(void){};
  const char *(*message)(void){};
  void (*pad)(const Re4CorePad *){};
  unsigned (*rumble)(void){};
  int (*audio)(int16_t *, int){};
  void (*audioStats)(unsigned *){};
  const Re4CoreSnapshot *(*acquire)(void){};
  void (*release)(const Re4CoreSnapshot *){};
  int (*logTail)(char *, int){};
  const Re4GxChunk *(*gxAcquire)(void){};
  void (*gxRelease)(const Re4GxChunk *){};
  void (*gxDetach)(void){};
  void (*vsync)(int64_t){};  // optional (cores since M9)
  // optional (cores since M11): the app's menu
  int (*pause)(int, int){};
  void (*debugMenu)(void){};
  void (*setDof)(int){};
  int (*cardList)(Re4CardFile *, int){};
  int (*cardFree)(void){};
  int (*cardName)(int, char *, int){};
  int (*cardExport)(int, unsigned char *, unsigned, char *, int){};
  int (*cardImport)(const unsigned char *, unsigned, int, char *, int){};
  unsigned (*frameCount)(void){};
  int (*gameThread)(void){};
  long (*freeAt)(void){};  // optional: the frame the player became free in the room (state_trace.cpp), -1 not yet
} api;

template <class F> bool bind(F &slot, const char *name) {
  slot = reinterpret_cast<F>(dlsym(api.main, name));
  if (!slot && api.error.empty()) api.error = std::string("missing in libre4_main.so: ") + name;
  return slot != nullptr;
}

#if defined(__aarch64__)
// 64 bit (milestone 12): the game keeps its addresses in 32 bits and tells
// its own from others by bit 31, so everything it reaches lies in the window
// 0x80000000..0xFFFFFFFF. MEM1 (0x80000000..0x83000000) is held with an
// inaccessible placeholder before anything is loaded (re4_mem1_init maps over
// it, RE4_MEM1_PLACEHOLDER), and the linker loads the two core libraries into
// ranges reserved at the top of the window. The core puts its module
// libraries and stacks there itself (module_loader.cpp, game_window.cpp).
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
constexpr unsigned long kWindowStart = 0x83000000ul;
constexpr unsigned long kWindowEnd = 0x100000000ul;

void *reserveFixed(unsigned long at, unsigned long size) {
  void *want = reinterpret_cast<void *>(at);
  void *p = mmap(want, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
  if (p == want) return p;
  if (p != MAP_FAILED) munmap(p, size);  // an old kernel took the address as a hint
  return nullptr;
}

// from the top of the window down, in 16 MB steps (ART and the drivers may
// hold parts of it)
void *reserveInWindow(unsigned long size) {
  for (unsigned long at = kWindowEnd - size; at >= kWindowStart; at -= 16ul << 20) {
    if (void *p = reserveFixed(at, size)) return p;
  }
  return nullptr;
}

// What lies below 4 GB when the core loads, for the error report (address-map.txt):
// MEM1 and the window above it have to be free, and on 64 bit ART, the zygote and
// the drivers decide what is there before the app runs
std::string gAddressMap;
std::string gMem1Owner;  // the first mapping in MEM1 (the reason when it is taken)

// "start-end perms name" of a /proc/self/maps line
std::string shortMapping(const char *line) {
  char range[64] = "", perms[8] = "";
  int name = 0;
  if (sscanf(line, "%63s %7s %*s %*s %*s %n", range, perms, &name) < 2) return line;
  std::string text = std::string(range) + ' ' + perms;
  if (name > 0 && line[name]) text += std::string(" ") + (line + name);
  while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) text.pop_back();
  return text;
}

void recordLowMappings(const char *when) {
  gAddressMap += std::string("--- ") + when + " (mappings below 4 GB; MEM1 0x80000000..0x83000000, window up to 4 GB)\n";
  FILE *f = fopen("/proc/self/maps", "re");
  if (!f) {
    gAddressMap += "(cannot read /proc/self/maps)\n";
    return;
  }
  char line[512];
  int lines = 0;
  unsigned long at = 0x80000000ul, largest = 0;
  while (fgets(line, sizeof(line), f)) {
    unsigned long a, b;
    if (sscanf(line, "%lx-%lx", &a, &b) != 2 || a >= kWindowEnd) continue;
    std::string text = shortMapping(line);
    if (a < kWindowStart && b > 0x80000000ul) {
      text += "   <- MEM1";
      if (gMem1Owner.empty()) gMem1Owner = shortMapping(line);
    } else if (b > kWindowStart) {
      text += "   <- window";
    }
    if (b > at) {
      if (a > at && a - at > largest) largest = a - at;
      at = b;
    }
    if (++lines <= 2000) gAddressMap += text + '\n';
    if (b > 0x80000000ul) LOG("below 4 GB: %s", text.c_str());
  }
  fclose(f);
  if (kWindowEnd > at && kWindowEnd - at > largest) largest = kWindowEnd - at;
  char sum[160];
  snprintf(sum, sizeof(sum), "%d mappings below 4 GB; largest free range in 0x80000000..4 GB: %lu MB\n", lines,
           largest >> 20);
  gAddressMap += sum;
  LOG("%s", sum);
}

// The mapping that holds `addr` (start, end, name), false when none does.
bool mappingAt(unsigned long addr, unsigned long &start, unsigned long &end, std::string &name) {
  FILE *f = fopen("/proc/self/maps", "re");
  if (!f) return false;
  char line[512];
  bool found = false;
  while (!found && fgets(line, sizeof(line), f)) {
    unsigned long a, b;
    int at = 0;
    if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %n", &a, &b, &at) < 2 || addr < a || addr >= b) continue;
    start = a;
    end = b;
    name = at > 0 ? line + at : "";
    while (!name.empty() && (name.back() == '\n' || name.back() == ' ')) name.pop_back();
    found = true;
  }
  fclose(f);
  return found;
}

// ART on arm64 keeps every Java object below 4 GB. Its area for large objects
// ("free list large object space", as large as dalvik.vm.heapsize) comes from
// the 64-bit zygote and lies in the first gap behind the boot image; on the
// Retroid (heapsize 512m) 0x73f8a000..0x93f8a000, over MEM1 (first 64-bit run,
// 17.10.2026). ART fills it from the bottom, objects there never move, and it
// never touches the data of a live byte[]. So the app takes MEM1 from it as the
// data of one array that it holds for the life of the process: arrays are put
// below it until the next one starts at the page before 0x80000000, that one
// reaches to 0x83000000, and the arrays below are let go (ART gets them back as
// free space). Needs the large heap (android:largeHeap): for a moment the
// arrays from the area's fill level up to 0x83000000 count as Java memory.
jobject gMem1Holder;

bool holdMem1InLargeObjectSpace(JNIEnv *env) {
  unsigned long a = 0, b = 0;
  std::string name;
  if (!env || !mappingAt(0x80000000ul, a, b, name) || name.find("large object space") == std::string::npos ||
      b < kWindowStart) {
    return false;
  }
  const unsigned long page = (unsigned long)sysconf(_SC_PAGESIZE);
  const unsigned long holderStart = 0x80000000ul - page;  // the array's own header in the page before MEM1
  // a byte[] taking `bytes` of the area (whole pages: its header is smaller than 64 bytes); its start
  auto take = [&](unsigned long bytes, unsigned long &at) -> jbyteArray {
    jbyteArray array = env->NewByteArray((jsize)(bytes - 64));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return nullptr;
    }
    if (!array) return nullptr;
    void *data = env->GetPrimitiveArrayCritical(array, nullptr);
    at = (unsigned long)data & ~(page - 1);
    if (data) env->ReleasePrimitiveArrayCritical(array, data, JNI_ABORT);
    return data ? array : nullptr;
  };
  std::vector<jbyteArray> below;
  jbyteArray holder = nullptr;
  unsigned long size = 32ul << 20, at = 0;  // first a probe: where the area is filled up to
  char note[256];
  const char *problem = nullptr;
  jbyteArray array = take(size, at);
  for (int step = 0; step < 8 && array && !holder && !problem; step++) {
    unsigned long end = at + size;
    if (at >= a && end <= b && at <= holderStart && end >= kWindowStart) {
      holder = array;
    } else if (at < a || at > holderStart || end > holderStart) {
      problem = "the large object space is filled beyond MEM1's start";
      below.push_back(array);
    } else {
      below.push_back(array);
      unsigned long gap = holderStart - end;
      // up to the holder's page, or for a small gap the holder at once from here
      size = gap >= (1ul << 20) ? gap : kWindowStart - end;
      array = take(size, at);
    }
  }
  if (!holder && !problem) problem = array ? "no array landed at MEM1" : "the Java heap refused the arrays";
  unsigned long filler = 0;
  for (jbyteArray x : below) {
    jsize n = env->GetArrayLength(x);
    filler += (unsigned long)n;
    env->DeleteLocalRef(x);
  }
  if (!holder) {
    snprintf(note, sizeof(note), "MEM1 in ART's large object space %lx-%lx: not possible (%s)\n", a, b, problem);
    gAddressMap += note;
    LOG("%s", note);
    return false;
  }
  gMem1Holder = env->NewGlobalRef(holder);
  env->DeleteLocalRef(holder);
  snprintf(note, sizeof(note),
           "MEM1 held in ART's large object space %lx-%lx: the data of a byte[] at 0x%lx..0x%lx (%lu MB below "
           "let go)\n",
           a, b, at, at + size, filler >> 20);
  gAddressMap += note;
  LOG("%s", note);
  // the placeholder over the array's data, which MEM1 then replaces (re4_mem1_init)
  void *want = reinterpret_cast<void *>(0x80000000ul);
  return mmap(want, kWindowStart - 0x80000000ul, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
              -1, 0) == want;
}

void *openInWindow(const char *name, unsigned long size) {
  void *range = reserveInWindow(size);
  if (!range) {
    LOG("no %lu MB below 4 GB for %s", size >> 20, name);
    return nullptr;
  }
  android_dlextinfo info;
  std::memset(&info, 0, sizeof(info));
  info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
  info.reserved_addr = range;
  info.reserved_size = size;
  void *handle = android_dlopen_ext(name, RTLD_NOW | RTLD_LOCAL, &info);
  if (!handle) {
    munmap(range, size);
  } else {
    LOG("%s at %p", name, range);
  }
  char note[192];
  snprintf(note, sizeof(note), "%s: %s at %p (%lu MB reserved)\n", name, handle ? "loaded" : "NOT loaded", range,
           size >> 20);
  gAddressMap += note;
  return handle;
}
#endif

void loadCore(JNIEnv *env) {
  // libre4_main.so brings libre4_platform.so and libc++_shared.so (DT_NEEDED);
  // the module libraries are loaded by name later (re4_module_load).
  // RTLD_LOCAL: the game's symbols must not serve libraries loaded later
#if defined(__aarch64__)
  recordLowMappings("when the core loads");
  if (reserveFixed(0x80000000ul, kWindowStart - 0x80000000ul)) {
    setenv("RE4_MEM1_PLACEHOLDER", "1", 1);
    gAddressMap += "MEM1 placeholder: reserved\n";
  } else if (holdMem1InLargeObjectSpace(env)) {
    setenv("RE4_MEM1_PLACEHOLDER", "1", 1);
    gAddressMap += "MEM1 placeholder: reserved inside the large object space\n";
  } else {
    gAddressMap += "MEM1 placeholder: NOT reserved\n";
  }
  // the platform first, each in a range of its own (its size on x86_64 -O0:
  // libre4_platform 8 MB, libre4_main 10 MB)
  if (openInWindow("libre4_platform.so", 32ul << 20)) {
    api.main = openInWindow("libre4_main.so", 64ul << 20);
  }
#else
  (void)env;
  api.main = dlopen("libre4_main.so", RTLD_NOW | RTLD_LOCAL);
#endif
  if (!api.main) {
    const char *e = dlerror();
    api.error = std::string("core libraries missing (port/tools/whole_build_android.sh): ") +
                (e ? e : "?");
    LOG("%s", api.error.c_str());
    return;
  }
  bool ok = bind(api.mem1Init, "re4_mem1_init");
  ok = bind(api.reserve, "re4_core_reserve") && ok;
  ok = bind(api.start, "re4_core_start") && ok;
  ok = bind(api.state, "re4_core_state") && ok;
  ok = bind(api.message, "re4_core_message") && ok;
  ok = bind(api.pad, "re4_core_pad") && ok;
  ok = bind(api.rumble, "re4_core_rumble") && ok;
  ok = bind(api.audio, "re4_core_audio") && ok;
  ok = bind(api.audioStats, "re4_core_audio_stats") && ok;
  ok = bind(api.acquire, "re4_core_acquire") && ok;
  ok = bind(api.release, "re4_core_release") && ok;
  ok = bind(api.logTail, "re4_core_log_tail") && ok;
  ok = bind(api.gxAcquire, "re4_core_gx_acquire") && ok;
  ok = bind(api.gxRelease, "re4_core_gx_release") && ok;
  ok = bind(api.gxDetach, "re4_core_gx_detach") && ok;
  api.vsync = reinterpret_cast<void (*)(int64_t)>(dlsym(api.main, "re4_core_vsync"));
  api.pause = reinterpret_cast<int (*)(int, int)>(dlsym(api.main, "re4_core_pause"));
  api.debugMenu = reinterpret_cast<void (*)(void)>(dlsym(api.main, "re4_core_debug_menu"));
  api.setDof = reinterpret_cast<void (*)(int)>(dlsym(api.main, "re4_core_set_dof"));
  api.cardList = reinterpret_cast<int (*)(Re4CardFile *, int)>(dlsym(api.main, "re4_core_card_list"));
  api.cardFree = reinterpret_cast<int (*)(void)>(dlsym(api.main, "re4_core_card_free_blocks"));
  api.cardName = reinterpret_cast<int (*)(int, char *, int)>(dlsym(api.main, "re4_core_card_file_name"));
  api.cardExport =
      reinterpret_cast<int (*)(int, unsigned char *, unsigned, char *, int)>(dlsym(api.main, "re4_core_card_export"));
  api.cardImport = reinterpret_cast<int (*)(const unsigned char *, unsigned, int, char *, int)>(
      dlsym(api.main, "re4_core_card_import"));
  api.frameCount = reinterpret_cast<unsigned (*)(void)>(dlsym(api.main, "re4_core_frame_count"));
  api.gameThread = reinterpret_cast<int (*)(void)>(dlsym(api.main, "re4_core_game_thread"));
  api.freeAt = reinterpret_cast<long (*)(void)>(dlsym(api.main, "re4_trace_free_at"));
  if (!ok) {
    LOG("%s", api.error.c_str());
    api.main = nullptr;
    return;
  }
  // MEM1 before anything else maps memory there (GL drivers, ART growth)
  if (!api.mem1Init()) {
    api.error = "MEM1 0x80000000..0x83000000 is taken in this process";
#if defined(__aarch64__)
    if (!gMem1Owner.empty()) api.error += " (by " + gMem1Owner + ")";
#endif
    LOG("%s", api.error.c_str());
  } else {
    LOG("core loaded, MEM1 reserved");
  }
}

std::string text(JNIEnv *env, jstring value) {
  if (!value) return {};
  const char *chars = env->GetStringUTFChars(value, nullptr);
  std::string result = chars ? chars : "";
  if (chars) env->ReleaseStringUTFChars(value, chars);
  return result;
}

std::mutex sceneLock;  // scene creation (UI thread) against GL/worker threads
std::unique_ptr<re4::CoreScene> scene;
re4::CoreFrame frame;  // GL thread
int width = 1, height = 1;
std::chrono::steady_clock::time_point lastDraw;

// GX view (GL thread): on unless RE4_VIEW=bridge; EFB scale from the menu
// (CoreBridge.setGraphics; automatic: from the screen's height, at most 3),
// else RE4_GX_SCALE
std::unique_ptr<re4gx::GxRenderer> gx;
int gxMode = -1;  // -1 undecided, 0 bridge, 1 GX
// the menu's picture options (M11, any thread -> GL thread): a change builds
// the renderer anew at the next picture while the game runs (the core then
// starts over with the full state); 0 scale: automatic
std::atomic<int> optScale{0}, optSamples{1}, optAnisotropy{1}, optAspect{-1};
// GPU step b (GxOptions::invalidate): off (it saved nothing on the Retroid) unless
// RE4_GX_INVALIDATE=1; GPU step d (GxOptions::tiledMsaa): on (RE4_GX_MSAA_TILES=0 in the renderer
// turns it off); the GPU measurement switches both
std::atomic<int> optInvalidate{-1}, optTiled{1};
std::atomic<unsigned> optSerial{0};
unsigned optApplied = ~0u;
std::atomic<bool> corePaused{false};
// what the device can do and what the renderer uses (CoreBridge.graphicsInfo)
std::atomic<int> infoMaxSamples{0}, infoMaxAnisotropy{0}, infoScale{0}, infoSamples{0}, infoAnisotropy{0};
unsigned gxChunks = 0;
// timing of the GX view since the last log line
std::chrono::steady_clock::time_point gxSince;
double gxRunMs = 0, gxRunMax = 0;
unsigned gxRunCount = 0, gxFrames = 0, gxDraws = 0;
re4gx::GxStats gxWork;  // the counters at the last log line
// GX dumps for the diagnosis (RE4_GX_DUMP_DIR); -1: settings not read yet
int gxDumpEvery = -1;
std::string gxDumpDir;
std::chrono::steady_clock::time_point gxDumpNext;
bool gxDumpWanted = false;
unsigned gxDumpCount = 0;
// dumps asked for with the controller (R3, CoreBridge.gxDump), any time, at
// most 40 in all; the folder comes with the request when no test set one
std::mutex gxDumpLock;
std::string gxDumpAskedDir;
unsigned gxDumpAsked = 0;
// a controller dump is a series: the chunk that starts over and the ones right
// after it (gx-geraet-NN-KK.bin), drawn one after another in the cloud, so
// flicker over several pictures shows
constexpr unsigned kGxDumpSeries = 12;
bool gxDumpSeriesWanted = false;
unsigned gxDumpSeriesLeft = 0, gxDumpSeriesIndex = 0, gxDumpSeriesPart = 0;
uint32_t gxDumpSeriesSequence = 0;
// a hit on the player (a dark bar at the bottom showed only then, too briefly
// for R3): the core marks the chunk of that moment (RE4_GX_CHUNK_MARK,
// state_trace.cpp, RE4_GX_SHOTS_ON_HIT hits); it starts over. The app keeps it
// and the 3 after it as GX data (RE4_GX_DUMP_ON_HIT hits) and the next 24
// pictures as shown (treffer-NN-KK.bmp), into RE4_GX_DUMP_FOLDER
constexpr unsigned kGxHitSeries = 4, kGxHitShots = 24;
int gxHitDumps = -1;  // hits still to record as GX data; -1: settings not read yet
unsigned gxHitShotsLeft = 0, gxHitShotPart = 0, gxHitCount = 0;
bool gxHitSeriesPending = false;  // the chunk after a marked one starts over: a series from there
GLuint gxShotFbo = 0, gxShotColor = 0;
unsigned gxShotW = 0, gxShotH = 0;

// The GPU measurement (RE4_GPU_BENCH, test-core-app.ps1 -GpuBench; GPU step (a), 19.10.2026):
// one run steps through picture options at the same place of the game (the test's start in r100,
// Leon standing) and logs for each the GPU time of a picture and of a present on the screen
// ("GPU bench N/M: ..."): what resolution, multisampling, anisotropic filtering and depth of field
// cost on this device. RE4_GPU_BENCH=1 takes the steps below, "S/M/A/D,..." (scale, samples,
// anisotropy, depth of field 0/1) names them. The first step comes RE4_GPU_BENCH_DELAY seconds
// (default 10) after the player is free in the room (the core's re4_trace_free_at: the arrival's
// event and the radio call are over); a core without it: that many seconds (default 60) after the
// first picture. Each step: 5 s to settle (the renderer starts over), then 10 s measured.
// Afterwards the options from before the run come back.
//
// Each step's line also has what lies in the picture (draws per picture: the same place for every
// step?), the GPU's load times its clock (the work, independent of the GPU timer) and how many
// pictures the GPU was behind (fences, GxStats::behind*). The first run on the Retroid
// (19.10.2026) began 60 s after the first picture, during the arrival, and its timer read 110 to
// 490 ms per picture with 2x MSAA and the autofocus while 30 pictures/s came: the timer alone does
// not tell there.
struct BenchStep {
  int scale, samples, anisotropy, dof;
  int invalidate = 0;  // GPU step b: copy targets forgotten before they are drawn over
  int tiled = 1;       // GPU step d: MSAA on the tiles
};
struct Bench {
  std::vector<BenchStep> steps;
  int index = -2;  // -2 settings not read, -3 off or done, -1 waiting for the start, else the step
  bool measuring = false;
  std::chrono::steady_clock::time_point next;  // the next change of phase
  std::chrono::steady_clock::time_point since, nextSample;
  re4gx::GxStats at;  // the renderer's counters when the step's measurement began
  double runMs = 0;   // the renderer's CPU time of the step's pictures
  unsigned runCount = 0;
  double clockSum = 0, busySum = 0, workSum = 0;
  unsigned clockCount = 0, busyCount = 0, workCount = 0;
  bool waitFree = false;  // waiting for the player to be free (api.freeAt)
  int delay = 0;          // seconds from then to the first step
  int keepScale = 0, keepSamples = 1, keepAnisotropy = 1, keepInvalidate = 0, keepTiled = 1;
};
Bench bench;
std::atomic<int> optDof{-1};  // the menu's depth of field (CoreBridge.setDof); -1 not set yet

// A number from a file of the kernel (the GPU's clock and load, where the device lets an app read
// them); -1 when it cannot.
double readNumber(const char *path) {
  FILE *f = std::fopen(path, "r");
  if (!f) return -1;
  double v = -1;
  if (std::fscanf(f, "%lf", &v) != 1) v = -1;
  std::fclose(f);
  return v;
}

// The GPU clock in MHz (Adreno: kgsl in Hz; others in MHz), -1 when no file can be read.
double gpuClockMhz() {
  for (const char *path : {"/sys/class/kgsl/kgsl-3d0/gpuclk", "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
                           "/sys/kernel/gpu/gpu_clock"}) {
    const double v = readNumber(path);
    if (v > 0) return v > 100000 ? v / 1e6 : v;
  }
  return -1;
}

// The GPU's load in percent as the kernel reports it, -1 when no file can be read.
double gpuBusyPercent() {
  for (const char *path : {"/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", "/sys/kernel/gpu/gpu_busy"}) {
    const double v = readNumber(path);
    if (v >= 0) return v;
  }
  return -1;
}

void benchApply(const BenchStep &step) {
  optScale = step.scale;
  optSamples = step.samples;
  optAnisotropy = step.anisotropy;
  optInvalidate = step.invalidate;
  optTiled = step.tiled;
  optSerial++;
  if (api.setDof) api.setDof(step.dof);
}

// Once per displayed frame (GL thread), after the chunk.
void benchTick(std::chrono::steady_clock::time_point now) {
  using std::chrono::seconds;
  if (bench.index == -2) {
    bench.index = -3;
    const char *spec = getenv("RE4_GPU_BENCH");
    if (!spec || !*spec || std::string(spec) == "0") return;
    if (std::string(spec) == "1") {
      // GPU step d: 2x MSAA in separate buffers, then on the tiles, with the tester's options
      // (8x anisotropy, depth of field; POCO X7 Pro, 18.10.2026) and with MSAA alone, each twice,
      // between nothing extra at the start and the end. The runs before (Retroid, 19.10.2026): MSAA
      // +74 % work, the depth of field +19 %, both +114 %, 8x anisotropy and step b (forgetting the
      // copy targets) nothing measurable; the same setting twice up to 5 % apart.
      // "S/M/A/D/I/T,..." names other steps (I: copy targets forgotten, T: MSAA on the tiles)
      bench.steps = {{3, 1, 1, 0, 0, 1}, {3, 2, 8, 1, 0, 0}, {3, 2, 8, 1, 0, 1}, {3, 2, 1, 0, 0, 0},
                     {3, 2, 1, 0, 0, 1}, {3, 2, 8, 1, 0, 0}, {3, 2, 8, 1, 0, 1}, {3, 1, 1, 0, 0, 1}};
    } else {
      for (const char *at = spec; *at;) {
        BenchStep step{};
        int used = 0;
        if (std::sscanf(at, "%d/%d/%d/%d%n", &step.scale, &step.samples, &step.anisotropy, &step.dof, &used) != 4)
          break;
        int more = 0;
        if (at[used] == '/' && std::sscanf(at + used, "/%d%n", &step.invalidate, &more) == 1) used += more;
        if (at[used] == '/' && std::sscanf(at + used, "/%d%n", &step.tiled, &more) == 1) used += more;
        bench.steps.push_back(step);
        at += used;
        if (*at == ',') at++;
      }
    }
    if (bench.steps.empty()) {
      LOG("GPU bench: no steps in RE4_GPU_BENCH=%s", spec);
      return;
    }
    const char *delay = getenv("RE4_GPU_BENCH_DELAY");
    bench.waitFree = api.freeAt != nullptr;
    bench.delay = delay ? std::max(1, std::atoi(delay)) : bench.waitFree ? 10 : 60;
    // waiting for the player: at most 5 minutes, then measured anyway
    bench.next = now + seconds(bench.waitFree ? 300 : bench.delay);
    bench.index = -1;
    const double clock = gpuClockMhz(), busy = gpuBusyPercent();
    LOG("GPU bench: %zu steps, the first %d s after %s (5 s to settle, 10 s measured each); GPU clock %s, load %s",
        bench.steps.size(), bench.delay, bench.waitFree ? "the player is free" : "the first picture",
        clock > 0 ? "readable" : "not readable", busy >= 0 ? "readable" : "not readable");
    return;
  }
  if (bench.index < -1 || !gx) return;
  if (bench.index == -1 && bench.waitFree) {
    const long freeAt = api.freeAt();
    if (freeAt >= 0) {
      bench.waitFree = false;
      bench.next = now + seconds(bench.delay);
      LOG("GPU bench: the player is free (frame %ld), the first step in %d s", freeAt, bench.delay);
    } else if (now >= bench.next) {
      bench.waitFree = false;
      LOG("GPU bench: the player did not get free in 300 s; measured anyway");
    }
  }
  if (bench.measuring && now >= bench.nextSample) {
    bench.nextSample = now + seconds(1);
    const double clock = gpuClockMhz(), busy = gpuBusyPercent();
    if (clock > 0) {
      bench.clockSum += clock;
      bench.clockCount++;
    }
    if (busy >= 0) {
      bench.busySum += busy;
      bench.busyCount++;
    }
    // the work: load times clock of the same moment (MHz the GPU was busy)
    if (clock > 0 && busy >= 0) {
      bench.workSum += clock * busy / 100.0;
      bench.workCount++;
    }
  }
  if (bench.waitFree || now < bench.next) return;
  if (bench.index == -1) {
    bench.keepScale = optScale.load();
    bench.keepSamples = optSamples.load();
    bench.keepAnisotropy = optAnisotropy.load();
    bench.keepInvalidate = optInvalidate.load();
    bench.keepTiled = optTiled.load();
    bench.index = 0;
    benchApply(bench.steps[0]);
    bench.next = now + seconds(5);
    return;
  }
  if (!bench.measuring) {
    bench.at = gx->Stats();
    bench.since = now;
    bench.runMs = 0;
    bench.runCount = 0;
    bench.clockSum = bench.busySum = bench.workSum = 0;
    bench.clockCount = bench.busyCount = bench.workCount = 0;
    bench.nextSample = now;
    bench.measuring = true;
    bench.next = now + seconds(10);
    return;
  }
  const re4gx::GxStats &s = gx->Stats();
  const BenchStep &step = bench.steps[static_cast<size_t>(bench.index)];
  const double secs = std::chrono::duration<double>(now - bench.since).count();
  const unsigned timed = s.gpuTimed - bench.at.gpuTimed, presentTimed = s.presentTimed - bench.at.presentTimed;
  const unsigned chunks = s.chunks - bench.at.chunks, checks = s.behindChecks - bench.at.behindChecks;
  const double pictures = secs > 0 ? chunks / secs : 0.0;
  char gpu[96] = "not measured", present[48] = "not measured", clock[24] = "-", busy[24] = "-", work[24] = "-";
  char behind[64] = "not checked";
  if (timed) {
    const double ms = (s.gpuMicros - bench.at.gpuMicros) / 1000.0 / timed;
    // longer than a picture's interval while the pictures keep coming: not the GPU's real time
    const bool plausible = pictures <= 0 || ms <= 1000.0 / pictures * 1.05;
    std::snprintf(gpu, sizeof(gpu), "%.1f ms (%u timed%s)", ms, timed,
                  plausible ? "" : ", NOT plausible: longer than a picture's interval");
  }
  if (presentTimed)
    std::snprintf(present, sizeof(present), "%.2f ms (%u timed)",
                  (s.presentMicros - bench.at.presentMicros) / 1000.0 / presentTimed, presentTimed);
  if (bench.clockCount) std::snprintf(clock, sizeof(clock), "%.0f MHz", bench.clockSum / bench.clockCount);
  if (bench.busyCount) std::snprintf(busy, sizeof(busy), "%.0f %%", bench.busySum / bench.busyCount);
  if (bench.workCount) std::snprintf(work, sizeof(work), "%.0f MHz", bench.workSum / bench.workCount);
  if (checks) {
    unsigned most = 0;
    for (unsigned n = 0; n < 9; n++)
      if (s.behindCount[n] != bench.at.behindCount[n]) most = n;
    std::snprintf(behind, sizeof(behind), "%.2f pictures on average, at most %u%s",
                  static_cast<double>(s.behindSum - bench.at.behindSum) / checks, most, most == 8 ? " or more" : "");
  }
  LOG("GPU bench %d/%zu: scale %d, %d samples%s, anisotropy %.0f, depth of field %s, copy targets forgotten %s: "
      "GPU %s per picture, "
      "present %s, %.1f pictures/s, %.1f presents/s, renderer CPU %.1f ms, GPU clock %s, load %s, "
      "work (load x clock) %s, GPU behind %s, %.0f draws per picture",
      bench.index + 1, bench.steps.size(), infoScale.load(), gx->Samples(),
      gx->Samples() > 1 ? (gx->TiledMsaa() ? " on the tiles" : " in own buffers") : "", gx->Anisotropy(),
      step.dof ? "on" : "off",
      gx->Invalidates() ? "yes" : "no", gpu, present, pictures, secs > 0 ? (s.presents - bench.at.presents) / secs : 0.0,
      bench.runCount ? bench.runMs / bench.runCount : 0.0, clock, busy, work, behind,
      chunks ? static_cast<double>(s.draws - bench.at.draws) / chunks : 0.0);
  bench.measuring = false;
  bench.index++;
  if (bench.index >= static_cast<int>(bench.steps.size())) {
    optScale = bench.keepScale;
    optSamples = bench.keepSamples;
    optAnisotropy = bench.keepAnisotropy;
    optInvalidate = bench.keepInvalidate;
    optTiled = bench.keepTiled;
    optSerial++;
    if (optDof.load() >= 0 && api.setDof) api.setDof(optDof.load());
    LOG("GPU bench done; the options from before the run are back");
    bench.index = -3;
    return;
  }
  benchApply(bench.steps[static_cast<size_t>(bench.index)]);
  bench.next = now + seconds(5);
}

// The picture as the screen shows it (the XFB, the game's own size) into a 24-bit BMP.
void SaveShot(const char *path) {
  unsigned w = 0, h = 0;
  if (!gx->Xfb(&w, &h) || !w || !h)
    return;
  if (!gxShotFbo || gxShotW != w || gxShotH != h) {
    if (!gxShotFbo)
      glGenFramebuffers(1, &gxShotFbo);
    if (gxShotColor)
      glDeleteTextures(1, &gxShotColor);
    glGenTextures(1, &gxShotColor);
    glBindTexture(GL_TEXTURE_2D, gxShotColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, gxShotFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gxShotColor, 0);
    gxShotW = w;
    gxShotH = h;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, gxShotFbo);
  glDisable(GL_SCISSOR_TEST);
  gx->Present(0, 0, static_cast<int>(w), static_cast<int>(h));
  std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
  glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // BMP rows go bottom up, as glReadPixels returns them
  const unsigned rowBytes = (w * 3 + 3) & ~3u;
  const uint32_t dataBytes = rowBytes * h, fileBytes = 54 + dataBytes;
  unsigned char head[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++)
      head[at + i] = static_cast<unsigned char>(v >> (8 * i));
  };
  put32(2, fileBytes);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  head[26] = 1;
  head[28] = 24;
  put32(34, dataBytes);
  if (FILE *f = std::fopen(path, "wb")) {
    std::fwrite(head, 1, sizeof(head), f);
    std::vector<unsigned char> row(rowBytes, 0);
    for (unsigned y = 0; y < h; y++) {
      for (unsigned x = 0; x < w; x++) {
        const unsigned char *p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
        row[x * 3] = p[2];
        row[x * 3 + 1] = p[1];
        row[x * 3 + 2] = p[0];
      }
      std::fwrite(row.data(), 1, rowBytes, f);
    }
    std::fclose(f);
  }
}

// Runs the chunks the core has finished and shows the latest display copy.
// False while there is nothing to show yet (the caller draws the bridge).
bool drawGx() {
  if (gxMode < 0) {
    const char *view = getenv("RE4_VIEW");
    gxMode = (view && std::string(view) == "bridge") || !api.gxAcquire ? 0 : 1;
    LOG("view: %s", gxMode ? "GX" : "bridge");
  }
  if (gxMode == 0) return false;
  if (gx && optSerial.load() != optApplied && !corePaused.load()) {
    // new picture options: the renderer anew, the core sends everything again
    gx->Release();
    gx.reset();
    if (api.gxDetach) api.gxDetach();
  }
  if (!gx) {
    optApplied = optSerial.load();
    // a new renderer counts from 0: the log's figures per chunk start over too
    gxWork = re4gx::GxStats();
    gxDraws = 0;
    gxRunMs = gxRunMax = 0;
    gxRunCount = gxFrames = 0;
    gxSince = std::chrono::steady_clock::now();
    int scale = optScale.load();
    if (scale < 1 || scale > 4) {
      const char *scaleText = getenv("RE4_GX_SCALE");
      scale = scaleText ? std::atoi(scaleText) : 0;
    }
    if (scale < 1 || scale > 4) {
      // automatic: the 336 rows of the picture to about the screen's rows
      const int rows = std::min(height, width * 9 / 16);
      scale = std::max(1, std::min(3, (rows + 168) / 336));
    }
    gx = std::make_unique<re4gx::GxRenderer>();
    re4gx::GxOptions options;
    options.samples = std::max(1, optSamples.load());
    options.anisotropy = static_cast<float>(std::max(1, optAnisotropy.load()));
    if (optInvalidate.load() < 0) {
      const char *invalidate = getenv("RE4_GX_INVALIDATE");
      optInvalidate = invalidate && *invalidate == '1' ? 1 : 0;
    }
    options.invalidate = optInvalidate.load() != 0;
    options.tiledMsaa = optTiled.load() != 0;
    gx->SetOptions(options);
    std::string error;
    if (!gx->Init(scale, &error)) {
      LOG("GX renderer: %s; the bridge draws", error.c_str());
      gx.reset();
      gxMode = 0;
      return false;
    }
    // linked shader programs from earlier runs (RE4_GX_PROGRAM_CACHE, set by CoreActivity)
    const char *cache = getenv("RE4_GX_PROGRAM_CACHE");
    bool cached = false;
    if (cache && *cache) {
      mkdir(cache, 0775);
      cached = gx->SetProgramCache(cache);
    }
    int maxSamples = 1;
    float maxAnisotropy = 1.0f;
    re4gx::GxRenderer::Limits(&maxSamples, &maxAnisotropy);
    infoMaxSamples = maxSamples;
    infoMaxAnisotropy = static_cast<int>(maxAnisotropy);
    infoScale = scale;
    infoSamples = gx->Samples();
    infoAnisotropy = static_cast<int>(gx->Anisotropy());
    LOG("GX renderer ready, EFB scale %d, %d samples (of %d)%s, anisotropy %.0f (of %.0f), program cache %s", scale,
        gx->Samples(), maxSamples, gx->Samples() > 1 ? (gx->TiledMsaa() ? " on the tiles" : " in own buffers") : "",
        gx->Anisotropy(), maxAnisotropy, cached ? cache : "off");
  }
  // one chunk per displayed frame: every picture the game makes is shown (a
  // renderer slower than the game slows the game down, the core waits for it,
  // instead of skipping pictures)
  const auto now = std::chrono::steady_clock::now();
  if (gxSince.time_since_epoch().count() == 0) gxSince = now;
  gxFrames++;
  // diagnosis (RE4_GX_DUMP_DIR, RE4_GX_DUMP_EVERY seconds, at most 6): the
  // renderer detaches, so the next chunk starts over with the full state and
  // every texture, and that chunk goes to a file (re4_gx_frame_io.h, drawn in
  // the cloud with gx_chunk_render)
  if (gxDumpEvery < 0) {
    const char *dir = getenv("RE4_GX_DUMP_DIR");
    const char *every = getenv("RE4_GX_DUMP_EVERY");
    gxDumpDir = dir ? dir : "";
    gxDumpEvery = gxDumpDir.empty() ? 0 : (every ? std::max(5, std::atoi(every)) : 20);
    if (gxDumpEvery) {
      mkdir(gxDumpDir.c_str(), 0775);
      gxDumpNext = now + std::chrono::seconds(gxDumpEvery);
      LOG("GX dumps every %d s into %s", gxDumpEvery, gxDumpDir.c_str());
    }
  }
  if (gxDumpEvery && !gxDumpWanted && !gxDumpSeriesLeft && gxDumpCount < 6 && now >= gxDumpNext) {
    api.gxDetach();
    gxDumpWanted = true;
    gxDumpNext = now + std::chrono::seconds(gxDumpEvery);
  }
  if (!gxDumpWanted && !gxDumpSeriesLeft && gxDumpCount < 40) {
    std::lock_guard<std::mutex> lock(gxDumpLock);
    if (gxDumpAsked) {
      gxDumpAsked--;
      if (gxDumpDir.empty()) {
        gxDumpDir = gxDumpAskedDir;
        mkdir(gxDumpDir.c_str(), 0775);
      }
      if (!gxDumpDir.empty()) {
        api.gxDetach();
        gxDumpWanted = true;
        gxDumpSeriesWanted = true;
        LOG("GX dump asked for (controller)");
        // where the player stands, for checks on the PC (RE4_TEST_PLACE)
        for (const Re4CoreModel &mdl : frame.models) {
          if (mdl.manager == 0 && mdl.id == 0) {
            LOG("R3: room 0x%03x, player at %.0f %.0f %.0f, camera %.0f %.0f %.0f -> %.0f %.0f %.0f", frame.room,
                mdl.matrix[3], mdl.matrix[7], mdl.matrix[11], frame.eye[0], frame.eye[1], frame.eye[2],
                frame.target[0], frame.target[1], frame.target[2]);
            break;
          }
        }
      }
    }
  }
  // a hit on the player: the settings
  if (gxHitDumps < 0) {
    const char *hits = getenv("RE4_GX_DUMP_ON_HIT");
    const char *folder = getenv("RE4_GX_DUMP_FOLDER");
    gxHitDumps = hits && folder && *folder ? std::max(0, std::atoi(hits)) : 0;
    if (folder && *folder && gxDumpDir.empty()) {
      gxDumpDir = folder;
      mkdir(gxDumpDir.c_str(), 0775);
    }
  }
  if (const Re4GxChunk *chunk = api.gxAcquire()) {
    char path[512];
    path[0] = 0;
    if (chunk->flags & RE4_GX_CHUNK_MARK) {
      LOG("GX: a hit on the player marked (chunk %u)", gxChunks);
      gxHitCount++;
      if (gxHitDumps > 0) {
        gxHitDumps--;
        gxHitSeriesPending = true;
      }
      if (!gxDumpDir.empty()) {
        gxHitShotsLeft = kGxHitShots;  // from this picture on
        gxHitShotPart = 0;
      }
    } else if (gxHitSeriesPending && (chunk->flags & RE4_GX_CHUNK_RESET)) {
      gxHitSeriesPending = false;
      if (!gxDumpSeriesLeft && !gxDumpWanted && !gxDumpDir.empty() && gxDumpCount < 40) {
        // this chunk starts over: a series from here (continued below)
        gxDumpSeriesIndex = gxDumpCount++;
        gxDumpSeriesPart = 0;
        gxDumpSeriesLeft = kGxHitSeries;
        gxDumpSeriesSequence = chunk->sequence - 1;
      }
    }
    if (gxDumpWanted && (chunk->flags & RE4_GX_CHUNK_RESET)) {
      gxDumpWanted = false;
      if (gxDumpSeriesWanted) {
        gxDumpSeriesWanted = false;
        gxDumpSeriesIndex = gxDumpCount++;
        gxDumpSeriesPart = 0;
        gxDumpSeriesLeft = kGxDumpSeries;
      } else {
        std::snprintf(path, sizeof(path), "%s/gx-geraet-%02u.bin", gxDumpDir.c_str(), gxDumpCount++);
      }
    } else if (gxDumpSeriesLeft && chunk->sequence != gxDumpSeriesSequence + 1) {
      LOG("GX dump series %02u ends after %u chunks (chunk sequence %u after %u)", gxDumpSeriesIndex,
          gxDumpSeriesPart, chunk->sequence, gxDumpSeriesSequence);
      gxDumpSeriesLeft = 0;
    }
    // a series also keeps each picture as this GPU drew it (gx-geraet-NN-KK.bmp):
    // flicker the cloud renderer does not show is then this device's (FILES, M9)
    char seriesShot[512];
    seriesShot[0] = 0;
    if (gxDumpSeriesLeft) {
      std::snprintf(seriesShot, sizeof(seriesShot), "%s/gx-geraet-%02u-%02u.bmp", gxDumpDir.c_str(),
                    gxDumpSeriesIndex, gxDumpSeriesPart);
      std::snprintf(path, sizeof(path), "%s/gx-geraet-%02u-%02u.bin", gxDumpDir.c_str(), gxDumpSeriesIndex,
                    gxDumpSeriesPart++);
      gxDumpSeriesSequence = chunk->sequence;
      gxDumpSeriesLeft--;
    }
    if (path[0]) {
      if (FILE *f = std::fopen(path, "wb")) {
        const bool ok = re4gxio::Write(f, *chunk, gxChunks);
        std::fclose(f);
        LOG("GX dump %s (%u draws, chunk %u)%s", path, chunk->draws, gxChunks, ok ? "" : ": write failed");
      }
    }
    gx->Run(*chunk);
    api.gxRelease(chunk);
    if (seriesShot[0]) {
      SaveShot(seriesShot);
    }
    if (gxHitShotsLeft) {
      char shot[512];
      std::snprintf(shot, sizeof(shot), "%s/treffer-%02u-%02u.bmp", gxDumpDir.c_str(), gxHitCount,
                    gxHitShotPart++);
      SaveShot(shot);
      gxHitShotsLeft--;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - now).count();
    gxRunMs += ms;
    gxRunMax = std::max(gxRunMax, ms);
    gxRunCount++;
    if (bench.measuring) {
      bench.runMs += ms;
      bench.runCount++;
    }
    if ((++gxChunks % 300) == 1) {
      const auto &s = gx->Stats();
      const double seconds = std::chrono::duration<double>(now - gxSince).count();
      LOG("GX: %u chunks, %u draws (%u per chunk), run %.1f ms (max %.1f), shown %.1f/s, %u programs, %u textures, "
          "%u copies, %u held pictures, %u missing, %u shader errors, %u inexact dst alpha, %u programs from the cache, "
          "base vertex %s, %u autofocus passes",
          s.chunks, s.draws, gxRunCount ? (s.draws - gxDraws) / gxRunCount : 0, gxRunCount ? gxRunMs / gxRunCount : 0.0,
          gxRunMax, seconds > 0 ? gxFrames / seconds : 0.0, s.programs, s.textures, s.textureCopies, s.heldCopies,
          s.missingTextures, s.shaderErrors, s.dstAlphaApprox, s.programsLoaded, gx->BaseVertex() ? "yes" : "no",
          s.autofocus);
      // the GL work per chunk since the last line, and the GPU time when the driver measures it
      if (gxRunCount) {
        const double n = gxRunCount;
        const unsigned timed = s.gpuTimed - gxWork.gpuTimed;
        char gpu[32] = "not measured";
        if (timed)
          std::snprintf(gpu, sizeof(gpu), "%.1f ms", (s.gpuMicros - gxWork.gpuMicros) / 1000.0 / timed);
        LOG("GX work per chunk: %.0f uniform uploads (%.0f KB), %.0f program switches, %.0f texture binds, "
            "%.0f state changes, %u stream waits, GPU %s",
            (s.uniformUploads - gxWork.uniformUploads) / n, (s.uniformBytes - gxWork.uniformBytes) / n / 1024.0,
            (s.programSwitches - gxWork.programSwitches) / n, (s.textureBinds - gxWork.textureBinds) / n,
            (s.stateChanges - gxWork.stateChanges) / n, s.streamWaits - gxWork.streamWaits, gpu);
      }
      gxWork = s;
      gxDraws = s.draws;
      gxRunMs = gxRunMax = 0;
      gxRunCount = gxFrames = 0;
      gxSince = now;
    }
  }
  benchTick(now);
  unsigned xw = 0, xh = 0;
  if (!gx->Xfb(&xw, &xh)) return false;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // the menu's picture format (1: 4:3, 0: 16:9), else RE4_GX_ASPECT
  const char *aspect = getenv("RE4_GX_ASPECT");
  const int format = optAspect.load();
  if (format == 1 || (format < 0 && aspect && std::string(aspect) == "4:3")) {
    // the whole picture: 640 wide in a 4:3 frame of 480 lines (the XFB holds xh of them)
    int boxW = width, boxH = width * 3 / 4;
    if (boxH > height) {
      boxH = height;
      boxW = height * 4 / 3;
    }
    const int picH = boxH * static_cast<int>(xh) / 480;
    gx->Present((width - boxW) / 2, (height - picH) / 2, boxW, picH);
  } else {
    // full screen: the game letterboxes everything (rows 56..391 of its 448,
    // on a 4:3 television exactly 16:9); those rows fill a 16:9 box
    const unsigned top = xh * 56 / 448, rows = xh * 336 / 448;
    int boxW = width, boxH = width * 9 / 16;
    if (boxH > height) {
      boxH = height;
      boxW = height * 16 / 9;
    }
    gx->Present((width - boxW) / 2, (height - boxH) / 2, boxW, boxH, top, rows);
  }
  return true;
}

re4::CoreScene *currentScene() {
  std::lock_guard<std::mutex> lock(sceneLock);
  return scene.get();
}

// A crash (M11, the testers' error log): where it happened into the log and
// into the crash file of the log folder (CoreBridge.setCrashFile), then the
// system's handler as before (its crash report). Signal-safe as far as it
// goes: write() into the file opened in advance; the log call and dladdr are
// what crash handlers of Android apps commonly use.
int crashFd = -1;
struct sigaction oldActions[NSIG];
const int kCrashSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};

void crashLine(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void crashLine(const char *fmt, ...) {
  char line[320];
  va_list ap;
  va_start(ap, fmt);
  const int n = std::vsnprintf(line, sizeof(line) - 1, fmt, ap);
  va_end(ap);
  __android_log_print(ANDROID_LOG_ERROR, "RE4Crash", "%s", line);
  if (crashFd >= 0 && n > 0) {
    line[std::min<int>(n, sizeof(line) - 2)] = '\n';
    (void)!write(crashFd, line, static_cast<size_t>(std::min<int>(n, sizeof(line) - 2) + 1));
  }
}

void crashWhere(const char *what, uintptr_t address) {
  Dl_info info{};
  if (address && dladdr(reinterpret_cast<void *>(address), &info) && info.dli_fname) {
    const char *file = std::strrchr(info.dli_fname, '/');
    crashLine("  %s 0x%08lx  %s+0x%lx  %s", what, static_cast<unsigned long>(address), file ? file + 1 : info.dli_fname,
              static_cast<unsigned long>(address - reinterpret_cast<uintptr_t>(info.dli_fbase)),
              info.dli_sname ? info.dli_sname : "");
  } else {
    crashLine("  %s 0x%08lx", what, static_cast<unsigned long>(address));
  }
}

// pc, lr and the words on the stack that point into a library (a guess at
// the return addresses), from a signal's context
void crashContext(void *context) {
#if defined(__arm__) || defined(__aarch64__)
  const auto *uc = static_cast<const ucontext_t *>(context);
#if defined(__arm__)
  const uintptr_t pc = uc->uc_mcontext.arm_pc, lr = uc->uc_mcontext.arm_lr, sp = uc->uc_mcontext.arm_sp;
#else
  // 64 bit (milestone 12): x30 is the link register
  const uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30], sp = uc->uc_mcontext.sp;
#endif
  crashWhere("pc", pc);
  crashWhere("lr", lr);
  // the words above sp as far as they can be read: the game's stacks end at their mapping, and on
  // 64 bit nothing readable follows (a read past the end crashed the handler itself, 18.10.2026);
  // process_vm_readv stops at the first chunk it cannot read instead of faulting
  static uintptr_t word[512];
  struct iovec local {word, sizeof(word)};
  struct iovec remote[16];
  for (int c = 0; c < 16; c++) {
    remote[c].iov_base = reinterpret_cast<void *>(sp + c * sizeof(word) / 16);
    remote[c].iov_len = sizeof(word) / 16;
  }
  const ssize_t got = process_vm_readv(getpid(), &local, 1, remote, 16, 0);
  const int words = got > 0 ? static_cast<int>(got / sizeof(uintptr_t)) : 0;
  int found = 0;
  for (int i = 0; i < words && found < 24; i++) {
    Dl_info d{};
    if (word[i] > 0x10000 && dladdr(reinterpret_cast<void *>(word[i]), &d) && d.dli_fname &&
        std::strstr(d.dli_fname, ".so")) {
      char what[24];
      std::snprintf(what, sizeof(what), "sp+%03x", static_cast<unsigned>(i * sizeof(uintptr_t)));
      crashWhere(what, word[i]);
      found++;
    }
  }
#else
  (void)context;
#endif
}

void onCrash(int sig, siginfo_t *info, void *context) {
  static volatile sig_atomic_t inside = 0;
  if (!inside) {
    inside = 1;
    crashLine("crash: signal %d (%s), fault address %p, thread %d", sig, strsignal(sig), info ? info->si_addr : nullptr,
              static_cast<int>(gettid()));
    crashContext(context);
    if (crashFd >= 0) fsync(crashFd);
  }
  // the handler before (the system's crash report), with the signal again
  for (std::size_t i = 0; i < sizeof(kCrashSignals) / sizeof(kCrashSignals[0]); i++)
    sigaction(kCrashSignals[i], &oldActions[kCrashSignals[i]], nullptr);
  if (sig == SIGABRT) raise(sig);
}

// The game stands without a pause (CoreBridge.reportHang): the game thread
// says where it is (SIGUSR2 to that thread), the game runs on.
void onHangSample(int, siginfo_t *, void *context) {
  crashLine("hang: the game thread (%d) is at", static_cast<int>(gettid()));
  crashContext(context);
}

void installCrashHandler() {
  struct sigaction action {};
  action.sa_sigaction = onCrash;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (int sig : kCrashSignals) sigaction(sig, &action, &oldActions[sig]);
  struct sigaction sample {};
  sample.sa_sigaction = onHangSample;
  sample.sa_flags = SA_SIGINFO | SA_RESTART;
  sigemptyset(&sample.sa_mask);
  sigaction(SIGUSR2, &sample, nullptr);
}

}  // namespace

// The mappings below 4 GB when the core loaded (64 bit; "" on 32 bit)
extern "C" JNIEXPORT jstring JNICALL Java_com_ast_re4decomp_CoreBridge_addressMap(JNIEnv *env, jclass) {
#if defined(__aarch64__)
  return env->NewStringUTF(gAddressMap.c_str());
#else
  return env->NewStringUTF("");
#endif
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  JNIEnv *env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) env = nullptr;
  loadCore(env);
  installCrashHandler();
  return JNI_VERSION_1_6;
}

// Starts the game once per process (the core cannot be restarted). Returns ""
// when it runs, else the reason.
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_CoreBridge_start(JNIEnv *env, jclass, jstring gameRoot, jstring settings) {
  const auto root = text(env, gameRoot);
  // the core's test switches: environment variables read when the game uses them
  if (api.state && api.state() == RE4_CORE_IDLE) {
    // defaults of the app: no debug displays of the debug build (RE4_DEBUG_HUD=1 shows them);
    // "New Game" starts the game without the debug build's start menu (room selection,
    // patch 0043; wish of the project owner, 17.10.2026: rooms are chosen in the debug
    // menu, SELECT held). RE4_ROOM_MENU=1 shows it (test-core-app.ps1 -StartRoom).
    setenv("RE4_DEBUG_HUD", "0", 0);
    setenv("RE4_ROOM_MENU", "0", 0);
    std::string list = text(env, settings);
    std::size_t pos = 0;
    while (pos < list.size()) {
      auto end = list.find(';', pos);
      if (end == std::string::npos) end = list.size();
      const auto item = list.substr(pos, end - pos);
      const auto eq = item.find('=');
      if (eq != std::string::npos && eq > 0 && item.compare(0, 4, "RE4_") == 0) {
        setenv(item.substr(0, eq).c_str(), item.substr(eq + 1).c_str(), 1);
        LOG("setting %s", item.c_str());
      }
      pos = end + 1;
    }
  }
  {
    std::lock_guard<std::mutex> lock(sceneLock);
    if (!scene) scene = std::make_unique<re4::CoreScene>(root);
  }
  std::string problem = api.error;
  if (problem.empty() && api.state() == RE4_CORE_IDLE) {
    // the disc folders of the import: <root>/disc1, <root>/disc2
    if (!api.reserve(root.c_str(), "") || !api.start()) problem = api.message();
  }
  if (problem.empty() && api.state() == RE4_CORE_FAILED) problem = api.message();
  return env->NewStringUTF(problem.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_pad(
    JNIEnv *, jclass, jint buttons, jint stickX, jint stickY, jint substickX, jint substickY,
    jint triggerL, jint triggerR) {
  if (!api.pad) return;
  auto clampS = [](jint v) { return static_cast<std::int8_t>(v < -127 ? -127 : v > 127 ? 127 : v); };
  auto clampU = [](jint v) { return static_cast<std::uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
  Re4CorePad pad{static_cast<std::uint16_t>(buttons), clampS(stickX), clampS(stickY),
                 clampS(substickX), clampS(substickY), clampU(triggerL), clampU(triggerR)};
  api.pad(&pad);
}

// The game's rumble motor (re4_core_rumble) for the vibrator (CoreActivity).
extern "C" JNIEXPORT jint JNICALL Java_com_ast_re4decomp_CoreBridge_rumble(JNIEnv *, jclass) {
  return api.rumble ? static_cast<jint>(api.rumble()) : 0;
}

// The game's sound for the app's audio thread (CoreAudio): `frames` stereo
// frames at 32 kHz into `pcm`; the frames that came from the game.
extern "C" JNIEXPORT jint JNICALL Java_com_ast_re4decomp_CoreBridge_audio(JNIEnv *env, jclass,
                                                                          jshortArray pcm, jint frames) {
  const jsize length = env->GetArrayLength(pcm);
  if (frames <= 0 || frames * 2 > length) return 0;
  auto *out = static_cast<jshort *>(env->GetPrimitiveArrayCritical(pcm, nullptr));
  if (!out) return 0;
  int got = 0;
  if (api.audio) {
    got = api.audio(reinterpret_cast<int16_t *>(out), frames);
  } else {
    std::fill(out, out + frames * 2, 0);
  }
  env->ReleasePrimitiveArrayCritical(pcm, out, 0);
  return got;
}

// A vsync of the display (CoreActivity, the Choreographer's frame time in ns of
// System.nanoTime = CLOCK_MONOTONIC): the core's retraces follow the display.
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_vsync(JNIEnv *, jclass, jlong ns) {
  if (api.vsync) api.vsync(static_cast<int64_t>(ns));
}

// The room the game is in (for the Java room loader), -1 outside a room.
extern "C" JNIEXPORT jint JNICALL Java_com_ast_re4decomp_CoreBridge_wantedRoom(JNIEnv *, jclass) {
  auto *s = currentScene();
  return s ? s->wantedRoom() : -1;
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_roomPrepared(
    JNIEnv *env, jclass, jint room, jstring decodedPath, jstring error) {
  if (auto *s = currentScene())
    s->roomPrepared(static_cast<std::uint16_t>(room), text(env, decodedPath), text(env, error));
}

extern "C" JNIEXPORT jstring JNICALL Java_com_ast_re4decomp_CoreBridge_status(JNIEnv *env, jclass) {
  std::string result;
  if (!api.error.empty()) {
    result = api.error;
  } else {
    static const char *states[] = {"not started", "running", "ended", "failed"};
    const int st = api.state();
    result = std::string("core ") + (st >= 0 && st <= 3 ? states[st] : "?") + ": " + api.message();
    if (auto *s = currentScene()) result += "\n" + s->status();
    if (api.audioStats) {
      unsigned a[5] = {};
      api.audioStats(a);
      char line[160];
      std::snprintf(line, sizeof(line), "\nsound: %u blocks, %u ms waiting, %u lost, %u ran dry, %u skips",
                    a[0], a[1] / 32, a[2], a[3], a[4]);
      result += line;
    }
    char tail[2048];
    api.logTail(tail, sizeof(tail));
    // the last few log lines
    std::string lines(tail);
    std::size_t pos = lines.size();
    for (int n = 0; n < 7 && pos > 0; ++n) {
      const auto nl = lines.rfind('\n', pos - 1);
      pos = nl == std::string::npos ? 0 : nl;
    }
    result += "\n" + lines.substr(pos == 0 ? 0 : pos + 1);
  }
  return env->NewStringUTF(result.c_str());
}

// One GX dump of the next picture (diagnosis, see drawGx); `folder` is used
// when no test switch named one.
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_gxDump(JNIEnv *env, jclass, jstring folder) {
  std::lock_guard<std::mutex> lock(gxDumpLock);
  gxDumpAskedDir = text(env, folder);
  gxDumpAsked++;
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_surfaceCreated(JNIEnv *, jclass) {
  if (auto *s = currentScene()) s->contextCreated();
  lastDraw = {};
  // a new GL context: the renderer's objects are gone; the core starts over
  // (full state, every texture again) once chunks are taken again
  gx.reset();
  if (api.gxDetach) api.gxDetach();
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_resize(JNIEnv *, jclass, jint w,
                                                                          jint h) {
  width = w > 0 ? w : 1;
  height = h > 0 ? h : 1;
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_draw(JNIEnv *, jclass) {
  auto *s = currentScene();
  if (!s) return;
  if (api.acquire) {
    if (const Re4CoreSnapshot *snapshot = api.acquire()) {
      re4::CopyCoreFrame(*snapshot, frame);
      api.release(snapshot);
    }
  }
  const auto now = std::chrono::steady_clock::now();
  const double seconds = lastDraw.time_since_epoch().count() == 0
                             ? 0.0
                             : std::chrono::duration<double>(now - lastDraw).count();
  lastDraw = now;
  if (drawGx()) return;
  try {
    s->draw(frame, width, height, seconds);
  } catch (const std::exception &e) {
    LOG("draw: %s", e.what());
  }
}

// --- the app's menu (M11) ----------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL Java_com_ast_re4decomp_CoreBridge_hasMenu(JNIEnv *, jclass) {
  return api.pause && api.cardList ? JNI_TRUE : JNI_FALSE;
}

// The game stops at its next frame (on) or runs on; true when it stands.
extern "C" JNIEXPORT jboolean JNICALL Java_com_ast_re4decomp_CoreBridge_pause(JNIEnv *, jclass, jboolean on,
                                                                             jint waitMs) {
  corePaused = on == JNI_TRUE;
  if (!api.pause) return JNI_FALSE;
  return api.pause(on ? 1 : 0, waitMs) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_debugMenu(JNIEnv *, jclass) {
  if (api.debugMenu) api.debugMenu();
}

// The depth of field switch of the menu (cores since 17.10.2026; older ones keep it on).
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_setDof(JNIEnv *, jclass, jboolean on) {
  optDof = on ? 1 : 0;
  if (api.setDof) api.setDof(on ? 1 : 0);
}

std::mutex cardLock;
std::string cardNote;

// The card's files, one row each: index, name, blocks, save, intact, room,
// chapter, times saved, play time, year, month, day, hour, minute, the two
// comment lines (tab-separated); null when the game uses the card or there is
// none (cardNote says which).
extern "C" JNIEXPORT jobjectArray JNICALL Java_com_ast_re4decomp_CoreBridge_cardList(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(cardLock);
  static Re4CardFile files[127];
  const int n = api.cardList ? api.cardList(files, 127) : RE4_CARD_NOCARD;
  if (n < 0) {
    cardNote = n == RE4_CARD_BUSY ? "busy" : "nocard";
    return nullptr;
  }
  jobjectArray rows = env->NewObjectArray(n, env->FindClass("java/lang/String"), nullptr);
  for (int i = 0; i < n; i++) {
    const Re4CardFile &f = files[i];
    char row[256];
    std::snprintf(row, sizeof(row), "%d\t%s\t%u\t%d\t%d\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%s\t%s", f.index,
                  f.name, f.blocks, f.save, f.intact, f.room, f.chapter, f.saveCount, f.playTime, f.year, f.month,
                  f.day, f.hour, f.minute, f.comment[0], f.comment[1]);
    for (char *c = row; *c; c++)  // the comments of other games may hold anything
      if (static_cast<unsigned char>(*c) >= 0x80 || (*c < 0x20 && *c != '\t')) *c = '?';
    jstring s = env->NewStringUTF(row);
    env->SetObjectArrayElement(rows, i, s);
    env->DeleteLocalRef(s);
  }
  return rows;
}

extern "C" JNIEXPORT jint JNICALL Java_com_ast_re4decomp_CoreBridge_cardFreeBlocks(JNIEnv *, jclass) {
  std::lock_guard<std::mutex> lock(cardLock);
  return api.cardFree ? api.cardFree() : RE4_CARD_NOCARD;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_ast_re4decomp_CoreBridge_cardFileName(JNIEnv *env, jclass,
                                                                                   jint index) {
  std::lock_guard<std::mutex> lock(cardLock);
  char name[80] = "";
  if (!api.cardName || api.cardName(index, name, sizeof(name)) != RE4_CARD_OK) return nullptr;
  return env->NewStringUTF(name);
}

// The .gci of file `index` (GameCube byte order), null on an error (cardNote).
extern "C" JNIEXPORT jbyteArray JNICALL Java_com_ast_re4decomp_CoreBridge_cardExport(JNIEnv *env, jclass,
                                                                                    jint index) {
  std::lock_guard<std::mutex> lock(cardLock);
  if (!api.cardExport) return nullptr;
  std::vector<unsigned char> out(RE4_GCI_HEADER + 251 * RE4_CARD_BLOCK);
  char note[256] = "";
  const int bytes = api.cardExport(index, out.data(), static_cast<unsigned>(out.size()), note, sizeof(note));
  cardNote = note;
  if (bytes <= 0) return nullptr;
  jbyteArray result = env->NewByteArray(bytes);
  env->SetByteArrayRegion(result, 0, bytes, reinterpret_cast<const jbyte *>(out.data()));
  return result;
}

// A .gci onto the card: RE4_CARD_OK (0) or an error (re4_card_files.h; cardNote).
extern "C" JNIEXPORT jint JNICALL Java_com_ast_re4decomp_CoreBridge_cardImport(JNIEnv *env, jclass, jbyteArray gci,
                                                                              jboolean replace) {
  std::lock_guard<std::mutex> lock(cardLock);
  if (!api.cardImport) return RE4_CARD_NOCARD;
  const jsize size = env->GetArrayLength(gci);
  std::vector<unsigned char> in(static_cast<size_t>(size));
  env->GetByteArrayRegion(gci, 0, size, reinterpret_cast<jbyte *>(in.data()));
  char note[256] = "";
  const int r = api.cardImport(in.data(), static_cast<unsigned>(size), replace ? 1 : 0, note, sizeof(note));
  cardNote = note;
  return r;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_ast_re4decomp_CoreBridge_cardNote(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(cardLock);
  return env->NewStringUTF(cardNote.c_str());
}

// The picture options: scale 0 (automatic) or 1-4, samples 1/2/4, anisotropy
// 1-16, aspect 0 (16:9) or 1 (4:3). Applied at the next picture while the game runs.
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_setGraphics(JNIEnv *, jclass, jint scale,
                                                                               jint samples, jint anisotropy,
                                                                               jint aspect) {
  const bool rebuild = optScale.load() != scale || optSamples.load() != samples || optAnisotropy.load() != anisotropy;
  optScale = scale;
  optSamples = samples;
  optAnisotropy = anisotropy;
  optAspect = aspect;
  if (rebuild) optSerial++;
  LOG("picture options: scale %d, %d samples, anisotropy %d, %s", scale, samples, anisotropy,
      aspect == 1 ? "4:3" : "16:9");
}

// {most samples, strongest anisotropy, scale, samples, anisotropy in use}; null before the renderer started.
extern "C" JNIEXPORT jintArray JNICALL Java_com_ast_re4decomp_CoreBridge_graphicsInfo(JNIEnv *env, jclass) {
  if (!infoMaxSamples.load()) return nullptr;
  const jint v[5] = {infoMaxSamples.load(), infoMaxAnisotropy.load(), infoScale.load(), infoSamples.load(),
                     infoAnisotropy.load()};
  jintArray result = env->NewIntArray(5);
  env->SetIntArrayRegion(result, 0, 5, v);
  return result;
}

// The file a crash writes where it happened (the error log's folder).
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_setCrashFile(JNIEnv *env, jclass,
                                                                                jstring path) {
  const auto file = text(env, path);
  const int fd = file.empty() ? -1 : open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  const int old = crashFd;
  crashFd = fd;
  if (old >= 0) close(old);
}

// Game frames so far (one per PADRead), -1 without the core's counter.
extern "C" JNIEXPORT jlong JNICALL Java_com_ast_re4decomp_CoreBridge_gameFrames(JNIEnv *, jclass) {
  return api.frameCount ? static_cast<jlong>(api.frameCount()) : -1;
}

// The game stands without a pause: three samples of where its thread is, 200 ms apart
// (a loop shows as places in one function, a wait as the same place).
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_reportHang(JNIEnv *, jclass, jint seconds) {
  const int tid = api.gameThread ? api.gameThread() : 0;
  crashLine("hang: no game frame for %d s (frame %u)", static_cast<int>(seconds),
            api.frameCount ? api.frameCount() : 0u);
  if (tid <= 0) return;
  for (int i = 0; i < 3; i++) {
    syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
    usleep(200000);
  }
  if (crashFd >= 0) fsync(crashFd);
}
