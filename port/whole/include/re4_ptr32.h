/* A pointer kept in 32 bits (milestone 12, way A: native 64 bit).
 *
 * Structures with a fixed form keep the 4-byte pointer fields they have on the
 * GameCube: data the game reads straight from the disc and relocates in place
 * (textures, collision, scene areas, models ...), the save data. whole_prepare.py
 * writes their pointer members as RE4_P32(T) (list: port/whole/fixed_form.json).
 * On ILP32 that is T* as before (an alias); on LP64 Re4Ptr32<T>, four bytes holding the
 * address, which converts to and from T* like the pointer it replaces. Right
 * because everything the game points to lies below 4 GB. */
#ifndef RE4_PTR32_H
#define RE4_PTR32_H

#ifndef __cplusplus

/* C (SDK headers that C units include, e.g. OSModule.h through os.h): data
 * pointers only; on LP64 the four bytes of the address, as the layout needs */
#if __SIZEOF_POINTER__ == 4
#define RE4_P32(...) __VA_ARGS__*
#else
#define RE4_P32(...) unsigned int
#endif

#else

#if __SIZEOF_POINTER__ == 4

/* the same type as before: T* (also for function types, RE4_P32(void(int))) */
template <class T> using Re4Ptr32 = T*;

#else

template <class T> struct Re4Ptr32Ref { typedef T& type; };
template <> struct Re4Ptr32Ref<void> { typedef void type; };
template <> struct Re4Ptr32Ref<const void> { typedef void type; };

template <class T>
struct Re4Ptr32 {
    unsigned int a;  /* the address (below 4 GB) */

    Re4Ptr32() = default;  /* trivial: fixed-form structures are in unions and arrays */
    Re4Ptr32(T* p) : a((unsigned int) (unsigned long) p) {}
    Re4Ptr32(int null) : a(0) { (void) null; }   /* = 0 */
    Re4Ptr32(long null) : a(0) { (void) null; }  /* = NULL (__null) */
    Re4Ptr32(decltype(nullptr)) : a(0) {}
    T* get() const { return (T*) (unsigned long) a; }
    operator T*() const { return get(); }
    T* operator->() const { return get(); }
    typename Re4Ptr32Ref<T>::type operator*() const { return *get(); }
    Re4Ptr32& operator++() { T* p = get(); *this = ++p; return *this; }
    Re4Ptr32& operator--() { T* p = get(); *this = --p; return *this; }
    Re4Ptr32 operator++(int) { Re4Ptr32 o = *this; ++*this; return o; }
    Re4Ptr32 operator--(int) { Re4Ptr32 o = *this; --*this; return o; }
    Re4Ptr32& operator+=(long n) { *this = get() + n; return *this; }
    Re4Ptr32& operator-=(long n) { *this = get() - n; return *this; }
    /* (u8*) p, (Other*) p: the casts the game writes on the pointer */
    template <class U> explicit operator U*() const { return (U*) get(); }
    /* (u32) p, (s32) p ...: the 32-bit address as the game computes with it */
    explicit operator unsigned int() const { return a; }
    explicit operator int() const { return (int) a; }
    explicit operator unsigned long() const { return a; }
    explicit operator long() const { return (long) a; }
};

#endif

/* variadic: function types with several parameters contain commas */
#define RE4_P32(...) Re4Ptr32<__VA_ARGS__>

/* The plain pointer of a field, for variadic calls (eprintf("%s", re4_ptr(info->name))):
 * a Re4Ptr32 object there would arrive as four bytes, not as a pointer. */
template <class T> inline T* re4_ptr(T* p) { return p; }
#if __SIZEOF_POINTER__ != 4
template <class T> inline T* re4_ptr(const Re4Ptr32<T>& p) { return p.get(); }
#endif

#endif /* __cplusplus */

#endif
