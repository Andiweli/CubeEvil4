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
#include "re4_gx_frame.h"
#include "re4_gx_frame_io.h"

#include <android/log.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

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
  const Re4CoreSnapshot *(*acquire)(void){};
  void (*release)(const Re4CoreSnapshot *){};
  int (*logTail)(char *, int){};
  const Re4GxChunk *(*gxAcquire)(void){};
  void (*gxRelease)(const Re4GxChunk *){};
  void (*gxDetach)(void){};
} api;

template <class F> bool bind(F &slot, const char *name) {
  slot = reinterpret_cast<F>(dlsym(api.main, name));
  if (!slot && api.error.empty()) api.error = std::string("missing in libre4_main.so: ") + name;
  return slot != nullptr;
}

void loadCore() {
  // libre4_main.so brings libre4_platform.so and libc++_shared.so (DT_NEEDED);
  // the module libraries are loaded by name later (re4_module_load).
  // RTLD_LOCAL: the game's symbols must not serve libraries loaded later
  api.main = dlopen("libre4_main.so", RTLD_NOW | RTLD_LOCAL);
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
  ok = bind(api.acquire, "re4_core_acquire") && ok;
  ok = bind(api.release, "re4_core_release") && ok;
  ok = bind(api.logTail, "re4_core_log_tail") && ok;
  ok = bind(api.gxAcquire, "re4_core_gx_acquire") && ok;
  ok = bind(api.gxRelease, "re4_core_gx_release") && ok;
  ok = bind(api.gxDetach, "re4_core_gx_detach") && ok;
  if (!ok) {
    LOG("%s", api.error.c_str());
    api.main = nullptr;
    return;
  }
  // MEM1 before anything else maps memory there (GL drivers, ART growth)
  if (!api.mem1Init()) {
    api.error = "MEM1 0x80000000..0x83000000 is taken in this process";
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

// GX view (GL thread): on unless RE4_VIEW=bridge; EFB scale from RE4_GX_SCALE
// (default 2: 1280x1056)
std::unique_ptr<re4gx::GxRenderer> gx;
int gxMode = -1;  // -1 undecided, 0 bridge, 1 GX
unsigned gxChunks = 0;
// timing of the GX view since the last log line
std::chrono::steady_clock::time_point gxSince;
double gxRunMs = 0, gxRunMax = 0;
unsigned gxRunCount = 0, gxFrames = 0, gxDraws = 0;
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

// Runs the chunks the core has finished and shows the latest display copy.
// False while there is nothing to show yet (the caller draws the bridge).
bool drawGx() {
  if (gxMode < 0) {
    const char *view = getenv("RE4_VIEW");
    gxMode = (view && std::string(view) == "bridge") || !api.gxAcquire ? 0 : 1;
    LOG("view: %s", gxMode ? "GX" : "bridge");
  }
  if (gxMode == 0) return false;
  if (!gx) {
    const char *scaleText = getenv("RE4_GX_SCALE");
    int scale = scaleText ? std::atoi(scaleText) : 2;
    if (scale < 1 || scale > 4) scale = 2;
    gx = std::make_unique<re4gx::GxRenderer>();
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
    LOG("GX renderer ready, EFB scale %d, program cache %s", scale, cached ? cache : "off");
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
  if (gxDumpEvery && !gxDumpWanted && gxDumpCount < 6 && now >= gxDumpNext) {
    api.gxDetach();
    gxDumpWanted = true;
    gxDumpNext = now + std::chrono::seconds(gxDumpEvery);
  }
  if (!gxDumpWanted && gxDumpCount < 40) {
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
        LOG("GX dump asked for (controller)");
      }
    }
  }
  if (const Re4GxChunk *chunk = api.gxAcquire()) {
    if (gxDumpWanted && (chunk->flags & RE4_GX_CHUNK_RESET)) {
      gxDumpWanted = false;
      char path[512];
      std::snprintf(path, sizeof(path), "%s/gx-geraet-%02u.bin", gxDumpDir.c_str(), gxDumpCount++);
      if (FILE *f = std::fopen(path, "wb")) {
        const bool ok = re4gxio::Write(f, *chunk, gxChunks);
        std::fclose(f);
        LOG("GX dump %s (%u draws, chunk %u)%s", path, chunk->draws, gxChunks, ok ? "" : ": write failed");
      }
    }
    gx->Run(*chunk);
    api.gxRelease(chunk);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - now).count();
    gxRunMs += ms;
    gxRunMax = std::max(gxRunMax, ms);
    gxRunCount++;
    if ((++gxChunks % 300) == 1) {
      const auto &s = gx->Stats();
      const double seconds = std::chrono::duration<double>(now - gxSince).count();
      LOG("GX: %u chunks, %u draws (%u per chunk), run %.1f ms (max %.1f), shown %.1f/s, %u programs, %u textures, "
          "%u copies, %u missing, %u shader errors, %u inexact dst alpha, %u programs from the cache, base vertex %s",
          s.chunks, s.draws, gxRunCount ? (s.draws - gxDraws) / gxRunCount : 0, gxRunCount ? gxRunMs / gxRunCount : 0.0,
          gxRunMax, seconds > 0 ? gxFrames / seconds : 0.0, s.programs, s.textures, s.textureCopies,
          s.missingTextures, s.shaderErrors, s.dstAlphaApprox, s.programsLoaded, gx->BaseVertex() ? "yes" : "no");
      gxDraws = s.draws;
      gxRunMs = gxRunMax = 0;
      gxRunCount = gxFrames = 0;
      gxSince = now;
    }
  }
  unsigned xw = 0, xh = 0;
  if (!gx->Xfb(&xw, &xh)) return false;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  const char *aspect = getenv("RE4_GX_ASPECT");
  if (aspect && std::string(aspect) == "4:3") {
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

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *, void *) {
  loadCore();
  return JNI_VERSION_1_6;
}

// Starts the game once per process (the core cannot be restarted). Returns ""
// when it runs, else the reason.
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_CoreBridge_start(JNIEnv *env, jclass, jstring gameRoot, jstring settings) {
  const auto root = text(env, gameRoot);
  // the core's test switches: environment variables read when the game uses them
  if (api.state && api.state() == RE4_CORE_IDLE) {
    // defaults of the app: no debug displays of the debug build (RE4_DEBUG_HUD=1 shows them)
    setenv("RE4_DEBUG_HUD", "0", 0);
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
