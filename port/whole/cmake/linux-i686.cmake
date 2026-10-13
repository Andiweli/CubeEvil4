# 32-bit x86 Linux host build: same pointer width as ARM32, runs in the cloud.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET i686-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET i686-linux-gnu)
set(CMAKE_ASM_COMPILER_TARGET i686-linux-gnu)

# Ubuntu's 32-bit glibc headers (libc6-dev-i386) come without the kernel's asm/
# directory that <errno.h> and <sys/stat.h> reach. The x86 uapi asm headers are
# shared by i386 and x86_64 (they test __i386__), so the 64-bit ones are put on
# the include path through a one-entry shim directory.
if(NOT EXISTS /usr/include/asm AND EXISTS /usr/include/x86_64-linux-gnu/asm)
    set(RE4_ASM_SHIM "${CMAKE_BINARY_DIR}/i686-asm-shim")
    if(NOT EXISTS "${RE4_ASM_SHIM}/asm")
        file(MAKE_DIRECTORY "${RE4_ASM_SHIM}")
        file(CREATE_LINK /usr/include/x86_64-linux-gnu/asm "${RE4_ASM_SHIM}/asm" SYMBOLIC)
    endif()
    set(CMAKE_C_FLAGS_INIT "-idirafter ${RE4_ASM_SHIM}")
    set(CMAKE_CXX_FLAGS_INIT "-idirafter ${RE4_ASM_SHIM}")
endif()
