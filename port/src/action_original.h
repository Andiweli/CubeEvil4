#pragma once
#include "re4/port/game_runtime.h"
#include "re4_action_types.h"
#include <array>
#include <functional>

namespace re4::port {
struct PlayerDomain;
// Logical OT links replace GC RAM tags. A callback carries its actual C++
// signature and a checked lifetime; neither code nor arguments enter a u32.
struct ActionCallback {
    std::function<void(void*,std::intptr_t)> function;
    std::weak_ptr<const void> lifetime;
    ModuleToken module{};
    void *taskModel{};
    std::thread::id owner=std::this_thread::get_id();
    explicit operator bool() const { return bool(function); }
    void invoke(void*,std::intptr_t) const;
};
struct ActionManager {
    struct ACT_WORK {
        int tag=-1;
        ActionCallback func;
        void *arg{};
        u8 kind{},slot{},type{},btn{};
        std::intptr_t d{};
        u32 flags{};
    };
    NativeGameState *pG;
    KEY &Key;
    NativeScheduler &scheduler;
    std::array<int,16> m_ot{};
    std::array<ACT_WORK,8> m_task{};
    u8 m_num{},m_stop_flag_old{},m_active_flag{};
    std::function<int()> playerActCheck;
    std::function<u32()> playerStatus;
    std::function<s16()> playerHp;
    // These consumers must be provided by the original cockpit/message and
    // scenario owners. An offered visible or scenario action cannot silently
    // succeed without them.
    std::function<void(u8)> displayFlag;
    std::function<void(const ACT_WORK&)> display;
    std::function<void(void*)> setScenarioExec;
    std::function<void(void*)> disableScenarioOnce;
    ActionManager(NativeGameState &g,KEY &k,NativeScheduler &s):pG(&g),Key(k),scheduler(s){m_ot.fill(-1);}
    void clearOrdering();
    void addWork(int,ACT_WORK*);
    ACT_WORK *work(int);
    int first() const;
    int next(const ACT_WORK*) const;
    void init();void moveOriginal();void move();
    void set(int,int,ActionCallback,void*,int,int,int,std::intptr_t=0);
    void setOriginal(int,int,ActionCallback,void*,int,int,int,std::intptr_t);
    ACT_WORK *pullWork();int checkButton(ACT_WORK*);int checkPLStatus(ACT_WORK*);
    void disp(ACT_WORK*);
    void setStopFlagOld(int v){m_stop_flag_old=u8(v);}
    void setActive(int v){m_active_flag=u8(v!=0);}
    int isActive() const {return m_active_flag==1;}
    void resetDisplay();
    void executeScenario(ACT_WORK*,int);
    void executeArea(ACT_WORK*);
};
// A typed window boundary, not a replacement window actor. The complete
// cEmWindow constructor/field registration is still required before a live
// room can provide a window here.
struct NativeActionWindow : ActorModelCpu {
    int ChkEnableFence(int){throw MissingGameConsumer("cEmWindow::ChkEnableFence/live original window owner");}
    u16 ChkStatus(){throw MissingGameConsumer("cEmWindow::ChkStatus/live original window owner");}
    u8 ChkBreakDir(Vec*){throw MissingGameConsumer("cEmWindow::ChkBreakDir/live original window work");}
};
struct NativeActionFieldInfo {NativeActionWindow *pParent{};u8 id{};};
struct NativeActionField {u8 be_flg{},id{};NativeActionFieldInfo field;};
namespace action_collision {
u32 At_em_rect_rect_ck(ModelCpu*,ModelCpu*);
int em_rect2_ck_sub(ModelCpu*,ModelCpu*);
u32 At_em_sphere_rect_ck(ModelCpu*,ModelCpu*);
u32 At_em_sphere_sphere_ck(ModelCpu*,ModelCpu*);
void core(ModelCpu*,ModelCpu*);
int At_rect_rect_ck(Vec*,Vec*);int At_rect_point_ck(Vec*,Vec*);
int Get_ang_dir(f32);f32 GetDistance(Vec*,Vec*);f32 GetDistance3(Vec*,Vec*);
}
} // namespace re4::port
#define SpfFlagChk(g,n) ((g)->Stop_flg & (0x80000000u >> (n)))
#define DpfFlagChk(g,n) ((g)->Disp_flg & (0x80000000u >> (n)))
