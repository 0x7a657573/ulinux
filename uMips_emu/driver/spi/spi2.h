/**********************************************************************
 * File : spi2_h
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
#ifndef spi2_h
#define spi2_h
#include <stdint.h>
#include <stdbool.h>

/**SPI2 GPIO Configuration
 PB12   ------> SPI2_CS
 PB13   ------> SPI2_SCK
 PB14   ------> SPI2_MISO
 PB15   ------> SPI2_MOSI
 */
void spi2_init(void);

void spi2_cs(bool cs);
uint8_t spi2_send(uint8_t data);
void spi2_speedUp(void);
#endif /* spi2_h */
