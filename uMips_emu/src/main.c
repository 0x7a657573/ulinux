/**********************************************************************
 * File : main.c
 * Copyright (c) 0x7a657573.
 * Created On : Mon Mar 13 2023
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
#include "main.h"
#include <stdio.h>
#include <spi/spi1.h>
#include <spi/spi2.h>
#include <psram/psram.h>
#include <usart/usart1.h>
#include <string.h>
//#include <SD/stm32f1_sd.h>
#include <timebase/timebase.h>
#include <stm32f1xx_ll_cortex.h>
#include <SD/sd.h>
#include <printf/printf.h>
#include "printf.h"
#include "decBus.h"
#include "ds1287.h"
#include "lance.h"
//#include "ucHw.h"
//#include "sdHw.h"
#include "dz11.h"
#include "soc.h"
#include "mem.h"
#include "sii.h"
#include "sd.h"
#include "graphics.h"
#include "scsiNothing.h"
#include "scsiDisk.h"
#include "hypercall.h"


#define TOTAL_RAM_MB    8

uint32_t spiRamGetAmt(void)
{
	return ((uint32_t)TOTAL_RAM_MB) << 20;
}

static uint32_t mSiiRamBase, mFbBase, mPaletteBase, mRamTop;
static uint8_t mDiskBuf[SD_BLOCK_SIZE];
static struct ScsiNothing gNoDisk;
static struct ScsiDisk gDisk;
psram_t sdram;
static const uint8_t gRom[] = 
{
	0x23, 0x00, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x10, 0x04, 0x00, 0x03, 0x24, 
	0x26, 0x00, 0x00, 0x10, 0x08, 0x00, 0x03, 0x24, 0x24, 0x00, 0x00, 0x10, 0x0C, 0x00, 0x03, 0x24, 
	0x22, 0x00, 0x00, 0x10, 0x10, 0x00, 0x03, 0x24, 0x20, 0x00, 0x00, 0x10, 0x14, 0x00, 0x03, 0x24, 
	0x1E, 0x00, 0x00, 0x10, 0x18, 0x00, 0x03, 0x24, 0x1C, 0x00, 0x00, 0x10, 0x1C, 0x00, 0x03, 0x24, 
	0x1A, 0x00, 0x00, 0x10, 0x20, 0x00, 0x03, 0x24, 0x18, 0x00, 0x00, 0x10, 0x24, 0x00, 0x03, 0x24, 
	0x16, 0x00, 0x00, 0x10, 0x28, 0x00, 0x03, 0x24, 0x14, 0x00, 0x00, 0x10, 0x2C, 0x00, 0x03, 0x24, 
	0x12, 0x00, 0x00, 0x10, 0x30, 0x00, 0x03, 0x24, 0x10, 0x00, 0x00, 0x10, 0x34, 0x00, 0x03, 0x24, 
	0x0E, 0x00, 0x00, 0x10, 0x38, 0x00, 0x03, 0x24, 0x0C, 0x00, 0x00, 0x10, 0x3C, 0x00, 0x03, 0x24, 
	0x0A, 0x00, 0x00, 0x10, 0x40, 0x00, 0x03, 0x24, 0x08, 0x00, 0x00, 0x10, 0x44, 0x00, 0x03, 0x24, 
	0x25, 0x28, 0x00, 0x00, 0x03, 0x00, 0x01, 0x24, 0x76, 0x67, 0x64, 0x4F, 0xFF, 0xFF, 0x40, 0x10, 
	0x00, 0x80, 0x10, 0x3C, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x3C, 
	0x08, 0x10, 0x21, 0x24, 0x08, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0xE8, 0xFF, 0x00, 0x10, 0x84, 0x00, 0x03, 0x24, 
	0x00 
};

void hwError(int err)
{
	printf("Stop Programm: %i",err);
	while(1);
}

static bool massStorageAccess(uint8_t op, uint32_t sector, void *buf);
void delayMsec(uint32_t msec)
{
	uint64_t till = getTime() + (uint64_t)msec * (TICKS_PER_SECOND / 1000);
	
	while (getTime() < till);
}

static bool accessRom(uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	const uint8_t *mem = gRom;

	pa -= (ROM_BASE & 0x1FFFFFFFUL);
	
	if (write)
		return false;
	else if (size == 4)
		*(uint32_t*)buf = *(uint32_t*)(mem + pa);
	else if (size == 1)
		*(uint8_t*)buf = mem[pa];
	else if (size == 2)
		*(uint16_t*)buf = *(uint16_t*)(mem + pa);
	else
		memcpy(buf, mem + pa, size);
	
	return true;
}

static bool accessRam(uint32_t pa, uint_fast8_t size, bool write, void* buf)
{	
	if (write)
		psram_Write(&sdram,pa, buf, size);
	else
		psram_Read(&sdram,pa, buf,size);

	return true;
}

bool cpuExtHypercall(void)	//call type in $at, params in $a0..$a3, return in $v0, if any
{
	uint32_t hyperNum = cpuGetRegExternal(MIPS_REG_AT), t,  ramMapNumBits, ramMapEachBitSz;
	uint_fast16_t ofst;
	uint32_t blk, pa;
	uint8_t chr;
	bool ret;

	switch (hyperNum) {
		case H_GET_MEM_MAP:		//stays for booting older images which expect this
		
			switch (pa = cpuGetRegExternal(MIPS_REG_A0)) {
				case 0:
					pa = 1;
					break;
				
				case 1:
					pa = spiRamGetAmt();
					break;
				
				case 2:
					pa = 1;
					break;
				
				default:
					pa = 0;
					break;
			}
			cpuSetRegExternal(MIPS_REG_V0, pa);
			break;
		
		case H_CONSOLE_WRITE:
			chr = cpuGetRegExternal(MIPS_REG_A0);
			if (chr == '\n') {
			//	prPutchar('\r');
				usart1_Send('\r');
			}
		//	prPutchar(chr);
			usart1_Send(chr);
			break;
		
		case H_STOR_GET_SZ:
			if (!massStorageAccess(MASS_STORE_OP_GET_SZ, 0, &t))
				return false;
			cpuSetRegExternal(MIPS_REG_V0, t);
			break;
		
		case H_STOR_READ:
			blk = cpuGetRegExternal(MIPS_REG_A0);
			pa = cpuGetRegExternal(MIPS_REG_A1);
			ret = massStorageAccess(MASS_STORE_OP_READ, blk, mDiskBuf);
			for (ofst = 0; ofst < SD_BLOCK_SIZE; ofst += OPTIMAL_RAM_WR_SZ)
				//spiRamWrite(pa + ofst, mDiskBuf + ofst, OPTIMAL_RAM_WR_SZ);
				psram_Write(&sdram,pa + ofst, mDiskBuf + ofst, OPTIMAL_RAM_WR_SZ);
			cpuSetRegExternal(MIPS_REG_V0, ret);
			if (!ret) {
				
				printf(" rd_block(%u, 0x%08x) -> %d\n", blk, pa, ret);
				hwError(6);
			}
			break;
		
		case H_STOR_WRITE:
			blk = cpuGetRegExternal(MIPS_REG_A0);
			pa = cpuGetRegExternal(MIPS_REG_A1);
			for (ofst = 0; ofst < SD_BLOCK_SIZE; ofst += OPTIMAL_RAM_RD_SZ)
				//spiRamRead(pa + ofst, mDiskBuf + ofst, OPTIMAL_RAM_RD_SZ);
				psram_Read(&sdram,pa + ofst, mDiskBuf + ofst, OPTIMAL_RAM_RD_SZ);
			ret = massStorageAccess(MASS_STORE_OP_WRITE, blk, mDiskBuf);
			cpuSetRegExternal(MIPS_REG_V0, ret);
			if (!ret) {
				
				printf(" wr_block(%u, 0x%08x) -> %d\n", blk, pa, ret);
				hwError(5);
			}
			break;
		
		case H_TERM:
			printf("termination requested\n");
			hwError(7);
			break;

		default:
			printf("hypercall %u @ 0x%08x\n", hyperNum, cpuGetRegExternal(MIPS_EXT_REG_PC));
			return false;
	}
	return true;
}

void dz11rxSpaceNowAvail(uint_fast8_t line)
{
	__NOP();
	(void)line;
}

void dz11charPut(uint_fast8_t line, uint_fast8_t chr)
{
	(void)chr;
	
	#ifdef MULTICHANNEL_UART
		
		usartTxEx(line, chr);
		
	#else
	
		if (line == 3)
			usart1_Send(chr);
	
	#endif
	
	if (line == 3) {
		/*	--	for benchmarking
		static uint8_t state = 0;
		
		switch (state) {
			case 0:
				if (chr == 'S')
					state = 1;
				break;
			case 1:
				state = (chr == 'C') ? 2 : 0;
				break;
			
			case 2:
				state = (chr == 'S') ? 3 : 1;
				break;
		
			case 3:
				state = (chr == 'I') ? 4 : 0;
				break;
			
			case 4:
				;
				uint64_t time = getTime();
				printf("\ntook 0x%08x%08x\n", (uint32_t)(time >> 32), (uint32_t)time);
				state = 5;
				break;
		}
		
		//*/
	//	prPutchar(chr);
	}
}


//dz11charRx(3, (uint8_t)ch);

void spiRamRead(uint32_t addr, void *data, uint_fast16_t sz)
{
	psram_Read(&sdram,addr, data, sz);
}

void spiRamWrite(uint32_t addr, const void *data, uint_fast16_t sz)
{
	psram_Write(&sdram,addr, data, sz);
}

void siiPrvBufferWrite(uint_fast16_t wordIdx, uint_fast16_t val)
{
	uint32_t addr = mSiiRamBase + wordIdx * 2;
	uint16_t v = val;
	
	//spiRamWrite(addr, &v, 2);
	psram_Write(&sdram,addr, &v, 2);
}

uint_fast16_t siiPrvBufferRead(uint_fast16_t wordIdx)
{
	uint16_t ret;
	
	//spiRamRead(mSiiRamBase + wordIdx * 2, &ret, 2);
	psram_Read(&sdram,mSiiRamBase + wordIdx * 2, &ret, 2);
	return ret;
}

int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);


	LL_RCC_ClocksTypeDef RCC_Clocks;
	LL_RCC_GetSystemClocksFreq(&RCC_Clocks);

 
  /*init driver*/
  spi1_init();
  spi2_init();
  usart1_Init(115200);
  asm volatile("cpsie i");
  timebaseInit();
  
  	printf("\r\n\r\nCPU ARM-CortexM3\r\n");
	printf("RUN @%ld MHZ\r\n\r\n",RCC_Clocks.SYSCLK_Frequency/1000000UL);
	pr("ready, time is 0x%016llx\r\n", getTime());
	pr("ready, time is 0x%016llx\r\n", getTime());
	/*PSRAM init*/
	//psram_t sdram = {.cs = spi1_cs, .spi=spi1_send};
	sdram.cs = spi1_cs;
	sdram.spi = spi1_send;

	psram_init(&sdram);
	psramID_t ramID = {0};
	psram_readID(&sdram,&ramID);
	printf("printf PSRAM INFO:\r\nMF ID :0x%02X\r\n",ramID.MF_ID);
	printf("KGD :0x%02X\r\n",ramID.KGD);
	printf("EID:0x%02X%02X%02X%02X%02X%02X\r\n",ramID.EID[5],ramID.EID[4],ramID.EID[3],ramID.EID[2],ramID.EID[1],ramID.EID[0]);
	printf("PSRAM IS %s\r\n",ramID.KGD==KGD_Pass ? "PASS":"Fail");

	if (!sdCardInit(mDiskBuf)) 
	{	
		printf("SD card init fail\r\n");
    	while(1);
	}

	uint32_t ramAmt = spiRamGetAmt();
  	uint_fast8_t i;

	//divvy up the RAM
	mSiiRamBase = ramAmt -= SII_BUFFER_SIZE;
	mFbBase = ramAmt -= SCREEN_BYTES;
	mPaletteBase = ramAmt -= SCREEN_PALETTE_BYTES;
	//round usable ram to page size
	mRamTop = ramAmt = (ramAmt >> 12) << 12;

	if (!memRegionAdd(RAM_BASE, ramAmt, accessRam))
				printf("failed to init %s\n", "RAM");
	else if (!memRegionAdd(ROM_BASE & 0x1FFFFFFFUL, sizeof(gRom), accessRom))
				printf("failed to init %s\n", "ROM");
	else if (!decBusInit())
				printf("failed to init %s\n", "DEC BUS");
	else if (!dz11init())
				printf("failed to init %s\n", "DZ11");
	else if (!siiInit(7))
				printf("failed to init %s\n", "SII");
	else if (!scsiDiskInit(&gDisk, 6, massStorageAccess, mDiskBuf, false))
				printf("failed to init %s\n", "SCSI disc");
	else {
				for (i = 0; i < 6; i++) {
					if (!scsiNothingInit(&gNoDisk, i)) {
						
						printf("failed to init %s\n", "SCSI dummy");
						break;
					}
				}

		if (i == 6) {

			if (!ds1287init())
						printf("failed to init %s\n", "DS1287");
			else if (!lanceInit())
						printf("failed to init %s\n", "LANCE");
			else 
			{
			
				cpuInit(ramAmt);
				while(1)
				{
					cpuCycle(ramAmt);
				}
			}


		}
	}
	printf("system load Ok;\r\n");      
	return 0; 
}

bool massStorageAccess(uint8_t op, uint32_t sector, void *buf)
{
	uint_fast8_t nRetries;
	
	switch (op) {
		case MASS_STORE_OP_GET_SZ:
				*(uint32_t*)buf = sdGetNumSecs();
				return true;
		
		case MASS_STORE_OP_READ:
			return sdSecRead(sector, buf);
			
		case MASS_STORE_OP_WRITE:
			return sdSecWrite(sector, buf);
	}
	return false;
}

void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_2)
  {
  }
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {

  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_16);
  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {

  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {

  }

	LL_RCC_ClocksTypeDef RCC_Clocks;
	LL_RCC_GetSystemClocksFreq(&RCC_Clocks);
	
  LL_Init1msTick(RCC_Clocks.HCLK_Frequency);
  LL_SetSystemCoreClock(RCC_Clocks.HCLK_Frequency);
}
