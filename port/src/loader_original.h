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
    bool ownsBuffer = false;
    int transferId = -1;
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
    int state = 0, command = 0;
    cDataUnit *pData = nullptr;
    u8 flags = 0, arg = 0;
    u8 no = 0;
    void setBlockCommand(int cmd, int a);
    void setBlockLoadToMram();
    int checkBlockLoadToMramSet();
    int checkBlockLoadToAramSet();
    int checkBlockLoadToAram();
    int checkBlockDelete();
};
struct cBlock {
    u8 nBlock = 0;
    int stopFlagSet = 0, noMemCtrl = 1;
    void *getBlockMemFree(u32);
    std::vector<cBlockUnit> units;
    cBlockUnit *getUnitPtr(u8 no) { return no < nBlock ? &units[no] : nullptr; }
    int getBlockWork(u8 no);
};
struct Log {
    void err(int, int, const char *message, ...);
};
struct cAramQueue {
    u32 be_flag = 0, type = 0, len = 0;
    std::uintptr_t src = 0, dst = 0;
    cAramQueue *next = nullptr;
    std::size_t cursor = 0;
    void SetFlag(u32 f) { be_flag |= f; }
    int CkFlag(u32 f) const { return (be_flag & f) ? 1 : 0; }
    void PushQueue() { be_flag = 0; }
};
struct cAram {
    cAramQueue *pCur_queue = nullptr;
    cAramQueue *pQueue_list = nullptr;
    cAramQueue AramQueue[16];
    int DmaTransReq(int type, std::uintptr_t src, std::uintptr_t dst, u32 len, int mode);
    cAramQueue *pullAramQueue(int *id);
    void DmaTrans(cAramQueue *q, int mode);
    int TransCheck(int id);
    int DmaCancel(int id);
    void DmaCancelAll();
    void watcher();
    void advance(std::size_t bytes);
};
struct Context {
    const std::thread::id owner = std::this_thread::get_id();
    const std::uint64_t runtime;
    std::shared_ptr<const ImportedFiles> files;
    u8 disc = 0;
    NativeScheduler scheduler;
    AsyncFileReader reader;
    cDvd dvd;
    cAram aram;
    cDataCtrl dc{};
    cBlock block;
    DVD_REQ_WORK req;
    GameLoadState game;
    Log log;
    struct Allocation {
        std::size_t bytes;
        std::shared_ptr<void> storage;
    };
    std::map<void *, Allocation> allocations;
    MemoryLimits limits;
    std::shared_ptr<std::atomic<std::uint64_t>> allocatedBytes =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::vector<u8> aramStorage;
    std::array<std::uint64_t, 32> resourceGenerations{};
    std::array<int, 32> resourceErrors{};
    std::array<std::shared_ptr<const LoadedFile>, 32> unitFiles{};
    struct FileCacheEntry {
        std::shared_ptr<LoadedFile> file;
        std::uint64_t used;
    };
    struct DecodedCacheEntry {
        std::shared_ptr<const std::vector<u8>> body;
        std::uint64_t used;
    };
    std::map<std::string, FileCacheEntry> fileCache;
    std::map<std::string, DecodedCacheEntry> decodedCache;
    std::uint64_t cacheClock = 0, fileCacheHits = 0, decodedCacheHits = 0, fileReads = 0;
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
                     std::function<void(std::uint64_t)> observer, MemoryLimits limits);
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
constexpr u32 ARAM_FREE_BASE = 0x800000;
u32 aramOffset(const void *address);
void *aramPointer(u32 address);
std::uintptr_t nativeAddress(const void *address);
u32 alignedSize(u32 bytes);
void validateAram(std::uintptr_t address, u32 bytes);
void validateMram(const void *address, u32 bytes);
void checkedMove(void *destination, const void *source, u32 bytes);
void resourceRegistered(cDataUnit *unit);
void unitFileReady(cDataUnit *unit);
void unitFileCleared(cDataUnit *unit);
void nativeUnitFault(cDataUnit *unit);
void waitAram(int id);
void checkAramRequest(int type, std::uintptr_t src, std::uintptr_t dst, u32 len);
std::string fileCacheKey(const cDvdQueue &queue);
void cacheFile(cDvdQueue &queue);
void cacheDecoded();
void resourceBlockCommands();
void resourceBlockConditions();
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
#define Aram (re4::port::loading::context().aram)
#define ARAM_END (re4::port::loading::context().limits.aramEnd)
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
#define BLOCK_NO_DATA 0
#define BLOCK_MRAM_LOAD_SET 1
#define BLOCK_MRAM_LOAD 2
#define BLOCK_CREATE 3
#define BLOCK_ARAM_LOAD_SET 4
#define BLOCK_ARAM_LOAD 5
#define BLOCK_ARAM_OK 6
#define BLOCK_DELETE 7
#define BLOCK_CMD_NONE 0
#define BLOCK_CMD_MRAM_LOAD 1
#define BLOCK_CMD_ARAM_LOAD 2
#define BLOCK_CMD_DELETE 3
