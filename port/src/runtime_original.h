#pragma once
#include "consumer_original.h"
#include "re4/port/game_runtime.h"
#include "rnd.h"
#include <cstring>
#include <functional>

namespace re4::port {
inline std::uint32_t nativeFlag(const u32 *p, int n) {
    return p[n / 32] & (0x80000000U >> (n % 32));
}
inline void nativeFlagOn(u32 *p, int n) { p[n / 32] |= 0x80000000U >> (n % 32); }
inline void nativeFlagOff(u32 *p, int n) { p[n / 32] &= ~(0x80000000U >> (n % 32)); }
#define SpfFlagChk(g, n) nativeFlag(&(g)->Stop_flg, n)
#define SpfFlagOn(g, n) nativeFlagOn(&(g)->Stop_flg, n)
#define SpfFlagOff(g, n) nativeFlagOff(&(g)->Stop_flg, n)
#define StaFlagChk(g, n) nativeFlag((g)->Status_flg, n)
#define StaFlagOn(g, n) nativeFlagOn((g)->Status_flg, n)
#define StaFlagOff(g, n) nativeFlagOff((g)->Status_flg, n)
#define DbgFlagChk(g, n) nativeFlag((g)->Debug_flg, n)
#define SysFlagChk(g, n) nativeFlag(&(g)->System_flg, n)
#define SysFlagOn(g, n) nativeFlagOn(&(g)->System_flg, n)
#define SysFlagOff(g, n) nativeFlagOff(&(g)->System_flg, n)
#define DpfFlagOn(g, n) nativeFlagOn(&(g)->Disp_flg, n)
#define DpfFlagOff(g, n) nativeFlagOff(&(g)->Disp_flg, n)
#define ScfFlagChk(g, n) nativeFlag((g)->Scenario_flg, n)
#define ScfFlagOn(g, n) nativeFlagOn((g)->Scenario_flg, n)
#define ScfFlagOff(g, n) nativeFlagOff((g)->Scenario_flg, n)
#define FlagChkSignW(flg, no) ((s32)((flg) << ((no) & 31)) < 0)
#define FlagChkSign(flg, no) FlagChkSignW((flg)[(no) >> 5], no)
#define G_ROOM_ID (pG->room_id)

void PADClamp(PADStatus *);
void PADClampCircle(PADStatus *);

struct OriginalContext {
    explicit OriginalContext(PlatformGamepads &input) : pads(input) {}
    PlatformGamepads &pads;
    GameRuntime *runtime = nullptr;
    GameplayHeaps *heaps = nullptr;
    SharedScenarioConsumers *consumers = nullptr;
    NativeGameState global{};
    NativeGameState *pG = &global;
    struct System {
        u32 Config_flg{};
        u8 language{}, eff_country{}, brightness{}, pad_type{};
    } system;
    System *pSys = &system;
    JOY Joy[4]{};
    KEY Key{};
    PADStatus Pad_data[4]{};
    int Vib_level = 0, system_vcnt = 2;
    u32 ResetBits = 0, ConnectedBits = 0;
    u64 un_stop_mask = 0;
    static const u32 Key_type_tbl[2][64];
    struct RenderMode {
        u16 fbWidth = 640, efbHeight = 480;
    } Rmode;
    using GXRenderModeObj = RenderMode; // Only the original scalar dimensions used here.
    struct ScreenState {
        f32 x{}, y{}, width{}, height{};
    } Screen;
    f32 ZNEAR{}, ZFAR{}, ORTHO_T{}, ORTHO_B{}, ORTHO_L{}, ORTHO_R{};
    char *pUser_name = nullptr;
    std::vector<GameplayHeaps::Allocation> allocations;
    const void *pSUB = nullptr;
    void *ItemEventTbl[16]{};
    std::function<u32()> seconds;
    std::vector<std::string> trace;
    struct Scenario {
        OriginalContext *context = nullptr;
        u8 event_no_cut_back{}, stop_bak_flg{}, event_start_cnt{}, up_cut_start_cnt{},
            task_kind_back{}, event_cancel_enable{};
        int m_str_stop_flag = 0, chapterEnd = 0;
        s8 m_room_flag = 0;
        TaskEntry pExitFunc, pDoorFunc, pCancelFunc;
        void *pExitParam = nullptr, *pDoorParam = nullptr, *pCancelParam = nullptr;
        std::optional<TaskHandle> pLadderTask;
        u8 m_door_fade_eff{}, m_item_get{};
        u16 m_debug_disp_y{};
        void clearChapterEnd() { chapterEnd = 0; }
        int checkChapterEnd() const { return chapterEnd; }
        void scheduler();
    } SceSys;
    struct RoomFacade {
        OriginalContext *context = nullptr;
        u16 m_RelNo = 0;
        u32 linkedModule = 0;
        u16 linkedRoom = 0;
        void stopRelData();
        int checkRelRead(u16 room);
        void linkRelData(u16 room);
        void execInitFunc(u16 room);
        void execMainFunc(u16 room);
    } RoomData;
    struct MessageFacade {
        OriginalContext *context = nullptr;
        void stageInit();
    } cMes;
    struct EventFacade {
        OriginalContext *context = nullptr;
        void Run();
        int SetFunc(const char *name, EventFunction f) {
            return context->runtime->engineConsumers().SetFunc(name, std::move(f));
        }
        [[noreturn]] void EvtReadAram(const char *, int, int, int, int) {
            throw MissingGameConsumer("EventMgr::EvtReadAram/SetEvd/SetBin");
        }
        [[noreturn]] void EvtReadExec(const char *, int, int) {
            throw MissingGameConsumer("EventMgr::EvtReadExec/Event::Init/ExePacket");
        }
    } EvtMgr;
    using Event = EventCpu;
    using cModel = ModelCpu;
    using cObj = ModelCpu; // Only typed CPU fields accessed by reviewed event callbacks.
    struct CameraFacade {
        OriginalContext *context{};
        int CurrentAreaNo() {
            return context->runtime->engineConsumers().cameraState.CurrentAreaNo();
        }
    } CamCtrl{this};
    struct PlayerFacade {
        [[noreturn]] cModel *operator->() { throw MissingGameConsumer("gameDebug/pPL/PlayerInit"); }
        [[noreturn]] operator cModel *() { throw MissingGameConsumer("gameDebug/pPL/PlayerInit"); }
    } pPL;
    [[noreturn]] void DbMenuExec() { throw MissingGameConsumer("DbMenuExec"); }
    [[noreturn]] void VecToCamVec(Vec *, Vec *) {
        throw MissingGameConsumer("VecToCamVec/camera matrices");
    }
    [[noreturn]] void PSVECAdd(Vec *, Vec *, Vec *) {
        throw MissingGameConsumer("PSVECAdd/unbound debug player path");
    }
    [[noreturn]] void PartsWorldPosCalc(cModel *) {
        throw MissingGameConsumer("PartsWorldPosCalc");
    }
    using TexRenderMng = TexRenderTarget;
    struct R120Work {
        TexRenderMng *mgr{};
        u8 pad_4[0x388 - 4]{};
    };
    R120Work *r120_work{};
    void R120Init();
    void R120Main();
    void R120Event();
    void Evt_R120S00_Func(Event *, u32);
    void Evt_R120S01_Func(Event *, u32);
    void EventCarInit(Event *);
    void EvtTexRenderCamTrans(Event *, int);
    void r120_setTrans(int);
    void TexRenderInit(TexRenderMng **out, int size, int repeat) {
        *out=runtime->texRender().initialize(size,repeat);
    }
    [[noreturn]] void TexRenderModAddOtMirror(int, cModel *) {
        throw MissingGameConsumer("TexRenderModAddOtMirror");
    }
    [[noreturn]] void TexRenderModAddOt(int, cModel *) {
        throw MissingGameConsumer("TexRenderModAddOt");
    }
    [[noreturn]] void Obj18CmfOn(cObj *, u32) {
        throw MissingGameConsumer("Obj18CmfOn/model material data");
    }
    [[noreturn]] void SmdSetTrans(int, int) {
        throw MissingGameConsumer("SmdSetTrans/SmdSetup/ObjMgr");
    }
    [[noreturn]] void FadeSetW(u32, int, int, int) {
        throw MissingGameConsumer("FadeSetW/presentation");
    }
    [[noreturn]] void SubScreenOpen(int, int) { throw MissingGameConsumer("SubScreenOpen"); }
    void systemVISetBlack(int);
    void VISetBlack(int sw) { runtime->graphics().setBlack(sw); }
    [[noreturn]] void SceEventStart(int) {
        throw MissingGameConsumer("SceEventStart/player/managers/audio");
    }
    [[noreturn]] void SceEventEnd(int) {
        throw MissingGameConsumer("SceEventEnd/player/managers/audio");
    }
    [[noreturn]] void SceAtExecRoomJump(u16, Vec *, Vec *, int) {
        throw MissingGameConsumer("SceAtExecRoomJump/gameDoordemo");
    }
    struct SofdecFacade {
        [[noreturn]] void Initialize(const char *, int) {
            throw MissingGameConsumer("Sofdec.Initialize/SFD video/audio");
        }
        [[noreturn]] bool isCancel() { throw MissingGameConsumer("Sofdec.isCancel"); }
    } Sofdec;
    struct LightFacade {
        struct Light {
            [[noreturn]] void setParent(cModel *) {
                throw MissingGameConsumer("cLight::setParent");
            }
        };
        [[noreturn]] Light *getKindLight(int) {
            throw MissingGameConsumer("LightMgr.getKindLight");
        }
    } LightMgr;
    using cLight = LightFacade::Light;
    struct IdFacade {
        [[noreturn]] void dispSw(int, int) { throw MissingGameConsumer("IdSys.dispSw"); }
    } IdSys;
    int DebugTrg(int);
    void SceSleep(int n) { runtime->loader().scheduler().sleep(n); }
    void SceExec(int level, TaskEntry task, int, int, int priority, int) {
        auto token = runtime->modules().lifetime(RoomData.m_RelNo);
        const auto room=RoomData.linkedRoom;
        TaskEntry owned=[this,room,task=std::move(task)] {
            try { task(); }
            catch(...) { if(room) runtime->queueFailedRoomRetirement(room); throw; }
        };
        if (!runtime->sceExec(level, std::move(owned), 0, priority, nullptr, token))
            throw std::runtime_error("original scenario slots exhausted");
    }
    struct ReadInto {
        LoadRequest request;
        u8 *destination;
        std::size_t capacity;
        TaskHandle caller;
    };
    std::map<int, ReadInto> reads;
    int nextRead = 1;
    struct DvdFacade {
        OriginalContext *context = nullptr;
        int ReadCheck(int id, int *result, void *, void *);
    } Dvd;
    int DvdReadN(const char *, void *, int, int, int, int, const char *, int);
    void collectCancelledReads();
    struct Log {
        [[noreturn]] void err(int, int, const char *) {
            throw std::logic_error("original unreachable log error");
        }
    } log;
    Log *pLog = &log;
    SharedScenarioConsumers &required(const char *name);
    void reset();
    void PadInit();
    void PadRead();
    void KeyStop(u64);
    void KeyClear(u64);
    void VibControl();
    int PadCheckStatus(JOY *);
    // SDK read/reset is a platform boundary, not a simulated GameCube device.
    int PADInit() { return 1; }
    int PADReset(u32) { return 1; } // Android connection state needs no GC probe reset.
    void PADSetAnalogMode(int mode) {
        if (mode != 3)
            throw std::invalid_argument("analog mode");
    }
    void PADRead(PADStatus *status);
    void PADControlMotor(int port, int command) {
        pads.rumble(static_cast<u8>(port), command == 1);
    }
    static void memclr_asm(JOY *joy, std::size_t bytes) {
        if (bytes != sizeof(JOY))
            throw std::logic_error("JOY clear size");
        *joy = JOY{};
    }
    void memclr_asm(u8 *buffer, std::size_t bytes) {
        if (buffer != global.Em_list || bytes != sizeof(global.Em_list))
            throw std::logic_error("ESL clear range");
        std::memset(buffer, 0, bytes);
    }
    void systemScreenInit();
    void systemWorkInit();
    void SetSystemVcnt(int);
    int GetSystemVcnt();
    using OSTime = u32;
    OSTime OSGetTime() { return seconds(); }
    static u32 OSTicksToSeconds(OSTime t) { return t; } // OS timer adapter already in seconds.
    void SecToTime(u32, u32 *, u32 *, u32 *);
    void InitGameTime();
    u32 GetGameTime(u32 *, u32 *, u32 *);
    void SetGameTime();
    template <class... A> void OSReport(const char *, A...) {} // Diagnostic sink only.
    template <class... A> void eprintf(int, int, int, int, const char *, A...) {}
    template <class... A> void eprintf2(int, int, int, int, int, int, const char *, A...) {}
    void *mem_calloc(u32, const char *, int, int, int);
    void DoorFlagInit();
    void GameTask();
    void gameDebug();
    void gameInit();
    // Only fields touched by ClothInit; cloth simulation remains unbound.
    struct ClothSlot { u32 be_flag{}; } ClothWk[8];
    void ClothInit();
    [[noreturn]] void missingGameInit(const char *name) { throw MissingGameConsumer(name); }
    void GXSetCopyClear(GXColor color,u32 depth) { runtime->graphics().setCopyClear(color,depth); }
    [[noreturn]] void gameStageInit() { throw MissingGameConsumer("gameStageInit/cMes/GameSave"); }
    [[noreturn]] void gameRoomInit() {
        throw MissingGameConsumer("gameRoomInit/model/light/player");
    }
    [[noreturn]] void gameMainLoop() {
        throw MissingGameConsumer("gameMainLoop/IdSys/trans/render/audio");
    }
    [[noreturn]] void gameDoordemo() { throw MissingGameConsumer("gameDoordemo"); }
    [[noreturn]] void gameEnding() { throw MissingGameConsumer("gameEnding"); }
    [[noreturn]] void gameOption() { throw MissingGameConsumer("gameOption"); }
    void TaskSleep(int);
    int emListVillage(int);
    int checkEmListNo(u16);
    const char *getEmListName(u32);
    const char *getEmListDbgName(int);
    int getEmListNum();
    void StageSet();
    int MemReplaceHeap(int a, int b) {
        if (!heaps->replace(a, b))
            throw std::logic_error(
                "MemReplaceHeap: native live-resource boundary prevents handoff");
        return 1;
    }
    int MemSetCurrentHeap(int n) {
        if (!heaps->setCurrent(n))
            throw std::logic_error("MemSetCurrentHeap: native heap unavailable");
        return 1;
    }
    void readEmList(int mode);
    void ScenarioInit();
    void ScenarioRoomInit();
    void ScenarioTaskAllOff();
    void ScenarioMove();
    void scenarioLoopBeforeInit();
    void scenarioLoopAfterInit();
    void SceInitItemEvent();
    void SceAtSetSaveItem() { required("SceAtSetSaveItem").SceAtSetSaveItem(); }
    void SceExecInitCondition() { required("SceExecInitCondition").SceExecInitCondition(); }
    void SceExecCheckCondition() { required("SceExecCheckCondition").SceExecCheckCondition(); }
    void SubCharInit(int n, Vec *position, float angle) {
        required("SubCharInit").SubCharInit(n, position, angle);
    }
    void SubCharCtrl(int n, int value) { required("SubCharCtrl").SubCharCtrl(n, value); }
    void EmSetFromList() { required("EmSetFromList").EmSetFromList(); }
    void SceAtRoomSet() { required("SceAtRoomSet").SceAtRoomSet(); }
    void SubMissionCheck() { required("SubMissionCheck").SubMissionCheck(); }
    void SceAtCheckMoveScrAt() { required("SceAtCheckMoveScrAt").SceAtCheckMoveScrAt(); }
    int scenarioCheckEventCancel() {
        return required("scenarioCheckEventCancel").scenarioCheckEventCancel();
    }
};
// sce.h's actual SCC_CHASE value is checked by the generator before use.
} // namespace re4::port
