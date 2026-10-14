/* Forced include for every game unit of the whole-program port build. */

/* The platform C library is declared before any game header. Game headers
 * redeclare a few C functions themselves (memcpy in joy.h, sprintf in xml.h);
 * after the system declaration those redeclarations are compatible.
 * math_sub.h defines its own inline fabsf, so libm's is renamed here exactly
 * as include/dolphin/types.h does it. */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#define fabsf fabsf_libm
#include <math.h>
#undef fabsf

/* C equivalents of the Gekko instructions of the replaced inline asm. */
#include "re4_ppc.h"

/* GameCube inline asm (register pinning, PPC instructions, section
 * directives) has no meaning on ARM or x86 and is dropped. Defined after the
 * system headers, which use __asm__ for symbol redirection. Symbol aliases
 * (`decl asm("name")`) are rewritten to __asm by whole_prepare.py and kept. */
#define asm(...)
#define __asm__(...)

/* C++ declares const-correct overloads of the C search functions; the game
 * assigns their results to char* as C allows. Route the calls to wrappers with
 * the C signatures. */
inline char* re4_strstr(const char* s, const char* n) { return (char*)strstr(s, n); }
inline char* re4_strchr(const char* s, int c) { return (char*)strchr(s, c); }
inline char* re4_strrchr(const char* s, int c) { return (char*)strrchr(s, c); }
inline char* re4_strpbrk(const char* s, const char* a) { return (char*)strpbrk(s, a); }
inline void* re4_memchr(const void* s, int c, size_t n) { return (void*)memchr(s, c, n); }
#define strstr re4_strstr
#define strchr re4_strchr
#define strrchr re4_strrchr
#define strpbrk re4_strpbrk
#define memchr re4_memchr

#ifdef __cplusplus
/* Two bytes of a big-endian halfword as an array view (u8 v[2] in a union
 * with the halfword, patch 0016): index 0 is the high byte, 1 the low byte,
 * as the original code expects, whatever the host byte order. */
struct Re4BeBytes2 {
    unsigned char v[2];
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    unsigned char& operator[](int i) { return v[1 - i]; }
    const unsigned char& operator[](int i) const { return v[1 - i]; }
#else
    unsigned char& operator[](int i) { return v[i]; }
    const unsigned char& operator[](int i) const { return v[i]; }
#endif
};

/* Big-endian views (port/whole/byte_views.json, applied by whole_prepare.py):
 * the game reads and writes several small fields at once through a cast word
 * pointer (`*(u32*) &col = 0xFF`, colours, tags, AX address pairs). On the
 * GameCube the first field is the most significant part of the word; on ARM
 * and x86 the least. RE4_BE(T, E, p) is the word T over the fields of E bytes
 * at p, each field in host order, the first field most significant, as on the
 * GameCube. RE4_BE_AT(T, p, i) reads part i (counted from the most
 * significant end, as on the GameCube) of the host-order scalar at p. */
template <int N> struct Re4Uint;
template <> struct Re4Uint<1> { typedef unsigned char T; };
template <> struct Re4Uint<2> { typedef unsigned short T; };
template <> struct Re4Uint<4> { typedef unsigned int T; };
template <> struct Re4Uint<8> { typedef unsigned long long T; };

template <class T, int E>
struct Re4BeView {
    typedef typename Re4Uint<sizeof(T)>::T U;
    typedef typename Re4Uint<E>::T Field;
    unsigned char* p;
    explicit Re4BeView(const volatile void* q) : p((unsigned char*) q) {}
    U load() const
    {
        U v = 0;
        for (unsigned k = 0; k < sizeof(T) / E; k++) {
            Field f;
            __builtin_memcpy(&f, p + k * E, E);
            v = (U) ((v << (8 * E)) | f);
        }
        return v;
    }
    void store(U v)
    {
        for (int k = (int) (sizeof(T) / E) - 1; k >= 0; k--) {
            Field f = (Field) v;
            __builtin_memcpy(p + k * E, &f, E);
            v = (U) (v >> (8 * E));
        }
    }
    operator T() const
    {
        U u = load();
        T t;
        __builtin_memcpy(&t, &u, sizeof t);
        return t;
    }
    Re4BeView& operator=(T t)
    {
        U u;
        __builtin_memcpy(&u, &t, sizeof u);
        store(u);
        return *this;
    }
    Re4BeView& operator=(const Re4BeView& o) { return *this = (T) o; }
    template <class V> Re4BeView& operator|=(V v) { return *this = (T) ((T) *this | v); }
    template <class V> Re4BeView& operator&=(V v) { return *this = (T) ((T) *this & v); }
    template <class V> Re4BeView& operator^=(V v) { return *this = (T) ((T) *this ^ v); }
    template <class V> Re4BeView& operator+=(V v) { return *this = (T) ((T) *this + v); }
    template <class V> Re4BeView& operator-=(V v) { return *this = (T) ((T) *this - v); }
};
/* a function, not a constructor call: `RE4_BE(...) = v;` at the start of a
 * statement would otherwise declare a reference */
template <class T, int E>
inline Re4BeView<T, E> re4_be(const volatile void* p)
{
    return Re4BeView<T, E>(p);
}
#define RE4_BE(T, E, p) re4_be<T, E>(p)

template <class T, class O>
inline T re4_be_part(const O* obj, int i)
{
    typename Re4Uint<sizeof(O)>::T whole;
    __builtin_memcpy(&whole, obj, sizeof whole);
    typename Re4Uint<sizeof(T)>::T part =
        (typename Re4Uint<sizeof(T)>::T) (whole >> (8 * (sizeof(O) - (i + 1) * sizeof(T))));
    T t;
    __builtin_memcpy(&t, &part, sizeof t);
    return t;
}
#define RE4_BE_AT(T, p, i) re4_be_part<T>((p), (i))
#endif
