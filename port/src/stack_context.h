#pragma once
#include <cstddef>
#include <memory>

namespace re4::port::detail {
// Owns the host thread's fiber conversion when necessary.
class ContextDomain {
public:
    ContextDomain();
    ~ContextDomain();
private:
#ifdef _WIN32
    bool converted_ = false;
#endif
};
class StackContext {
public:
    using Entry = void (*)(void*);
    StackContext(std::size_t bytes, Entry entry, void* argument);
    ~StackContext();
    StackContext(const StackContext&) = delete;
    StackContext& operator=(const StackContext&) = delete;
    void resume();
    void yield();
    // True when `address` lies on this context's stack (Windows: unknown, false).
    bool contains(const void* address) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port::detail
