#include "re4/port/module_verification.h"
#include <jni.h>
#include <stdexcept>
extern "C" JNIEXPORT jstring JNICALL
Java_org_re4_port_NativeChecks_nativeModules(JNIEnv *env, jclass, jstring directory) {
    try {
        if (!directory) {
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                          "Null module fixture directory");
            return nullptr;
        }
        auto *raw = env->GetStringUTFChars(directory, nullptr);
        if (!raw)
            return nullptr;
        std::string path;
        try {
            path = raw;
        } catch (...) {
            env->ReleaseStringUTFChars(directory, raw);
            throw;
        }
        env->ReleaseStringUTFChars(directory, raw);
        const auto result = re4::port::verifyModules(path);
        return env->NewStringUTF(result.c_str());
    } catch (const std::exception &e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
        return nullptr;
    }
}
