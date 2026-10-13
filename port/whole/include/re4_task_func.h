#ifndef RE4_TASK_FUNC_H
#define RE4_TASK_FUNC_H

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
    TaskFunc(void* ptr) : p(ptr) {}
    template <class F> TaskFunc(F* ptr) : p((void*)ptr) {}
    template <class F> operator F*() const { return (F*)p; }
    operator void*() const { return p; }
    bool operator==(const TaskFunc& o) const { return p == o.p; }
    bool operator!=(const TaskFunc& o) const { return p != o.p; }
    bool operator==(int null) const { return p == (void*)(long)null; }
    bool operator!=(int null) const { return p != (void*)(long)null; }
    bool operator==(long null) const { return p == (void*)null; }
    bool operator!=(long null) const { return p != (void*)null; }
    bool operator!() const { return p == 0; }
private:
    void* p;
};

static_assert(sizeof(TaskFunc) == sizeof(void*), "TaskFunc must keep the layout of the original void*");

#endif
