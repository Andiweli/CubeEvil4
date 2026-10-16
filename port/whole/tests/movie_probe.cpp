// Compiled with the game's flags and headers (re4_game_flags) for the movie
// test (re4_host --movie-test, port/tools/test_whole_movie.py): a Sofdec movie
// of the data folder is played through the CRI middleware of src/lib the way
// game/sofdec.cpp does it (SofdecInit, cSofdec::initApp / startApp / appMain /
// loadMvFrmFx / finishMovie), with the boot order of main.cpp (DVD, ARAM,
// Sofdec, then AI/AX as Snd_system_init). One loop pass is one video field:
// the audio frames of the field as vi.cpp delivers them (an interrupt), then
// the movie's main step. Every new frame (its number from the frame record)
// is converted to the Y8 / C4:4 textures like loadMvFrmFx and written back as
// planar YUV 4:2:0, the sound of every field is taken from the AI output.
#include "mwply.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
void ADXGC_SetupDvdFs(int mode);
void ADXM_ExecMain(void);
u32 ARInit(u32* stack, u32 entries);
u32 ARAlloc(u32 length);
void ARQInit(void);
void AIInit(u8* stack);
void AXInitEx(u32 mode);
void MIXInit(void);
void MIXSetSoundMode(u32 mode);
void MIXUpdateSettings(void);
typedef void (*AXCallback)(void);
AXCallback AXRegisterCallback(AXCallback callback);
u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap, u8 max_lod);
void re4_os_interrupt_begin(void);
void re4_os_interrupt_end(void);
void re4_ai_field(void);
int re4_audio_take(short* out, int frames);
const char* re4_data_dir(void);
int re4_mem1_init(void);
}

namespace {

int gErrors;
char gLastError[256];

void errorCallback(void*, const char* msg)
{
    // cSofdec's callback hangs (ap_mwply_err_func); the test counts and goes on
    gErrors++;
    snprintf(gLastError, sizeof(gLastError), "%s", msg ? msg : "?");
    fprintf(stderr, "[movie] CRI error: %s\n", gLastError);
}

// the part of the game's audio frame (snd_main.cpp cb_audio_frame) the movie
// voices need: MIX hands their volume and pan to AX
void audioFrame()
{
    MIXUpdateSettings();
}

void field()
{
    re4_os_interrupt_begin();
    re4_ai_field();
    re4_os_interrupt_end();
}

void writeWavHeader(FILE* f, unsigned frames)
{
    unsigned bytes = frames * 4;
    unsigned char h[44];
    memcpy(h, "RIFF", 4);
    unsigned v = 36 + bytes;
    memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16;
    memcpy(h + 16, &v, 4);
    unsigned short s = 1;
    memcpy(h + 20, &s, 2);
    s = 2;
    memcpy(h + 22, &s, 2);
    v = 32000;
    memcpy(h + 24, &v, 4);
    v = 32000 * 4;
    memcpy(h + 28, &v, 4);
    s = 4;
    memcpy(h + 32, &s, 2);
    s = 16;
    memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &bytes, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

// The Y8 (I8: 8x4 tiles) and C4:4 (IA8: 4x4 tiles, Cb then Cr) textures of
// loadMvFrmFx back to planes of the movie's size.
void untile(const u8* ybuf, const u8* cbuf, int tw, int th, int cw, int w, int h, u8* y, u8* cb, u8* cr)
{
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int tile = (py / 4) * (tw / 8) + px / 8;
            y[py * w + px] = ybuf[tile * 32 + (py % 4) * 8 + px % 8];
        }
    }
    (void) th;
    for (int py = 0; py < h / 2; py++) {
        for (int px = 0; px < w / 2; px++) {
            int tile = (py / 4) * (cw / 4) + px / 4;
            const u8* t = cbuf + tile * 32 + ((py % 4) * 4 + px % 4) * 2;
            cb[py * (w / 2) + px] = t[0];
            cr[py * (w / 2) + px] = t[1];
        }
    }
}

}  // namespace

// Plays `fname` for at most `maxFields` fields. Writes the frames to `yuvPath`
// (4:2:0 planes, one record per new frame number: a 16-byte header "FRM",
// frame number, first field, then the planes), the sound to `wavPath` (32 kHz
// stereo) and a summary line per frame to stderr. Returns the number of
// frames written, -1 when the player could not start.
extern "C" int re4probe_movie_play(const char* fname, const char* yuvPath, const char* wavPath, int maxFields)
{
    static bool booted;
    if (!booted) {
        booted = true;
        re4_mem1_init();  // MEM1 with the OS globals (bus clock at 0x800000F8)
        ADXGC_SetupDvdFs(0);  // cDvd::Init
        static u32 aramStack[3];
        ARInit(aramStack, 3);  // SndInit
        ARAlloc(0x6FC000);
        ARQInit();
        MWS_PLY_INIT_SFD prm;  // SofdecInit
        memset(&prm, 0, sizeof(prm));
        prm.disp_cycle = 59.94f;
        prm.x04 = 1;
        prm.x08 = 1;
        prm.x0C = 1;
        ADXM_SetCbErr(errorCallback, NULL);
        mwPlyInitSfdFx(&prm);
        AIInit(NULL);  // Snd_system_init (Snd_sound_mode_init: stereo)
        AXInitEx(1);
        MIXInit();
        MIXSetSoundMode(1);
        AXRegisterCallback(audioFrame);
    }
    gErrors = 0;

    // cSofdec::initApp: the size from the first 0x5000 bytes of the file
    char path[1024];
    snprintf(path, sizeof(path), "%s/disc0/files/%s", re4_data_dir(), fname);
    FILE* in = fopen(path, "rb");
    if (!in) {
        fprintf(stderr, "[movie] %s missing\n", path);
        return -1;
    }
    static u8 head[0x5000];
    size_t got = fread(head, 1, sizeof(head), in);
    fclose(in);
    MWS_SFD_HDRINF info;
    memset(&info, 0, sizeof(info));
    mwPlyGetHdrInf(head, (int) got, &info);
    int width = info.width, height = info.height;
    fprintf(stderr, "[movie] header: %d x %d\n", width, height);
    if (width <= 0 || height <= 0) {
        return -1;
    }

    // cSofdec::startApp
    // in AP_OBJ the parameters are followed by zeroed bytes the CRI library
    // reads as its buffer format (MWSFD_CRPRM is 0x30 bytes, include/sofdec.h)
    static struct {
        MWS_PLY_CPRM_SFD cprm;
        u8 pad[0xC];
    } ap;
    memset(&ap, 0, sizeof(ap));
    MWS_PLY_CPRM_SFD& cprm = ap.cprm;
    cprm.compo_mode = 0;
    cprm.ftype = 1;
    cprm.max_bps = 8000000;
    cprm.nfrm_pool_wk = 4;
    cprm.max_width = width;
    cprm.max_height = height;
    cprm.max_stm = 2;
    cprm.wksize = mwPlyCalcWorkCprmSfd(&cprm);
    void* work = aligned_alloc(32, ((size_t) cprm.wksize + 31) & ~(size_t) 31);
    memset(work, 0, (size_t) cprm.wksize);
    cprm.work = work;
    MWPLY hn = mwPlyCreateSofdec(&cprm);
    if (!hn) {
        fprintf(stderr, "[movie] mwPlyCreateSofdec failed (work %d bytes)\n", cprm.wksize);
        return -1;
    }
    mwPlyStartFname(hn, fname);

    // cSofdec::allocTexMem (draw mode 0)
    int w2 = (width / 2 + 31) & ~31;
    int texW = w2 * 2, texH = height;
    u32 ysize = GXGetTexBufferSize((u16) texW, (u16) texH, 1, 0, 0);
    u32 csize = GXGetTexBufferSize((u16) w2, (u16) (height / 2), 3, 0, 0);
    u8* ybuf = (u8*) aligned_alloc(32, (ysize + 31) & ~31u);
    u8* cbuf = (u8*) aligned_alloc(32, (csize + 31) & ~31u);
    memset(ybuf, 0, ysize);
    memset(cbuf, 0x80, csize);

    FILE* yuv = fopen(yuvPath, "wb");
    FILE* wav = fopen(wavPath, "wb");
    if (!yuv || !wav) {
        return -1;
    }
    unsigned char zero[44] = {0};
    fwrite(zero, 1, 44, wav);
    unsigned audioFrames = 0;
    size_t planes = (std::size_t) width * height * 3 / 2;
    u8* plane = (u8*) malloc(planes);
    int lastFno = -1, frames = 0, stat = 0, f = 0;
    bool trace = getenv("RE4_MOVIE_TRACE") != NULL;
    // real time, as in the game: the CRI player smooths the sound clock with
    // the time base (sfd_adxt.c sfadxt_UpdateTime, OSGetTime), so fields
    // faster than 59.94 Hz would make the clock lag at the end of the sound
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (; f < maxFields; f++) {
        long long due = (long long) f * 1000000000LL * 1001 / 60000;
        for (;;) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long long ns = (now.tv_sec - start.tv_sec) * 1000000000LL + (now.tv_nsec - start.tv_nsec);
            if (ns >= due) {
                break;
            }
            struct timespec wait = {0, (long) (due - ns)};
            nanosleep(&wait, NULL);
        }
        field();
        static short pcm[2 * 1024];
        int n = re4_audio_take(pcm, 1024);
        if (n > 0) {
            fwrite(pcm, 4, (size_t) n, wav);
            audioFrames += (unsigned) n;
        }
        // cSofdec::appMain
        ADXM_ExecMain();
        MWS_FRM frm;
        mwPlyGetCurFrm(hn, &frm);
        if (frm.bufadr != NULL) {
            mwPlyFxSetOutBufSize(hn, texW, texH);  // loadMvFrmFx
            mwPlyFxCnvFrmY84C44(hn, &frm, ybuf, cbuf);
            if (frm.fno != lastFno) {
                lastFno = frm.fno;
                untile(ybuf, cbuf, texW, texH, w2, width, height, plane, plane + width * height,
                       plane + width * height + (width / 2) * (height / 2));
                unsigned rec[4] = {0x4D5246u, (unsigned) frm.fno, (unsigned) f, (unsigned) (width | height << 16)};
                fwrite(rec, 4, 4, yuv);
                fwrite(plane, 1, planes, yuv);
                frames++;
            }
            mwPlyRelCurFrm(hn);
        }
        stat = mwPlyGetStat(hn);
        if (trace && f % 10 == 0) {
            int c = 0, s = 1;
            mwPlyGetTime(hn, &c, &s);
            fprintf(stderr, "[movie] field %d: status %d, time %d/%d, frame %d\n", f, stat, c, s, lastFno);
        }
        if (stat == MWE_PLY_STAT_PLAYEND || stat == MWE_PLY_STAT_ERROR) {
            break;
        }
    }
    int count = 0, scale = 0;
    mwPlyGetTime(hn, &count, &scale);
    fprintf(stderr, "[movie] end: status %d after %d fields, %d frames, time %d/%d, skipped %d decode / %d display, %d CRI errors\n",
            stat, f, frames, count, scale, mwPlyGetNumSkipDec(hn), mwPlyGetNumSkipDisp(hn), gErrors);
    // cSofdec::finishMovie
    mwPlyDestroy(hn);
    free(work);
    free(ybuf);
    free(cbuf);
    free(plane);
    fclose(yuv);
    writeWavHeader(wav, audioFrames);
    fclose(wav);
    return stat == MWE_PLY_STAT_PLAYEND ? frames : -2;
}
