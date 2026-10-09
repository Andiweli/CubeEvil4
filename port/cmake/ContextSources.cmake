# The four assembly files are unchanged Boost.Context 1.88.0 sources.
function(re4_add_context_sources target)
    set(base "third_party/boost_context")
    re4_check_original("${CMAKE_CURRENT_SOURCE_DIR}" "${base}/jump_arm_aapcs_elf_gas.S"
        7a9c50f5178778b4f920c7091235e9ea4fb9329ed73bd25d756bc4f36e335320)
    re4_check_original("${CMAKE_CURRENT_SOURCE_DIR}" "${base}/make_arm_aapcs_elf_gas.S"
        48b89b69b2ef51f49ee078b45a6a508ae0f61297410b5333cf707a1b07b64d1d)
    re4_check_original("${CMAKE_CURRENT_SOURCE_DIR}" "${base}/jump_arm64_aapcs_elf_gas.S"
        c7d0e3687cc8d678097befebc0a5072f3db0e87e1f1894b9287d0dbf0c9f248f)
    re4_check_original("${CMAKE_CURRENT_SOURCE_DIR}" "${base}/make_arm64_aapcs_elf_gas.S"
        6c4a66435182615097418217af504604ac28f233c4c5289fc2751f05f8aaa78e)
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(arch arm)
    elseif(ANDROID_ABI STREQUAL "arm64-v8a")
        set(arch arm64)
    else()
        message(FATAL_ERROR "No verified fcontext backend for ${ANDROID_ABI}")
    endif()
    set(sources "${base}/make_${arch}_aapcs_elf_gas.S" "${base}/jump_${arch}_aapcs_elf_gas.S")
    target_sources(${target} PRIVATE ${sources})
    if(arch STREQUAL "arm")
        # Android softfp still has callee-saved d8-d15. Force their preservation
        # in upstream's conditional assembly; this changes NO C++ calling ABI.
        # Compile assembly in ARM mode; Thumb callers interwork via BLX/PC bit.
        set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "-marm;-U__SOFTFP__")
    endif()
endfunction()
