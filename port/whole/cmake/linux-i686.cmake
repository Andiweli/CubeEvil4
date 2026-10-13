# 32-bit x86 Linux host build: same pointer width as ARM32, runs in the cloud.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET i686-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET i686-linux-gnu)
set(CMAKE_ASM_COMPILER_TARGET i686-linux-gnu)
