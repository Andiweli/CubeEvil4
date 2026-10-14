// re4_format_compare: the two sides report what they read as key/value pairs.
//
// The game side (game_side.cpp) is compiled with the game's flags and headers
// (gnu++11, the whole-program prelude), the app side (app_side.cpp) as C++17
// with the app's headers; the two cannot share C++ library types, so they
// meet at this plain C interface. Keys name the field ("SMD0.place.3.pos.x"),
// values are text: integers in decimal, floats as their bit pattern in hex
// (exact), strings as they are.
#ifndef RE4_COMPARE_FACTS_H
#define RE4_COMPARE_FACTS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Re4FactSink)(void* ctx, const char* key, const char* value);

struct Re4Facts {
    Re4FactSink sink;
    void* ctx;
};

// Game side: converts a copy of the big-endian input with the Formatumwandler
// (port/whole/runtime/formats) and reports the fields through the original
// structure definitions. Returns 0 when the input was not accepted.
int re4cmp_game_archive(const unsigned char* data, unsigned size, const char* name, struct Re4Facts* out);
int re4cmp_game_esl(const unsigned char* data, unsigned size, const char* name, struct Re4Facts* out);
int re4cmp_game_evd(const unsigned char* data, unsigned size, const char* name, struct Re4Facts* out);
int re4cmp_game_roominfo(const unsigned char* data, unsigned size, const char* name, struct Re4Facts* out);

#ifdef __cplusplus
}
#endif

// 32-bit FNV-1a, for long arrays (positions, display lists): both sides hash
// the same sequence of values.
static inline unsigned re4cmp_hash(unsigned h, const void* data, unsigned size)
{
    const unsigned char* p = (const unsigned char*) data;
    for (unsigned i = 0; i < size; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}
static inline unsigned re4cmp_hash_u32(unsigned h, unsigned v)
{
    unsigned char b[4] = { (unsigned char) (v >> 24), (unsigned char) (v >> 16), (unsigned char) (v >> 8),
                           (unsigned char) v };
    return re4cmp_hash(h, b, 4);
}
#define RE4CMP_HASH_START 2166136261u
// Arrays longer than this are compared by count and hash; the first entries
// are also listed one by one, so a difference shows where it starts.
#define RE4CMP_LIST 48

#ifdef __cplusplus
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Writes facts under a key prefix; both sides format keys and values with
// this one implementation, so equal fields give equal text.
struct Re4Out {
    struct Re4Facts* facts;
    char pre[200];

    void emit(const char* value, const char* fmt, va_list ap) const
    {
        char key[400];
        int n = snprintf(key, sizeof(key), "%s", pre);
        vsnprintf(key + n, sizeof(key) - n, fmt, ap);
        facts->sink(facts->ctx, key, value);
    }
    void u(unsigned long v, const char* fmt, ...) const
    {
        char b[24];
        snprintf(b, sizeof(b), "%lu", v);
        va_list ap;
        va_start(ap, fmt);
        emit(b, fmt, ap);
        va_end(ap);
    }
    void i(long v, const char* fmt, ...) const
    {
        char b[24];
        snprintf(b, sizeof(b), "%ld", v);
        va_list ap;
        va_start(ap, fmt);
        emit(b, fmt, ap);
        va_end(ap);
    }
    void f(float v, const char* fmt, ...) const
    {
        unsigned bits;
        memcpy(&bits, &v, 4);
        char b[16];
        snprintf(b, sizeof(b), "0x%08x", bits);
        va_list ap;
        va_start(ap, fmt);
        emit(b, fmt, ap);
        va_end(ap);
    }
    void d(double v, const char* fmt, ...) const
    {
        char b[40];
        snprintf(b, sizeof(b), "%.17g", v);
        va_list ap;
        va_start(ap, fmt);
        emit(b, fmt, ap);
        va_end(ap);
    }
    void s(const char* v, const char* fmt, ...) const
    {
        va_list ap;
        va_start(ap, fmt);
        emit(v, fmt, ap);
        va_end(ap);
    }
    Re4Out sub(const char* fmt, ...) const
    {
        Re4Out o = *this;
        size_t n = strlen(o.pre);
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(o.pre + n, sizeof(o.pre) - n, fmt, ap);
        va_end(ap);
        return o;
    }
};

static inline Re4Out re4cmp_out(struct Re4Facts* facts)
{
    Re4Out o;
    o.facts = facts;
    o.pre[0] = 0;
    return o;
}

static inline unsigned re4cmp_float_bits(float v)
{
    unsigned bits;
    memcpy(&bits, &v, 4);
    return bits;
}
#endif

#endif
