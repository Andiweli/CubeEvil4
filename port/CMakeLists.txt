cmake_minimum_required(VERSION 3.22)
project(re4_native_foundation VERSION 0.2.1 LANGUAGES CXX)

option(RE4_BUILD_TESTS "Build the original-code verification executable" ON)
option(RE4_ENABLE_SANITIZERS "Enable ASan/UBSan on a compatible host" OFF)

get_filename_component(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include(cmake/OriginalSources.cmake)
re4_prepare_original_sources("${RE4_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/original")

# Only approved headers enter this target. The legacy include/ directory also
# contains an old libc, which must never shadow the host or NDK standard library.
add_library(re4_original STATIC
    "${RE4_ROOT}/src/game/rnd.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/math.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/yz2_setup.cpp")
target_include_directories(re4_original PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
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

add_library(re4_scheduler STATIC src/scheduler.cpp src/stack_context.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/original/scheduler_control.cpp")
target_link_libraries(re4_scheduler PUBLIC re4_original)
set_target_properties(re4_scheduler PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(ANDROID)
    enable_language(ASM)
    include(cmake/ContextSources.cmake)
    re4_add_context_sources(re4_scheduler)
elseif(NOT WIN32)
    message(FATAL_ERROR "Scheduler currently supports Android ARM32/ARM64 and Windows hosts")
endif()
if(MSVC)
    target_compile_options(re4_scheduler PRIVATE /W4 /WX /fp:strict)
else()
    target_compile_options(re4_scheduler PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Werror;-fno-fast-math;-ffp-contract=off>)
endif()

add_library(re4_yz2 STATIC src/yz2.cpp src/yz2_kernel.cpp)
target_link_libraries(re4_yz2 PUBLIC re4_scheduler)
target_include_directories(re4_yz2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(re4_yz2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_yz2 PRIVATE /W4 /WX)
else()
    target_compile_options(re4_yz2 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_library(re4_async_file STATIC src/async_file.cpp)
add_library(re4_asset_index STATIC src/asset_index.cpp)
add_library(re4_loader STATIC src/engine_loader.cpp src/sha256.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/original/loader_control.cpp")
target_link_libraries(re4_loader PUBLIC re4_asset_index re4_yz2 re4_async_file)
target_include_directories(re4_loader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(re4_loader PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_loader PRIVATE /W4 /WX)
    target_compile_definitions(re4_loader PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(re4_loader PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(re4_asset_index PUBLIC re4_original)
set_target_properties(re4_asset_index PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_asset_index PRIVATE /W4 /WX)
else()
    target_compile_options(re4_asset_index PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
find_package(Threads REQUIRED)
target_link_libraries(re4_async_file PUBLIC re4_scheduler Threads::Threads)
set_target_properties(re4_async_file PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_async_file PRIVATE /W4 /WX)
else()
    target_compile_options(re4_async_file PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_library(re4_verification STATIC src/verification.cpp src/scheduler_verification.cpp
    src/background_verification.cpp "${CMAKE_CURRENT_BINARY_DIR}/original/dvd_adapter_probe.cpp")
target_include_directories(re4_verification PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
find_package(Threads REQUIRED)
target_link_libraries(re4_verification PUBLIC re4_original re4_scheduler re4_yz2 re4_async_file Threads::Threads)
set_target_properties(re4_verification PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_verification PRIVATE /W4 /WX /fp:strict)
else()
    target_compile_options(re4_verification PRIVATE
        -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
endif()

if(RE4_ENABLE_SANITIZERS)
    message(FATAL_ERROR "Sanitizers require a verified context backend with fiber hooks; not yet available")
endif()

if(RE4_BUILD_TESTS)
    add_executable(re4_loader_tool tests/loader_tool.cpp)
    target_link_libraries(re4_loader_tool PRIVATE re4_loader)
    add_executable(re4_asset_tool tests/asset_tool.cpp)
    target_link_libraries(re4_asset_tool PRIVATE re4_asset_index re4_yz2)
    add_executable(re4_core_tests tests/test_main.cpp)
    target_link_libraries(re4_core_tests PRIVATE re4_verification)
    if(NOT ANDROID)
        enable_testing()
        add_test(NAME engine_loader COMMAND re4_loader_tool self-test "${CMAKE_CURRENT_BINARY_DIR}/loader-tests")
        set_tests_properties(engine_loader PROPERTIES TIMEOUT 90)
        add_test(NAME asset_index COMMAND re4_asset_tool self-test)
        foreach(group abi rng flags math endian scheduler_stack scheduler_order scheduler_lifecycle scheduler_modules background yz2 background_io)
            add_test(NAME original_${group} COMMAND re4_core_tests ${group})
            set_tests_properties(original_${group} PROPERTIES TIMEOUT 30)
        endforeach()
        add_test(NAME original_source_guard
            COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_ROOT}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/source_guard.cmake")
        add_test(NAME original_source_guard_rejects_change
            COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_ROOT}"
                "-DRE4_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/source-guard-probe"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/source_guard_negative.cmake")
        # Optional independent Python oracle; native builds never depend on it.
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            add_test(NAME import_validation
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_import.py")
            set_tests_properties(import_validation PROPERTIES TIMEOUT 30)
            add_test(NAME yz2_fixture_reference
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_yz2_fixtures.py" --check)
            set_tests_properties(yz2_fixture_reference PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()

if(ANDROID)
    if(NOT ANDROID_ABI MATCHES "^(armeabi-v7a|arm64-v8a)$")
        message(FATAL_ERROR "Supported Android targets: armeabi-v7a and arm64-v8a")
    endif()
    add_library(re4_port_jni SHARED platform/android/jni.cpp platform/android/loader_jni.cpp)
    target_link_libraries(re4_port_jni PRIVATE re4_verification re4_loader log)
    target_link_options(re4_port_jni PRIVATE -Wl,--no-undefined)
endif()
