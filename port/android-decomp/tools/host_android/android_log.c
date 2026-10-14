/* Host stand-in for liblog (see android/log.h). */
#include "android/log.h"
#include <stdio.h>
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap)
{
    fprintf(stderr, "[%d %s] ", prio, tag);
    int n = vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    return n;
}
int __android_log_print(int prio, const char* tag, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return n;
}
