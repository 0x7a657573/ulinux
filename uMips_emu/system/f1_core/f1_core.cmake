set(SYS_SRC_FILES 
        system/f1_core/initialize-hardware.c
        system/f1_core/src/cmsis/system_stm32f1xx.c
        system/f1_core/src/cmsis/vectors_stm32f103xb.c
        system/f1_core/src/cortexm/exception-handlers.c
        system/f1_core/src/cortexm/initialize-hardware.c
        system/f1_core/src/cortexm/reset-hardware.c
        system/f1_core/src/diag/trace-impl.c
        system/f1_core/src/diag/trace.c
        system/f1_core/src/newlib/assert.c
        system/f1_core/src/newlib/cxx.cpp
        system/f1_core/src/newlib/exit.c
        system/f1_core/src/newlib/sbrk.c
        system/f1_core/src/newlib/startup.c
        system/f1_core/src/newlib/syscalls.c    
        # HAL Library Source
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_eth.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_dma.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_dac_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_dac.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_crc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_cortex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_cec.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_can.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_adc_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_adc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_iwdg.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_irda.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_i2s.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_i2c.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_hcd.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_gpio_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_gpio.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_flash_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_flash.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_exti.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_smartcard.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_sd.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_rtc_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_rtc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_rcc_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_rcc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_pwr.c
        # stm32f1xx_hal_driver/Src/stm32f1xx_hal_pcd_ex.c
        # stm32f1xx_hal_driver/Src/stm32f1xx_hal_pcd.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_pccard.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_nor.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_nand.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_mmc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_wwdg.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_usart.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_uart.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_tim_ex.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_tim.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_sram.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_hal_spi.c
        # LL Library Source
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_rcc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_pwr.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_i2c.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_gpio.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_fsmc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_exti.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_dma.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_dac.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_crc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_adc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_utils.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_usb.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_usart.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_tim.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_spi.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_sdmmc.c
        system/stm32f1xx_hal_driver/Src/stm32f1xx_ll_rtc.c
)

set(SYS_INC_FILES
        system/f1_core    
        system/f1_core/include
        system/f1_core/include/cmsis
        system/stm32f1xx_hal_driver/Inc
)



list(APPEND SRC_FILES ${SYS_SRC_FILES})
list(APPEND INC_FILES ${SYS_INC_FILES})