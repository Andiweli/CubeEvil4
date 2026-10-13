# Whole-program port build: compiles every original game unit (src/ without
# src/lib) in one go. See port/docs/whole-program-status.md.
cmake_minimum_required(VERSION 3.20)
project(re4_whole C CXX)

get_filename_component(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

set(RE4_PREPARED "${CMAKE_CURRENT_BINARY_DIR}/prepared")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${RE4_ROOT}/port/tools/whole_prepare.py" --out "${RE4_PREPARED}"
    RESULT_VARIABLE prepare_result)
if(NOT prepare_result EQUAL 0)
    message(FATAL_ERROR "whole_prepare.py failed")
endif()
# Re-run the preparation whenever an original source, a patch or the script changes.
file(GLOB_RECURSE RE4_PREPARE_INPUTS CONFIGURE_DEPENDS
    "${RE4_ROOT}/src/*" "${RE4_ROOT}/include/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RE4_ROOT}/port/tools/whole_prepare.py" ${RE4_PREPARE_INPUTS})

file(STRINGS "${RE4_PREPARED}/sources.txt" RE4_GAME_UNITS)
list(TRANSFORM RE4_GAME_UNITS PREPEND "${RE4_PREPARED}/tree/")

add_library(re4_game OBJECT ${RE4_GAME_UNITS})
target_include_directories(re4_game BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${RE4_PREPARED}/tree/include"
    "${RE4_PREPARED}/tree/src")  # overlay units include shared units as "wep/..." or "tools/..."
target_compile_options(re4_game PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/include/re4_whole_prelude.h"
    -std=gnu++11
    -Wno-c++11-narrowing
    -fdelayed-template-parsing
    -fno-strict-aliasing
    -ffp-contract=off
    -fwrapv
    -w
    -Wno-address-of-temporary
    -ferror-limit=0)
# Each unit includes headers from its own directory with "".
foreach(unit IN LISTS RE4_GAME_UNITS)
    get_filename_component(unit_dir "${unit}" DIRECTORY)
    set_property(SOURCE "${unit}" APPEND PROPERTY INCLUDE_DIRECTORIES "${unit_dir}")
endforeach()
