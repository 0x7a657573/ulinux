cmake_minimum_required (VERSION 3.15.3)

# Path to toolchain file. This one has to be before 'project()' below
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/arm-none-eabi-gcc.cmake)

# Switch for FreeRTOS support, enable it for some Applications who needs it.
set(ENABLE_FREERTOS OFF CACHE BOOL "Enable FreeRTOS")

# Switch for use newlib-nano , enable it for some Applications who needs it.
set(USE_NEWLIBNANO OFF CACHE BOOL "Use newlib-nano")

# Set Project Name
project (stm_basic_cmake)

#Optimization Options
set (OPTIM Os CACHE STRING "Default Optimization is Os") 

# Allow switching of target mcu
set(MCU_LIST STM32F1xx STM32F4xx STM32F0xx)
set(MCU STM32F1xx CACHE STRING "Default target is STM32F1xx")
set_property(CACHE MCU PROPERTY STRINGS ${MCU_LIST})

if(MCU STREQUAL STM32F1xx)
    message(STATUS "-------------STM32F1xx-------------")
    include(stm32f1xx.cmake)
elseif(MCU STREQUAL STM32F4xx)
    message(STATUS "-------------STM32F4xx-------------")
    include(stm32f4xx.cmake)
elseif(MCU STREQUAL STM32F0xx)
    message(STATUS "-------------STM32F0xx-------------")
    include(stm32f0xx.cmake)
else()
    list(JOIN MCU_LIST "\r\n" strlist)
    message(FATAL_ERROR "Please Select correct MCU from list: \n${strlist}")
endif()

enable_language(C ASM)
# List of source files
set(SRC_FILES
    src/main.c

    driver/spi/spi1.c
    driver/spi/spi2.c
    driver/psram/psram.c
    driver/usart/usart1.c
    #driver/SD/stm32f1_sd.c
    driver/timebase/timebase.c
    driver/SD/sd.c
    driver/SD/sdHwStm32_spi.c
    driver/printf/printf.c
    #driver/psram/spiRam.c
    driver/ds1287/ds1287atsamd21.c

    #MIPS
    mips/mem.c
    mips/decBus.c
    micro/dz11.c
    micro/lance.c
    micro/esar.c
    micro/sii.c
    micro/scsiDevice.c
    micro/scsiDisk.c
    micro/scsiNothing.c

    mips/cpuAsm.S
    #mips/cpu.c
    mips/fpu.c
)

# List of include files
set(INC_FILES
    Inc/
    BSP/
    driver/
    driver/printf
    driver/psram
    driver/SD
    driver/ds1287
    mips
    micro
)

# List of defines
set (TARGET_DEF_LISTS
    ${TARGET_MCU_DEF}
    -DSUPPORT_DEBUG_PRINTF
    -Derr_str=printf
	-DSLOW_FLASH 
    -DCPU_SAMD 
    -DCPU_TYPE_CM0 
    -DZWT_ADDR=$(ZWT_ADDR) 
	-DICACHE_NUM_SETS_ORDER=8
    -DL2CACHE_NUM_SETS=32 
    -DOPTIMAL_RAM_WR_SZ=32 
    -DOPTIMAL_RAM_RD_SZ=32
	#-DMULTICHANNEL_UART
	-DMICRO_LANCE 
    -DNO_FRAMEBUFFER
    -DFPU_SUPPORT_FULL
)

# List of libraries
set(LIB_LIST

)

# compiler option
set(COMPILER_OPTION ${TARGET_MCU_COMPILER})

# Linker Selection 
set(LINKER_SECTIONS
   -Tmem.ld
   -Tsections.ld
)

configure_file (
    "${PROJECT_SOURCE_DIR}/system/ldscripts/mem.ld.in"
    "${PROJECT_BINARY_DIR}/system/ldscripts/mem.ld"
)
configure_file (
    "${PROJECT_SOURCE_DIR}/system/ldscripts/sections.ld.in"
    "${PROJECT_BINARY_DIR}/system/ldscripts/sections.ld"
)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Optional: issue a message to be sure it uses the correct toolchain file.
message(STATUS "CMAKE_TOOLCHAIN_FILE is: ${CMAKE_TOOLCHAIN_FILE}")

# Add System core file
include(system/stm_system.cmake)

# Add FreeRTOS
if(ENABLE_FREERTOS)
    message(STATUS "ٍEnable FreeRTOS")

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM
    INTERFACE
    system
    )

    target_compile_definitions(freertos_config
    INTERFACE
    projCOVERAGE_TEST=0
    FREERTOS_HEAPMEM=${FREERTOS_HEAPMEM}
    )
    
    target_compile_options(freertos_config INTERFACE ${COMPILER_OPTION})

    # Select the native compile PORT
    
    set(FREERTOS_HEAP "4" CACHE STRING "")
    set(FREERTOS_MAINSTACK "512" CACHE STRING "FreeRTOS main task stack in Byte")

    add_subdirectory(system/FreeRTOS)
    list(APPEND LIB_LIST "freertos_kernel")
    list(APPEND TARGET_DEF_LISTS USE_FREERTOS)
    list(APPEND TARGET_DEF_LISTS FREERTOS_MAINSTACK=${FREERTOS_MAINSTACK})
endif(ENABLE_FREERTOS)

# use new lib nano
if(USE_NEWLIBNANO)
    list(APPEND TARGET_MCU_LINKER --specs=nano.specs)
endif(USE_NEWLIBNANO)

# Build the executable based on the source files
set(EXECUTABLE ${PROJECT_NAME}.elf)
add_executable(${EXECUTABLE} ${SRC_FILES})


# include directories to use when compiling a given target.
target_include_directories(${EXECUTABLE} PUBLIC ${INC_FILES})

# Link the executable to the simple_cmake library.
target_link_libraries(${EXECUTABLE} ${LIB_LIST})

# List of compiler defines, prefix with -D compiler option
target_compile_definitions(${EXECUTABLE} PUBLIC ${TARGET_DEF_LISTS})

# Compiler options
target_compile_options(${EXECUTABLE} PUBLIC ${COMPILER_OPTION})

# Linker options
target_link_options(${EXECUTABLE} PRIVATE
    ${LINKER_SECTIONS}
    -L${CMAKE_CURRENT_BINARY_DIR}/system/ldscripts
    ${TARGET_MCU_LINKER}
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--gc-sections
)      

# Print executable size as part of the post build process
add_custom_command(TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND ${CMAKE_SIZE_UTIL} ${EXECUTABLE}
)

# Create hex and bin files after the build
add_custom_command(TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE} ${PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXECUTABLE} ${PROJECT_NAME}.hex
)

#include Doxygen module 
include (doxygen.cmake)