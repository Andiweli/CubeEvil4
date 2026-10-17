# 64-bit x86 Linux host build (milestone 12): LP64 like arm64-v8a (pointers
# and long 8 bytes), runs in the cloud. Checks the 64-bit port (way A:
# native 64 bit, everything the game points to below 4 GB) without a device.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_ASM_COMPILER_TARGET x86_64-linux-gnu)
