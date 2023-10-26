/*
	(c) 2021 Dmitry Grinberg   https://dmitry.gr
	Non-commercial use only OR licensing@dmitry.gr
*/


#include <string.h>
#include <timebase/timebase.h>
#include "sdHw.h"
#include <spi/spi2.h>
#include <stdio.h>
#include <printf/printf.h>

// #define MAX_SPEED			17000000
// #define SERCOM_SD			SERCOM3

static uint16_t mTimeoutBytes;
static uint32_t mRdTimeoutTicks, mWrTimeoutTicks;


void sdHwNotifyRCA(uint_fast16_t rca)
{
	//nothing
}

void sdHwSetTimeouts(uint_fast16_t timeoutBytes, uint32_t rdTimeoutTicks, uint32_t wrTimeoutTicks)
{
	mTimeoutBytes = timeoutBytes;
	mRdTimeoutTicks = rdTimeoutTicks;
	mWrTimeoutTicks = wrTimeoutTicks;
}

bool sdHwSetBusWidth(bool useFourWide)
{
	return !useFourWide;
}

void sdHwSetSpeed(uint32_t maxSpeed)
{
	spi2_speedUp();
}

uint32_t sdHwInit(void)
{
	spi2_init();
	
	//default timeouts
	sdHwSetTimeouts(10000, 0, 0);
	
	return SD_HW_FLAG_INITED;
}

static uint8_t sdSpiByte(uint_fast8_t val)
{
	
	uint_fast8_t ret = spi2_send(val);
	return ret;
}

void sdHwGiveInitClocks(void)
{
	uint_fast8_t i;
	
	for (i = 0; i < 16; i++)	//give card time to init with CS deasserted
		sdSpiByte(0xff);
}

static void sdChipSelect(void)
{
	spi2_cs(false);
	asm volatile("dsb sy");
	asm volatile("dsb sy");
}

void sdHwChipDeselect(void)
{
	spi2_cs(true);
	sdSpiByte(0xff);
	// while (!SERCOM_SD->SPI.INTFLAG.bit.TXC);
	// asm volatile("dsb sy");
	// PORT->Group[0].OUTSET.reg = PORT_PA28;
	// asm volatile("dsb sy");
	// asm volatile("dsb sy");
	// sdSpiByte(0xff);
	// while (!SERCOM_SD->SPI.INTFLAG.bit.TXC);
	// asm volatile("dsb sy");
}

static uint_fast8_t sdCrc7(uint_fast8_t data, uint_fast8_t crc)
{
	uint_fast8_t i;
	
	for (i = 0; i < 8; i++, data <<= 1) {
		
		crc <<= 1;

		if ((data & 0x80) ^ (crc & 0x80))
			crc ^= 0x09;
	}
	
	return crc & 0x7F;
}

static void sdPrvSendCmd(uint8_t cmd, uint32_t param, bool cmdCrcRequired)
{
	uint_fast8_t crc = 0;
	
	cmd |= 0x40;
	
	if (cmdCrcRequired) {
		
		crc = sdCrc7(cmd, crc);
		crc = sdCrc7(param >> 24, crc);
		crc = sdCrc7(param >> 16, crc);
		crc = sdCrc7(param >> 8, crc);
		crc = sdCrc7(param, crc);
	}
	
	sdSpiByte(cmd);
	sdSpiByte(param >> 24);
	sdSpiByte(param >> 16);
	sdSpiByte(param >> 8);
	sdSpiByte(param >> 0);
	sdSpiByte(crc * 2 + 1);
}

enum SdHwCmdResult sdHwCmd(uint_fast8_t cmd, uint32_t param, bool cmdCrcRequired, enum SdHwRespType respTyp, void *respBufOut, enum SdHwDataDir dataDir, uint_fast16_t blockSz, uint32_t numBlocks)
{
	uint8_t *rsp = (uint8_t*)respBufOut;
	uint_fast8_t ret, i = 0;
		
	sdChipSelect();
	
	if (cmd & 0x40) {		//cmd is usually 0..0x3f, we use 0x40 bit to signal that init failed, and we need to bit-resync. try that by injecting a bit
		
		//send an extra clock pulse some cards get into a weird state on boot, this will resync with them
		
		
		// PORT->Group[0].OUTCLR.reg = PORT_PA23;
		// PORT->Group[0].PINCFG[PIN_PA23].reg = PORT_PINCFG_DRVSTR;
		// asm volatile("dsb sy\ndsb sy\ndsb sy\n");
		// PORT->Group[0].OUTSET.reg = PORT_PA23;
		// asm volatile("dsb sy\ndsb sy\ndsb sy\n");
		// PORT->Group[0].OUTCLR.reg = PORT_PA23;
		// asm volatile("dsb sy\ndsb sy\ndsb sy\n");
		// PORT->Group[0].PINCFG[PIN_PA23].reg = PORT_PINCFG_DRVSTR | PORT_PINCFG_PMUXEN;
	}
	
	sdPrvSendCmd(cmd, param, cmdCrcRequired);
	
	if (cmd == 12)		//do not ask!
		sdSpiByte(0xff);
	
	while ((ret = sdSpiByte(0xff)) == 0xff) {
		
		if (++i == 128) {
			sdHwChipDeselect();
			return SdHwCmdResultRespTimeout;
		}
	}
	
	switch (respTyp) {
		case SdRespTypeNone:
			break;
		
		case SdRespTypeR1:
		case SdRespTypeR1withBusy:
			*rsp++ = ret;
			break;
		
		case SdRespTypeR3:
		case SdRespTypeR7:
			if (ret & FLAG_ILLEGAL_CMD) {
				sdHwChipDeselect();
				return SdCmdInvalid;
			}
			if (ret &~ FLAG_IN_IDLE_MODE) {
				sdHwChipDeselect();
				return SdCmdInternalError;
			}
			for (i = 0; i < 4; i++)
				*rsp++ = sdSpiByte(0xff);
			break;
		
		case SdRespTypeSpiR2:
			if (sdSpiByte(0xff))
				ret |= FLAG_MISC_ERR;
			*rsp = ret;
			if (ret &~ FLAG_IN_IDLE_MODE) {
				sdHwChipDeselect();
				return SdCmdInternalError;
			}
			break;
		
		default:
			sdHwChipDeselect();
			return SdCmdInternalError;
	}
	
	if (dataDir == SdHwDataNone)
		sdHwChipDeselect();
		
	return SdHwCmdResultOK;
}

static bool sdHwPrvDataWait(void)
{
	uint_fast16_t tries, timeoutBytes = mTimeoutBytes;
	uint_fast8_t byte;
	uint64_t time, rt;
	
	for (tries = 0; tries < timeoutBytes; tries++) {
		
		byte = sdSpiByte(0xFF);
		
		if (!(byte & 0xf0))
			return false;
		
		if (byte == 0xfe)
			return true;
	}
	
	time = getTime();
	do {
		byte = sdSpiByte(0xFF);
		
		if (!(byte & 0xf0))
			return false;
		
		if (byte == 0xfe)
			return true;
	
	} while ((rt = getTime()) - time < mRdTimeoutTicks);
	
	pr("read timeout. waited %u ticks, %lu msec. max was %u ticks\n",
		(uint32_t)(rt - time), (uint32_t)(((getTime() - time) * 1000) / TICKS_PER_SECOND), mRdTimeoutTicks);
	
	return false;
}

bool sdHwReadData(uint8_t* data, uint_fast16_t sz)	//length must be even, pointer must be halfword aligned
{
	//uint32_t num = sz + 2;
	
	if (!sdHwPrvDataWait())
		return false;

	for(uint_fast16_t i=0;i<sz;i++)
		data[i] = spi2_send(0xff);

	// mDmaDescrsInitial[0].BTCNT.bit.BTCNT = sz;
	// mDmaDescrsInitial[0].DSTADDR.bit.DSTADDR = ((uintptr_t)data) + sz;
	// asm volatile("":::"memory");
	// DMAC->CHID.bit.ID = 0;
	// DMAC->CHCTRLA.bit.ENABLE = 1;

	// do {
	// 	while (!SERCOM_SD->SPI.INTFLAG.bit.DRE);
	// 	SERCOM_SD->SPI.DATA.reg = 0xff;
	// } while (--num);
	
	// while (!SERCOM_SD->SPI.INTFLAG.bit.TXC);
	
	// asm volatile("":::"memory");
	
	return true;
}

enum SdHwWriteReply sdHwWriteData(const uint8_t *data, uint_fast16_t sz, bool isMultiblock)
{
	uint_fast16_t tries, timeoutBytes = mTimeoutBytes;
	uint_fast8_t byte;
	uint64_t time;
	
	sdSpiByte(isMultiblock ? 0xFC : 0xFE);	//start block
	while (sz--)
		sdSpiByte(*data++);
	//crc
	sdSpiByte(0xff);
	sdSpiByte(0xff);
	
	//wait for a reply
	
	for (tries = 0; tries < timeoutBytes; tries++) {
		
		byte = sdSpiByte(0xFF);
		
		if ((byte & 0x11) == 0x01) {
		
			switch (byte & 0x1f) {
				case 0x05:
					return SdHwWriteAccepted;
				
				case 0x0b:
					return SdHwWriteCrcErr;
				
				case 0x0d:
					return SdHwWriteError;
				
				default:
					return SdHwCommErr;
			}
		}
	}
	
	time = getTime();
	do {
		byte = sdSpiByte(0xFF);
		
		if ((byte & 0x11) == 0x01) {
		
			switch (byte & 0x1f) {
				case 0x05:
					return SdHwWriteAccepted;
				
				case 0x0b:
					return SdHwWriteCrcErr;
				
				case 0x0d:
					return SdHwWriteError;
				
				default:
					return SdHwCommErr;
			}
		}
	
	} while (getTime() - time < mWrTimeoutTicks);
	
	return SdHwTimeout;
}

bool sdHwPrgBusyWait(void)
{
	uint_fast16_t tries, timeoutBytes = mTimeoutBytes;
	uint32_t timeoutTicks = mWrTimeoutTicks;
	uint64_t time;
	
	for (tries = 0; tries < timeoutBytes; tries++) {
		
		if (sdSpiByte(0xFF) == 0xff)
			return true;
	}
	
	time = getTime();
	do {
		if (sdSpiByte(0xFF) == 0xff)
			return true;
	
	} while (getTime() - time < timeoutTicks);
	
	return false;
}

void sdHwRxRawBytes(void *dstP /* can be NULL*/, uint_fast16_t numBytes)
{
	uint8_t *dst = (uint8_t*)dstP;
	
	while (numBytes--) {
		
		uint_fast8_t val = sdSpiByte(0xff);
		
		if (dst)
			*dst++ = val;
	}
}

bool sdHwMultiBlockWriteSignalEnd(void)
{
	//stoptran token
	(void)sdSpiByte(0xFD);
	
	return true;
}

bool sdHwMultiBlockReadSignalEnd(void)
{
	//nothing
	
	return true;
}


