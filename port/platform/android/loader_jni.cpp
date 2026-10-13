#include "re4/port/engine_loader.h"
#include <jni.h>
#include <limits>
#include <map>
#include <stdexcept>
namespace {
struct Session {
    re4::port::EngineLoader loader;
    re4::port::FrameViAdapter clock;
    Session(std::shared_ptr<const re4::port::ImportedFiles> files, std::uint8_t disc)
        : loader(std::move(files), disc), clock(loader) {}
};
// IDs, never serialized native pointers. JNI calls stay on the Java owner thread.
thread_local std::map<jlong, std::unique_ptr<Session>> sessions;
thread_local jlong nextId = 1;
Session &session(jlong id) {
    auto found = sessions.find(id);
    if (found == sessions.end())
        throw std::logic_error("Invalid loader handle or owner thread");
    return *found->second;
}
void failure(JNIEnv *env, const std::exception &e) {
    auto cls = env->FindClass("java/lang/IllegalStateException");
    if (cls)
        env->ThrowNew(cls, e.what());
}
std::string text(JNIEnv *env, jstring value) {
    if (!value)
        throw std::invalid_argument("Null loader path");
    auto *raw = env->GetStringUTFChars(value, nullptr);
    if (!raw)
        throw std::runtime_error("Cannot access loader path");
    std::string result;
    try {
        result = raw;
    } catch (...) {
        env->ReleaseStringUTFChars(value, raw);
        throw;
    }
    env->ReleaseStringUTFChars(value, raw);
    return result;
}
} // namespace
extern "C" JNIEXPORT jlong JNICALL Java_org_re4_port_LoaderFrameLoop_nativeOpen(JNIEnv *env, jclass,
                                                                                jstring root,
                                                                                jstring catalog,
                                                                                jint disc) {
    try {
        if (disc < 0 || disc > 1 || nextId == std::numeric_limits<jlong>::max())
            throw std::invalid_argument("Invalid disc/handle");
        auto files =
            std::make_shared<re4::port::ImportedFiles>(text(env, root), text(env, catalog));
        auto value = std::make_unique<Session>(std::move(files), static_cast<std::uint8_t>(disc));
        auto id = nextId++;
        sessions.emplace(id, std::move(value));
        return id;
    } catch (const std::exception &e) {
        failure(env, e);
        return 0;
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_LoaderFrameLoop_nativeClose(JNIEnv *env, jclass,
                                                                                jlong id) {
    try {
        session(id);
        sessions.erase(id);
    } catch (const std::exception &e) {
        failure(env, e);
    }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_LoaderFrameLoop_nativeReset(JNIEnv *env, jclass,
                                                                                jlong id) {
    try {
        session(id).clock.reset();
    } catch (const std::exception &e) {
        failure(env, e);
    }
}
extern "C" JNIEXPORT jint JNICALL Java_org_re4_port_LoaderFrameLoop_nativeFrame(JNIEnv *env, jclass,
                                                                                jlong id,
                                                                                jlong ns) {
    try {
        return static_cast<jint>(session(id).clock.onDisplayFrame(ns));
    } catch (const std::exception &e) {
        failure(env, e);
        return 0;
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_re4_port_LoaderFrameLoop_nativeLoadRoom(JNIEnv *env,
                                                                                       jclass,
                                                                                       jlong id,
                                                                                       jint room) {
    try {
        if (room < 0 || room > 0xffff)
            throw std::invalid_argument("Invalid room ID");
        return session(id).loader.loadRoom(static_cast<std::uint16_t>(room)) ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception &e) {
        failure(env, e);
        return JNI_FALSE;
    }
}
extern "C" JNIEXPORT jlongArray JNICALL Java_org_re4_port_LoaderFrameLoop_nativeTiming(JNIEnv *env,
                                                                                       jclass,
                                                                                       jlong id) {
    try {
        auto &s = session(id);
        jlong values[] = {static_cast<jlong>(s.clock.retraces()),
                          static_cast<jlong>(s.clock.gameplayFrames())};
        auto array = env->NewLongArray(2);
        if (array)
            env->SetLongArrayRegion(array, 0, 2, values);
        return array;
    } catch (const std::exception &e) {
        failure(env, e);
        return nullptr;
    }
}
