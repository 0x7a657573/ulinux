/**********************************************************************
 * File : psram.h
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
#ifndef psram_h
#define psram_h
#include <stdint.h>
#include <stdbool.h>


typedef uint8_t (spisend_t)(uint8_t c);
typedef void (spi_setcs)(bool cs);

typedef struct
{
    spisend_t *spi;
    spi_setcs *cs;
}psram_t;

typedef enum
{
    KGD_Fail = 0b01010101,
    KGD_Pass = 0b01011101
}psKgd_t;

typedef struct __attribute__((__packed__)) 
{
    uint8_t MF_ID;
    psKgd_t KGD;
    uint8_t EID[6];
}psramID_t;



bool psram_init(psram_t *ram);
void psram_readID(psram_t *ram,psramID_t *id);

void psram_Read(psram_t *ram,uint32_t addr, uint8_t *data, uint16_t sz);
void psram_Write(psram_t *ram,uint32_t addr, const uint8_t *data, uint16_t sz);

#endif /* psram.h */
