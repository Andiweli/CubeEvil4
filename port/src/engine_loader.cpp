#include "loader_original.h"
#include "re4/port/byte_order.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace re4::port {
namespace {
std::string key(std::string value) {
    if (value.empty() || value.size() > 255 || value.front() == '/' ||
        value.find('\\') != std::string::npos || value.find(':') != std::string::npos)
        throw std::invalid_argument("invalid original resource path");
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, '/'))
        if (part.empty() || part == "." || part == "..")
            throw std::invalid_argument("resource traversal");
    for (auto &c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
bool digest(const std::string &s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
std::uint32_t integer(const std::string &s) {
    std::size_t used = 0;
    auto n = std::stoull(s, &used);
    if (used != s.size() || s.empty() || s.front() == '-' || n > 0xffffffffu)
        throw std::invalid_argument("invalid catalog integer");
    return static_cast<std::uint32_t>(n);
}
} // namespace
ImportedFiles::ImportedFiles(std::string root, std::string catalog) {
    root_ = std::filesystem::canonical(std::filesystem::u8path(root)).u8string();
    std::ifstream in(std::filesystem::u8path(catalog));
    std::string line;
    if (!std::getline(in, line) || line != "RE4-IMPORT-CATALOG-1")
        throw std::invalid_argument("unsupported import catalog");
    std::map<std::pair<unsigned, std::string>, bool> seen;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        std::stringstream s(line);
        std::string fields[5], extra;
        for (auto &f : fields)
            if (!std::getline(s, f, '\t'))
                throw std::invalid_argument("truncated catalog row");
        if (std::getline(s, extra, '\t'))
            throw std::invalid_argument("extra catalog field");
        auto disc = integer(fields[0]);
        if (disc > 1)
            throw std::invalid_argument("invalid disc number");
        auto normalized = key(fields[3]);
        if (!digest(fields[4]) || !seen.emplace(std::make_pair(disc, normalized), true).second)
            throw std::invalid_argument("duplicate path or invalid hash");
        entries_.push_back({static_cast<std::uint8_t>(disc), integer(fields[1]), integer(fields[2]),
                            fields[3], fields[4]});
    }
    if (!in.eof() || entries_.empty())
        throw std::invalid_argument("empty/unreadable catalog");
}
const FileIdentity *ImportedFiles::find(std::uint8_t disc, const std::string &path) const {
    auto name = key(path);
    for (const auto &f : entries_)
        if (f.disc == disc && key(f.path) == name)
            return &f;
    return nullptr;
}
std::string ImportedFiles::blobPath(const FileIdentity &f) const {
    if (!digest(f.sha256))
        throw std::invalid_argument("invalid blob hash");
    auto base = std::filesystem::canonical(std::filesystem::u8path(root_) / "blobs");
    auto path = std::filesystem::canonical(base / f.sha256);
    if (path.parent_path() != base || !std::filesystem::is_regular_file(path))
        throw std::invalid_argument("blob escapes imported data directory");
    return path.u8string();
}
} // namespace re4::port
namespace re4::port::loading {
namespace {
thread_local Context *bound = nullptr;
std::atomic<std::uint64_t> runtimeIds{1};
struct Binding {
    Context *previous;
    explicit Binding(Context &c) : previous(bound) {
        if (std::this_thread::get_id() != c.owner)
            throw std::logic_error("loader owner thread required");
        bound = &c;
    }
    ~Binding() { bound = previous; }
};
} // namespace
Context::Context(std::shared_ptr<const ImportedFiles> catalog, u8 selected,
                 std::function<void(std::uint64_t)> observer, MemoryLimits memoryLimits)
    : runtime(runtimeIds.fetch_add(1)), files(std::move(catalog)), disc(selected),
      reader(std::move(observer)), limits(memoryLimits) {
    if (!limits.transferBytesPerFrame || limits.aramEnd <= ARAM_FREE_BASE ||
        limits.aramEnd > 0xD00000 || (limits.aramEnd & 31u))
        throw std::invalid_argument("invalid native memory limits");
}
Context &context() {
    if (!bound)
        throw std::logic_error("no bound engine loader");
    return *bound;
}
void Log::err(int, int, const char *message, ...) { throw std::runtime_error(message); }
void loaderTrace(const char *function) { context().observed.emplace(function); }
void releaseQueueStorage(cDvdQueue *q) {
    if (q->transferId >= 0)
        Aram.DmaCancel(q->transferId);
    if (q->ownsBuffer)
        release(q->pBuff);
    q->transferId = -1;
    q->ownsBuffer = false;
    q->io.reset();
    q->result.reset();
    q->pBuff = nullptr;
}
Context::~Context() {
    Binding binding(*this);
    if (roomTask)
        scheduler.kill(*roomTask);
    scheduler.killBackground();
    aram.DmaCancelAll();
    for (auto &q : dvd.DvdQueue) {
        if (q.io)
            q.io->cancel();
        if (q.task && q.task->slot != 4)
            scheduler.kill(*q.task);
    }
}
void resetQueue(cDvdQueue *q) {
    auto generation = ++context().generation;
    *q = cDvdQueue{};
    q->generation = generation;
}
u8 nextReadID() {
    for (unsigned n = 0; n < 255; ++n) {
        auto id = Dvd.ReadID++;
        if (Dvd.ReadID == 0)
            Dvd.ReadID = 1;
        bool used = false;
        for (const auto &q : Dvd.DvdQueue)
            if (q.CkFlag(1) && q.m_Id == id)
                used = true;
        if (!used)
            return id;
    }
    throw std::logic_error("DVD request ID exhaustion");
}
void TaskSleep(int n) { context().scheduler.sleep(n); }
[[noreturn]] void TaskExit() { context().scheduler.exit(); }
[[noreturn]] void iTaskExit() { context().scheduler.exitBackground(); }
void boundedDvdStep(cDvdQueue *q) {
    if (q->CkFlag(0x100))
        context().scheduler.backgroundCheckpoint(64 * 1024);
    else if (q->CkFlag(0x40000000)) {
        Aram.watcher();
        Aram.advance(context().limits.transferBytesPerFrame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
u32 OSGetTick() {
    return static_cast<u32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
}
u32 OSTicksToMilliseconds(u32 n) { return n; }
void StopwatchStart() { context().stopwatch = OSGetTick(); }
u32 StopwatchStop(void *) { return OSGetTick() - context().stopwatch; }
bool SysFlagChk(GameLoadState *game, int mask) {
    return (game->systemFlags & static_cast<u32>(mask)) != 0;
}
void SysFlagOff(GameLoadState *game, int mask) { game->systemFlags &= ~static_cast<u32>(mask); }
void *roomReadDestination() {
    return nullptr;
} // Owning native destination replaces the fixed heap-end staging address.
void cDvdQueue::Initialize() {
    request = DvdReqWork;
    if (request.mode & 8)
        validateAram(request.aram_addr, request.size);
    if (request.file_no != 0xffff) {
        if (request.file_no >= FileTblCount)
            throw std::out_of_range("FileTbl index");
        std::snprintf(m_Name, sizeof(m_Name), "%s", FileTbl[request.file_no].name);
    } else
        std::strcpy(m_Name, request.fname);
    pBuff = request.mram_addr;
    m_Prio = request.prio;
    if (request.mode & 1)
        SetFlag(0x40000000);
    else if (request.mode & 0x100)
        SetFlag(0x100);
    if (request.mode & 0x8000)
        SetFlag(0x80000000);
    if (request.mode & 0x40)
        SetFlag(0x20000000);
    identity = context().files->find(context().disc, m_Name);
}
int cDvdQueue::Read() {
    if (suspended)
        return 1;
    auto finish = [this](int status, int error) {
        SetQueueStatus(status);
        m_ResultCode = static_cast<s8>(-error);
        m_Rno0 = 3;
        SetFlag(0x400000);
        return 0;
    };
    if (CkFlag(0x100000)) {
        if (transferId >= 0) {
            Aram.DmaCancel(transferId);
            transferId = -1;
        }
        if (io) {
            io->cancel();
            if (io->state() == ReadState::Reading || io->state() == ReadState::Pending)
                return 1;
        }
        return finish(ST_CANCEL, 4);
    }
    try {
        auto &c = context();
        if (m_Rno0 == 0) {
            if (!identity)
                return finish(ST_ERROR, 1);
            auto hit = c.fileCache.find(fileCacheKey(*this));
            if (hit != c.fileCache.end() && hit->second.file->identity.sha256 == identity->sha256) {
                result = hit->second.file;
                hit->second.used = ++c.cacheClock;
                ++c.fileCacheHits;
            } else {
                io = c.reader.read(c.files->blobPath(*identity), 64 * 1024 * 1024);
                if (!io)
                    return 1;
                ++c.fileReads;
            }
            SetQueueStatus(ST_READ);
            m_Rno0 = 1;
            return 1;
        }
        if (m_Rno0 == 2) {
            if (Aram.TransCheck(transferId) != 1)
                return 1;
            transferId = -1;
            addrTbl[1][0] = request.aram_addr;
            sizeTbl[1][0] = result->bodySize;
            aramSize = result->bodySize;
            return finish(ST_COMPLETE, 0);
        }
        if (!result) {
            if (!io)
                throw std::logic_error("DVD read without I/O owner");
            if (io->state() == ReadState::Failed)
                return finish(ST_ERROR, 1);
            if (io->state() == ReadState::Cancelled)
                return finish(ST_CANCEL, 4);
            if (io->state() != ReadState::Complete)
                return 1;
            const auto &input = io->output();
            if (input.size() != identity->size)
                return finish(ST_ERROR, 6);
            auto n = std::min<std::size_t>(64 * 1024, input.size() - cursor);
            if (n)
                hash.update(input.data() + cursor, n);
            cursor += n;
            if (cursor < input.size())
                return 1;
            if (hash.finish() != identity->sha256)
                return finish(ST_ERROR, 6);
            result = std::make_shared<LoadedFile>();
            result->identity = *identity;
            result->bytes = io->takeOutput();
            u32 offset = request.offset, size = request.size ? request.size : identity->size;
            if (offset > result->bytes.size() || size > result->bytes.size() - offset)
                return finish(ST_ERROR, 6);
            if (CkFlag(0x80000000)) {
                auto parts = indexAssetContainer(result->bytes.data() + offset, size);
                result->bodyOffset = offset + parts.bodyOffset;
                result->bodySize = parts.bodySize;
                result->soundOffset = offset + parts.soundOffset;
                result->soundSize = parts.soundSize;
            } else {
                result->bodyOffset = offset;
                result->bodySize = size;
            }
            cacheFile(*this);
        }
        auto *src = result->bytes.empty() ? nullptr : result->bytes.data() + result->bodyOffset;
        if (request.mode & 8) {
            validateAram(request.aram_addr, result->bodySize);
            if (!ownsBuffer) {
                pBuff = allocate(result->bodySize);
                if (!pBuff)
                    return finish(ST_ERROR, 3);
                ownsBuffer = true;
                if (result->bodySize)
                    std::memcpy(pBuff, src, result->bodySize);
            }
            transferId =
                Aram.DmaTransReq(0, nativeAddress(pBuff), request.aram_addr, result->bodySize, 0);
            if (transferId < 0)
                return 1; // Original ARQ slots are bounded; retry, never fake completion.
            m_Rno0 = 2;
            return 1;
        }
        if (pBuff) {
            validateMram(pBuff, result->bodySize);
            if (result->bodySize)
                std::memcpy(pBuff, src, result->bodySize);
        } else
            pBuff = src;
        addrTbl[0][0] = nativeAddress(pBuff);
        sizeTbl[0][0] = result->bodySize;
        mramSize = result->bodySize;
        return finish(ST_COMPLETE, 0);
    } catch (const std::exception &) {
        return finish(ST_ERROR, 6);
    }
}
void cDvdQueue::ErrMemFree() {
    result.reset();
    io.reset();
}
cDvdQueue *cDvd::getQueuePtr(int id) {
    if (id <= 0 || id > 255)
        return nullptr;
    for (auto &q : DvdQueue)
        if (q.CkFlag(1) && q.m_Id == id)
            return &q;
    return nullptr;
}
int cDvd::ReadCheck(int id, int *mram, int *aram, void **address) {
    auto *q = id > 0 && id <= 255 ? getQueuePtr(static_cast<u8>(id)) : nullptr;
    auto owner = q ? q->result : nullptr;
    DVD_READINFO info;
    int ret = readCheckMain(id, &info);
    if (ret == 1) {
        if (mram)
            *mram = static_cast<int>(info.mram_readsize);
        if (aram)
            *aram = static_cast<int>(info.aram_readsize);
        if (address)
            *address = reinterpret_cast<void *>(info.start_addr[0][0]);
        context().lastFile = owner;
    }
    return ret; // The original decompiled overload lacks this return.
}
int cDvd::FileExistCheck(const char *name, u32 *length) {
    auto *f = context().files->find(context().disc, name);
    if (!f)
        return -1;
    if (length)
        *length = f->size;
    return static_cast<int>(f->fstIndex);
}
void cDvd::Watcher() {
    Aram.watcher();
    if (!pCur_queue && pQueue_list) {
        auto *q = pQueue_list;
        if (q->suspended)
            return;
        std::optional<TaskHandle> task;
        if (q->CkFlag(0x100))
            task = context().scheduler.execBackground(DvdReadProc);
        else
            task = context().scheduler.exec(3, DvdReadProc);
        if (task) {
            q->task = task;
            pCur_queue = q;
            pQueue_list = q->m_Next;
        }
    }
}
int cDvd::ReadCancel(int id, int mode) {
    auto *q = getQueuePtr(static_cast<u8>(id));
    if (!q)
        return -2;
    if (q->GetQueueStatus() >= ST_COMPLETE)
        return -1;
    q->SetFlag(0x100000);
    q->suspended = false;
    if (q->io)
        q->io->cancel();
    if (q->transferId >= 0) {
        Aram.DmaCancel(q->transferId);
        q->transferId = -1;
    }
    if (q != pCur_queue) {
        auto **link = &pQueue_list;
        while (*link && *link != q)
            link = &(*link)->m_Next;
        if (*link)
            *link = q->m_Next;
        q->SetQueueStatus(ST_CANCEL);
        q->SetFlag(0x800);
    }
    if (mode == 0x40) {
        // Worker never touches engine/destination buffers. Detach safely before
        // original setClear frees a data unit, without waiting for the file thread.
        if (q == pCur_queue) {
            if (q->task)
                context().scheduler.kill(*q->task);
            pCur_queue = nullptr;
        }
        q->SetQueueStatus(ST_CANCEL);
        q->SetFlag(0x800);
    }
    return 0;
}
void cDvd::ReadNblk2Blk(int id) {
    auto *q = getQueuePtr(static_cast<u8>(id));
    if (!q)
        return;
    // Original explicit demand for blocking data is honored; no frame calls this
    // unless a consumer asks to wait. Cancel/start never invokes this path.
    if (q == pCur_queue) {
        if (q->task)
            context().scheduler.kill(*q->task);
        pCur_queue = nullptr;
    } else {
        auto **p = &pQueue_list;
        while (*p && *p != q)
            p = &(*p)->m_Next;
        if (*p)
            *p = q->m_Next;
    }
    q->ResetFlag(0x100);
    q->SetFlag(0x40000000);
    blockRead(q);
}
void cDvd::blockRead(cDvdQueue *q) {
    // Unlike the shared GC header/scratch buffers, each native queue owns its
    // input and hash state. A suspended asynchronous read needs no buffer swap.
    auto *save = pCur_queue == q ? nullptr : pCur_queue;
    pCur_queue = q;
    try {
        ReadProc();
    } catch (...) {
        pCur_queue = save;
        throw;
    }
    pCur_queue = save;
}
void cDataUnit::setName(char *name) {
    if (!name) {
        m_name[0] = 0;
        return;
    }
    if (std::strlen(name) >= sizeof(m_name))
        throw std::invalid_argument("DC filename overrun");
    std::strcpy(m_name, name);
}
int roomReadCheck(int id, void **address) {
    int ret = Dvd.ReadCheck(id, nullptr, nullptr, address);
    if (ret < 0)
        throw std::runtime_error("ReadAreaData DVD failure " + std::to_string(ret));
    return ret;
}
bool iTaskExec(void (*entry)()) {
    if (entry != decodeData)
        throw std::logic_error("unexpected room background callback");
    if (context().scheduler.backgroundActive())
        return false;
    entry();
    return context().scheduler.backgroundActive();
}
void decodeData() {
    loaderTrace("native decodeData/Yz2DecodeExec");
    auto &c = context();
    if (!c.lastFile)
        throw std::logic_error("decodeData without completed DVD read");
    const auto &f = *c.lastFile;
    auto hit = c.decodedCache.find(f.identity.sha256);
    if (hit != c.decodedCache.end()) {
        c.decoded = hit->second.body;
        hit->second.used = ++c.cacheClock;
        ++c.decodedCacheHits;
        // Cached data is still published through the stackful background boundary.
        if (!c.scheduler.execBackground([&c] {
                c.scheduler.backgroundCheckpoint(std::numeric_limits<u32>::max());
                c.scheduler.exitBackground();
            }))
            throw std::logic_error("cached decoder slot lost");
        c.stage = RoomLoadStage::Decoding;
        return;
    }
    c.decoder = std::make_shared<Yz2Decoder>(f.bytes.data() + f.bodyOffset, f.bodySize);
    if (!startYz2Decode(c.scheduler, c.decoder))
        throw std::logic_error("decoder slot lost during submission");
    c.stage = RoomLoadStage::Decoding;
}
int iTaskStatus() {
    auto &c = context();
    if (c.scheduler.backgroundActive())
        return 1;
    if (!c.decoded && (!c.decoder || c.decoder->state() != DecodeState::Complete))
        throw std::runtime_error("ReadAreaData decode failed or cancelled");
    if (!c.decoded)
        c.decoded = c.decoder->sharedOutput();
    cacheDecoded();
    c.decoder.reset(); // Release compressed staging/work; body has separate ownership.
    c.game.pRoom = const_cast<u8 *>(c.decoded->data());
    return 0;
}
void *GetDataExt(void *data, const char *tag, int occurrence) {
    loaderTrace("native GetDataExt (bounded BE)");
    auto &c = context();
    if (!data || !tag || occurrence < 0)
        return nullptr;
    if (!c.decoded || data != c.decoded->data())
        throw std::invalid_argument("unowned GetDataExt archive");
    auto index = indexAssetBody(c.decoded->data(), c.decoded->size());
    auto *entry = index.find(tag, static_cast<u32>(occurrence));
    return entry ? const_cast<u8 *>(c.decoded->data() + entry->offset) : nullptr;
}
} // namespace re4::port::loading
namespace re4::port {
using loading::Binding;
struct EngineLoader::Impl {
    loading::Context c;
    explicit Impl(std::shared_ptr<const ImportedFiles> f, u8 disc,
                  std::function<void(std::uint64_t)> o, MemoryLimits limits)
        : c(std::move(f), disc, std::move(o), limits) {
        if (!c.files || disc > 1)
            throw std::invalid_argument("invalid loader catalog/disc");
        Binding binding(c);
        c.dc.initDataUnit();
    }
};
EngineLoader::EngineLoader(std::shared_ptr<const ImportedFiles> f, u8 disc,
                           std::function<void(std::uint64_t)> observer, MemoryLimits limits)
    : impl_(std::make_unique<Impl>(std::move(f), disc, std::move(observer), limits)) {}
EngineLoader::~EngineLoader() = default;
LoadRequest EngineLoader::request(const std::string &path, u16 mode, u16 priority, u32 offset,
                                  u32 length) {
    Binding binding(impl_->c);
    key(path);
    auto &c = impl_->c;
    if (path.size() >= 32)
        throw std::invalid_argument("original DVD name capacity exceeded");
    c.req = loading::DVD_REQ_WORK{};
    c.req.prio = priority;
    c.req.mode = mode;
    c.req.offset = offset;
    c.req.size = length;
    std::strcpy(c.req.fname, path.c_str());
    auto id = c.dvd.ReadReq();
    if (id < 0)
        throw std::runtime_error("DVD queue full");
    auto *q = c.dvd.getQueuePtr(static_cast<u8>(id));
    if (!q) {
        for (auto &slot : c.dvd.DvdQueue)
            if (slot.m_Id == id) {
                q = &slot;
                break;
            }
    }
    if (!q)
        throw std::logic_error("lost DVD request identity");
    return {c.runtime, q->generation, static_cast<u8>(q - c.dvd.DvdQueue), q->m_Id};
}
LoadRequest EngineLoader::requestFile(u16 index, u16 mode, u32 offset, u32 length) {
    Binding binding(impl_->c);
    auto &c = impl_->c;
    auto id = loading::DvdRead(index, nullptr, 0, offset, length, mode, __FILE__, __LINE__);
    if (id < 0)
        throw std::runtime_error("DvdRead FileTbl request rejected");
    auto *q = c.dvd.getQueuePtr(id);
    if (!q) {
        for (auto &slot : c.dvd.DvdQueue)
            if (slot.m_Id == id) {
                q = &slot;
                break;
            }
    }
    if (!q)
        throw std::logic_error("lost FileTbl request identity");
    return {c.runtime, q->generation, static_cast<u8>(q - c.dvd.DvdQueue), q->m_Id};
}
LoadStatus EngineLoader::status(LoadRequest r) const {
    Binding binding(impl_->c);
    if (r.runtime != impl_->c.runtime || r.slot >= 16)
        return LoadStatus::Released;
    const auto &q = impl_->c.dvd.DvdQueue[r.slot];
    if (!q.CkFlag(1) || q.m_Id != r.id || q.generation != r.generation)
        return LoadStatus::Released;
    switch (q.GetQueueStatus()) {
    case loading::ST_PUSH:
        return LoadStatus::Pending;
    case loading::ST_READ:
        return LoadStatus::Reading;
    case loading::ST_COMPLETE:
        return q.CkFlag(0x800) ? LoadStatus::Complete : LoadStatus::Reading;
    case loading::ST_CANCEL:
        return q.CkFlag(0x800) ? LoadStatus::Cancelled : LoadStatus::Reading;
    default:
        return LoadStatus::Failed;
    }
}
int EngineLoader::readCheck(LoadRequest r, std::shared_ptr<const LoadedFile> &output) {
    Binding binding(impl_->c);
    if (status(r) == LoadStatus::Released)
        return -5;
    auto &q = impl_->c.dvd.DvdQueue[r.slot];
    auto result = q.result;
    auto ret = impl_->c.dvd.ReadCheck(r.id, nullptr, nullptr, nullptr);
    if (ret == 1)
        output = std::move(result);
    return ret;
}
bool EngineLoader::cancel(LoadRequest r) {
    Binding binding(impl_->c);
    if (status(r) == LoadStatus::Released)
        return false;
    return impl_->c.dvd.ReadCancel(r.id, 0) == 0;
}
bool EngineLoader::suspend(LoadRequest r) {
    Binding binding(impl_->c);
    auto s = status(r);
    if (s != LoadStatus::Pending && s != LoadStatus::Reading)
        return false;
    impl_->c.dvd.DvdQueue[r.slot].suspended = true;
    return true;
}
bool EngineLoader::resume(LoadRequest r) {
    Binding binding(impl_->c);
    if (status(r) == LoadStatus::Released)
        return false;
    impl_->c.dvd.DvdQueue[r.slot].suspended = false;
    return true;
}
void EngineLoader::cancelAll() {
    Binding binding(impl_->c);
    auto &c = impl_->c;
    for (auto &q : c.dvd.DvdQueue)
        if (q.CkFlag(1)) {
            c.dvd.ReadCancel(q.m_Id, 0x40);
            q.PushQueue();
        }
    c.dvd.pQueue_list = nullptr;
    c.dvd.pCur_queue = nullptr;
}
void EngineLoader::frame(BackgroundBudget budget) {
    Binding binding(impl_->c);
    auto &c = impl_->c;
    c.scheduler.runMain();
    c.dvd.Watcher();
    c.scheduler.runBackground(budget);
    c.aram.advance(c.limits.transferBytesPerFrame);
    try {
        loading::resourceBlockCommands();
        c.dc.check();
        loading::resourceBlockConditions();
    } catch (const std::exception &e) {
        const std::string message = e.what();
        cancelRoom();
        c.failure = message;
        c.stage = RoomLoadStage::Failed;
    }
}
bool EngineLoader::loadRoom(u16 id, bool blocks) {
    Binding binding(impl_->c);
    auto &c = impl_->c;
    if (c.busy || c.roomTask)
        return false;
    if ((id >> 8) < 1 || (id >> 8) > 4)
        throw std::invalid_argument("room outside original stage range");
    for (auto &unit : c.block.units)
        if (unit.pData)
            unit.pData->setDelete();
    c.block.units.clear();
    c.block.nBlock = 0;
    c.room.reset();
    c.decoded.reset();
    c.decoder.reset();
    c.lastFile.reset();
    c.failure.clear();
    c.game = {};
    c.game.room_id = id;
    c.game.stage_no = id >> 8;
    c.observed.clear();
    auto task = c.scheduler.exec(0, [&c, blocks] {
        try {
            loading::ReadAreaData();
            c.roomDraft = std::make_shared<RoomArchive>();
            auto &room = c.roomDraft;
            room->id = c.game.room_id;
            room->source = c.lastFile;
            room->body = c.decoded;
            room->index = indexAssetBody(room->body->data(), room->body->size());
            if (blocks) {
                auto *blk = room->find("BLK");
                if (blk) {
                    auto *data = room->body->data() + blk->offset;
                    BigEndianView view(data, blk->size);
                    if (blk->size < 24 || std::memcmp(data, "BLK\0", 4) || view.u16(4) != 0x100)
                        throw std::invalid_argument("invalid BLK header");
                    c.block.nBlock = data[7];
                    auto links = *view.u32(12);
                    if (links > blk->size ||
                        static_cast<std::size_t>(c.block.nBlock) * 12 > blk->size - links)
                        throw std::invalid_argument("BLK links outside archive");
                    c.block.units.resize(c.block.nBlock);
                    for (u8 i = 0; i < c.block.nBlock; ++i)
                        if (data[links + static_cast<std::size_t>(i) * 12] & 1) {
                            if (!c.block.getBlockWork(i))
                                throw std::runtime_error("original block file missing");
                            c.block.units[i].setBlockCommand(BLOCK_CMD_MRAM_LOAD, 0);
                            c.stage = RoomLoadStage::Blocks;
                        }
                    // Block commands are applied after background work/DC in the
                    // frame pipeline. Let that first visit issue the data requests
                    // before isUseOk diagnoses an idle unit as error 5.
                    if (c.block.nBlock)
                        loading::TaskSleep(1);
                    for (auto &unit : c.block.units)
                        if (unit.pData) {
                            while (!unit.pData->isUseOk()) {
                                if (unit.pData->getErr())
                                    throw std::runtime_error("block data unit failure");
                                loading::TaskSleep(1);
                            }
                            auto file = c.files->find(c.disc, unit.pData->getName());
                            if (!file)
                                throw std::logic_error("lost block identity");
                            auto slot = static_cast<std::size_t>(unit.pData - c.dc.nativeUnit(0));
                            auto owned = c.unitFiles[slot];
                            if (!owned || owned->identity.sha256 != file->sha256)
                                throw std::logic_error("lost verified block owner");
                            room->blockIndices.push_back(
                                indexAssetBody(owned->bytes.data(), owned->bytes.size()));
                            room->blocks.push_back(std::move(owned));
                        }
                }
            }
            c.room = std::move(room);
            c.stage = RoomLoadStage::Ready;
        } catch (const std::exception &e) {
            c.failure = e.what();
            c.stage = RoomLoadStage::Failed;
            for (auto &unit : c.block.units)
                if (unit.pData)
                    unit.pData->setDelete();
            c.roomDraft.reset();
        }
        c.busy = false;
        c.roomTask.reset();
        c.scheduler.exit();
    });
    if (!task)
        return false;
    c.roomTask = task;
    c.busy = true;
    c.stage = RoomLoadStage::Reading;
    return true;
}
bool EngineLoader::roomBusy() const {
    Binding binding(impl_->c);
    return impl_->c.busy;
}
std::shared_ptr<const RoomArchive> EngineLoader::room() const {
    Binding binding(impl_->c);
    return impl_->c.room;
}
std::string EngineLoader::roomError() const {
    Binding binding(impl_->c);
    return impl_->c.failure;
}
RoomLoadStage EngineLoader::roomStage() const {
    Binding binding(impl_->c);
    return impl_->c.stage;
}
void EngineLoader::cancelRoom() {
    Binding binding(impl_->c);
    auto &c = impl_->c;
    if (c.roomTask)
        c.scheduler.kill(*c.roomTask);
    c.roomTask.reset();
    c.scheduler.killBackground();
    cancelAll();
    for (auto &unit : c.block.units)
        if (unit.pData)
            unit.pData->setDelete();
    c.block.units.clear();
    c.busy = false;
    c.decoder.reset();
    c.decoded.reset();
    c.lastFile.reset();
    c.room.reset();
    c.roomDraft.reset();
    c.failure = "room load cancelled";
    c.stage = RoomLoadStage::Cancelled;
}
NativeScheduler &EngineLoader::scheduler() { return impl_->c.scheduler; }
std::size_t EngineLoader::liveAllocations() const {
    Binding binding(impl_->c);
    return impl_->c.allocations.size();
}
std::vector<std::string> EngineLoader::observedFunctions() const {
    Binding binding(impl_->c);
    auto &seen = impl_->c.observed;
    return {seen.begin(), seen.end()};
}
namespace {
loading::cDataUnit *unitFor(loading::Context &c, ResourceHandle h) {
    if (h.runtime != c.runtime || h.slot >= 32 || c.resourceGenerations[h.slot] != h.generation ||
        !c.dc.nativeUnit(h.slot)->isBeAlive())
        throw std::invalid_argument("stale/foreign resource handle");
    return c.dc.nativeUnit(h.slot);
}
ResourceHandle handleFor(loading::Context &c, loading::cDataUnit *u) {
    auto i = static_cast<std::size_t>(u - c.dc.nativeUnit(0));
    return {c.runtime, c.resourceGenerations[i], static_cast<u8>(i)};
}
} // namespace
ResourceHandle EngineLoader::registerResource(const std::string &path) {
    Binding bind(impl_->c);
    key(path);
    if (path.size() >= 32)
        throw std::invalid_argument("original resource name capacity");
    auto *u = impl_->c.dc.setData(const_cast<char *>(path.c_str()));
    if (!u)
        throw std::runtime_error("data unit registration failed");
    return handleFor(impl_->c, u);
}
ResourceInfo EngineLoader::resourceInfo(ResourceHandle h) const {
    Binding bind(impl_->c);
    auto *u = unitFor(impl_->c, h);
    auto state = u->getCondition();
    u32 aram = 0;
    if (state == COND_ARAM_OK || state == COND_ARAM_TO_MRAM || state == COND_ARAM_TO_ARAM)
        aram = loading::aramOffset(u->getAddr());
    else if (state == COND_ARAM_LOAD || state == COND_MRAM_TO_ARAM)
        aram = loading::aramOffset(u->getDestAddr());
    return {static_cast<ResourceState>(state),
            u->getCommand(),
            impl_->c.resourceErrors[h.slot] ? impl_->c.resourceErrors[h.slot] : u->getErr(),
            u->getSize(),
            aram,
            u->getName()};
}
void EngineLoader::loadResource(ResourceHandle h, Residency dest, bool wait,
                                const MemoryReservation &memory, u32 address) {
    Binding bind(impl_->c);
    auto &c = impl_->c;
    auto *u = unitFor(c, h);
    void *arg = nullptr;
    if (dest == Residency::Mram) {
        if (address)
            throw std::invalid_argument("ARAM address in MRAM request");
        if (memory.data) {
            if (memory.runtime != c.runtime || memory.size < u->getSize() || !memory.owner)
                throw std::invalid_argument("foreign/undersized MRAM reservation");
            loading::validateMram(memory.data, u->getSize());
            arg = memory.data;
        }
    } else {
        if (memory.data)
            throw std::invalid_argument("MRAM pointer in ARAM request");
        if (address) {
            loading::validateAram(address, u->getSize());
            auto end = address + loading::alignedSize(u->getSize());
            for (std::size_t i = 0; i < 32; ++i) {
                auto *other = c.dc.nativeUnit(i);
                if (other == u || !other->isBeAlive())
                    continue;
                auto st = other->getCondition();
                auto overlap = [&](const void *p) {
                    auto start = loading::aramOffset(p);
                    if (start && address < start + loading::alignedSize(other->getSize()) &&
                        start < end)
                        throw std::invalid_argument("overlapping ARAM destination");
                };
                if (st == COND_ARAM_OK || st == COND_ARAM_TO_MRAM || st == COND_ARAM_TO_ARAM)
                    overlap(other->getAddr());
                if (st == COND_ARAM_LOAD || st == COND_MRAM_TO_ARAM)
                    overlap(other->getDestAddr());
                if (st == COND_ARAM_TO_ARAM)
                    overlap(other->getArgAddr());
            }
            arg = loading::aramPointer(address);
        }
    }
    c.resourceErrors[h.slot] = 0;
    u->setErr(0);
    try {
        u->setCommand(dest == Residency::Mram ? CMND_MRAM_LOAD : CMND_ARAM_LOAD, arg, wait ? 1 : 0);
        if (wait) {
            int ok = dest == Residency::Mram ? u->waitUseOk() : u->waitLoadOk();
            if (!ok)
                throw std::runtime_error("original resource wait failed");
        }
    } catch (...) {
        loading::nativeUnitFault(u);
        throw;
    }
}
void EngineLoader::clearResource(ResourceHandle h) {
    Binding bind(impl_->c);
    auto *u = unitFor(impl_->c, h);
    u->setClear();
    u->setErr(0);
    impl_->c.resourceErrors[h.slot] = 0;
    impl_->c.unitFiles[h.slot].reset();
}
void EngineLoader::deleteResource(ResourceHandle h) {
    Binding bind(impl_->c);
    auto *u = unitFor(impl_->c, h);
    u->setDelete();
    impl_->c.unitFiles[h.slot].reset();
}
MemoryReservation EngineLoader::reserveMemory(u32 bytes) {
    Binding bind(impl_->c);
    auto &c = impl_->c;
    auto *p = loading::allocate(bytes);
    if (!p)
        throw std::bad_alloc();
    return {c.allocations.at(p).storage, static_cast<u8 *>(p), bytes, c.runtime};
}
void EngineLoader::releaseMemory(MemoryReservation &m) {
    Binding bind(impl_->c);
    auto &c = impl_->c;
    if (m.runtime != c.runtime || !m.data)
        throw std::invalid_argument("foreign memory reservation");
    for (std::size_t i = 0; i < 32; ++i) {
        auto *u = c.dc.nativeUnit(i);
        if (u->isBeAlive() && (u->getAddr() == m.data || u->getDestAddr() == m.data))
            throw std::logic_error("reservation still referenced by a data unit");
    }
    loading::release(m.data);
    m = {};
}
ResourceReference EngineLoader::reference(ResourceHandle h, u32 offset, u32 bytes) const {
    Binding bind(impl_->c);
    auto *u = unitFor(impl_->c, h);
    if (offset > u->getSize() || bytes > u->getSize() - offset)
        throw std::out_of_range("resource reference bounds");
    return {h, offset, bytes};
}
ResourceLease EngineLoader::resolve(ResourceReference ref) const {
    Binding bind(impl_->c);
    auto *u = unitFor(impl_->c, ref.resource);
    if (u->getCondition() != COND_MRAM_OK)
        throw std::logic_error("resource is not MRAM ready");
    reference(ref.resource, ref.offset, ref.size);
    auto *p = static_cast<const u8 *>(u->getAddr());
    loading::validateMram(p, u->getSize());
    for (const auto &a : impl_->c.allocations) {
        auto base = loading::nativeAddress(a.first), address = loading::nativeAddress(p);
        if (address >= base && address - base <= a.second.bytes &&
            u->getSize() <= a.second.bytes - (address - base))
            return {a.second.storage, p + ref.offset, ref.size};
    }
    throw std::logic_error("lost MRAM owner");
}
std::vector<BlockResource> EngineLoader::blockResources() const {
    Binding bind(impl_->c);
    std::vector<BlockResource> out;
    for (const auto &u : impl_->c.block.units)
        if (u.pData)
            out.push_back({u.no, handleFor(impl_->c, u.pData), u.state});
    return out;
}
void EngineLoader::setBlockResidency(u8 no, Residency dest) {
    Binding bind(impl_->c);
    auto *u = impl_->c.block.getUnitPtr(no);
    if (!u || !u->pData)
        throw std::out_of_range("unregistered block");
    u->setBlockCommand(dest == Residency::Mram ? BLOCK_CMD_MRAM_LOAD : BLOCK_CMD_ARAM_LOAD, 0);
}
void EngineLoader::clearBlock(u8 no) {
    Binding bind(impl_->c);
    auto *u = impl_->c.block.getUnitPtr(no);
    if (!u || !u->pData)
        throw std::out_of_range("unregistered block");
    // Explicit resource abort is immediate; normal deferred block deletion uses
    // the original checkBlockDelete once data/transfer is resident.
    clearResource(handleFor(impl_->c, u->pData));
    u->state = BLOCK_NO_DATA;
    u->command = BLOCK_CMD_NONE;
}
void EngineLoader::setResourceControl(bool enabled, bool hold) {
    Binding bind(impl_->c);
    impl_->c.dc.setDataCtrl(enabled ? 1 : 0);
    impl_->c.dc.setNBlkStop(hold ? 1 : 0);
}
ResourceMemoryStats EngineLoader::memoryStats() const {
    Binding bind(impl_->c);
    auto &c = impl_->c;
    ResourceMemoryStats s;
    s.mramBytes = c.allocatedBytes->load();
    s.fileCacheHits = c.fileCacheHits;
    s.decodedCacheHits = c.decodedCacheHits;
    s.fileReads = c.fileReads;
    s.aramBackingBytes = static_cast<u32>(c.aramStorage.size());
    for (const auto &e : c.fileCache)
        s.fileCacheBytes += e.second.file->bytes.size();
    for (const auto &e : c.decodedCache)
        s.decodedCacheBytes += e.second.body->size();
    for (const auto &q : c.aram.AramQueue)
        if (q.be_flag && !q.CkFlag(0x04000000))
            ++s.transfers;
    for (std::size_t i = 0; i < 32; ++i) {
        auto *u = c.dc.nativeUnit(i);
        if (!u->isBeAlive())
            continue;
        auto st = u->getCondition();
        if (st == COND_ARAM_LOAD || st == COND_ARAM_OK || st == COND_MRAM_TO_ARAM ||
            st == COND_ARAM_TO_MRAM)
            s.aramReservedBytes += loading::alignedSize(u->getSize());
        else if (st == COND_ARAM_TO_ARAM)
            s.aramReservedBytes += 2 * loading::alignedSize(u->getSize());
    }
    return s;
}
void EngineLoader::trimCaches() {
    Binding bind(impl_->c);
    impl_->c.fileCache.clear();
    impl_->c.decodedCache.clear();
}
void EngineLoader::setAramCompaction(bool on) {
    Binding bind(impl_->c);
    impl_->c.dc.setAramSort(on ? 1 : 0);
}
FrameViAdapter::FrameViAdapter(EngineLoader &l, u8 v) : loader_(l), vcnt_(v) {
    if (v == 0)
        throw std::invalid_argument("zero original Vcnt");
}
void FrameViAdapter::reset() {
    previous_ = -1;
    fraction_ = pending_ = 0;
}
std::uint32_t FrameViAdapter::onDisplayFrame(std::int64_t now) {
    if (now < 0)
        throw std::invalid_argument("negative frame timestamp");
    if (previous_ < 0) {
        previous_ = now;
        return 0;
    }
    if (now < previous_)
        throw std::invalid_argument("non-monotonic frame timestamp");
    auto delta = static_cast<std::uint64_t>(now - previous_);
    previous_ = now;
    if (delta > 1000000000ULL) {
        fraction_ = pending_ = 0;
        return 0;
    } // Lifecycle discontinuity, explicit no gameplay catch-up after pause.
    auto scaled = delta * 60 + fraction_;
    auto ticks = scaled / 1000000000ULL;
    fraction_ = scaled % 1000000000ULL;
    ticks_ += ticks;
    pending_ += ticks;
    auto frames = pending_ / vcnt_;
    pending_ %= vcnt_;
    for (std::uint64_t i = 0; i < frames; ++i) {
        loader_.frame();
        ++frames_;
    }
    return static_cast<std::uint32_t>(frames);
}
namespace {
struct EnglishBlock {
    u32 offset, count;
    std::size_t end;
};
EnglishBlock englishBlock(const u8 *data, std::size_t bytes) {
    BigEndianView view(data, bytes);
    auto off = view.u32(8);
    if (bytes < 28 || !off || *off > bytes || bytes - *off < 8)
        throw std::invalid_argument("incomplete original English MDT block");
    auto blockEnd = bytes;
    for (std::size_t i = 4; i <= 24; i += 4) {
        auto next = *view.u32(i);
        if (next > *off && next < blockEnd)
            blockEnd = next;
    }
    if (blockEnd - *off < 8)
        throw std::invalid_argument("overlapping language headers");
    auto count = *view.u32(*off + 4);
    if (count > (blockEnd - *off - 8) / 4)
        throw std::invalid_argument("English MDT offsets outside block");
    return {*off, count, blockEnd};
}
} // namespace
u32 englishMessageCount(const u8 *data, std::size_t bytes) {
    return englishBlock(data, bytes).count;
}
AssetSpan englishMessage(const u8 *data, std::size_t bytes, u32 message) {
    BigEndianView view(data, bytes);
    auto block = englishBlock(data, bytes);
    auto off = block.offset;
    auto count = block.count;
    auto blockEnd = block.end;
    if (message >= count)
        throw std::out_of_range("English message index");
    auto first = *view.u32(off + 8 + static_cast<std::size_t>(message) * 4);
    auto end = static_cast<u32>(blockEnd - off);
    // Original tables can alias messages and are not necessarily sorted by index.
    for (u32 i = 0; i < count; ++i) {
        auto next = *view.u32(off + 8 + static_cast<std::size_t>(i) * 4);
        if (next > first && next < end)
            end = next;
    }
    if (first < 8ULL + count * 4ULL || first % 2 || first > end || end - first < 2 ||
        end > blockEnd - off)
        throw std::invalid_argument("English message span outside block");
    return {message, off + first, end - first, 0, {'M', 'D', 'T', 0}};
}
} // namespace re4::port
