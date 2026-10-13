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

/* Folder with the extracted disc files (dvd.cpp): <dir>/disc0, <dir>/disc1,
 * each with sys/fst.bin, sys/boot.bin and files/. Set by the launcher. */
void re4_set_data_dir(const char* dir);
const char* re4_data_dir(void);
/* Disc number in the virtual drive (0/1), -1 when no disc folder was found. */
int re4_dvd_inserted_disc(void);

/* Byte order (byte_order.cpp): in-place swaps of big-endian disc data. */
void re4_swap32_words(void* p, unsigned bytes);
void re4_swap16_words(void* p, unsigned bytes);

/* Dolphin OS threads (os_thread.cpp). re4_os_run runs `entry` as the boot
 * thread and dispatches every OSThread until it returns. */
int re4_os_run(int (*entry)(void));
int re4_os_in_dispatcher(void);
void re4_os_exit_current(void* val);
void re4_os_interrupt_begin(void);
void re4_os_interrupt_end(void);

/* Interrupts (vi.cpp). re4_interrupt_point delivers the retraces that are due
 * (cheap when none is); re4_os_idle / re4_wait_interrupt wait for the next one. */
int re4_interrupt_point(void);
void re4_os_idle(void);
void re4_wait_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif
