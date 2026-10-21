// Write watchpoints for diagnosis (milestone 12): re4_watch_write(address,
// label) asks the CPU to report every write to the 4 bytes at `address` made
// by the calling OS thread (the game's threads all run on it, os_thread.cpp).
// Linux and Android: perf_event_open with a hardware breakpoint
// (PERF_TYPE_BREAKPOINT), each hit a signal. The handler logs the label, the
// new value, the writing instruction and the return addresses on the stack
// that lie in the game's libraries ("watch: ..."), the first 3 hits per place.
// At most 4 places at once (the CPU's debug registers): a new place replaces
// the oldest. Hits from functions listed in `quiet` (the owner's own writes)
// are not logged. Where the kernel refuses breakpoints (perf_event_paranoid,
// no debug registers) it says so once and does nothing.
#include "re4_runtime.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#if defined(__linux__) && __has_include(<linux/hw_breakpoint.h>)
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#define RE4_WATCH_SUPPORTED 1
#endif

namespace {

#ifdef RE4_WATCH_SUPPORTED

struct Slot {
    int fd = -1;
    uintptr_t address = 0;
    const char* label = "";
    const char* quiet = "";
    unsigned hits = 0;
    unsigned long serial = 0;
};

const int kSlots = 4;
Slot gSlots[kSlots];
unsigned long gSerial;
bool gRefused;
bool gHandlerSet;
volatile unsigned gHits;  // every hit, logged or not (runtime test)

int watchSignal()
{
#if defined(__ANDROID__) && __ANDROID_API__ < 21
    // Android 4.x (API 16, the OUYA): no __libc_current_sigrtmin; the kernel's
    // first real-time signal is 32 (only for diagnosis, re4_watch_write)
    return 32 + 6;
#else
    return SIGRTMIN + 6;
#endif
}

// "libre4_main.so+0x23095c (MotionMoveCore+0x338)"
void describe(uintptr_t pc, char* out, size_t size)
{
    Dl_info info;
    if (pc && dladdr((void*) pc, &info) && info.dli_fname) {
        const char* lib = strrchr(info.dli_fname, '/');
        lib = lib ? lib + 1 : info.dli_fname;
        if (info.dli_sname) {
            snprintf(out, size, "%s+0x%lx (%s+0x%lx)", lib, (unsigned long) (pc - (uintptr_t) info.dli_fbase),
                     info.dli_sname, (unsigned long) (pc - (uintptr_t) info.dli_saddr));
        } else {
            snprintf(out, size, "%s+0x%lx", lib, (unsigned long) (pc - (uintptr_t) info.dli_fbase));
        }
        return;
    }
    snprintf(out, size, "0x%lx", (unsigned long) pc);
}

bool inGame(uintptr_t pc)
{
    Dl_info info;
    return pc && dladdr((void*) pc, &info) && info.dli_fname && strstr(info.dli_fname, "libre4_");
}

bool quietWriter(const Slot& s, uintptr_t pc)
{
    Dl_info info;
    return s.quiet[0] && dladdr((void*) pc, &info) && info.dli_sname && strstr(info.dli_sname, s.quiet);
}

void onHit(int, siginfo_t* si, void* context)
{
    const int saved = errno;
    Slot* s = 0;
    for (auto& slot : gSlots) {
        if (slot.fd >= 0 && slot.fd == si->si_fd) {
            s = &slot;
        }
    }
    const ucontext_t* uc = (const ucontext_t*) context;
#if defined(__aarch64__)
    const uintptr_t pc = uc->uc_mcontext.pc, sp = uc->uc_mcontext.sp, lr = uc->uc_mcontext.regs[30];
#elif defined(__x86_64__)
    const uintptr_t pc = uc->uc_mcontext.gregs[REG_RIP], sp = uc->uc_mcontext.gregs[REG_RSP], lr = 0;
#elif defined(__i386__)
    const uintptr_t pc = uc->uc_mcontext.gregs[REG_EIP], sp = uc->uc_mcontext.gregs[REG_ESP], lr = 0;
#elif defined(__arm__)
    const uintptr_t pc = uc->uc_mcontext.arm_pc, sp = uc->uc_mcontext.arm_sp, lr = uc->uc_mcontext.arm_lr;
#else
    const uintptr_t pc = 0, sp = 0, lr = 0;
#endif
    if (s) {
        gHits++;
    }
    if (!s || quietWriter(*s, pc) || s->hits >= 3) {
        errno = saved;
        return;
    }
    s->hits++;
    char where[160];
    describe(pc, where, sizeof(where));
    re4_log("watch: write to %s at 0x%lx, now 0x%08x, by the instruction before %s", s->label,
            (unsigned long) s->address, *(const volatile unsigned*) s->address, where);
    if (lr) {
        describe(lr, where, sizeof(where));
        re4_log("watch:   lr %s", where);
    }
    // the return addresses on the stack that lie in the game (no unwinder in a signal handler)
    const uintptr_t* stack = (const uintptr_t*) sp;
    int shown = 0;
    for (int i = 0; i < 256 && shown < 16; i++) {
        uintptr_t v = stack[i];
        if (inGame(v)) {
            describe(v, where, sizeof(where));
            re4_log("watch:   sp+%03x %s", i * (int) sizeof(uintptr_t), where);
            shown++;
        }
    }
    errno = saved;
}

void installHandler()
{
    if (gHandlerSet) {
        return;
    }
    gHandlerSet = true;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = onHit;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(watchSignal(), &sa, 0);
}

int openBreakpoint(uintptr_t address)
{
    struct perf_event_attr a;
    memset(&a, 0, sizeof(a));
    a.type = PERF_TYPE_BREAKPOINT;
    a.size = sizeof(a);
    a.bp_type = HW_BREAKPOINT_W;
    a.bp_addr = address;
    a.bp_len = HW_BREAKPOINT_LEN_4;
    a.sample_period = 1;
    a.wakeup_events = 1;
    a.disabled = 1;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    return (int) syscall(__NR_perf_event_open, &a, 0, -1, -1, 0);
}

#endif  // RE4_WATCH_SUPPORTED

}  // namespace

// Hits so far (all places); -1 when the kernel refused breakpoints.
extern "C" int re4_watch_hits(void)
{
#ifdef RE4_WATCH_SUPPORTED
    return gRefused ? -1 : (int) gHits;
#else
    return -1;
#endif
}

extern "C" void re4_watch_write(const void* address, const char* label, const char* quiet)
{
#ifdef RE4_WATCH_SUPPORTED
    const uintptr_t at = (uintptr_t) address;
    if (gRefused || !at) {
        return;
    }
    for (auto& s : gSlots) {
        if (s.fd >= 0 && s.address == at) {
            return;  // watched already
        }
    }
    installHandler();
    // a free slot, else the oldest
    Slot* slot = &gSlots[0];
    for (auto& s : gSlots) {
        if (s.fd < 0) {
            slot = &s;
            break;
        }
        if (s.serial < slot->serial) {
            slot = &s;
        }
    }
    if (slot->fd >= 0) {
        close(slot->fd);
        slot->fd = -1;
    }
    int fd = openBreakpoint(at);
    if (fd < 0) {
        gRefused = true;
        re4_log("watch: the kernel refused a hardware breakpoint (%s); no watching", strerror(errno));
        return;
    }
    fcntl(fd, F_SETFL, O_ASYNC);
    fcntl(fd, F_SETSIG, watchSignal());
    struct f_owner_ex owner = {F_OWNER_TID, (pid_t) syscall(SYS_gettid)};
    fcntl(fd, F_SETOWN_EX, &owner);
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    slot->fd = fd;
    slot->address = at;
    slot->label = label ? label : "";
    slot->quiet = quiet ? quiet : "";
    slot->hits = 0;
    slot->serial = ++gSerial;
    static bool announced;
    if (!announced) {
        announced = true;
        re4_log("watch: hardware write breakpoints on (%s at 0x%lx first)", slot->label, (unsigned long) at);
    }
#else
    (void) address;
    (void) label;
    (void) quiet;
#endif
}
