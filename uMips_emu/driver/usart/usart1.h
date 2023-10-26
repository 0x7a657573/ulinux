/**********************************************************************
 * File : usart1.h
 * Copyright (c) 0x7a657573.
 * Created On : Fri Oct 13 2023
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
#ifndef usart1_h
#define usart1_h
#include <stdint.h>

 /**USART1 GPIO Configuration
 PA9   ------> USART1_TX
 PA10   ------> USART1_RX
*/

void usart1_Init(uint32_t baud);
void usart1_Send(uint8_t data);

#endif /* usart1_h */
