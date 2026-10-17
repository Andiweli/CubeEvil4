#ifndef RE4_TASK_FUNC_H
#define RE4_TASK_FUNC_H

#include "re4_ptr32.h"

/* Port replacement for `typedef void* TaskFunc` (include/scheduler.h).
 *
 * The original passes task entry points of any signature as void*; GCC 2.95
 * converted function pointers to void* implicitly, modern C++ does not. This
 * wrapper has the size and layout of one pointer, accepts any function or
 * object pointer, and converts back to whatever pointer type the caller casts
 * it to. */
class TaskFunc {
public:
    TaskFunc() = default;  /* trivial: TaskFunc is a member of unions in sce_at.h */
    TaskFunc(int null) : p(0) { (void)null; }
    TaskFunc(long null) : p(0) { (void)null; }  /* NULL is __null, a long, on LP64 */
    TaskFunc(void* ptr) : p(store(ptr)) {}
    template <class F> TaskFunc(F* ptr) : p(store((void*)ptr)) {}
#if __SIZEOF_POINTER__ != 4
    /* a function pointer field of a fixed-form structure (sce_at.h hide.func) */
    template <class F> TaskFunc(Re4Ptr32<F> ptr) : p(ptr.a) {}
#endif
    template <class F> operator F*() const { return (F*)load(); }
    operator void*() const { return load(); }
    bool operator==(const TaskFunc& o) const { return p == o.p; }
    bool operator!=(const TaskFunc& o) const { return p != o.p; }
    bool operator==(int null) const { return load() == (void*)(long)null; }
    bool operator!=(int null) const { return load() != (void*)(long)null; }
    bool operator==(long null) const { return load() == (void*)null; }
    bool operator!=(long null) const { return load() != (void*)null; }
    bool operator!() const { return p == 0; }
private:
#if __SIZEOF_POINTER__ == 4
    void* p;
    static void* store(void* x) { return x; }
    void* load() const { return p; }
#else
    /* LP64 (milestone 12): four bytes as on the GameCube, the scene area records
       of sce_at.h holding it come from the disc; code and data lie below 4 GB */
    unsigned int p;
    static unsigned int store(void* x) { return (unsigned int)(unsigned long)x; }
    void* load() const { return (void*)(unsigned long)p; }
#endif
};

static_assert(sizeof(TaskFunc) == 4, "TaskFunc must keep the four bytes of the original void*");

#endif
