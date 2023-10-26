/**********************************************************************
 * File : bsp.h.h
 * Copyright (c) 0x7a657573.
 * Created On : Wed Mar 29 2023
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#ifndef bsp_h
#define bsp_h

#if(STM32F1xx)
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_rcc.h>
#include <stm32f1xx_ll_system.h>
#include <stm32f1xx_ll_utils.h>
#include <stm32f1xx_ll_gpio.h>

#define LED_PIN                           LL_GPIO_PIN_13
#define LED_GPIO_PORT                     GPIOC
#define LED_GPIO_CLK_ENABLE()             LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC)


#elif (STM32F4xx)
#include <stm32f4xx.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_rcc.h>
#include <stm32f4xx_ll_system.h>

#define LED_PIN                           LL_GPIO_PIN_12
#define LED_GPIO_PORT                     GPIOD
#define LED_GPIO_CLK_ENABLE()             LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD)

#elif (STM32F0xx)
#include <stm32f0xx_ll_bus.h>
#include <stm32f0xx_ll_rcc.h>
#include <stm32f0xx_ll_system.h>
#include <stm32f0xx_ll_utils.h>
#include <stm32f0xx_ll_gpio.h>

#define LED_PIN                           LL_GPIO_PIN_13
#define LED_GPIO_PORT                     GPIOC
#define LED_GPIO_CLK_ENABLE()             LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC)

#else
    #error Please add your board config
#endif


#endif /* bsp_h */
