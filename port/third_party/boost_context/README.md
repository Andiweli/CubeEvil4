# Boost.Context low-level ARM backend

Unmodified `make` and `jump` assembly from the upstream tag `boost-1.88.0`:
https://github.com/boostorg/context/tree/boost-1.88.0/src/asm

Four files only, plus the Boost Software License. Their normalized LF SHA-256
hashes are checked by `port/cmake/ContextSources.cmake`. No Boost package,
network fetch at build time, coroutine wrapper or gameplay code is required.
The ABI declarations are from upstream `include/boost/context/detail/fcontext.hpp`.
`port/src/stack_context.cpp` implements stack ownership and caller transfers.

ARM32 assembly is compiled with `-marm -U__SOFTFP__`, because Android's softfp
calling convention still requires d8-d15 preservation across function calls.
Upstream's save/restore conditional otherwise omits these registers. The C++
compiler retains the NDK's original softfp ABI. ARM64 saves x19-x30 and d8-d15.
The per-context floating-point environment is separately preserved in C++.
Android mappings have inaccessible guard pages and RW, non-executable stacks.

This integration does not support cross-thread migration, asynchronous signal
preemption, ASan fiber hooks, PAC/BTI-enabled context entry or unwinding across
contexts. Android's reserved x18 is never modified. Evaluate newer toolchain
branch-protection requirements before enabling PAC/BTI, not by ignoring them.

Upstream description: https://www.boost.org/doc/libs/1_88_0/libs/context/doc/html/context/overview.html
