#pragma once
#include "re4/port/engine_loader.h"
#include "re4_runtime_types.h"
#include <array>
#include <map>
#include <thread>

namespace re4::port {
// Native CPU views. These are deliberately not the partial 32-bit wire classes.
struct ModelJoint {
    u8 partsNo{}, parentNo{}, x2{}, x3{};
    Vec center{};
};
struct ModelDataView {
    ModelJoint *pHead{};
    u8 nParts{};
    u32 version{};
};
struct ModelInfoView {
    ModelDataView *model_addr{};
    std::array<u8, 4> color{255, 255, 255, 255};
};
struct ModelCoord {
    Vec pos{}, ang{}, scale{1, 1, 1}, world{}, world_old{}, world_old2{};
    ModelCoord *pParent{};
};
struct ModelPart : ModelCoord {
    ModelPart *pList{};
};
// Selected original model methods execute over checked, stable native views.
// No claim of modelInit, skinning, collision, lighting or GPU registration.
struct ModelCpu : ModelPart {
    u32 be_flag = 1; // cCoord : cUnit(1), include/model.h.
    u8 nParts{}, kindid{}, ot_type{}, z_mode{};
    Vec pos_old{};
    struct {
        u32 EnableMask{};
    } LightInfo;
    ModelInfoView *pModelInfo{};
    void setPartsParent();
    ModelPart *getPartsPtr(int);
    void setNoSuspend(int);
    int isTrans();
    void updateOldPos();
};
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
struct CameraDataView {
    ResourceLease bytes;
    u8 version{}, cameras{}, areas{}, links{};
};
CameraDataView indexCameraData(ResourceLease);

class EngineConsumers;
struct EventCpu {
    EngineConsumers *manager{};
    EventPackageView package;
    u32 StatusFlag{};
    int FuncType{}, NowCut{}, NowFrame{};
    int GetFuncType() const { return FuncType; }
    int GetNowCut() const { return NowCut; }
    int GetNowFrame() const { return NowFrame; }
    bool FlgCkStatus(u32 n) const;
    void SetFuncType(int n) { FuncType = n; }
    void GetRoomNo(char *) const;
    void GetEventNo(char *) const;
    int ExeFunc(int mode, int parameter);
    int GetMod(void **, const char *, int, int);
};
using EventFunction = std::function<void(EventCpu *, u32)>;
struct MessageIndexCpu {
    u32 m_language = 1;
    u8 *m_Data[5]{};
    u16 *getAddr(int, int);
    int getMesNum(int);
    int getSpaceWidth();
};
struct CameraCpu {
    s8 areaNo{}, cameraNo{};
    u8 r0{};
    u32 m_system_flag{};
    int CurrentAreaNo();
    int CurrentCameraNo();
    void Disable();
    void AreaCheckOnOff(int);
};

// Room ownership, checked references and original CPU consumers on the frame owner.
// Package indexing is not Event::Init/ExePacket or a successfully played cutscene.
class EngineConsumers final {
  public:
    explicit EngineConsumers(NativeScheduler *domain = nullptr);
    ~EngineConsumers();
    EngineConsumers(const EngineConsumers &) = delete;
    EngineConsumers &operator=(const EngineConsumers &) = delete;
    void clear(); // Rejects live callback stacks; caller must quiesce other borrowers first.
    ConsumerHandle model(ResourceLease bin, std::string name);
    ModelCpu &model(ConsumerHandle);
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
