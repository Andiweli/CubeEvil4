#include "re4/port/game_runtime.h"
#include <atomic>
#include <iomanip>
#include <jni.h>
#include <map>
#include <sstream>

namespace {
using namespace re4::port;
struct InputDomain {
    ControllerInput input;
    std::unique_ptr<PadRuntime> pad = std::make_unique<PadRuntime>(input);
    std::unique_ptr<GameRuntime> game;
    bool running = false;
    FrameViAdapter vi{[this] {
        if (game)
            game->tick();
        else
            pad->tick();
    }};
    void pause() {
        running = false;
        vi.reset();
        input.clearHeld();
        if (game)
            game->pause();
        else
            pad->clear();
    }
};
thread_local std::map<std::uint64_t, std::unique_ptr<InputDomain>> domains;
std::atomic<std::uint64_t> nextDomain{1};
InputDomain &domain(jlong handle) {
    auto it = domains.find(static_cast<std::uint64_t>(handle));
    if (it == domains.end())
        throw std::invalid_argument("Closed/foreign owner-thread controller handle");
    return *it->second;
}
void error(JNIEnv *env, const std::exception &e) {
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
}
std::string utf8(JNIEnv *env, jstring s) {
    if (!s)
        throw std::invalid_argument("null string");
    auto *raw = env->GetStringUTFChars(s, nullptr);
    if (!raw)
        throw std::runtime_error("UTF8 allocation failed");
    std::string result;
    try {
        result = raw;
    } catch (...) {
        env->ReleaseStringUTFChars(s, raw);
        throw;
    }
    env->ReleaseStringUTFChars(s, raw);
    return result;
}
} // namespace
extern "C" JNIEXPORT jlong JNICALL Java_org_re4_port_ControllerLoop_nativeOpen(JNIEnv *env,
                                                                               jclass) {
    try {
        auto id = nextDomain.fetch_add(1);
        if (!id || id > static_cast<std::uint64_t>(INT64_MAX))
            throw std::overflow_error("controller identity");
        domains.emplace(id, std::make_unique<InputDomain>());
        return static_cast<jlong>(id);
    } catch (const std::exception &e) {
        error(env, e);
        return 0;
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_ControllerLoop_nativeClose(JNIEnv *env, jclass,
                                                                               jlong h) {
    try {
        domain(h).pause();
        domains.erase(static_cast<std::uint64_t>(h));
    } catch (const std::exception &e) {
        error(env, e);
    }
}
extern "C" JNIEXPORT void JNICALL
Java_org_re4_port_ControllerLoop_nativeConnect(JNIEnv *env, jclass, jlong h, jint device, jint port,
                                               jboolean analogLeft, jboolean analogRight) {
    try {
        if (port < 0 || port > 3)
            throw std::invalid_argument("native port");
        domain(h).input.connect(device, static_cast<std::uint8_t>(port), analogLeft, analogRight);
    } catch (const std::exception &e) {
        error(env, e);
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_ControllerLoop_nativeDisconnect(JNIEnv *env,
                                                                                    jclass, jlong h,
                                                                                    jint device) {
    try {
        domain(h).input.disconnect(device);
    } catch (const std::exception &e) {
        error(env, e);
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_re4_port_ControllerLoop_nativeButton(
    JNIEnv *env, jclass, jlong h, jint device, jint key, jboolean down) {
    try {
        return domain(h).input.button(device, key, down);
    } catch (const std::exception &e) {
        error(env, e);
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_re4_port_ControllerLoop_nativeAxes(
    JNIEnv *env, jclass, jlong h, jint device, jfloat x, jfloat y, jfloat z, jfloat rz, jfloat hx,
    jfloat hy, jfloat lt, jfloat rt) {
    try {
        return domain(h).input.axes(device, x, y, z, rz, hx, hy, lt, rt);
    } catch (const std::exception &e) {
        error(env, e);
        return false;
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_ControllerLoop_nativeRunning(JNIEnv *env,
                                                                                 jclass, jlong h,
                                                                                 jboolean running) {
    try {
        auto &d = domain(h);
        if (!running)
            d.pause();
        else {
            d.vi.reset();
            if (d.game)
                d.game->resume();
            d.running = true;
        }
    } catch (const std::exception &e) {
        error(env, e);
    }
}
extern "C" JNIEXPORT jint JNICALL Java_org_re4_port_ControllerLoop_nativeFrame(JNIEnv *env, jclass,
                                                                               jlong h, jlong ns) {
    try {
        auto &d = domain(h);
        return d.running ? static_cast<jint>(d.vi.onDisplayFrame(ns)) : 0;
    } catch (const std::exception &e) {
        error(env, e);
        return 0;
    }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_re4_port_ControllerLoop_nativeSnapshot(JNIEnv *env,
                                                                                     jclass,
                                                                                     jlong h) {
    try {
        auto &d = domain(h);
        const auto &key = d.game ? d.game->key() : d.pad->key();
        const auto &joy = d.game ? d.game->joy() : d.pad->joy();
        std::ostringstream out;
        out << "{\"pointer_bits\":" << sizeof(void *) * 8
            << ",\"running\":" << (d.running ? "true" : "false") << ",\"mode\":\""
            << (d.game ? "shared-game-runtime" : "original-pad-diagnostic") << "\","
            << "\"pad_error\":" << static_cast<int>(joy.err) << ",\"joy_on\":" << joy.on
            << ",\"joy_trg\":" << joy.trg << ",\"joy_rel\":" << joy.rel << ','
            << "\"key_on\":" << key.on << ",\"key_trg\":" << key.trg << ",\"key_rel\":" << key.rel
            << ",\"key_rep\":" << key.rep << ",\"key_rep2\":" << key.rep2 << ",\"stick\":["
            << static_cast<int>(key.stickX) << ',' << static_cast<int>(key.stickY) << ','
            << static_cast<int>(key.substickX) << ',' << static_cast<int>(key.substickY)
            << "],\"triggers\":[" << static_cast<int>(key.triggerLeft) << ','
            << static_cast<int>(key.triggerRight) << "]}";
        return env->NewStringUTF(out.str().c_str());
    } catch (const std::exception &e) {
        error(env, e);
        return nullptr;
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_ControllerLoop_nativeConfigureGame(
    JNIEnv *env, jclass, jlong h, jstring root, jstring catalog, jint disc) {
    try {
        auto &d = domain(h);
        if (disc < 0 || disc > 1 || d.running)
            throw std::logic_error("configure stopped game runtime / disc 0..1");
        if (d.game)
            throw std::logic_error("game runtime already configured");
        for (const auto &entry : domains)
            if (entry.second->game)
                throw std::logic_error("one shared game runtime per input owner thread");
        auto files = std::make_shared<ImportedFiles>(utf8(env, root), utf8(env, catalog));
        auto game = std::make_unique<GameRuntime>(std::move(files), d.input,
                                                  static_cast<std::uint8_t>(disc));
        game->initialize();
        game->pause();
        d.pad.reset();
        d.game = std::move(game);
    } catch (const std::exception &e) {
        error(env, e);
    }
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_re4_port_NativeChecks_nativeGameRuntime(JNIEnv *env, jclass, jstring directory) {
    try {
        auto result = verifyGameRuntime(utf8(env, directory));
        return env->NewStringUTF(result.c_str());
    } catch (const std::exception &e) {
        error(env, e);
        return nullptr;
    }
}
