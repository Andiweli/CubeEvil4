// Logging for the port runtime and the generated stubs.
#include "re4_runtime.h"

#include <stdarg.h>
#include <stdio.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

extern "C" void re4_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __ANDROID__
    __android_log_vprint(ANDROID_LOG_INFO, "re4", fmt, ap);
#else
    fputs("[re4] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}

extern "C" void re4_stub_hit(const char* const* names, unsigned char* seen, int index)
{
    if (!seen[index]) {
        seen[index] = 1;
        re4_log("stub called: %s", names[index]);
    }
}
