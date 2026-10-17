# 64-bit ARM Linux cross build (milestone 12; Ubuntu g++-aarch64-linux-gnu
# sysroot): the arm64-v8a code generation of the Android build, run under
# qemu-aarch64 in the cloud (whole_build.sh aarch64). LP64 like x86_64; the
# Android build uses the NDK toolchain instead.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_ASM_COMPILER_TARGET aarch64-linux-gnu)
