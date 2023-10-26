/**********************************************************************
 * File : spi0.h
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
#ifndef spi0_h
#define spi0_h
#include <stdint.h>
#include <stdbool.h>

  /**SPI1 GPIO Configuration
  PA4   ------> SPI1_CS
  PA5   ------> SPI1_SCK
  PA6   ------> SPI1_MISO
  PA7   ------> SPI1_MOSI
  */
void spi1_init(void);

void spi1_cs(bool cs);
uint8_t spi1_send(uint8_t data);
void spi1_speedUp(void);
#endif /* spi0.h */
