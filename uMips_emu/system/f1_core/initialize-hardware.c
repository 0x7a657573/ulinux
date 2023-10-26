/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_cortex.h"

#include "diag/trace.h"

#ifdef USE_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <portmacro.h>
#endif
// ----------------------------------------------------------------------------

// The external clock frequency is specified as a preprocessor definition
// passed to the compiler via a command line option (see the 'C/C++ General' ->
// 'Paths and Symbols' -> the 'Symbols' tab, if you want to change it).
// The value selected during project creation was HSE_VALUE=8000000.
//
// The code to set the clock is at the end.
//
// Note1: The default clock settings assume that the HSE_VALUE is a multiple
// of 1MHz, and try to reach the maximum speed available for the
// board. It does NOT guarantee that the required USB clock of 48MHz is
// available. If you need this, please update the settings of PLL_M, PLL_N,
// PLL_P, PLL_Q to match your needs.
//
// Note2: The external memory controllers are not enabled. If needed, you
// have to define DATA_IN_ExtSRAM or DATA_IN_ExtSDRAM and to configure
// the memory banks in system/src/cmsis/system_stm32f4xx.c to match your needs.

// ----------------------------------------------------------------------------

// Forward declarations.

void
__initialize_hardware(void);

void
SystemClock_Config(void);

// ----------------------------------------------------------------------------

// This is the application hardware initialisation routine,
// redefined to add more inits.
//
// Called early from _start(), right after data & bss init, before
// constructors.
//
// After Reset the Cortex-M processor is in Thread mode,
// priority is Privileged, and the Stack is set to Main.
//
// Warning: The HAL requires the system timer, running at 1000 Hz
// and calling HAL_IncTick().

void
__initialize_hardware(void)
{
  // Initialise the HAL Library; it must be the first function
  // to be executed before the call of any HAL function.
  HAL_Init();

  // Enable HSE Oscillator and activate PLL with HSE as source
  SystemClock_Config();

  // Call the CSMSIS system clock routine to store the clock frequency
  // in the SystemCoreClock global RAM location.
  SystemCoreClockUpdate();

// If use RTOS Try Setup RTOS
#ifdef USE_FREERTOS
    extern void main( void * pvParameters );
    /* Create main tasks defined within main.c itself */
    xTaskCreate( main, "main", FREERTOS_MAINSTACK/sizeof(int), NULL, tskIDLE_PRIORITY, NULL );
    /* Start the tasks and timer running. */
    vTaskStartScheduler();
#endif
}

// Disable when using RTOSes, since they have their own handler.
#ifndef USE_FREERTOS
void disk_timerproc(void);
// // This is a sample SysTick handler, use it if you need HAL timings.
// void __attribute__ ((section(".after_vectors")))
// SysTick_Handler(void)
// {
// #if defined(USE_HAL_DRIVER)
// 	HAL_IncTick();
// #endif
//   disk_timerproc();
// }

#else

/* redefine HAL_Delay use RTOS*/
void HAL_Delay(uint32_t Delay)
{
  vTaskDelay(Delay/portTICK_PERIOD_MS);
}

uint32_t HAL_GetTick(void)
{
  return xTaskGetTickCount();
}

/* redefine malloc and free to use freertos heap manager */
void *pvPortMalloc( size_t xWantedSize );
void vPortFree( void *pv );

void free(void *p)
{
	vPortFree(p);
}

void *malloc(size_t s)
{
	if(s==0)	return 0;
	return pvPortMalloc(s);
}

#endif

// ----------------------------------------------------------------------------

/**
 * @brief  System Clock Configuration
 * @param  None
 * @retval None
 */
void
__attribute__((weak))
SystemClock_Config(void)
{
  // TODO: add clock inits.
  
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

// ----------------------------------------------------------------------------

#if defined(USE_FULL_ASSERT)
/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void
assert_failed (uint8_t* file, uint32_t line)
{
  // Change to a custom implementation to report the file name and line number.
  trace_printf("Wrong parameters value: file %s on line %d\r\n", file, line);

  // Infinite loop
  while (1)
  {
    ;
  }
}
#endif

// ----------------------------------------------------------------------------
