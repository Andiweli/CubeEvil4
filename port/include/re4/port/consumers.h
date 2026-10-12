#pragma once
#include "re4/port/engine_loader.h"
#include "re4_runtime_types.h"
#include "re4/port/model_types.h"
#include "re4/port/event_resources.h"
#include <array>
#include <map>
#include <thread>

namespace re4::port {
class ModelRuntime;
struct ConsumerHandle {
    u64 runtime{}, epoch{}, index{};
};
struct EventBinView {
    std::string name;
    ResourceLease bytes;
};
struct EventPackageView {
    ResourceLease bytes;
    std::string room, number;
    u32 packetOffset{}, packetSize{};
    std::vector<EventBinView> bins;
};
EventPackageView indexEventPackage(ResourceLease);
// Index actual SetOm/SetParts/Mot packets; this does not execute their handlers.
struct EventResourceReference {
    u32 packetOffset{}, type{};
    std::string model, parent, bin, tpl;
};
std::vector<EventResourceReference> indexEventResourceReferences(const EventPackageView &);
struct CameraDataView {
    ResourceLease bytes;
    u8 version{}, cameras{}, areas{}, links{};
};
CameraDataView indexCameraData(ResourceLease);

class EngineConsumers;
struct MotionActorContext;
struct EventCpu {
    EngineConsumers *manager{};
    EventPackageView package;
    u32 StatusFlag{};
    int FuncType{}, NowCut{}, NowFrame{};
    NativeDatTbl ModTbl{0x60};
    int GetFuncType() const { return FuncType; }
    int GetNowCut() const { return NowCut; }
    int GetNowFrame() const { return NowFrame; }
    bool FlgCkStatus(u32 n) const;
    void SetFuncType(int n) { FuncType = n; }
    void GetRoomNo(char *) const;
    void GetEventNo(char *) const;
    int ExeFunc(int mode, int parameter);
    int SetMod(const char *, void *, u8, void *, u8, int *);
    int GetMod(void **, const char *, u8 *, int *);
    int ExePacket_SetPartsSub(const char *,const char *,const char *,const char *);
};
using EventFunction = std::function<void(EventCpu *, u32)>;
struct MessageIndexCpu {
    u32 m_language = 1;
    u8 *m_Data[5]{};
    void setLanguage(u32);
    u32 getLanguage();
    void registData(int,u8 *);
    u16 *getAddr(int, int);
    int getMesNum(int);
    int getSpaceWidth();
};
struct CameraCpu {
    s8 areaNo{}, cameraNo{};
    u8 r0{};
    u32 m_system_flag{};
    u32 m_attach_num{};
    ModelCpu *m_p_model[3]{},*m_p_attach_model_old{};
    MotionCamera *m_p_attach[3]{};
    int CurrentAreaNo();
    int CurrentCameraNo();
    void Disable();
    void AreaCheckOnOff(int);
    void clearAttachCamera();
    void registAttachCamera(MotionCamera*,ModelCpu*);
    void deleteAttachCamera(MotionCamera*,ModelCpu*);
    ModelCpu *getAttachModel(ModelCpu*);
    MotionCamera *getAttachCamera(ModelCpu*);
};

// Room ownership, checked references and original CPU consumers on the frame owner.
// Package indexing is not Event::Init/ExePacket or a successfully played cutscene.
class EngineConsumers final {
  public:
    EventResources &eventResources() { check(); return eventResources_; }
    void setAdaOverrideProvider(std::function<bool()> f) { check(); adaOverride_=std::move(f); }
    bool debugAdaOverride() const { check(); return adaOverride_ && adaOverride_(); }
    void initializeMotion(ConsumerHandle, ResourceLease, int frame=0, int flags=1, int hokan=0);
    void initializeMotion(ConsumerHandle,ResourceLease motion,ResourceLease sequence,int frame=0,int flags=1,int hokan=0);
    void initializeMotionBlend(ConsumerHandle,ResourceLease primary,ResourceLease positive,
                               ResourceLease negative,ResourceLease sequence={},u8 hokan=0,
                               int mode=0,u16 flags=1,u16 frame=0);
    void setMotionBlendRate(ConsumerHandle,f32);
    void clearMotionBlend(ConsumerHandle);
    // Executes original MotionMove/Core/SequenceCtrl on the persistent owned model.
    // IK needs a typed actor/collision context; Hokan and cMot3 use owned work.
    u32 advanceMotion(ConsumerHandle, f32 gameplaySpeed=1.f,const MotionActorContext *actor=nullptr);
    Vec sampleMotion(ConsumerHandle,u32 joint,f32 frame) const;
    void registerEventModel(ConsumerHandle event,ConsumerHandle model,const std::string &name,u8 type);
    ModelInfoView *createModelInfoFor(ModelCpu &,const char *bin,const char *tpl,bool resetOffsets);
    ModelInfoView *createModelInfoFor(ModelCpu &,ResourceLease bin,ResourceLease tpl);
    explicit EngineConsumers(NativeScheduler *domain = nullptr);
    ~EngineConsumers();
    EngineConsumers(const EngineConsumers &) = delete;
    EngineConsumers &operator=(const EngineConsumers &) = delete;
    void clear(); // Rejects live callback stacks; caller must quiesce other borrowers first.
    ConsumerHandle model(ResourceLease bin, std::string name);
    void configureModelPools(u32 infos, u32 parts); // Actual room CONS/SMD counts must be supplied.
    ConsumerHandle initializeModel(ResourceLease bin, ResourceLease tpl, std::string name,
                                   bool systemInvisible = false, u8 kindId = 0, u32 initialFlags = 0,
                                   std::shared_ptr<ModelCpu> originalOwner = {});
    ModelCpu &model(ConsumerHandle);
    // A frame-owner borrow prevents pool retirement while a manager retains a
    // native model/parts pointer. No pointer is serialized or narrowed to u32.
    std::shared_ptr<const void> borrowModel(ConsumerHandle);
    void checkModelRelease(ConsumerHandle) const;
    void releaseModel(ConsumerHandle); // Frame-owner boundary; borrowers must be quiescent.
    std::size_t modelCount() const;
    void bindMessage(int type, ResourceLease, u32 language = 1);
    int messageCount(int type) const;
    ResourceLease message(int type, int number) const; // Raw BE words, no invented decoding.
    void bindCamera(ResourceLease);
    const CameraDataView &camera() const;
    CameraCpu cameraState;
    // Native binding table for typed original callbacks, owned by the room lifetime.
    int SetFunc(const char *, EventFunction);
    int GetFunc(EventFunction *, const char *);
    int GetFunc(const EventFunction **, const char *); // Borrowed outside cancellable stack.
    ConsumerHandle event(ResourceLease);
    EventCpu &event(ConsumerHandle);
    void invoke(ConsumerHandle, int mode, int argument);
    void setInvocationBoundary(std::function<void(const TaskEntry &)>);
    u64 epoch() const {
        check();
        return epoch_;
    }

  private:
    friend class PlayerRuntime;
    friend struct NativeObjectManager;
    // Constructor rollback only, never a public room/model release shortcut.
    void releaseUnpublishedModel(ConsumerHandle,const std::shared_ptr<ModelCpu>&);
    // Object manager owns the stable native slot and has already validated it.
    void releaseObjectModel(ConsumerHandle,ModelCpu &);
    void checkModelReferences(ConsumerHandle) const;
    struct Model;
    struct Message;
    struct Control {
        EngineConsumers *owner;
        const std::thread::id thread;
    };
    struct Invocation {
        TaskEntry call;
        std::optional<TaskHandle> task;
        bool finished = false;
    };
    NativeScheduler *domain_;
    const std::thread::id owner_ = std::this_thread::get_id();
    u64 identity_, epoch_ = 1, next_ = 1;
    std::map<u64, std::unique_ptr<Model>> models_;
    EventResources eventResources_;
    std::function<bool()> adaOverride_;
    std::unique_ptr<ModelRuntime> modelRuntime_;
    std::map<u64, std::unique_ptr<EventCpu>> events_;
    std::map<int, std::unique_ptr<Message>> messages_;
    std::optional<CameraDataView> camera_;
    std::array<std::pair<std::string, EventFunction>, 16> functions_{};
    std::function<void(const TaskEntry &)> invocation_;
    std::shared_ptr<Control> control_;
    std::vector<std::unique_ptr<Invocation>> invocations_;
    void check() const;
    void validate(ConsumerHandle) const;
};
} // namespace re4::port
