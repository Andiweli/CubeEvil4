cmake_minimum_required(VERSION 3.22)
project(re4_native_foundation VERSION 0.1.0 LANGUAGES CXX)

option(RE4_BUILD_TESTS "Build the original-code verification executable" ON)
option(RE4_ENABLE_SANITIZERS "Enable ASan/UBSan on a compatible host" OFF)

get_filename_component(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include(cmake/OriginalSources.cmake)
re4_prepare_original_sources("${RE4_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/original")

# Only approved headers enter this target. The legacy include/ directory also
# contains an old libc, which must never shadow the host or NDK standard library.
add_library(re4_original STATIC
    "${RE4_ROOT}/src/game/rnd.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/math.cpp")
target_include_directories(re4_original PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/include"
    "${CMAKE_CURRENT_BINARY_DIR}/original/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(re4_original PUBLIC cxx_std_17)
set_target_properties(re4_original PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Original RNG code intentionally type-puns IEEE float words. Keep its source
# unchanged and disable strict-aliasing optimization for this target only.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(re4_original PRIVATE
        -fno-strict-aliasing -fno-fast-math -ffp-contract=off)
elseif(MSVC)
    target_compile_options(re4_original PRIVATE /fp:strict)
else()
    message(FATAL_ERROR "Unverified compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

add_library(re4_verification STATIC src/verification.cpp)
find_package(Threads REQUIRED)
target_link_libraries(re4_verification PUBLIC re4_original Threads::Threads)
set_target_properties(re4_verification PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_verification PRIVATE /W4 /WX /fp:strict)
else()
    target_compile_options(re4_verification PRIVATE
        -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
endif()

if(RE4_ENABLE_SANITIZERS)
    if(ANDROID OR MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Sanitizers require a GCC/Clang host toolchain")
    endif()
    foreach(target re4_original re4_verification)
        target_compile_options(${target} PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endforeach()
endif()

if(RE4_BUILD_TESTS)
    add_executable(re4_core_tests tests/test_main.cpp)
    target_link_libraries(re4_core_tests PRIVATE re4_verification)
    if(NOT ANDROID)
        enable_testing()
        foreach(group abi rng flags math endian)
            add_test(NAME original_${group} COMMAND re4_core_tests ${group})
        endforeach()
        add_test(NAME original_source_guard
            COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_ROOT}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/source_guard.cmake")
        add_test(NAME original_source_guard_rejects_change
            COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_ROOT}"
                "-DRE4_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/source-guard-probe"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/source_guard_negative.cmake")
    endif()
endif()

if(ANDROID)
    if(NOT ANDROID_ABI MATCHES "^(armeabi-v7a|arm64-v8a)$")
        message(FATAL_ERROR "Supported Android targets: armeabi-v7a and arm64-v8a")
    endif()
    add_library(re4_port_jni SHARED platform/android/jni.cpp)
    target_link_libraries(re4_port_jni PRIVATE re4_verification log)
    target_link_options(re4_port_jni PRIVATE -Wl,--no-undefined)
endif()
