/* Forced include for the whole-program compile probe: GameCube inline asm
 * (register pinning, PPC instructions, section directives) has no meaning
 * on ARM and is dropped. */
#define asm(...)
#define __asm__(...)
#define __asm(...)

/* libc functions the original include/libc headers leave out; the port links
 * the platform C library. */
#ifdef __cplusplus
extern "C" {
#endif
char *strcat(char *, const char *);
char *strstr(const char *, const char *);
#ifdef __cplusplus
}
#endif
