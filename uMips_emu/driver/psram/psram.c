/**********************************************************************
 * File : psram.c
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
#include "psram.h"


#define ce_clr(ram) ram->cs(false)
#define ce_set(ram) ram->cs(true)

bool psram_init(psram_t *ram)
{
    
    // /*set ram output pin*/
    ce_set(ram);

    for(int i=0;i<10;i++)
        ram->spi(0xFF);

    ce_clr(ram);
        ram->spi(0x99);
    ce_set(ram);

    for(int i=0;i<10;i++)
        ram->spi(0xFF);
    return true;
}


void psram_readID(psram_t *ram,psramID_t *id)
{
    uint8_t *cid = (uint8_t*)id;
    ce_clr(ram);
    /*Send CMD*/
    ram->spi(0x9F);
    ram->spi(0x00);
    ram->spi(0x00);
    ram->spi(0x00);
    
    /*READ ID*/
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    *cid++ = ram->spi(0xFF);
    ce_set(ram);
}


void psram_Read(psram_t *ram,uint32_t addr, uint8_t *data, uint16_t sz)
{
    ce_clr(ram);
    ram->spi(0x03);
    ram->spi(addr>>16);
    ram->spi(addr>>8);
    ram->spi(addr);

    for(uint16_t i=0;i<sz;i++)
        *data++ =(uint8_t) ram->spi(0xFF);
    ce_set(ram);
}

void psram_Write(psram_t *ram,uint32_t addr, const uint8_t *data, uint16_t sz)
{
    ce_clr(ram);
    ram->spi(0x02);
    ram->spi(addr>>16);
    ram->spi(addr>>8);
    ram->spi(addr);

    for(uint16_t i=0;i<sz;i++)
        ram->spi(*data++);
    ce_set(ram);
}