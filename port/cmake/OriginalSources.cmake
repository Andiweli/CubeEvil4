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
    re4_check_original("${root}" src/game/scheduler.cpp
        eae7df58b3b08f6e3960d288a1cb9df24cfc1d9335ed2de447086a5d8081270f)
    re4_check_original("${root}" src/game/sce_sys.cpp
        42959ebb61ff26b16a46a432120ce1b1fd36572576269dadfe9623052f9c6c61)
    re4_check_original("${root}" include/scheduler.h
        04d99c7fa72b403e4a75ae5d24693382af9de3beabe61dceda57063006e2cd33)
    re4_check_original("${root}" src/game/yz2code.cpp
        943e24fd981660ddaa17b86e9aac5e46578200ebdf6c2cc866d18630fce204fc)
    re4_check_original("${root}" src/game/yz2asm.cpp
        0b58a81e5be3b5ecf130662f9962e06edb556db4a832dd78c23c7ecdfabdc89c)
    re4_check_original("${root}" tools/motion/yz2.py
        bcb52e0fcf77d96a70e5b4d1fc4cd3ad51e743950f2cb868268a1943b9c0f947)
    re4_check_original("${root}" src/game/dvd.cpp
        c7edd921ecbc59d43c4e34a7b5b4d22baa8bbc70aa922f2715089db730a72bb9)
    re4_check_original("${root}" src/game/read.cpp
        0ef5bbee701eebc44a329c65014902ed8bdee425d4164feb514ac3ab15063772)
    re4_check_original("${root}" src/game/main.cpp
        945a8df0f4997f7a920eff67296b0cb586758dc61a83854c49f44fba893fd9e6)
endfunction()

# This deliberately narrow extractor accepts only the reviewed fingerprinted
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
    set(scheduler_definitions "")
    foreach(signature "u32 GetStackSize(int level)" "void TaskSuspend(int level)"
        "void TaskSignal(int level)" "u8 TaskStatus(int level)"
        "void SetTaskModelPtr(void* model, TASK* t)")
        re4_extract_function("${root}/src/game/scheduler.cpp" "${signature}" definition)
        string(APPEND scheduler_definitions "${definition}\n")
    endforeach()
    foreach(signature "void SceSleep(int ctr)" "void SceExit()" "int cSceSys::checkCTaskRange()")
        re4_extract_function("${root}/src/game/sce_sys.cpp" "${signature}" definition)
        string(APPEND scheduler_definitions "${definition}\n")
    endforeach()
    file(CONFIGURE OUTPUT "${destination}/scheduler_control.cpp" CONTENT
        "// Generated verbatim from fingerprinted originals.\n#include \"re4_scheduler_original.h\"\nnamespace re4::port::original {\n${scheduler_definitions}\n}\n" @ONLY)
    file(READ "${root}/include/scheduler.h" header)
    string(REPLACE "\r\n" "\n" header "${header}")
    string(FIND "${header}" "    int isNoSuspend()" start)
    string(FIND "${header}" "};                            // 0x348" end)
    if(start LESS 0 OR end LESS start)
        message(FATAL_ERROR "Original TASK methods not found")
    endif()
    math(EXPR count "${end} - ${start}")
    string(SUBSTRING "${header}" ${start} ${count} methods)
    file(CONFIGURE OUTPUT "${destination}/include/scheduler_methods.inc" CONTENT "${methods}" @ONLY)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Yz2Sources.cmake")
    re4_prepare_yz2("${root}" "${destination}")
endfunction()
