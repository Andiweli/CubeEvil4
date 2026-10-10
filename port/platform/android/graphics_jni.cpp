#include "vulkan_bootstrap.h"
#include "re4/port/game_runtime.h"
#include <android/native_window_jni.h>
#include <atomic>
#include <jni.h>
#include <map>
namespace {
using namespace re4::port;
struct Domain {
    std::shared_ptr<VulkanBootstrap> presenter;
    GraphicsState graphics;
    explicit Domain(ANativeWindow *w):presenter(std::make_shared<VulkanBootstrap>(w)) {
        graphics.attach(presenter);
        graphics.setCopyClear({18,52,86,120},0x123456);
        graphics.setBlack(0);
    }
};
thread_local std::map<u64,std::unique_ptr<Domain>> domains;
std::atomic<u64> sequence{1};
Domain &get(jlong handle) {
    auto it=domains.find(static_cast<u64>(handle));
    if(it==domains.end()) throw std::logic_error("closed/foreign graphics surface owner");
    return *it->second;
}
void error(JNIEnv *env,const std::exception &e) { env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),e.what()); }
}
extern "C" JNIEXPORT jlong JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeOpen(JNIEnv *env,jclass,jobject surface) {
    auto *window=ANativeWindow_fromSurface(env,surface);
    if(!window) { env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),"null Android surface"); return 0; }
    try {
        auto id=sequence.fetch_add(1);
        if(!id || id>u64(INT64_MAX)) throw std::overflow_error("graphics surface identity");
        domains.emplace(id,std::make_unique<Domain>(window)); ANativeWindow_release(window); return static_cast<jlong>(id);
    } catch(const std::exception &e) { ANativeWindow_release(window); error(env,e); return 0; }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeClose(JNIEnv *env,jclass,jlong h) {
    try { get(h); domains.erase(static_cast<u64>(h)); } catch(const std::exception &e) { error(env,e); }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeFrame(JNIEnv *env,jclass,jlong h,jboolean black) {
    try { auto &d=get(h); d.graphics.setBlack(black?1:0); d.graphics.present(); auto r=d.presenter->report(); return env->NewStringUTF(r.c_str()); }
    catch(const std::exception &e) { error(env,e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeOriginalProbe(JNIEnv *env,jclass,jlong h,jstring directory) {
    try {
        auto &d=get(h); auto *raw=env->GetStringUTFChars(directory,nullptr);
        if(!raw) return nullptr;
        std::string path;
        try { path=raw; } catch(...) { env->ReleaseStringUTFChars(directory,raw); throw; }
        env->ReleaseStringUTFChars(directory,raw);
        auto result=verifyGameRuntime(path,d.presenter);
        auto report="{\"original_runtime\":"+result+",\"graphics\":"+d.presenter->report()+"}";
        return env->NewStringUTF(report.c_str());
    } catch(const std::exception &e) { error(env,e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeLoadScene(
    JNIEnv *env,jclass,jlong h,jstring root,jstring catalog,jint room,jstring event,jint frame,jstring model,jint cut) {
    try {
        auto string=[&](jstring value){auto *p=env->GetStringUTFChars(value,nullptr);if(!p)throw std::runtime_error("JNI scene string");std::string r;try{r=p;}catch(...){env->ReleaseStringUTFChars(value,p);throw;}env->ReleaseStringUTFChars(value,p);return r;};
        auto files=std::make_shared<ImportedFiles>(string(root),string(catalog));
        ControllerInput input;GameRuntime game(files,input,0);game.initialize();
        if(room<0||room>0xffff||frame<0)throw std::invalid_argument("original room/frame range");
        if(cut< -1||cut>32767)throw std::invalid_argument("original cut range");
        auto scene=loadDiagnosticScene(game.loader(),game.engineConsumers(),u16(room),string(event),u32(frame),string(model),cut<0?UINT32_MAX:u32(cut));
        get(h).presenter->loadScene(std::move(scene));
    }catch(const std::exception &e){error(env,e);}
}
extern "C" JNIEXPORT jstring JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeSceneFrame(
    JNIEnv *env,jclass,jlong h,jstring output) {
    try {
        auto &p=*get(h).presenter;p.presentScene();
        if(output){auto *raw=env->GetStringUTFChars(output,nullptr);if(!raw)return nullptr;std::string path;try{path=raw;}catch(...){env->ReleaseStringUTFChars(output,raw);throw;}env->ReleaseStringUTFChars(output,raw);p.saveSceneReadback(path);}
        auto report=p.report();report.insert(report.size()-1,",\"graphics_domains\":"+std::to_string(domains.size()));return env->NewStringUTF(report.c_str());
    }catch(const std::exception &e){error(env,e);return nullptr;}
}
