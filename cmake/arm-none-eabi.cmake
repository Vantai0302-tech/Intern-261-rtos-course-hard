set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

# Compiler
set(CMAKE_C_COMPILER            arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER          arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER          ${CMAKE_C_COMPILER})


set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "Object copy tool")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "Binary size tool")

set(CMAKE_C_FLAGS_INIT 
        "-mcpu=cortex-m3 -mthumb -Wall -Wextra -Werror"
)
set(CMAKE_CXX_FLAGS_INIT
        "-mcpu=cortex-m3 -mthumb -Wall -Wextra -Werror"
)
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m3 -mthumb")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcpu=cortex-m3 -mthumb")




