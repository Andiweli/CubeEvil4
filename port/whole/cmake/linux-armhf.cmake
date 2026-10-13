# ARM32 Linux cross build (Ubuntu g++-arm-linux-gnueabihf sysroot). Checks that
# the game compiles for armv7; the Android build uses the NDK toolchain instead.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET arm-linux-gnueabihf)
set(CMAKE_CXX_COMPILER_TARGET arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
