// Byte order helpers for data from disc (big-endian, as the GameCube reads it
// natively). Converters swap a structure's fields in place right after it is
// loaded, so the original code reads host-order values.
#include "re4_runtime.h"

#include <string.h>

extern "C" {

void re4_swap32_words(void* p, unsigned bytes)
{
    unsigned char* b = (unsigned char*) p;
    for (unsigned i = 0; i + 4 <= bytes; i += 4) {
        unsigned v;
        memcpy(&v, b + i, 4);
        v = __builtin_bswap32(v);
        memcpy(b + i, &v, 4);
    }
}

void re4_swap16_words(void* p, unsigned bytes)
{
    unsigned char* b = (unsigned char*) p;
    for (unsigned i = 0; i + 2 <= bytes; i += 2) {
        unsigned char t = b[i];
        b[i] = b[i + 1];
        b[i + 1] = t;
    }
}

}  // extern "C"
