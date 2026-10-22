#pragma once
// Host stand-in for <android/log.h>: the link check of the app's native
// library on Linux (CMakeLists.txt, re4decomp_link_check) writes to stderr.
#include <stdarg.h>
enum { ANDROID_LOG_VERBOSE = 2, ANDROID_LOG_DEBUG = 3, ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
#ifdef __cplusplus
extern "C" {
#endif
int __android_log_print(int prio, const char* tag, const char* fmt, ...);
int __android_log_write(int prio, const char* tag, const char* text);
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap);
#ifdef __cplusplus
}
#endif
