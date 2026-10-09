# Fingerprints are SHA-256 of UTF-8 text with CRLF normalized to LF. This
# supports Windows/Linux checkouts while rejecting changes to approved sources.
function(re4_check_original root relative expected)
    set(path "${root}/${relative}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing original source: ${relative}")
    endif()
    file(READ "${path}" contents)
    string(REPLACE "\r\n" "\n" contents "${contents}")
    string(SHA256 actual "${contents}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Original source fingerprint mismatch: ${relative}\n"
            "Expected ${expected}\nActual   ${actual}\n"
            "Review source changes before updating the port manifest.")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
endfunction()

function(re4_verify_original_sources root)
    re4_check_original("${root}" src/game/rnd.cpp
        ef577e2151d2452c1d1a068f75e95d9e8b3ab63b696422e6346fb435d65f203b)
    re4_check_original("${root}" include/rnd.h
        2df217a48dba775810100e9debc096d8c2c58e379b9643c24f24a6f3a3933c89)
    re4_check_original("${root}" include/flag.h
        b7c0d5dc0af481e1934493bd26972b918fec40c9806952871a7e0125c7730495)
    re4_check_original("${root}" src/game/math_sub.cpp
        8ab63b9eda407cc37b4c679842e723d8e4564669149103ef422030498162a637)
endfunction()

# This deliberately narrow extractor accepts only the two fingerprinted pure
# functions below. Their bodies contain no braces in strings or comments. It
# copies the complete definition without rewriting expressions or signatures.
function(re4_extract_function path signature output_variable)
    file(READ "${path}" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    string(FIND "${source}" "${signature}\n{" start)
    if(start LESS 0)
        message(FATAL_ERROR "Original signature not found: ${signature}")
    endif()
    string(SUBSTRING "${source}" 0 ${start} prefix)
    string(REGEX MATCHALL "\n" newlines "${prefix}")
    list(LENGTH newlines line)
    math(EXPR line "${line} + 1")
    string(SUBSTRING "${source}" ${start} -1 tail)
    string(LENGTH "${tail}" length)
    set(depth 0)
    set(opened FALSE)
    set(done FALSE)
    math(EXPR last "${length} - 1")
    foreach(index RANGE 0 ${last})
        string(SUBSTRING "${tail}" ${index} 1 char)
        if(char STREQUAL "{")
            math(EXPR depth "${depth} + 1")
            set(opened TRUE)
        elseif(char STREQUAL "}")
            math(EXPR depth "${depth} - 1")
            if(opened AND depth EQUAL 0)
                math(EXPR count "${index} + 1")
                string(SUBSTRING "${tail}" 0 ${count} definition)
                set(done TRUE)
                break()
            endif()
        endif()
    endforeach()
    if(NOT done)
        message(FATAL_ERROR "Unbalanced original function: ${signature}")
    endif()
    set(${output_variable} "#line ${line} \"${path}\"\n${definition}\n" PARENT_SCOPE)
endfunction()

function(re4_prepare_original_sources root destination)
    re4_verify_original_sources("${root}")
    file(MAKE_DIRECTORY "${destination}/include")
    configure_file("${root}/include/rnd.h" "${destination}/include/rnd.h" COPYONLY)
    configure_file("${root}/include/flag.h" "${destination}/include/flag.h" COPYONLY)
    re4_extract_function("${root}/src/game/math_sub.cpp"
        "f32 hermite(f32* x, f32* v, f32 t)" hermite_definition)
    re4_extract_function("${root}/src/game/math_sub.cpp"
        "f32 IPOW(f32 x, int y)" ipow_definition)
    file(CONFIGURE OUTPUT "${destination}/math.cpp" CONTENT
        "// Generated from fingerprinted original code; do not edit.\n#include \"re4/port/original_math.h\"\n${hermite_definition}\n${ipow_definition}"
        @ONLY)
endfunction()
