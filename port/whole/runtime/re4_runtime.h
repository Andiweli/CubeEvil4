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
void re4_module_unload(const char* name);
const char* re4_module_name(unsigned id);
/* Finds an exported function of a loaded module. */
void* re4_try_resolve_import(const char* symbol);
/* Like re4_try_resolve_import, but logs and aborts when nothing provides it. */
void* re4_resolve_import(const char* importer, const char* symbol);

/* The game's entry point (src/game/main.cpp, renamed by patch 0008). */
int re4_game_main(void);

#ifdef __cplusplus
}
#endif

#endif
