
if(MCU STREQUAL STM32F1xx)
        include(system/f1_core/f1_core.cmake)
elseif(MCU STREQUAL STM32F4xx)
        include(system/f4_core/f4_core.cmake)
elseif(MCU STREQUAL STM32F0xx)
        include(system/f0_core/f0_core.cmake)
else()
        list(JOIN MCU_LIST "\r\n" strlist)
        message(FATAL_ERROR "Please Select correct MCU from list: \n${strlist}")
endif()

# add system file's
list(APPEND SRC_FILES system/syscall.c)