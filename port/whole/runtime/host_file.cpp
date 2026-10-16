// File access of the memory card file (runtime/memcard.cpp), built with the
// platform's own flags. memcard.cpp is built with the game's flags (it uses
// the SDK's SRAM structures), where the prelude's empty __asm__ drops the C
// library's symbol renames: glibc's 64-bit offset functions, and on Android
// the fortified wrappers of open/pread64/pwrite64, which call __open_real
// and friends, declared as renames of the plain functions (the first
// Android build of M10 did not link: "undefined symbol: __open_real"). Here
// the headers work as intended.
#include "re4_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {

int re4_host_file_open(const char* path)
{
    // the folders on the way, as needed (<data dir>/memcard/...)
    char dir[512];
    for (size_t i = 1; path[i] && i < sizeof(dir); i++) {
        if (path[i] == '/') {
            memcpy(dir, path, i);
            dir[i] = 0;
            mkdir(dir, 0775);
        }
    }
    return open(path, O_RDWR | O_CREAT, 0664);
}

long re4_host_file_read(int fd, void* data, unsigned long length, unsigned long long offset)
{
    return (long) pread(fd, data, length, (off_t) offset);
}

long re4_host_file_write(int fd, const void* data, unsigned long length, unsigned long long offset)
{
    return (long) pwrite(fd, data, length, (off_t) offset);
}

void re4_host_file_sync(int fd)
{
    fsync(fd);
}

void re4_host_file_close(int fd)
{
    close(fd);
}

const char* re4_host_file_error(void)
{
    return strerror(errno);
}

}  // extern "C"
