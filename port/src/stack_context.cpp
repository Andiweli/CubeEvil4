#include "stack_context.h"
#include <cstdlib>
#include <cfenv>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
// ABI declarations from Boost.Context's detail/fcontext.hpp (1.88.0).
// Only the two low-level assembly operations are linked; no Boost C++ runtime.
struct Transfer { void* context; void* data; };
extern "C" Transfer jump_fcontext(void* to, void* data);
extern "C" void* make_fcontext(void* stackTop, std::size_t size, void (*entry)(Transfer));
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
    static void start(Transfer transfer) {
        auto* self = static_cast<Impl*>(transfer.data);
        self->caller = transfer.context;
        self->entry(self->argument);
        std::terminate();
    }
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
        context = make_fcontext(bottom + usable, usable, start);
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
void StackContext::resume() {
#ifdef _WIN32
    impl_->caller = GetCurrentFiber();
    SwitchToFiber(impl_->context);
#else
    std::fegetenv(&impl_->callerFloatingPoint);
    std::fesetenv(&impl_->floatingPoint);
    impl_->context = jump_fcontext(impl_->context, impl_.get()).context;
#endif
}
void StackContext::yield() {
#ifdef _WIN32
    SwitchToFiber(impl_->caller);
#else
    std::fegetenv(&impl_->floatingPoint);
    std::fesetenv(&impl_->callerFloatingPoint);
    impl_->caller = jump_fcontext(impl_->caller, impl_.get()).context;
#endif
}
} // namespace re4::port::detail
