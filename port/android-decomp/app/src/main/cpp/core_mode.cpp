// Core mode (armeabi-v7a): JNI of com.ast.re4decomp.CoreBridge. Loads the
// game core (libre4_main.so with libre4_platform.so and the module libraries,
// built by port/tools/whole_build_android.sh), reserves the GameCube main
// memory at 0x80000000 as early as the process allows (JNI_OnLoad, before any
// GL work), starts the game in its own thread, passes the controller and draws
// the published frames with CoreScene.
#include "core_scene.h"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>

#include <atomic>
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

extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_CoreBridge_surfaceCreated(JNIEnv *, jclass) {
  if (auto *s = currentScene()) s->contextCreated();
  lastDraw = {};
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
  try {
    s->draw(frame, width, height, seconds);
  } catch (const std::exception &e) {
    LOG("draw: %s", e.what());
  }
}
