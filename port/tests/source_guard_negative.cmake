if(NOT DEFINED RE4_ROOT OR NOT DEFINED RE4_PROBE_DIR)
    message(FATAL_ERROR "RE4_ROOT and RE4_PROBE_DIR are required")
endif()
file(MAKE_DIRECTORY "${RE4_PROBE_DIR}/src/game" "${RE4_PROBE_DIR}/include")
foreach(path src/game/rnd.cpp src/game/math_sub.cpp include/rnd.h include/flag.h)
    configure_file("${RE4_ROOT}/${path}" "${RE4_PROBE_DIR}/${path}" COPYONLY)
endforeach()
# Mutate a build-directory copy only, never the repository's original source.
file(APPEND "${RE4_PROBE_DIR}/src/game/rnd.cpp" "\n// Deliberate source guard probe.\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DRE4_ROOT=${RE4_PROBE_DIR}"
        -P "${CMAKE_CURRENT_LIST_DIR}/source_guard.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Original source fingerprint mismatch")
    message(FATAL_ERROR "Source guard failed to reject changed RNG: ${output}\n${error}")
endif()
message(STATUS "Changed original source was rejected as required")
