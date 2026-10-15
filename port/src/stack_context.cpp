#include "stack_context.h"
#include <cstdlib>
#include <cfenv>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__ANDROID__) || (defined(__linux__) && (defined(__arm__) || defined(__aarch64__)))
// Android, and the Linux ARM test builds of the whole-program port (run under
// qemu), share the Boost.Context assembly.
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
// ABI declarations from Boost.Context's detail/fcontext.hpp (1.88.0).
// Only the two low-level assembly operations are linked; no Boost C++ runtime.
struct Transfer { void* context; void* data; };
extern "C" Transfer jump_fcontext(void* to, void* data);
extern "C" void* make_fcontext(void* stackTop, std::size_t size, void (*entry)(Transfer));
#elif defined(__linux__)
// Linux x86 test builds of the whole-program port: glibc ucontext.
#define RE4_UCONTEXT 1
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#else
#error "A tested stack context backend is required for this platform"
#endif

namespace re4::port::detail {
ContextDomain::ContextDomain() {
#ifdef _WIN32
    if (!IsThreadAFiber()) {
        if (!ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "ConvertThreadToFiberEx");
        converted_ = true;
    }
#endif
}
ContextDomain::~ContextDomain() {
#ifdef _WIN32
    if (converted_ && !ConvertFiberToThread()) std::terminate();
#endif
}

struct StackContext::Impl {
    Entry entry;
    void* argument;
    void* context = nullptr;
    void* caller = nullptr;
#ifdef _WIN32
    static void WINAPI start(void* data) {
        auto* self = static_cast<Impl*>(data);
        self->entry(self->argument);
        std::terminate(); // A context entry must always yield before completion.
    }
#else
    std::fenv_t floatingPoint{};
    std::fenv_t callerFloatingPoint{};
    void* mapping = MAP_FAILED;
    std::size_t mappingBytes = 0;
#ifdef RE4_UCONTEXT
    ucontext_t self_{};
    ucontext_t callerContext{};
    static void start(unsigned low, unsigned high) {
        auto* self = reinterpret_cast<Impl*>((static_cast<std::uintptr_t>(high) << 16 << 16) | low);
        self->entry(self->argument);
        std::terminate();
    }
#else
    static void start(Transfer transfer) {
        auto* self = static_cast<Impl*>(transfer.data);
        self->caller = transfer.context;
        self->entry(self->argument);
        std::terminate();
    }
#endif
#endif
    Impl(std::size_t bytes, Entry function, void* arg) : entry(function), argument(arg) {
        if (!entry || bytes < 64 * 1024 || bytes > 16 * 1024 * 1024)
            throw std::invalid_argument("Native stack size must be 64 KiB..16 MiB and entry non-null");
#ifdef _WIN32
        context = CreateFiberEx(bytes, bytes, FIBER_FLAG_FLOAT_SWITCH, start, this);
        if (!context) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateFiberEx");
#else
        std::fegetenv(&floatingPoint);
        const auto rawPage = sysconf(_SC_PAGESIZE);
        if (rawPage <= 0) throw std::runtime_error("Cannot query page size");
        const auto page = static_cast<std::size_t>(rawPage);
        const auto usable = ((bytes + page - 1) / page) * page;
        mappingBytes = usable + 2 * page;
        // Both ends are protected. The stack is writable and never executable.
        mapping = mmap(nullptr, mappingBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mmap stack");
        auto* bottom = static_cast<char*>(mapping) + page;
        if (mprotect(bottom, usable, PROT_READ | PROT_WRITE) != 0) {
            const int error = errno;
            munmap(mapping, mappingBytes);
            mapping = MAP_FAILED;
            throw std::system_error(error, std::generic_category(), "mprotect stack");
        }
#ifdef RE4_UCONTEXT
        if (getcontext(&self_) != 0) throw std::system_error(errno, std::generic_category(), "getcontext");
        self_.uc_stack.ss_sp = bottom;
        self_.uc_stack.ss_size = usable;
        self_.uc_link = nullptr;
        const auto self = reinterpret_cast<std::uintptr_t>(this);
        makecontext(&self_, reinterpret_cast<void (*)()>(start), 2, static_cast<unsigned>(self),
                    static_cast<unsigned>(self >> 16 >> 16));
        context = &self_;
#else
        context = make_fcontext(bottom + usable, usable, start);
#endif
#endif
    }
    ~Impl() {
#ifdef _WIN32
        DeleteFiber(context);
#else
        if (mapping != MAP_FAILED) munmap(mapping, mappingBytes);
#endif
    }
};
StackContext::StackContext(std::size_t bytes, Entry entry, void* argument)
    : impl_(std::make_unique<Impl>(bytes, entry, argument)) {}
StackContext::~StackContext() = default;
bool StackContext::contains(const void* address) const {
#ifdef _WIN32
    (void)address;
    return false;
#else
    if (impl_->mapping == MAP_FAILED) return false;
    const auto* begin = static_cast<const char*>(impl_->mapping);
    const auto* at = static_cast<const char*>(address);
    return at >= begin && at < begin + impl_->mappingBytes;
#endif
}
void StackContext::resume() {
#ifdef _WIN32
    impl_->caller = GetCurrentFiber();
    SwitchToFiber(impl_->context);
#else
    std::fegetenv(&impl_->callerFloatingPoint);
    std::fesetenv(&impl_->floatingPoint);
#ifdef RE4_UCONTEXT
    swapcontext(&impl_->callerContext, &impl_->self_);
#else
    impl_->context = jump_fcontext(impl_->context, impl_.get()).context;
#endif
#endif
}
void StackContext::yield() {
#ifdef _WIN32
    SwitchToFiber(impl_->caller);
#else
    std::fegetenv(&impl_->floatingPoint);
    std::fesetenv(&impl_->callerFloatingPoint);
#ifdef RE4_UCONTEXT
    swapcontext(&impl_->self_, &impl_->callerContext);
#else
    impl_->caller = jump_fcontext(impl_->caller, impl_.get()).context;
#endif
#endif
}
} // namespace re4::port::detail
