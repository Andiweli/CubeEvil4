#ifndef RE4_RUNTIME_H
#define RE4_RUNTIME_H

/* Platform runtime of the whole-program port: logging, generated stubs and
 * the REL module loader. Exported by libre4_main; plain C linkage so the
 * generated files and the launcher can use it without game headers. */

#ifdef __cplusplus
extern "C" {
#endif

void re4_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/* Generated stubs (stubs.cpp) report their first call through this. */
void re4_stub_hit(const char* const* names, unsigned char* seen, int index);
int re4_stub_count(void);

typedef struct Re4ModuleEntry {
    unsigned id;
    const char* name;
} Re4ModuleEntry;
extern const Re4ModuleEntry kRe4Modules[];

/* Directory holding libre4_<module>.so; set by the launcher. */
void re4_set_module_dir(const char* dir);
/* Loads a module library (idempotent); returns 0 on failure. */
void* re4_module_load(const char* name);
/* Unloads a module library; trampolines of the others into it resolve again. */
void re4_module_unload(const char* name);
int re4_module_is_loaded(const char* name);
/* OSLink support (os_link.cpp): converts a big-endian REL header from disc to
 * host byte order in place. 1 = converted, 0 = was host order, -1 = no REL. */
int re4_rel_header_to_host(void* rel);
const char* re4_module_name(unsigned id);
/* Adds a module id outside the original table (tests). */
void re4_module_register(unsigned id, const char* name);
/* Finds an exported function of a loaded module. */
void* re4_try_resolve_import(const char* symbol);
/* Like re4_try_resolve_import, but logs and aborts when nothing provides it. */
void* re4_resolve_import(const char* importer, const char* symbol);

/* The game's entry point (src/game/main.cpp, renamed by patch 0008). */
int re4_game_main(void);

/* GameCube main RAM at its original address (mem1.cpp). 48 MB like a
 * development console; the game itself assumes the 24 MB retail map. */
#define RE4_MEM1_BASE 0x80000000u
#define RE4_MEM1_SIZE 0x03000000u
#define RE4_MEM1_RETAIL_SIZE 0x01800000u
/* OSGetArenaLo at boot: end of main.dol in the original (SystemMemInit halts
 * above 0x8034FFFF). The DOL itself lives in libre4_main.so. */
#define RE4_MEM1_ARENA_LO 0x80300000u
/* The disc's file system table sits at the top of retail memory, above the
 * game's last heap (0x817F4000), as the IPL places it. */
#define RE4_MEM1_FST_ADDR 0x817F4000u
#define RE4_MEM1_FST_MAX 0x0000C000u
/* Retail console: the development paths (SN host file I/O) stay off. */
#define RE4_CONSOLE_TYPE 0x00000003u
/* Reserves MEM1 and writes the boot globals; 0 when the range is taken. */
int re4_mem1_init(void);
int re4_in_mem1(const void* p);
/* Game libraries must lie at 0x80000000 and above: the game tells its data
 * from offsets and table links by bit 31. Logs and aborts otherwise. */
void re4_check_game_address(const char* what, const void* addr);

/* Folder with the extracted disc files (dvd.cpp): <dir>/disc0, <dir>/disc1,
 * each with sys/fst.bin, sys/boot.bin and files/. Set by the launcher. */
void re4_set_data_dir(const char* dir);
const char* re4_data_dir(void);
/* Disc number in the virtual drive (0/1), -1 when no disc folder was found. */
int re4_dvd_inserted_disc(void);

/* Byte order (byte_order.cpp): in-place swaps of big-endian disc data. */
void re4_swap32_words(void* p, unsigned bytes);
void re4_swap16_words(void* p, unsigned bytes);

/* Formatumwandler (formats/): converts disc data to host byte order once per
 * load. Called by the game's DVD queue for every completed part (patch 0015;
 * `part` is its DvdHeader, `addr` the MRAM or ARAM address) and by the Yz2
 * decoder for every unpacked stream. */
void re4_formats_part_loaded(const char* name, int nest, int index, const void* part, unsigned addr);
void re4_formats_unpacked(void* dst, unsigned size);
// ARAM -> main memory DMA (aram.cpp): the data at dst came from src (ARAM)
void re4_formats_aram_copied(void* dst, const void* src, unsigned length);
// MemorySwap (patch 0028) exchanged size bytes between MRAM and ARAM
void re4_formats_memory_swapped(void* mram, unsigned aram, unsigned size);

/* ARAM contents (aram.cpp), 16 MB. */
unsigned char* re4_aram(void);

/* Dolphin OS threads (os_thread.cpp). re4_os_run runs `entry` as the boot
 * thread and dispatches every OSThread until it returns. */
int re4_os_run(int (*entry)(void));
int re4_os_in_dispatcher(void);
void re4_os_exit_current(void* val);
void re4_os_interrupt_begin(void);
void re4_os_interrupt_end(void);
int re4_os_in_interrupt(void);
/* Runs the registered handler of hardware interrupt `interrupt`
 * (__OSSetInterruptHandler) in interrupt context; 0 when there is none. */
int re4_os_hw_interrupt(short interrupt);
/* The emulated graphics processor (gx_fifo.cpp): its pending PE interrupts
 * (draw done, token) are delivered when a thread starts waiting and with the
 * video retraces, the moments the GP would have caught up on the GameCube. */
void re4_gx_deliver_interrupts(void);
/* Completions (DVD reads, ARAM DMAs) started inside an interrupt handler run
 * when the handler has returned: re4_os_defer queues one (0 when the queue is
 * full: complete at once), re4_os_deferred_set changes the value of a queued
 * one (a cancelled read), re4_os_run_deferred runs the queued ones (after
 * every audio frame and at the end of the interrupt). */
typedef void (*Re4Deferred)(void* object, int value);
int re4_os_defer(Re4Deferred fn, void* object, int value);
int re4_os_deferred_set(Re4Deferred fn, void* object, int value);
void re4_os_run_deferred(void);

/* AX (ax.cpp): the audio frames of one video field (200 per second; vi.cpp
 * calls it with every retrace), one audio frame, and the frames so far. */
void re4_ax_field(void);
void re4_ax_frame(void);
unsigned re4_ax_frames(void);

/* Interrupts (vi.cpp). re4_interrupt_point delivers the retraces that are due
 * (cheap when none is); re4_os_idle / re4_wait_interrupt wait for the next one. */
int re4_interrupt_point(void);
void re4_os_idle(void);
void re4_wait_interrupt(void);

/* Per-frame observer, called from PADRead (pad.cpp; the state trace and the
 * run control, state_trace.cpp). re4_pad_script_stop ends RE4_PAD_SCRIPT. */
void re4_set_frame_hook(void (*hook)(unsigned));
void re4_pad_script_stop(void);
/* The player is free in the room: RE4_PAD_FREE_SCRIPT starts (pad.cpp). */
void re4_pad_script_free(void);

/* Every OS thread and where it waits (os_thread.cpp; RE4_WAIT_TRACE=1 records
 * the places): the hang search of host/main.cpp on SIGUSR1. */
void re4_os_dump_threads(int fd);

/* Debug start room (dolphin_os.cpp): RE4_START_ROOM=0x100 preselects that
 * room in the original debug start menu (patch 0026, title.cpp titleExit). */
void re4_debug_start_room(unsigned short* room_id);
int re4_debug_start_room_set(void);

#ifdef __cplusplus
}
#endif

#endif
