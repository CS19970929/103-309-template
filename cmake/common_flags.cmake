function(target_arm_none_eabi_options target)
    if(NOT MCU_CPU)
        message(FATAL_ERROR "MCU_CPU is not set")
    endif()
    if(NOT MCU_FLOAT_ABI)
        message(FATAL_ERROR "MCU_FLOAT_ABI is not set")
    endif()
    if(NOT LINKER_SCRIPT)
        message(FATAL_ERROR "LINKER_SCRIPT is not set")
    endif()

    target_compile_options(${target} PRIVATE
        -mcpu=${MCU_CPU}
        -mthumb
        -mfloat-abi=${MCU_FLOAT_ABI}
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wall
        -Wno-main
        -include
        "${PROJECT_SOURCE_DIR}/cmake/compat/compiler_port.h"
        $<$<COMPILE_LANGUAGE:ASM>:-x>
        $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
        $<$<CONFIG:Debug>:-Og>
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Release>:-Os>
        $<$<CONFIG:Release>:-g>
    )

    target_link_options(${target} PRIVATE
        -mcpu=${MCU_CPU}
        -mthumb
        -mfloat-abi=${MCU_FLOAT_ABI}
        -T${LINKER_SCRIPT}
        -Wl,-Map=$<TARGET_FILE_DIR:${target}>/firmware.map
        -Wl,--gc-sections
        -Wl,--cref
        -Wl,--print-memory-usage
        --specs=nano.specs
        --specs=nosys.specs
    )
endfunction()
