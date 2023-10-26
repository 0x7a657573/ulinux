# Microcontroller memory config
set(FLASH_SIZE "1024K" CACHE STRING "microcontroller FLASH size") 
set(RAM_SIZE  "96K" CACHE STRING "microcontroller RAM size")


#FreeRTOS Port Config
if(ENABLE_FREERTOS)
    set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "" FORCE)
    set(FREERTOS_HEAPMEM "20480" CACHE STRING "FreeRTOS HEAP Memory in Byte")
endif()

# compiler option
set(TARGET_MCU_COMPILER
    -mcpu=cortex-m4 
    -mfloat-abi=hard 
    -mfpu=fpv4-sp-d16
    -mthumb 
    -std=gnu99
    -${OPTIM}
    -g3
    -fmessage-length=0 
    -fsigned-char 
    -ffunction-sections 
    -fdata-sections 
    -Wno-unused-parameter
    -ffreestanding 
    -fno-move-loop-invariants 
    -Wall 
    -Wextra 
)

# linker option
set(TARGET_MCU_LINKER
    -mcpu=cortex-m4
    -mthumb 
    -mfloat-abi=hard 
    -mfpu=fpv4-sp-d16
    -fmessage-length=0 
    -fsigned-char 
    -ffunction-sections
    -Wall
    -nostartfiles
    -Xlinker --gc-sections
    -Wl,--print-memory-usage
    -Wl,--print-memory-usage
)

# target define
set(TARGET_MCU_DEF
    -DDEBUG 
    -DUSE_FULL_ASSERT 
    -DSTM32F407xx 
    -DUSE_HAL_DRIVER 
    -DUSE_FULL_LL_DRIVER
    -DHSE_VALUE=8000000 
    -D${MCU}
)