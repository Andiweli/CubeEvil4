if(NOT DEFINED RE4_ROOT OR NOT DEFINED RE4_PROBE_DIR)
    message(FATAL_ERROR "RE4_ROOT and RE4_PROBE_DIR are required")
endif()
file(MAKE_DIRECTORY "${RE4_PROBE_DIR}/src/game" "${RE4_PROBE_DIR}/include")
foreach(path src/game/rnd.cpp src/game/math_sub.cpp include/rnd.h include/flag.h
    src/game/scheduler.cpp src/game/sce_sys.cpp include/scheduler.h)
    configure_file("${RE4_ROOT}/${path}" "${RE4_PROBE_DIR}/${path}" COPYONLY)
endforeach()
# Mutate build-directory copies one at a time, never the original sources.
foreach(probe src/game/rnd.cpp src/game/scheduler.cpp src/game/sce_sys.cpp include/scheduler.h)
    file(APPEND "${RE4_PROBE_DIR}/${probe}" "\n// Deliberate source guard probe.\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_PROBE_DIR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/source_guard.cmake"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "Original source fingerprint mismatch: ${probe}")
        message(FATAL_ERROR "Source guard failed to reject changed ${probe}: ${output}\n${error}")
    endif()
    configure_file("${RE4_ROOT}/${probe}" "${RE4_PROBE_DIR}/${probe}" COPYONLY)
endforeach()
message(STATUS "Changed RNG, scheduler, scenario source and TASK methods were rejected")
