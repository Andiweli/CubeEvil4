#include "re4/port/verification.h"
#include <android/log.h>
#include <exception>
#include <jni.h>
#include <string>

extern "C" JNIEXPORT jobject JNICALL
Java_org_re4_port_NativeChecks_nativeRun(JNIEnv* env, jclass, jstring group) {
    if (!group) {
        jclass error = env->FindClass("java/lang/IllegalArgumentException");
        if (error) env->ThrowNew(error, "Verification group must not be null");
        return nullptr;
    }
    const char* raw = env->GetStringUTFChars(group, nullptr);
    if (!raw) return nullptr; // JNI exception is already pending.
    std::string requested;
    try {
        requested.assign(raw);
    } catch (const std::exception&) {
        env->ReleaseStringUTFChars(group, raw);
        jclass error = env->FindClass("java/lang/OutOfMemoryError");
        if (error) env->ThrowNew(error, "Cannot copy verification group");
        return nullptr;
    }
    env->ReleaseStringUTFChars(group, raw);
    try {
        const auto result = re4::port::verifyOriginalCode(requested);
        const auto text = result.text();
        __android_log_write(result.failures() ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO,
            "RE4Foundation", text.c_str());
        jclass resultClass = env->FindClass("org/re4/port/NativeChecks$Result");
        if (!resultClass) return nullptr;
        jmethodID constructor = env->GetMethodID(resultClass, "<init>", "(IILjava/lang/String;)V");
        if (!constructor) return nullptr;
        jstring report = env->NewStringUTF(text.c_str());
        if (!report) return nullptr;
        return env->NewObject(resultClass, constructor,
            static_cast<jint>(result.checks()), static_cast<jint>(result.failures()), report);
    } catch (const std::exception& failure) {
        jclass error = env->FindClass("java/lang/IllegalStateException");
        if (error) env->ThrowNew(error, failure.what());
        return nullptr;
    }
}
