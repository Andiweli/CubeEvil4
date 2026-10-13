#include "vulkan_bootstrap.h"
#include "re4/port/game_runtime.h"
#include "re4/port/shelf_callbacks.h"
#include <android/native_window_jni.h>
#include <android/log.h>
#include <chrono>
#include <atomic>
#include <jni.h>
#include <map>
#include <set>
#include <fstream>
namespace {
using namespace re4::port;
struct Domain {
    std::shared_ptr<VulkanBootstrap> presenter;
    GraphicsState graphics;
    u64 cpuSceneMicros{},gpuSceneMicros{};
    ControllerInput callbackInput;
    std::unique_ptr<GameRuntime> callbackGame;
    std::unique_ptr<ShelfCallbackSession> shelf;
    std::chrono::steady_clock::time_point shelfTick;
    std::set<u32> shelfCaptures;
    bool shelfFirstFrame{};
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
    try {
        auto &d=get(h);
        if(d.shelf){d.shelf.reset();
            if(d.callbackGame->engineConsumers().modelCount()||d.callbackGame->loader().scheduler().allocatedStacks())throw std::logic_error("shelf surface retirement retains models/task stacks");
            __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","Shelf callback owner retired: models=0 stacks=0");d.callbackGame.reset();
        }
        domains.erase(static_cast<u64>(h));
    } catch(const std::exception &e) { error(env,e); }
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
    JNIEnv *env,jclass,jlong h,jstring root,jstring catalog,jint room,jstring event,jint frame,jstring model,jint cut,jint debugView) {
    try {
        auto string=[&](jstring value){auto *p=env->GetStringUTFChars(value,nullptr);if(!p)throw std::runtime_error("JNI scene string");std::string r;try{r=p;}catch(...){env->ReleaseStringUTFChars(value,p);throw;}env->ReleaseStringUTFChars(value,p);return r;};
        auto files=std::make_shared<ImportedFiles>(string(root),string(catalog));
        ControllerInput input;GameRuntime game(files,input,0);game.initialize();
        if(room<0||room>0xffff||frame<0)throw std::invalid_argument("original room/frame range");
        if(cut< -1||cut>32767)throw std::invalid_argument("original cut range");
        if(debugView<0||debugView>6)throw std::invalid_argument("debug view range");
        auto start=std::chrono::steady_clock::now();
        auto scene=loadDiagnosticScene(game.loader(),game.engineConsumers(),u16(room),string(event),u32(frame),string(model),cut<0?UINT32_MAX:u32(cut),u32(debugView));
        auto &domain=get(h);auto gpuStart=std::chrono::steady_clock::now();
        domain.cpuSceneMicros=u64(std::chrono::duration_cast<std::chrono::microseconds>(gpuStart-start).count());
        __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","Original scene CPU ready (%llu us), GPU setup starts",static_cast<unsigned long long>(domain.cpuSceneMicros));
        domain.presenter->loadScene(std::move(scene));
        domain.gpuSceneMicros=u64(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-gpuStart).count());
        __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","GPU setup ready (%llu us)",static_cast<unsigned long long>(domain.gpuSceneMicros));
    }catch(const std::exception &e){error(env,e);}
}
extern "C" JNIEXPORT jstring JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeSceneFrame(
    JNIEnv *env,jclass,jlong h,jstring output) {
    try {
        auto &domain=get(h);auto &p=*domain.presenter;
        if(domain.shelf&&!domain.shelfFirstFrame&&!domain.shelf->finished()&&std::chrono::steady_clock::now()-domain.shelfTick>=std::chrono::nanoseconds(33333333)){
            domain.shelf->tick();p.updateSceneGeometry(domain.shelf->snapshot());domain.shelfTick=std::chrono::steady_clock::now();
        }
        p.presentScene();
        domain.shelfFirstFrame=false;
        std::string evidence;
        if(output){auto *raw=env->GetStringUTFChars(output,nullptr);if(!raw)return nullptr;std::string path;try{path=raw;}catch(...){env->ReleaseStringUTFChars(output,raw);throw;}env->ReleaseStringUTFChars(output,raw);
            if(domain.shelf){auto tick=domain.shelf->visits();if((tick==0||tick==15||tick==30||tick==31)&&domain.shelfCaptures.insert(tick).second){evidence=path+"-tick"+std::to_string(tick);p.saveSceneReadback(evidence+".rgba");}}
            else p.saveSceneReadback(path);
        }
        auto report=p.report();report.insert(report.size()-1,",\"graphics_domains\":"+std::to_string(domains.size())+",\"cpu_scene_load_us\":"+std::to_string(domain.cpuSceneMicros)+",\"gpu_scene_setup_us\":"+std::to_string(domain.gpuSceneMicros)+(domain.shelf?",\"shelf_callback\":"+domain.shelf->report():""));
        if(!evidence.empty()){std::ofstream out(evidence+".json");out<<report;if(!out)throw std::runtime_error("cannot save shelf GPU evidence");}
        return env->NewStringUTF(report.c_str());
    }catch(const std::exception &e){error(env,e);return nullptr;}
}
extern "C" JNIEXPORT void JNICALL Java_org_re4_port_GraphicsBootstrapView_nativeLoadShelf(
    JNIEnv *env,jclass,jlong h,jstring root,jstring catalog,jint room,jint selection) {
    try {
        auto string=[&](jstring value){auto *p=env->GetStringUTFChars(value,nullptr);if(!p)throw std::runtime_error("JNI shelf string");std::string r;try{r=p;}catch(...){env->ReleaseStringUTFChars(value,p);throw;}env->ReleaseStringUTFChars(value,p);return r;};
        if(room<0||room>65535||selection<0||selection>2)throw std::invalid_argument("shelf request range");
        auto &d=get(h);d.shelf.reset();d.callbackGame.reset();
        auto files=std::make_shared<ImportedFiles>(string(root),string(catalog));
        d.callbackGame=std::make_unique<GameRuntime>(files,d.callbackInput,0);d.callbackGame->initialize();
        d.shelf=std::make_unique<ShelfCallbackSession>(*d.callbackGame,u16(room),u32(selection));
        d.presenter->loadScene(d.shelf->snapshot());d.shelf->start();d.shelfCaptures.clear();d.shelfFirstFrame=true;d.shelfTick=std::chrono::steady_clock::now();
    }catch(const std::exception &e){error(env,e);}
}
