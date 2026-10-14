#pragma once
#include <stdarg.h>
#include <stdio.h>

// Host-only Android logging facade for the actual JNI bridge GLES check.
enum {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT = 1,
  ANDROID_LOG_VERBOSE = 2,
  ANDROID_LOG_DEBUG = 3,
  ANDROID_LOG_INFO = 4,
  ANDROID_LOG_WARN = 5,
  ANDROID_LOG_ERROR = 6,
  ANDROID_LOG_FATAL = 7,
  ANDROID_LOG_SILENT = 8
};
static inline int __android_log_print(int priority, const char *tag,
                                      const char *format, ...) {
  if (priority < ANDROID_LOG_WARN)
    return 0;
  fprintf(stderr, "%s: ", tag);
  va_list args;
  va_start(args, format);
  int result = vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
  return result;
}
