# Whole-program port build. See port/docs/whole-program-status.md.
#
#   libre4_main.so            main.dol game units + platform runtime + generated stubs
#   modules/libre4_<mod>.so   one library per original REL module (114)
#   re4_host                  Linux launcher (not built for Android)
cmake_minimum_required(VERSION 3.20)
project(re4_whole C CXX ASM)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
get_filename_component(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$")
    set(RE4_ARCH i686)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7.*|arm)$")
    set(RE4_ARCH armv7)
else()
    message(FATAL_ERROR "unsupported processor ${CMAKE_SYSTEM_PROCESSOR}: the port is ILP32 for now (milestone 12 adds ARM64)")
endif()

# --- prepared original sources -------------------------------------------------
set(RE4_PREPARED "${CMAKE_CURRENT_BINARY_DIR}/prepared")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${RE4_ROOT}/port/tools/whole_prepare.py" --out "${RE4_PREPARED}"
    RESULT_VARIABLE prepare_result)
if(NOT prepare_result EQUAL 0)
    message(FATAL_ERROR "whole_prepare.py failed")
endif()
file(GLOB_RECURSE RE4_PREPARE_INPUTS CONFIGURE_DEPENDS
    "${RE4_ROOT}/src/*" "${RE4_ROOT}/include/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch"
    "${RE4_ROOT}/config/G4BE08/*.py" "${RE4_ROOT}/config/G4BE08/config.yml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RE4_ROOT}/port/tools/whole_prepare.py" ${RE4_PREPARE_INPUTS})

file(READ "${RE4_PREPARED}/modules.json" RE4_PLAN)
string(JSON RE4_MAIN_COUNT LENGTH "${RE4_PLAN}" main)
string(JSON RE4_MODULE_COUNT LENGTH "${RE4_PLAN}" modules)
set(RE4_TREE "${RE4_PREPARED}/tree")

# --- compile settings of original game code ------------------------------------
add_library(re4_game_flags INTERFACE)
target_include_directories(re4_game_flags BEFORE INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${RE4_TREE}/include"
    "${RE4_TREE}/src")  # overlay units include shared units as "wep/..." or "tools/..."
target_compile_options(re4_game_flags INTERFACE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/include/re4_whole_prelude.h"
    -std=gnu++11
    -Wno-c++11-narrowing
    -fdelayed-template-parsing
    -fno-strict-aliasing
    -ffp-contract=off
    -fwrapv
    -fno-strict-return
    -w
    -Wno-address-of-temporary
    -ferror-limit=0)

set(RE4_LINK_OBJECTS "")
function(re4_unit_list json_path out_var)
    string(JSON n LENGTH "${RE4_PLAN}" ${json_path})
    math(EXPR last "${n} - 1")
    set(units "")
    foreach(i RANGE ${last})
        string(JSON unit GET "${RE4_PLAN}" ${json_path} ${i})
        set(path "${RE4_TREE}/${unit}")
        get_filename_component(dir "${path}" DIRECTORY)
        # each unit includes headers from its own directory with ""
        set_property(SOURCE "${path}" APPEND PROPERTY INCLUDE_DIRECTORIES "${dir}")
        list(APPEND units "${path}")
    endforeach()
    set(${out_var} "${units}" PARENT_SCOPE)
endfunction()

re4_unit_list("main" RE4_MAIN_UNITS)
add_library(re4_main_obj OBJECT ${RE4_MAIN_UNITS})
target_link_libraries(re4_main_obj PRIVATE re4_game_flags)
string(APPEND RE4_LINK_OBJECTS "main=$<TARGET_OBJECTS:re4_main_obj>\n")

add_library(re4_runtime_obj OBJECT
    runtime/aram.cpp
    runtime/dolphin_os.cpp
    runtime/dvd.cpp
    runtime/log.cpp
    runtime/mem1.cpp
    runtime/module_loader.cpp
    runtime/sn_runtime.cpp)
target_include_directories(re4_runtime_obj PUBLIC runtime)
target_compile_features(re4_runtime_obj PRIVATE cxx_std_17)

# Portable C parts of the Dolphin SDK (src/lib), compiled unchanged: the heap
# allocator, whose descriptors the game reads directly (main_mem.cpp).
set(RE4_SDK_UNITS OSAlloc OSArena)
set(RE4_SDK_SOURCES "")
foreach(unit IN LISTS RE4_SDK_UNITS)
    list(APPEND RE4_SDK_SOURCES "${RE4_TREE}/src/lib/${unit}.c")
endforeach()
add_library(re4_sdk_obj OBJECT ${RE4_SDK_SOURCES})
target_include_directories(re4_sdk_obj BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${RE4_TREE}/include" "${RE4_TREE}/src/lib")
target_compile_definitions(re4_sdk_obj PRIVATE SDK_REVISION=1 NDEBUG=1)
target_compile_options(re4_sdk_obj PRIVATE -std=gnu99 -fno-strict-aliasing -fwrapv -w)
string(APPEND RE4_LINK_OBJECTS
    "runtime=$<TARGET_OBJECTS:re4_runtime_obj>;$<TARGET_OBJECTS:re4_sdk_obj>\n")

set(RE4_MODULES "")
math(EXPR last_module "${RE4_MODULE_COUNT} - 1")
foreach(i RANGE ${last_module})
    string(JSON mod MEMBER "${RE4_PLAN}" modules ${i})
    list(APPEND RE4_MODULES ${mod})
    re4_unit_list("modules;${mod};sources" units)
    string(JSON ndef LENGTH "${RE4_PLAN}" modules ${mod} defines)
    math(EXPR last_def "${ndef} - 1")
    set(defines "")
    foreach(d RANGE ${last_def})
        string(JSON def GET "${RE4_PLAN}" modules ${mod} defines ${d})
        list(APPEND defines "${def}")
    endforeach()
    add_library(re4_mod_${mod}_obj OBJECT ${units})
    target_link_libraries(re4_mod_${mod}_obj PRIVATE re4_game_flags)
    target_compile_definitions(re4_mod_${mod}_obj PRIVATE ${defines})
    string(APPEND RE4_LINK_OBJECTS "${mod}=$<TARGET_OBJECTS:re4_mod_${mod}_obj>\n")
endforeach()

# --- link plan: stubs, module table, cross-module trampolines ------------------
set(RE4_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/link_objects.txt" CONTENT "${RE4_LINK_OBJECTS}")
set(RE4_GEN_FILES "${RE4_GEN}/stubs.cpp" "${RE4_GEN}/modules.cpp" "${RE4_GEN}/link_report.json")
set(RE4_OBJ_DEPS re4_main_obj re4_runtime_obj re4_sdk_obj
    "$<TARGET_OBJECTS:re4_main_obj>" "$<TARGET_OBJECTS:re4_runtime_obj>"
    "$<TARGET_OBJECTS:re4_sdk_obj>")
foreach(mod IN LISTS RE4_MODULES)
    list(APPEND RE4_GEN_FILES "${RE4_GEN}/imports/${mod}.cpp" "${RE4_GEN}/imports/${mod}.S")
    list(APPEND RE4_OBJ_DEPS re4_mod_${mod}_obj "$<TARGET_OBJECTS:re4_mod_${mod}_obj>")
endforeach()
if(CMAKE_CXX_COMPILER_TARGET)
    set(RE4_TARGET_FLAG "--target=${CMAKE_CXX_COMPILER_TARGET}")
endif()
add_custom_command(
    OUTPUT "${RE4_GEN}/link_plan.stamp"
    BYPRODUCTS ${RE4_GEN_FILES}
    COMMAND "${Python3_EXECUTABLE}" "${RE4_ROOT}/port/tools/whole_link_plan.py"
        --plan "${RE4_PREPARED}/modules.json"
        --objects "${CMAKE_CURRENT_BINARY_DIR}/link_objects.txt"
        --arch ${RE4_ARCH} --out "${RE4_GEN}"
        -- "${CMAKE_CXX_COMPILER}" ${RE4_TARGET_FLAG}
    COMMAND "${CMAKE_COMMAND}" -E touch "${RE4_GEN}/link_plan.stamp"
    DEPENDS ${RE4_OBJ_DEPS} "${RE4_ROOT}/port/tools/whole_link_plan.py"
    COMMENT "Planning the link (stubs, module imports)"
    VERBATIM)
add_custom_target(re4_link_plan DEPENDS "${RE4_GEN}/link_plan.stamp")

set(RE4_LINK_FLAGS -fuse-ld=lld -Wl,--no-undefined)

# --- libre4_main ------------------------------------------------------------------
add_library(re4_main SHARED
    $<TARGET_OBJECTS:re4_main_obj>
    $<TARGET_OBJECTS:re4_runtime_obj>
    $<TARGET_OBJECTS:re4_sdk_obj>
    "${RE4_GEN}/stubs.cpp"
    "${RE4_GEN}/modules.cpp")
add_dependencies(re4_main re4_link_plan)
target_include_directories(re4_main PRIVATE runtime)
target_link_options(re4_main PRIVATE ${RE4_LINK_FLAGS})
target_link_libraries(re4_main PRIVATE ${CMAKE_DL_LIBS} m)
if(ANDROID)
    target_link_libraries(re4_main PRIVATE log)
endif()

# --- one library per REL module ---------------------------------------------------
foreach(mod IN LISTS RE4_MODULES)
    add_library(re4_${mod} SHARED
        $<TARGET_OBJECTS:re4_mod_${mod}_obj>
        runtime/module_crt.cpp
        "${RE4_GEN}/imports/${mod}.cpp"
        "${RE4_GEN}/imports/${mod}.S")
    add_dependencies(re4_${mod} re4_link_plan)
    target_include_directories(re4_${mod} PRIVATE runtime)
    # -Bsymbolic: a module's references bind to its own copies of duplicated units
    target_link_options(re4_${mod} PRIVATE ${RE4_LINK_FLAGS} -Wl,-Bsymbolic)
    target_link_libraries(re4_${mod} PRIVATE re4_main)
    set_target_properties(re4_${mod} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules")
endforeach()

# --- trampoline self-test (re4_host --self-test) -------------------------------------
add_library(re4test_target SHARED tests/trampoline_target.cpp)
add_custom_command(
    OUTPUT "${RE4_GEN}/test/re4test_caller.cpp" "${RE4_GEN}/test/re4test_caller.S"
    COMMAND "${Python3_EXECUTABLE}" "${RE4_ROOT}/port/tools/whole_link_plan.py"
        --emit-imports re4test_caller ${RE4_ARCH} "${RE4_GEN}/test" re4test_mix re4test_wide
    DEPENDS "${RE4_ROOT}/port/tools/whole_link_plan.py"
    VERBATIM)
add_library(re4test_caller SHARED tests/trampoline_caller.cpp
    "${RE4_GEN}/test/re4test_caller.cpp" "${RE4_GEN}/test/re4test_caller.S")
target_include_directories(re4test_caller PRIVATE runtime)
foreach(t re4test_target re4test_caller)
    target_link_options(${t} PRIVATE ${RE4_LINK_FLAGS} -Wl,-Bsymbolic)
    target_link_libraries(${t} PRIVATE re4_main)
    set_target_properties(${t} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules"
        OUTPUT_NAME re4_${t})
endforeach()

# --- Linux launcher -----------------------------------------------------------------
if(NOT ANDROID)
    add_executable(re4_host host/main.cpp)
    target_include_directories(re4_host PRIVATE runtime)
    target_link_options(re4_host PRIVATE -fuse-ld=lld)
    target_link_libraries(re4_host PRIVATE re4_main ${CMAKE_DL_LIBS})
endif()
