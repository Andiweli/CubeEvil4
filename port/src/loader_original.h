#pragma once
#include "datactrl_native.h"
#include "re4/port/async_file.h"
#include "re4/port/engine_loader.h"
#include "re4/port/sha256.h"
#include "re4/port/yz2.h"
#include "types.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <thread>
namespace re4::port::loading {
enum { ST_PUSH = 0, ST_READ = 1, ST_COMPLETE = 2, ST_CANCEL = 3, ST_ERROR = 4 };
struct DVD_REQ_WORK {
    void *mram_addr = nullptr;
    u32 aram_addr = 0;
    u16 file_no = 0xffff, prio = 4, mode = 0;
    u32 offset = 0, size = 0;
    char fname[32]{}, reqfile[48]{};
    int reqline = 0;
};
struct FileTblEntry {
    const char *name;
    s32 entrynum;
};
extern const FileTblEntry FileTbl[];
extern const std::size_t FileTblCount;
struct DVD_READINFO {
    std::uintptr_t start_addr[2][32]{};
    u32 read_size[2][32]{}, mram_readsize = 0, aram_readsize = 0;
};
struct cDvdQueue {
    u32 m_Flag = 0;
    u8 m_Id = 0, m_Rno0 = 0;
    s8 m_ResultCode = 0;
    u16 m_Prio = 4;
    cDvdQueue *m_Next = nullptr;
    void *pBuff = nullptr;
    u32 tick = 0, mramSize = 0, aramSize = 0;
    char m_Name[32]{};
    std::uintptr_t addrTbl[2][32]{};
    u32 sizeTbl[2][32]{};
    std::uint64_t generation = 0;
    bool suspended = false;
    DVD_REQ_WORK request;
    std::shared_ptr<FileRead> io;
    std::shared_ptr<LoadedFile> result;
    const FileIdentity *identity = nullptr;
    Sha256 hash;
    std::size_t cursor = 0;
    std::optional<TaskHandle> task;
    void SetFlag(u32 f) { m_Flag |= f; }
    void ResetFlag(u32 f) { m_Flag &= ~f; }
    int CkFlag(u32 f) const { return (m_Flag & f) ? 1 : 0; }
    int EmptyCk() const { return !CkFlag(1); }
    void SetQueueStatus(int s) { m_Flag = (m_Flag & ~0x70000u) | (static_cast<u32>(s) << 16); }
    int GetQueueStatus() const { return static_cast<int>((m_Flag >> 16) & 7); }
    void Initialize();
    int LinkQueue();
    void PushQueue();
    int Read();
    void ErrMemFree();
};
class cDvd {
  public:
    cDvdQueue DvdQueue[16];
    cDvdQueue *pCur_queue = nullptr;
    cDvdQueue *pQueue_list = nullptr;
    u8 ReadID = 1;
    cDvdQueue *pullReadQueue();
    cDvdQueue *getQueuePtr(int id);
    int ReadReq();
    void ReadProc();
    void readProcMain(cDvdQueue *q);
    void blockRead(cDvdQueue *q);
    void Watcher();
    int FileExistCheck(const char *name, u32 *size);
    int ReadCheck(int id, int *mram, int *aram, void **address);
    int readCheckMain(int id, DVD_READINFO *info);
    int ReadCancel(int id, int mode);
    void ReadNblk2Blk(int id);
};
struct GameLoadState {
    u16 stage_no = 0, room_id = 0;
    u32 systemFlags = 0;
    int IsDevConsole = 0;
    void *pRoom = nullptr;
    void *Rtp = nullptr;
    void *RoomMes = nullptr;
    void *pOsd = nullptr;
    void *pEmi = nullptr;
};
using EmiData = void;
struct cBlockUnit {
    cDataUnit *pData = nullptr;
    u16 flags = 0;
    u8 no = 0;
};
struct cBlock {
    u8 nBlock = 0;
    std::vector<cBlockUnit> units;
    cBlockUnit *getUnitPtr(u8 no) { return no < nBlock ? &units[no] : nullptr; }
    int getBlockWork(u8 no);
};
struct Log {
    void err(int, int, const char *message, ...);
};
struct Context {
    const std::thread::id owner = std::this_thread::get_id();
    const std::uint64_t runtime;
    std::shared_ptr<const ImportedFiles> files;
    u8 disc = 0;
    NativeScheduler scheduler;
    AsyncFileReader reader;
    cDvd dvd;
    cDataCtrl dc{};
    cBlock block;
    DVD_REQ_WORK req;
    GameLoadState game;
    Log log;
    struct Allocation {
        std::size_t bytes;
        void *base;
    };
    std::map<void *, Allocation> allocations;
    std::uint64_t generation = 0;
    u32 stopwatch = 0;
    std::shared_ptr<const LoadedFile> lastFile;
    std::shared_ptr<Yz2Decoder> decoder;
    std::shared_ptr<const std::vector<u8>> decoded;
    std::shared_ptr<RoomArchive> room;
    std::shared_ptr<RoomArchive> roomDraft;
    bool busy = false;
    RoomLoadStage stage = RoomLoadStage::Idle;
    std::string failure;
    std::optional<TaskHandle> roomTask;
    std::set<std::string> observed;
    explicit Context(std::shared_ptr<const ImportedFiles> catalog, u8 selected,
                     std::function<void(std::uint64_t)> observer);
    ~Context();
};
Context &context();
void resetQueue(cDvdQueue *q);
void releaseQueueStorage(cDvdQueue *q);
void loaderTrace(const char *function);
u8 nextReadID();
void DvdReadProc();
int DvdReadN(const char *name, void *dst, int a, int b, int c, int mode, const char *file,
             int line);
int DvdRead(int fileNo, void *dst, u32 aram, u32 offset, u32 length, int mode, const char *file,
            int line);
void ReadAreaData();
void decodeData();
void *GetDataExt(void *data, const char *tag, int occurrence);
void *roomReadDestination();
void *allocate(u32 size);
void release(void *p);
int unsupportedAram(); // Always throws; int matches the original DMA assignment.
void TaskSleep(int n);
[[noreturn]] void TaskExit();
[[noreturn]] void iTaskExit();
bool iTaskExec(void (*entry)());
int iTaskStatus();
bool SysFlagChk(GameLoadState *, int);
void SysFlagOff(GameLoadState *, int);
u32 OSGetTick();
u32 OSTicksToMilliseconds(u32 n);
void StopwatchStart();
u32 StopwatchStop(void *);
void boundedDvdStep(cDvdQueue *q);
// These real platform operations replace SDK heap/cache APIs, not gameplay.
inline void Mem_free_h(void *p, u8) { release(p); }
inline void Debug_free_h(void *p, u8) { release(p); }
inline u8 MemGetCurrentHeap() { return 0; }
inline u8 MemGetCurrentDbgHeap() { return 0; }
inline void DCFlushRange(void *, u32) {
} // Coherent CPU-only buffers: no GPU/DMA accesses in this phase.
inline void OSReport(const char *, ...) {
} // Original debug output; test observations use typed state APIs.
} // namespace re4::port::loading
#define Dvd (re4::port::loading::context().dvd)
#define DC (re4::port::loading::context().dc)
#define Block (re4::port::loading::context().block)
#define DvdReqWork (re4::port::loading::context().req)
#define pG (&re4::port::loading::context().game)
#define pLog (&re4::port::loading::context().log)
#define in_data_addr (re4::port::loading::context().game.pRoom)
#define SYS_DATA_READ 0x02000000
#define MEM_ALLOC(size, alignment, tag) re4::port::loading::allocate(size)
#define Debug_alloc(size, alignment) re4::port::loading::allocate(size)
#define HALT() throw std::runtime_error("Original loader HALT")
#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)
