cmake_minimum_required(VERSION 3.22)
project(re4_native_foundation VERSION 0.2.1 LANGUAGES CXX)

option(RE4_BUILD_TESTS "Build the original-code verification executable" ON)
option(RE4_ENABLE_SANITIZERS "Enable ASan/UBSan on a compatible host" OFF)

get_filename_component(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include(cmake/OriginalSources.cmake)
re4_prepare_original_sources("${RE4_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/original")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RE4_ROOT}/src/game/TexRender.cpp" "${RE4_ROOT}/include/TexRender.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tex_render.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/tex-render-sources.json")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tex_render.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original"
    RESULT_VARIABLE tex_generation_result)
if(NOT tex_generation_result EQUAL 0)
    message(FATAL_ERROR "Original TexRender generation failed")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/module-sources.json" module_sources_manifest)
string(JSON module_source_count LENGTH "${module_sources_manifest}")
math(EXPR module_source_last "${module_source_count} - 1")
foreach(index RANGE 0 ${module_source_last})
    string(JSON module_source MEMBER "${module_sources_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${module_source}")
endforeach()
file(GLOB_RECURSE module_config_inputs CONFIGURE_DEPENDS "${RE4_ROOT}/config/G4BE08/modules/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${module_config_inputs}
    "${RE4_ROOT}/config/G4BE08/config.yml" "${RE4_ROOT}/config/G4BE08/modules.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/module-sources.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_modules.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_modules.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/runtime-sources.json" runtime_sources_manifest)
string(JSON runtime_source_count LENGTH "${runtime_sources_manifest}")
math(EXPR runtime_source_last "${runtime_source_count} - 1")
foreach(index RANGE 0 ${runtime_source_last})
    string(JSON runtime_source MEMBER "${runtime_sources_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${runtime_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/runtime-sources.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_runtime.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_runtime.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/consumer-sources.json" consumer_manifest)
string(JSON consumer_count LENGTH "${consumer_manifest}")
math(EXPR consumer_last "${consumer_count} - 1")
foreach(index RANGE 0 ${consumer_last})
    string(JSON consumer_source MEMBER "${consumer_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${consumer_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/consumer-sources.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_consumers.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_consumers.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original" COMMAND_ERROR_IS_FATAL ANY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/subscreen-sources.json" subscreen_manifest)
string(JSON subscreen_count LENGTH "${subscreen_manifest}")
math(EXPR subscreen_last "${subscreen_count} - 1")
foreach(index RANGE 0 ${subscreen_last})
    string(JSON subscreen_source MEMBER "${subscreen_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${subscreen_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/subscreen-sources.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_subscreen.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_subscreen.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original" COMMAND_ERROR_IS_FATAL ANY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/id-save-sources.json" id_save_manifest)
string(JSON id_save_count LENGTH "${id_save_manifest}")
math(EXPR id_save_last "${id_save_count} - 1")
foreach(index RANGE 0 ${id_save_last})
    string(JSON id_save_source MEMBER "${id_save_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${id_save_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/id-save-sources.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_id_save.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_id_save.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original" COMMAND_ERROR_IS_FATAL ANY)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/light-sources.json" light_manifest)
string(JSON light_count LENGTH "${light_manifest}")
math(EXPR light_last "${light_count} - 1")
foreach(index RANGE 0 ${light_last})
    string(JSON light_source MEMBER "${light_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${light_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/light-sources.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_lights.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_lights.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original" COMMAND_ERROR_IS_FATAL ANY)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/player-sources.json" player_manifest)
string(JSON player_count LENGTH "${player_manifest}")
math(EXPR player_last "${player_count} - 1")
foreach(index RANGE 0 ${player_last})
    string(JSON player_source MEMBER "${player_manifest}" ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RE4_ROOT}/${player_source}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/player-sources.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player.py"
    --root "${RE4_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/original" COMMAND_ERROR_IS_FATAL ANY)

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
add_library(re4_loader STATIC src/engine_loader.cpp src/resource_runtime.cpp src/sha256.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/original/loader_control.cpp")
target_link_libraries(re4_loader PUBLIC re4_asset_index re4_yz2 re4_async_file)
add_library(re4_modules STATIC src/modules.cpp src/module_rel_index.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/original/module_catalog.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/module_stages.cpp")
target_link_libraries(re4_modules PUBLIC re4_loader)
add_library(re4_game_runtime STATIC src/game_runtime.cpp src/gameplay_heaps.cpp src/controller_input.cpp
    src/consumers.cpp src/event_resources.cpp src/model_runtime.cpp src/motion.cpp src/tex_render.cpp src/graphics.cpp src/messages.cpp src/subscreen.cpp
    src/id_save.cpp "${CMAKE_CURRENT_BINARY_DIR}/original/id_save_control.cpp"
    src/lights.cpp "${CMAKE_CURRENT_BINARY_DIR}/original/light_control.cpp"
    src/player.cpp "${CMAKE_CURRENT_BINARY_DIR}/original/player_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/subscreen_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/message_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/tex_render_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/consumer_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/runtime_control.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/original/pad_clamp.cpp")
target_include_directories(re4_game_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(re4_game_runtime PUBLIC re4_modules)
set_target_properties(re4_game_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(re4_game_verification STATIC tests/game_runtime_verification.cpp)
target_link_libraries(re4_game_verification PUBLIC re4_game_runtime)
set_target_properties(re4_game_verification PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_game_runtime PRIVATE /W4 /WX /fp:strict)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/original/runtime_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/id_save_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/light_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/player_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/subscreen_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/message_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/tex_render_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/consumer_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/pad_clamp.cpp" PROPERTIES COMPILE_OPTIONS "/WX-")
else()
    target_compile_options(re4_game_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
    # Preserve unused parameters/locals of complete original bodies. Native
    # adapter files still compile with the full warning/error policy.
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/original/consumer_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/id_save_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/light_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/player_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/message_control.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/original/subscreen_control.cpp"
        PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unused-variable")
endif()
target_include_directories(re4_modules PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(re4_modules PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_modules PRIVATE /W4 /WX)
else()
    target_compile_options(re4_modules PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_library(re4_module_verification STATIC tests/module_verification.cpp)
target_link_libraries(re4_module_verification PUBLIC re4_modules)
set_target_properties(re4_module_verification PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(re4_module_verification PRIVATE /W4 /WX)
else()
    target_compile_options(re4_module_verification PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
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
    add_executable(re4_game_tool tests/game_runtime_tool.cpp)
    target_link_libraries(re4_game_tool PRIVATE re4_game_verification)
    add_executable(re4_module_tool tests/module_tool.cpp)
    target_link_libraries(re4_module_tool PRIVATE re4_module_verification)
    add_executable(re4_resource_tool tests/resource_tool.cpp)
    target_link_libraries(re4_resource_tool PRIVATE re4_loader)
    add_executable(re4_loader_tool tests/loader_tool.cpp)
    target_link_libraries(re4_loader_tool PRIVATE re4_loader)
    add_executable(re4_asset_tool tests/asset_tool.cpp)
    target_link_libraries(re4_asset_tool PRIVATE re4_asset_index re4_yz2)
    add_executable(re4_core_tests tests/test_main.cpp)
    target_link_libraries(re4_core_tests PRIVATE re4_verification)
    if(NOT ANDROID)
        enable_testing()
        add_test(NAME game_runtime COMMAND re4_game_tool self-test "${CMAKE_CURRENT_BINARY_DIR}/game-tests")
        set_tests_properties(game_runtime PROPERTIES TIMEOUT 90)
        add_test(NAME native_modules COMMAND re4_module_tool self-test "${CMAKE_CURRENT_BINARY_DIR}/module-tests")
        set_tests_properties(native_modules PROPERTIES TIMEOUT 90)
        add_test(NAME resource_runtime COMMAND re4_resource_tool self-test "${CMAKE_CURRENT_BINARY_DIR}/resource-tests")
        set_tests_properties(resource_runtime PROPERTIES TIMEOUT 90)
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
        # Independent oracle tests reuse the configure-time Python interpreter.
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            add_test(NAME runtime_generation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runtime_sources.py")
            add_test(NAME consumer_generation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_consumer_sources.py")
            add_test(NAME tex_render_generation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tex_render_sources.py")
            add_test(NAME module_generation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_modules.py")
            set_tests_properties(module_generation PROPERTIES TIMEOUT 90)
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
    add_library(re4_port_jni SHARED platform/android/jni.cpp platform/android/loader_jni.cpp platform/android/module_jni.cpp platform/android/game_jni.cpp
        platform/android/graphics_jni.cpp platform/android/vulkan_bootstrap.cpp)
    target_link_libraries(re4_port_jni PRIVATE re4_verification re4_module_verification re4_game_verification re4_loader log android dl)
    target_link_options(re4_port_jni PRIVATE -Wl,--no-undefined)
endif()
