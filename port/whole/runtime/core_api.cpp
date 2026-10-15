// Embedding interface of the game core (runtime/re4_core.h): the Android app
// starts the game through it, the Linux checks too (re4_host --core-test).
//
// Threads: the game runs as the boot thread of the Dolphin OS emulation
// (re4_os_run) on a host thread of its own. It publishes one snapshot per game
// frame from its frame hook (PADRead, once per frame); the app's GL thread
// takes the newest one. Three buffers: the game writes one, the newest waits
// in another, the reader holds the third, so neither side ever waits for the
// other's work and a reader never sees a half-written frame.
#include "re4_core.h"
#include "re4_gx_frame_io.h"
#include "re4_runtime.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" void re4_pad_live(const Re4CorePad* pad);
extern "C" void re4_add_frame_hook(void (*hook)(unsigned));
extern "C" int re4_log_tail(char* out, int size);
extern "C" void re4_state_trace_reconfigure(void);

namespace {

pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
int gState = RE4_CORE_IDLE;
bool gReserved;
char gMessage[256] = "not started";

Re4CoreSnapshot gBuffers[3];
int gWriting = 0;   // the game's buffer
int gReady = -1;    // newest published frame, -1 none
int gReading = -1;  // held by the reader
uint32_t gSerial;

void setMessage(const char* text)
{
    pthread_mutex_lock(&gLock);
    snprintf(gMessage, sizeof(gMessage), "%s", text);
    pthread_mutex_unlock(&gLock);
    re4_log("core: %s", text);
}

// PADRead, once per game frame, on the game's thread.
void frameHook(unsigned frame)
{
    Re4CoreSnapshot* s = &gBuffers[gWriting];
    re4_bridge_capture(s);
    s->frame = frame;
    pthread_mutex_lock(&gLock);
    s->serial = ++gSerial;
    int published = gWriting;
    // the next buffer to write: neither the one held by the reader nor the
    // one just published
    int next = 0;
    while (next == published || next == gReading) {
        next++;
    }
    gReady = published;
    gWriting = next;
    pthread_mutex_unlock(&gLock);
}

int (*gEntry)(void) = re4_game_main;

void* gameThread(void*)
{
    re4_log("core: starting the game");
    int ret = re4_os_run(gEntry);
    char text[64];
    snprintf(text, sizeof(text), "re4_game_main returned %d", ret);
    setMessage(text);
    pthread_mutex_lock(&gLock);
    gState = RE4_CORE_ENDED;
    pthread_mutex_unlock(&gLock);
    return 0;
}

// RE4_BRIDGE_DUMP=FILE (Linux checks with disc data): every 300 frames one
// JSON line with what the app would draw -- step, room, camera and each model
// with its BIN/TPL disc sources -- so the local r100 run shows whether every
// drawn model can be found on the disc.
FILE* gDump;
Re4CoreSnapshot gDumpSnap;

// unknown sources as "?<where the address lies>" (formats.cpp)
extern "C" int re4_formats_describe(const void* addr, char* out, int size);

void dumpSource(FILE* f, const Re4CoreSource& s, uint32_t address)
{
    if (s.file[0]) {
        fprintf(f, "\"%s@0x%x#%d", s.file, (unsigned) s.fileOffset, (int) s.entry);
        if (s.innerOffset) {
            fprintf(f, "+0x%x", (unsigned) s.innerOffset);
        }
        fprintf(f, "\"");
    } else {
        char where[160];
        re4_formats_describe((const void*) (uintptr_t) address, where, sizeof(where));
        fprintf(f, "\"?%s\"", where);
    }
}

void dumpHook(unsigned frame)
{
    if (frame % 300 != 0) {
        return;
    }
    Re4CoreSnapshot* s = &gDumpSnap;
    re4_bridge_capture(s);
    FILE* f = gDump;
    fprintf(f, "{\"frame\":%u,\"Rno0\":%u,\"room\":\"0x%03x\",\"camera\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.1f]",
            frame, (unsigned) s->rno[0], (unsigned) s->room, s->cameraEye[0], s->cameraEye[1], s->cameraEye[2],
            s->cameraTarget[0], s->cameraTarget[1], s->cameraTarget[2], s->cameraRoll, s->cameraFovy);
    fprintf(f, ",\"screen\":%u,\"parts\":%u,\"dropped\":%u,\"droppedInfos\":%u,\"models\":[", (unsigned) s->screen,
            (unsigned) s->nParts, (unsigned) s->droppedModels, (unsigned) s->droppedInfos);
    for (unsigned i = 0; i < s->nModels; i++) {
        const Re4CoreModel& m = s->models[i];
        bool hidden = (m.flags & RE4_CORE_MODEL_HIDDEN) != 0;
        fprintf(f, "%s{\"mgr\":%u,\"id\":\"0x%02x\",\"type\":%u,\"drawn\":%d,\"parts\":%u,\"pos\":[%.1f,%.1f,%.1f],",
                i ? "," : "", (unsigned) m.manager, (unsigned) m.id, (unsigned) m.type,
                (m.beFlag & 6) == 6 && !hidden, (unsigned) m.nParts, m.matrix[3], m.matrix[7], m.matrix[11]);
        if (hidden) {
            fprintf(f, "\"hidden\":1,");  // the game leaves it out this frame (re4_core.h)
        }
        // how it is drawn, where it differs from a plain model
        fprintf(f, "\"ot\":%u,", (unsigned) m.otType);
        if (m.opacity != 1.0f) {
            fprintf(f, "\"opacity\":%.3f,", m.opacity);
        }
        if (m.zMode) {
            fprintf(f, "\"z\":%u,", (unsigned) m.zMode);
        }
        if (m.cullMode) {
            fprintf(f, "\"cull\":%u,", (unsigned) m.cullMode);
        }
        if (m.scrollIndex != RE4_CORE_NO_SCROLL) {
            fprintf(f, "\"smd\":[%d,%u],", (int) m.scrollBlock, (unsigned) m.scrollIndex);
        }
        fprintf(f, "\"infos\":[");
        for (unsigned k = 0; k < m.nInfos && m.firstInfo + k < s->nInfos; k++) {
            const Re4CoreModelInfo& mi = s->infos[m.firstInfo + k];
            fprintf(f, "%s{\"bin\":", k ? "," : "");
            dumpSource(f, mi.bin, mi.binAddress);
            fprintf(f, ",\"tpl\":");
            dumpSource(f, mi.tpl, mi.tplAddress);
            fprintf(f, ",\"shown\":%d,\"rigid\":%d", (mi.flags & 8) != 0, (int) mi.rigidPart);
            if (mi.blendMode) {
                fprintf(f, ",\"blend\":%u", (unsigned) mi.blendMode);
            }
            if (mi.color[3] != 0xFF || mi.opacity != 1.0f) {
                fprintf(f, ",\"alpha\":%u,\"opacity\":%.3f", (unsigned) mi.color[3], mi.opacity);
            }
            if (mi.nMorph) {
                fprintf(f, ",\"morph\":%u", (unsigned) mi.nMorph);
            }
            fprintf(f, "}");
        }
        fprintf(f, "]}");
    }
    fprintf(f, "]}\n");
    fflush(f);
}

// RE4_BRIDGE_SNAPSHOT=DIR (Linux checks with disc data): whole snapshots as
// binary files (DIR/snapshot-<frame>.bin, the Re4CoreSnapshot bytes; the
// layout is the same on 32 and 64 bit) once the game is in its main loop: 30,
// 150, 300, 450, 600 and 750 frames after it starts and every 2500 frames, at
// most 14. The app's host tool
// core_snapshot_render draws them as the app would (pictures in the report).
char gSnapDir[1024];
unsigned gSnapCount;
long gSnapFirst = -1;

// the opening of a room (r100: the car ride of event s40 runs about 700
// frames) in steps, then every 2500 frames
const unsigned kSnapAfterStart[] = {30, 150, 300, 450, 600, 750};

void snapshotHook(unsigned frame)
{
    if (gSnapCount >= 14) {
        return;
    }
    Re4CoreSnapshot* s = &gDumpSnap;
    re4_bridge_capture(s);
    if (s->rno[0] != 3) {
        return;
    }
    if (gSnapFirst < 0) {
        gSnapFirst = frame;
    }
    bool wanted = frame % 2500 == 0;
    for (unsigned after : kSnapAfterStart) {
        wanted = wanted || frame == (unsigned) gSnapFirst + after;
    }
    if (!wanted) {
        return;
    }
    s->frame = frame;
    s->serial = gSnapCount + 1;
    char path[1100];
    snprintf(path, sizeof(path), "%s/snapshot-%06u.bin", gSnapDir, frame);
    if (FILE* f = fopen(path, "wb")) {
        fwrite(s, sizeof(*s), 1, f);
        fclose(f);
        gSnapCount++;
        re4_log("core: snapshot %s (%u models, %u parts, room r%03x)", path, (unsigned) s->nModels,
                (unsigned) s->nParts, (unsigned) s->room);
    }
}

// RE4_GX_DUMP=DIR (Linux checks with disc data): chosen frames as the GX
// renderer of the app gets them (DIR/gx-<frame>.bin, re4_gx_frame_io.h): some
// frames of the start (logos, title), the room schedule of the snapshots and
// every 2500 frames, at most 24. The hook takes chunks only around those
// frames: the first chunk after attaching starts over (RESET), so each file
// holds the whole state and its textures. The app's host tool
// gx_chunk_render draws them (pictures in the report).
char gGxDir[1024];
unsigned gGxCount;
bool gGxAttached;

bool gxWanted(unsigned frame)
{
    static const unsigned kEarly[] = {60, 300, 600, 900, 1200, 1500, 1800, 2400};
    for (unsigned f : kEarly) {
        if (frame == f) {
            return true;
        }
    }
    if (frame % 2500 == 0) {
        return true;
    }
    if (gSnapFirst >= 0) {
        for (unsigned after : kSnapAfterStart) {
            if (frame == (unsigned) gSnapFirst + after) {
                return true;
            }
        }
    }
    return false;
}

void gxDumpHook(unsigned frame)
{
    if (gGxAttached) {
        if (const Re4GxChunk* c = re4_core_gx_acquire()) {
            if (c->flags & RE4_GX_CHUNK_RESET) {
                char path[1100];
                snprintf(path, sizeof(path), "%s/gx-%06u.bin", gGxDir, frame);
                if (FILE* f = fopen(path, "wb")) {
                    bool ok = re4gxio::Write(f, *c, frame);
                    fclose(f);
                    if (ok) {
                        gGxCount++;
                        re4_log("core: GX frame %s (%u draws, %u vertices, %u texture bytes)", path,
                                (unsigned) c->draws, (unsigned) c->vertexCount, (unsigned) c->textureBytes);
                    }
                }
            }
            re4_core_gx_release(c);
            re4_core_gx_detach();
            gGxAttached = false;
        }
        return;
    }
    if (gGxCount < 24 && gxWanted(frame + 1)) {
        re4_core_gx_acquire();  // attaches; the chunk after the next display copy starts over
        gGxAttached = true;
    }
}

struct DumpInstall {
    DumpInstall()
    {
        const char* path = getenv("RE4_BRIDGE_DUMP");
        if (path && *path) {
            gDump = fopen(path, "w");
            if (gDump) {
                re4_add_frame_hook(dumpHook);
            }
        }
        const char* dir = getenv("RE4_BRIDGE_SNAPSHOT");
        if (dir && *dir) {
            snprintf(gSnapDir, sizeof(gSnapDir), "%s", dir);
            re4_add_frame_hook(snapshotHook);
        }
        const char* gx = getenv("RE4_GX_DUMP");
        if (gx && *gx) {
            snprintf(gGxDir, sizeof(gGxDir), "%s", gx);
            re4_add_frame_hook(gxDumpHook);
        }
    }
} gDumpInstall;

}  // namespace

extern "C" {

int re4_core_reserve(const char* dataDir, const char* moduleDir)
{
    if (gReserved) {
        return 1;
    }
    re4_set_data_dir(dataDir ? dataDir : "");
    re4_set_module_dir(moduleDir ? moduleDir : "");
    if (!re4_mem1_init()) {
        setMessage("MEM1 (0x80000000..0x83000000) is taken in this process");
        pthread_mutex_lock(&gLock);
        gState = RE4_CORE_FAILED;
        pthread_mutex_unlock(&gLock);
        return 0;
    }
    // The game tells its own data from offsets by bit 31 of the address
    // (re4_check_game_address aborts; here the app shows the reason instead).
    Dl_info info;
    const void* base = (const void*) &re4_game_main;
    if (dladdr(base, &info) && info.dli_fbase) {
        base = info.dli_fbase;
    }
    if ((unsigned long) base < RE4_MEM1_BASE) {
        char text[160];
        snprintf(text, sizeof(text), "libre4_main loaded at %p, below 0x80000000: unsupported", base);
        setMessage(text);
        pthread_mutex_lock(&gLock);
        gState = RE4_CORE_FAILED;
        pthread_mutex_unlock(&gLock);
        return 0;
    }
    gReserved = true;
    setMessage("memory reserved");
    return 1;
}

int re4_core_start(void)
{
    return re4_core_start_entry(re4_game_main);
}

int re4_core_start_entry(int (*entry)(void))
{
    gEntry = entry;
    pthread_mutex_lock(&gLock);
    bool ready = gReserved && gState == RE4_CORE_IDLE;
    if (ready) {
        gState = RE4_CORE_RUNNING;
    }
    pthread_mutex_unlock(&gLock);
    if (!ready) {
        return gState == RE4_CORE_RUNNING;
    }
    re4_add_frame_hook(frameHook);
    // the app sets the test switches (RE4_PAD_UNTIL_ROOM ...) after loading
    re4_state_trace_reconfigure();
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    int err = pthread_create(&thread, &attr, gameThread, 0);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        setMessage("cannot create the game thread");
        pthread_mutex_lock(&gLock);
        gState = RE4_CORE_FAILED;
        pthread_mutex_unlock(&gLock);
        return 0;
    }
    setMessage("running");
    return 1;
}

int re4_core_state(void)
{
    pthread_mutex_lock(&gLock);
    int s = gState;
    pthread_mutex_unlock(&gLock);
    return s;
}

const char* re4_core_message(void)
{
    // a static copy: the caller may read it while the game writes a new one
    static char copy[sizeof(gMessage)];
    pthread_mutex_lock(&gLock);
    memcpy(copy, gMessage, sizeof(copy));
    pthread_mutex_unlock(&gLock);
    return copy;
}

void re4_core_pad(const Re4CorePad* pad)
{
    re4_pad_live(pad);
}

const Re4CoreSnapshot* re4_core_acquire(void)
{
    pthread_mutex_lock(&gLock);
    const Re4CoreSnapshot* s = 0;
    if (gReady >= 0 && gReading < 0) {
        gReading = gReady;
        s = &gBuffers[gReading];
    }
    pthread_mutex_unlock(&gLock);
    return s;
}

void re4_core_release(const Re4CoreSnapshot* snapshot)
{
    pthread_mutex_lock(&gLock);
    if (gReading >= 0 && snapshot == &gBuffers[gReading]) {
        gReading = -1;
    }
    pthread_mutex_unlock(&gLock);
}

int re4_core_log_tail(char* out, int size)
{
    return re4_log_tail(out, size);
}

}  // extern "C"
