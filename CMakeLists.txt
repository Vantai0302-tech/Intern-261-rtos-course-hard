cmake_minimum_required(VERSION 3.21)
project(rtos_course LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(WEEK1_DIR "${CMAKE_CURRENT_SOURCE_DIR}/week1" CACHE PATH "Week 1 source directory")
set(WEEK2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/week2" CACHE PATH "Week 2 source directory")
set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel" CACHE PATH "FreeRTOS V11.1.0")
set(CMSIS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMSIS/Core/Include" CACHE PATH "CMSIS Core headers")
set(CMSIS_DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMSIS/Device/Include" CACHE PATH "STM32F1 device headers")
set(PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/platform")
set(LINKER_SCRIPT "${PLATFORM}/linker/stm32f103c8.ld")
foreach(required "${FREERTOS_DIR}/tasks.c" "${CMSIS_CORE_DIR}/core_cm3.h" "${CMSIS_DEVICE_DIR}/stm32f1xx.h")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing dependency: ${required}.")
    endif()
endforeach()
find_program(OPENOCD_EXECUTABLE openocd)
function(add_firmware name exercise static_memory trace optimization)
    add_executable(${name}
        ${PLATFORM}/startup.c ${PLATFORM}/system.c ${PLATFORM}/gpio.c
        ${PLATFORM}/uart.c ${PLATFORM}/hooks.c
        ${FREERTOS_DIR}/tasks.c ${FREERTOS_DIR}/queue.c ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/timers.c ${FREERTOS_DIR}/portable/GCC/ARM_CM3/port.c
        ${ARGN})
    if(NOT static_memory)
        target_sources(${name} PRIVATE ${FREERTOS_DIR}/portable/MemMang/heap_4.c)
    endif()
    if(trace)
        target_sources(${name} PRIVATE ${WEEK1_DIR}/ex2/tracer.c)
    endif()
    target_include_directories(${name} PRIVATE
        ${PLATFORM} ${WEEK1_DIR}/${exercise}/include
        ${WEEK1_DIR}/ex2/include ${WEEK1_DIR}/ex3/include ${WEEK1_DIR}/ex1/include
        ${FREERTOS_DIR}/include ${FREERTOS_DIR}/portable/GCC/ARM_CM3
        ${CMSIS_CORE_DIR} ${CMSIS_DEVICE_DIR})
    target_compile_definitions(${name} PRIVATE STM32F103xB APP_STATIC=${static_memory} APP_TRACE=${trace})
    target_compile_options(${name} PRIVATE
        -Wall -Wextra -Werror -mcpu=cortex-m3 -mthumb ${optimization}
        -ffunction-sections -fdata-sections -fno-common -g3)
    target_link_options(${name} PRIVATE -mcpu=cortex-m3 -mthumb
        -nostartfiles --specs=nosys.specs "-T${LINKER_SCRIPT}"
        "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/${name}.map"
        LINKER:--gc-sections LINKER:--print-memory-usage)
    set_target_properties(${name} PROPERTIES SUFFIX .elf LINK_DEPENDS "${LINKER_SCRIPT}")
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${name}> ${name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}>
        VERBATIM)
    if(OPENOCD_EXECUTABLE)
        add_custom_target(${name}_flash
            COMMAND ${OPENOCD_EXECUTABLE} -f interface/stlink.cfg -f target/stm32f1x.cfg
                -c "program {$<TARGET_FILE:${name}>} verify reset exit"
            DEPENDS ${name} USES_TERMINAL VERBATIM)
    else()
        add_custom_target(${name}_flash
            COMMAND ${CMAKE_COMMAND} -E echo "Install OpenOCD, add to PATH, and configure again."
            COMMAND ${CMAKE_COMMAND} -E false
            DEPENDS ${name} VERBATIM)
    endif()
endfunction()
add_firmware(w1e1 ex1 0 0 -O0 ${WEEK1_DIR}/ex1/main.c ${WEEK1_DIR}/ex1/blinky.c)
add_firmware(w1e2 ex2 0 1 -O0 ${WEEK1_DIR}/ex2/main.c)
add_firmware(w1e2_o2 ex2 0 1 -O2 ${WEEK1_DIR}/ex2/main.c)
add_firmware(w2e1 ex1 1 0 -O0
    ${WEEK2_DIR}/ex1/main.c
    ${WEEK2_DIR}/ex1/i2c1_driver.c)
target_include_directories(w2e1 BEFORE PRIVATE ${WEEK2_DIR}/ex1/include)
add_firmware(w2e1_fault ex1 1 0 -O0
    ${WEEK2_DIR}/ex1/fault_main.c
    ${WEEK2_DIR}/ex1/i2c1_driver.c)
target_include_directories(w2e1_fault BEFORE PRIVATE ${WEEK2_DIR}/ex1/include)
add_firmware(w2e2 ex2 1 0 -O0
    ${WEEK2_DIR}/ex2/main.c
    ${WEEK2_DIR}/ex2/spsc_ring.c)
target_include_directories(w2e2 BEFORE PRIVATE ${WEEK2_DIR}/ex2/include)
add_firmware(w2e3 ex3 1 1 -O0
    ${WEEK2_DIR}/ex3/main.c
    ${WEEK2_DIR}/ex3/priority_ceiling.c)
target_include_directories(w2e3 BEFORE PRIVATE ${WEEK2_DIR}/ex3/include)
add_firmware(w2e3_chain ex3 1 1 -O0
    ${WEEK2_DIR}/ex3/chained_main.c)
target_include_directories(w2e3_chain BEFORE PRIVATE ${WEEK2_DIR}/ex3/include)
add_firmware(w2e3_deadlock ex3 1 1 -O0
    ${WEEK2_DIR}/ex3/deadlock_main.c)
target_include_directories(w2e3_deadlock BEFORE PRIVATE ${WEEK2_DIR}/ex3/include)
add_firmware(w2e3_ceiling ex3 1 1 -O0
    ${WEEK2_DIR}/ex3/ceiling_main.c
    ${WEEK2_DIR}/ex3/priority_ceiling.c)
target_include_directories(w2e3_ceiling BEFORE PRIVATE ${WEEK2_DIR}/ex3/include)
add_firmware(w2e3_ceiling_inversion ex3 1 1 -O0
    ${WEEK2_DIR}/ex3/ceiling_inversion_main.c
    ${WEEK2_DIR}/ex3/priority_ceiling.c)
target_include_directories(w2e3_ceiling_inversion BEFORE PRIVATE ${WEEK2_DIR}/ex3/include)
# add_firmware(w1e3 ex3 0 1 -O0 ${WEEK1_DIR}/ex3/main.c ${WEEK1_DIR}/ex3/rta_app.c)
# add_firmware(w1e4 ex4 0 0 -O0
#     ${WEEK1_DIR}/ex4/main.c
#     ${WEEK1_DIR}/ex4/heap_experiment.c)
add_firmware(w2e4 ex4 1 1 -O0
    ${WEEK2_DIR}/ex4/main.c
    ${WEEK2_DIR}/ex4/sync_app.c
    ${FREERTOS_DIR}/event_groups.c)
target_include_directories(w2e4 BEFORE PRIVATE ${WEEK2_DIR}/ex4/include)
