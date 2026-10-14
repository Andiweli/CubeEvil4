#ifndef RE4_CORE_H
#define RE4_CORE_H

/* Embedding interface of the game core (libre4_main): what the Android app
 * (RE4-Android-Decomp, core mode) calls through dlsym, and what the Linux
 * checks (re4_host --core-test) call directly. Plain C and fixed-width types
 * only: the app is compiled separately and must not see game headers.
 *
 * Startup order (the app's JNI_OnLoad, before any GL work):
 *   re4_core_reserve(data, modules)   MEM1 at 0x80000000, folders, address check
 *   re4_core_start()                  the game in its own thread
 * Every frame (any thread):
 *   re4_core_pad(&pad)                controller channel 0
 *   s = re4_core_acquire() ... re4_core_release(s)
 *                                     the newest published game frame
 * The game publishes one snapshot per game frame (PADRead), so a reader never
 * sees a half-written frame; the matrices are the parts' world matrices
 * (cParts::mat after partsWorldCalc), the camera is pG->Camera.param. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE4_CORE_API_VERSION 3

enum Re4CoreState {
    RE4_CORE_IDLE = 0,     /* not started */
    RE4_CORE_RUNNING = 1,  /* the game thread runs */
    RE4_CORE_ENDED = 2,    /* re4_game_main returned (never on the GameCube) */
    RE4_CORE_FAILED = 3    /* could not start; re4_core_message says why */
};

/* GameCube controller state (PADStatus without the error byte); buttons as
 * the SDK defines them: LEFT 0x0001 RIGHT 0x0002 DOWN 0x0004 UP 0x0008 Z 0x0010
 * R 0x0020 L 0x0040 A 0x0100 B 0x0200 X 0x0400 Y 0x0800 START 0x1000. */
typedef struct Re4CorePad {
    uint16_t buttons;
    int8_t stickX, stickY;        /* main stick, about -100..100 like a real pad */
    int8_t substickX, substickY;  /* C stick */
    uint8_t triggerL, triggerR;   /* analog triggers 0..255 */
} Re4CorePad;

/* Where a model's data lies on the disc: `innerOffset` bytes into archive
 * entry `entry` of the archive that starts `fileOffset` bytes into
 * files/<file> (a DVD container part or a plain archive file) and is
 * `archiveSize` bytes long. innerOffset > 0: nested data (a BIN inside an SMD
 * entry, a model inside a room block). fileOffset 0xFFFFFFFF: the archive is
 * Yz2-packed on the disc (room archives; the app reads its decoded copy).
 * file[0] == 0: unknown (not loaded from an archive the converter saw). */
typedef struct Re4CoreSource {
    char file[52];
    uint32_t fileOffset;
    uint32_t archiveSize;
    int32_t entry;
    uint32_t innerOffset;
} Re4CoreSource;

/* One cModelInfo of a model: its BIN (cModelData) and TPL. The addresses
 * identify the loaded data across frames. */
typedef struct Re4CoreModelInfo {
    uint32_t binAddress, tplAddress;
    Re4CoreSource bin, tpl;
} Re4CoreModelInfo;

#define RE4_CORE_MAX_INFOS 4

/* A drawable model of the game (cModel): every live work of the enemy and
 * object managers, the player included. */
typedef struct Re4CoreModel {
    uint32_t address;      /* the cModel, stable while the work lives */
    uint32_t beFlag;       /* cUnit::be_flag; drawn when (beFlag & 6) == 6 */
    uint8_t manager;       /* 0 EmMgr (player, partner, enemies), 1 ObjMgr */
    uint8_t id, type;      /* cModel::id / type */
    uint8_t nInfos;
    uint16_t firstMatrix;  /* into Re4CoreSnapshot::parts */
    uint16_t nParts;
    float matrix[12];      /* cModel::mat, row-major 3x4 (world placement) */
    Re4CoreModelInfo infos[RE4_CORE_MAX_INFOS];
} Re4CoreModel;

/* Pose of one cParts: world matrix and bind position (the translation that
 * cParts::lt_inv_mat removes: palette = mat * lt_inv_mat). */
typedef struct Re4CorePart {
    float matrix[12];  /* row-major 3x4 */
    float bind[3];
} Re4CorePart;

#define RE4_CORE_MAX_MODELS 256

/* Re4CoreSnapshot::screen */
#define RE4_CORE_SCREEN_SUB 1       /* a sub screen is open (inventory, map, radio ...): drawn by
                                       the game in 2D; the camera fields hold the world camera
                                       (CamCtrl) instead of the sub screen's own */
#define RE4_CORE_SCREEN_EVENT 2     /* an event (cutscene) runs */
#define RE4_CORE_SCREEN_DEBUGCAM 4  /* the debug camera (pad 2) is on */
#define RE4_CORE_MAX_PARTS 4096

typedef struct Re4CoreSnapshot {
    uint32_t apiVersion;   /* RE4_CORE_API_VERSION */
    uint32_t serial;       /* +1 per published frame */
    uint32_t frame;        /* PADRead calls so far */
    uint8_t rno[4];        /* pG->Rno0..3: game task step (2 room init, 3 main loop) */
    uint16_t room;         /* pG->room_id (stage << 8 | room) */
    int8_t disc;           /* inserted disc 0/1, -1 none */
    uint8_t cameraValid;
    float cameraEye[3];    /* pG->Camera.param.Campos */
    float cameraTarget[3]; /* .Target */
    float cameraRoll;      /* .Roll, radians */
    float cameraFovy;      /* .Fovy, degrees (C_MTXPerspective) */
    uint16_t nModels;
    uint16_t nParts;
    uint32_t droppedModels;  /* models beyond the budget */
    uint32_t screen;         /* RE4_CORE_SCREEN_* */
    Re4CoreModel models[RE4_CORE_MAX_MODELS];
    Re4CorePart parts[RE4_CORE_MAX_PARTS];
} Re4CoreSnapshot;

/* Reserves MEM1 (call as early as possible), sets the disc folder (layout:
 * runtime/dvd.cpp) and the module library folder ("" = load by library name,
 * as an APK with uncompressed libraries needs) and checks that the game
 * library lies above 0x80000000. 1 = ready, 0 = not (see re4_core_message). */
int re4_core_reserve(const char* dataDir, const char* moduleDir);
/* Starts re4_game_main in a thread of its own (8 MB stack). 1 = started. */
int re4_core_start(void);
int re4_core_state(void);
/* Last start problem or state, for the screen. */
const char* re4_core_message(void);
/* Controller channel 0 from now on (replaces the test script). */
void re4_core_pad(const Re4CorePad* pad);
/* The newest snapshot (0 before the first frame); stays valid and unchanged
 * until re4_core_release. One reader at a time. */
const Re4CoreSnapshot* re4_core_acquire(void);
void re4_core_release(const Re4CoreSnapshot* snapshot);
/* The last log lines, newest last, '\n'-separated (diagnostic overlay). */
int re4_core_log_tail(char* out, int size);

/* Tests: like re4_core_start with another boot thread entry. */
int re4_core_start_entry(int (*entry)(void));

/* Internal: fills `out` from the game state (runtime/core_bridge.cpp, game
 * flags); called once per game frame by the core's frame hook. */
void re4_bridge_capture(Re4CoreSnapshot* out);
/* Internal (formats.cpp): the disc source of a converted archive entry that
 * starts at `addr`; 0 when no archive the converter saw holds it. */
int re4_formats_locate(const void* addr, Re4CoreSource* out);

#ifdef __cplusplus
}
#endif

#endif
