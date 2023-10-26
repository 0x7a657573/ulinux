/*
	(c) 2021 Dmitry Grinberg   https://dmitry.gr
	Non-commercial use only OR licensing@dmitry.gr
*/

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <timebase/timebase.h>
#include "sdHw.h"
#include "sd.h"


#pragma GCC optimize ("Os")

#define VERBOSE				0






struct SD {
	uint32_t numSec;
	uint32_t snum;
	union SdFlags flags;
	uint8_t mid;
	uint16_t oid;
	uint16_t rca;
	
	uint32_t numSecLeft;		//for long operation
};


static struct SD mSD;

static uint8_t mLastErrorType;
static uint32_t mLastErrorLoc, mErrData;
static uint32_t mCurBlock;

#define ERR_NONE					0
#define ERR_WRITE_REPLY_TIMEOUT		1
#define ERR_READ_CMD17_REPLY		2
#define ERR_READ_CMD18_ERROR		3
#define ERR_READ_CMD23_ERROR		4
#define ERR_READ_DATA_TIMEOUT		5
#define ERR_WRITE_ACMD23_ERROR		6
#define ERR_WRITE_CMD23_ERROR		7
#define ERR_WRITE_CMD24_ERROR		8
#define ERR_WRITE_CMD25_ERROR		9
#define ERR_WRITE_DATA_REPLY_ERROR	10
#define ERR_WRITE_BUSY_WAIT_TIMEOUT	11

void sdReportLastError(void)
{
	static const char *mErrorNames[] = {
		[ERR_NONE] = "none",
		[ERR_WRITE_REPLY_TIMEOUT] = "write reply timeout",
		
		[ERR_READ_CMD17_REPLY] = "CMD17 error on read",
		[ERR_READ_CMD18_ERROR] = "CMD18 error on read",
		[ERR_READ_CMD23_ERROR] = "CMD23 error on read",
		[ERR_READ_DATA_TIMEOUT] = "read data timeout",
		
		[ERR_WRITE_ACMD23_ERROR] = "ACMD23 error on write",
		[ERR_WRITE_CMD23_ERROR] = "CMD23 error on write",
		[ERR_WRITE_CMD24_ERROR] = "CMD24 error on write",
		[ERR_WRITE_CMD25_ERROR] = "CMD25 error on write",
		[ERR_WRITE_DATA_REPLY_ERROR] = "reply token for write was bad",
		[ERR_WRITE_BUSY_WAIT_TIMEOUT] = "write busy wait took too long",
	};
	const char *errName = (mLastErrorType <= sizeof(mErrorNames) / sizeof(*mErrorNames)) ? mErrorNames[mLastErrorType] : NULL;
	if (!errName)
		errName = "UNKNOWN";
	
	printf("last error type %u(%s) at sector %lu. Extra data: 0x%08x\r\n", mLastErrorType, errName, mLastErrorLoc, mErrData);
}

static void sdPrvErrHappened(uint_fast8_t errType, uint32_t secNum, uint32_t extraData)
{
	mLastErrorType = errType;
	mLastErrorLoc = secNum;
	mErrData = extraData;
}


enum SdHwCmdResult sdPrvSimpleCommand(uint_fast8_t cmd, uint32_t param, bool cmdCrc, enum SdHwRespType respTyp, void *respBufOut, enum SdHwDataDir dataDir, uint_fast16_t blockSz, uint32_t numBlocks)
{
	enum SdHwCmdResult ret;
	
	if (VERBOSE)
		printf("cmd %u (0x%08x)\r\n", cmd, param);
	
	ret = sdHwCmd(cmd, param, cmdCrc, respTyp, respBufOut, dataDir, blockSz, numBlocks);
	
	if (VERBOSE)
		printf(" -> %02x %02x\r\n", ret, respBufOut ? *(uint8_t*)respBufOut : 0xff);
	
	return ret;
}

static enum SdHwCmdResult sdPrvACMD(uint_fast8_t cmd, uint32_t param, bool cmdCrc, enum SdHwRespType respTyp, void *respBufOut, enum SdHwDataDir dataDir, uint_fast16_t blockSz, uint32_t numBlocks)
{
	enum SdHwCmdResult ret;
	uint8_t rsp;
	
	ret = sdPrvSimpleCommand(55, ((uint32_t)mSD.rca) << 16, cmdCrc, SdRespTypeR1, &rsp, SdHwDataNone, 0, 0);
	if (ret != SdHwCmdResultOK)
		return ret;
	
	if (rsp &~ FLAG_IN_IDLE_MODE)
		return SdCmdInternalError;
	
	return sdPrvSimpleCommand(cmd, param, cmdCrc, respTyp, respBufOut, dataDir, blockSz, numBlocks);
}

static bool sdPrvCardInit(bool isSdCard, bool signalHighCapacityHost)
{
	uint32_t param, time = 0;
	bool first = true;
	uint8_t rsp[4];
		
	param = (signalHighCapacityHost ? 0x40000000 : 0x00000000);
	
	while (time++ < 1024) {
		
		enum SdHwCmdResult result;
		
		//busy bit is top bit of OCR, which is top bit of resp[1]
		//busy bit at 1 means init complete
		//see pages 85 and 26
		
		result = (isSdCard ? sdPrvACMD : sdPrvSimpleCommand)(isSdCard ? 41 : 1, param, true, mSD.flags.sdioIface ? SdRespTypeR3 : SdRespTypeR1, &rsp, SdHwDataNone, 0, 0);
		
		if (result != SdHwCmdResultOK)
			return false;
		
		if (mSD.flags.sdioIface) {	//bytes sent MSB to LSB
			
			if (!first && (rsp[0] & 0x80)) {
				
				if (isSdCard)
					mSD.flags.HC = !!(rsp[0] & 0x40);
				else
					mSD.flags.HC = ((rsp[0] >> 5) & 3) == 2;
				
				return true;
			}
			param |= 0x00200000;
		}
		else {
			
			if (rsp[0] & FLAG_MISC_ERR)
				break;
			
			if (!first && !(rsp[0] & FLAG_IN_IDLE_MODE))
				return true;
		}
		first = false;
	}
	return false;
}

static uint32_t sdPrvGetBits(const uint8_t* data, uint_fast16_t numBytesInArray, int_fast16_t startBit, uint_fast16_t len)	//for CID and CSD data..
{
	uint32_t ret = 0;
	int_fast16_t i;
	
	for (i = startBit + len - 1; i >= startBit; i--)
		ret = (ret << 1) + ((data[numBytesInArray - 1 - i / 8] >> (i % 8)) & 1);
	
	return ret;
}

static bool sdPrvCmd6(uint32_t param, uint8_t *buf)
{
	enum SdHwCmdResult result;
	uint64_t time;
	bool dataRxed;
	uint8_t rsp;
	
	//do not ask
	time = getTime();
	while (getTime() - time < TICKS_PER_SECOND / 100);
	
	result = sdPrvSimpleCommand(6, param, false, SdRespTypeR1, &rsp, SdHwDataRead, 64, 1);
	if (result != SdHwCmdResultOK)
		return false;

	dataRxed = sdHwReadData(buf, 64);
	sdHwChipDeselect();
	if (!dataRxed)
		return false;
	
	if (VERBOSE) {
		
		uint_fast8_t i;
		
		printf("CMD6(0x%08x) reply:", param);
		for (i = 0; i < 64; i++) {
			if (!(i & 15))
				printf("\n %03x ", i);
			printf(" %02x", buf[i]);
		}
		printf("\r\n");
	}
	
	return true;
}

bool __attribute__((noinline)) sdCardInit(uint8_t respBuf [static 64])
{
	uint32_t maxSpeed, hwFlags, readToTicks, writeToTicks;
	enum SdHwCmdResult result;
	uint_fast16_t toBytes;
	uint_fast8_t i;
	bool dataRxed;
	uint64_t t;
	
	memset(&mSD, 0, sizeof(mSD));
	
	hwFlags = sdHwInit();
	if (VERBOSE)
		printf("SD HW init flags: 0x%08x\r\n", hwFlags);
	if (!(hwFlags & SD_HW_FLAG_INITED))
		return false;
	
	mSD.flags.sdioIface = !!(hwFlags & SD_HW_FLAG_SDIO_IFACE);
	
	t = getTime();
	while (getTime() - t < TICKS_PER_SECOND / 500);	//give it 2 ms to init
	
	sdHwGiveInitClocks();
	
	t = getTime();
	while (getTime() - t < TICKS_PER_SECOND / 1000);
	
	if (mSD.flags.sdioIface) {	//reset has no reply, we send it once
		
		for (i = 0; i < 16; i++) {	//try CMD0 a few times
			
			if (SdHwCmdResultOK != sdPrvSimpleCommand(0, 0, true, SdRespTypeNone, NULL, SdHwDataNone, 0, 0))
				return false;
		}
	}
	else {						//reset has a reply - we try a few times
	
		for (i = 0; i < 64; i++) {	//try CMD0 a few times, then try it with an extra pulse
			
			if (SdHwCmdResultOK != sdPrvSimpleCommand((i < 32) ? 0 : 0x40, 0, true, SdRespTypeR1, respBuf, SdHwDataNone, 0, 0))
				continue;
			
			if (respBuf[0] == FLAG_IN_IDLE_MODE)
				break;
		}
		
		if (i == 16)
			return false;
	}
	
	result = sdPrvSimpleCommand(8, 0x000001AA, true, SdRespTypeR7, respBuf, SdHwDataNone, 0, 0);
	if (result == SdCmdInvalid)
		mSD.flags.v2 = false;
	else if (result != SdHwCmdResultOK)
		return false;
	else if (respBuf[3] != 0xaa)
		return false;
	else
		mSD.flags.v2 = true;
	
	result = sdPrvSimpleCommand(55, 0, true, SdRespTypeR1, respBuf, SdHwDataNone, 0, 0);
	if (result == SdCmdInvalid) {
		maxSpeed = 16000000;
		mSD.flags.SD = false;
	}
	else if (result != SdHwCmdResultOK || (respBuf[0] &~ FLAG_IN_IDLE_MODE))
		return false;
	else
		mSD.flags.SD = true;

	if (!sdPrvCardInit(mSD.flags.SD, true) && !sdPrvCardInit(mSD.flags.SD, false))
		return false;
	
	if (!mSD.flags.sdioIface) {	//check for HC (by reading OCR), turn off CRC
	
		result = sdPrvSimpleCommand(58, 0, true, SdRespTypeR3, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK)
			return false;
		
		mSD.flags.HC = !!(respBuf[0] & 0x40);
		
		//only turn CRC off if the spi HAL doesn't want to do CRC, else turn it on (from unknown initial state)
		result = sdPrvSimpleCommand(59, (hwFlags & SD_HW_FLAG_SPI_DOES_CRC) ? 1 : 0, true, SdRespTypeR1, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK || respBuf[0])
			return false;
	}
	
	//read CID
	if (mSD.flags.sdioIface) {
		
		result = sdPrvSimpleCommand(2, 0, false, SdRespTypeSdR2, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK)
			return false;
	}
	else {
		
		result = sdPrvSimpleCommand(10, 0, false, SdRespTypeR1, respBuf, SdHwDataRead, 16, 1);
		if (result != SdHwCmdResultOK || respBuf[0]) {
			printf("no read\r\n");
			return false;
		}
		dataRxed = sdHwReadData(respBuf, 16);
		sdHwChipDeselect();
		if (!dataRxed) {
			printf("read err\r\n");
			return false;
		}
	}

	//parse CID
	mSD.mid = sdPrvGetBits(respBuf, 16, 120, 8);
	mSD.oid = sdPrvGetBits(respBuf, 16, 104, mSD.flags.SD ? 16 : 8);
	mSD.snum = sdPrvGetBits(respBuf, 16, mSD.flags.SD ? 24 : 16, 32);

	if (VERBOSE)
		printf("CARD ID: %02x %04x %u\r\n", mSD.mid, mSD.oid, mSD.snum);

	//assign RCA
	if (mSD.flags.sdioIface) {
		
		mSD.rca = 2;
		result = sdPrvSimpleCommand(3, ((uint32_t)mSD.rca) << 16, false, SdRespTypeSdR6, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK)
			return false;
		
		if (mSD.flags.SD)
			mSD.rca = (((uint32_t)respBuf[0]) << 8) + respBuf[1];
		
		if (VERBOSE)
			printf("RCA %04x\r\n", mSD.rca);
		
		sdHwNotifyRCA(mSD.rca);
	}

	//read CSD
	if (mSD.flags.sdioIface) {
		
		result = sdPrvSimpleCommand(9, ((uint32_t)mSD.rca) << 16, false, SdRespTypeSdR2, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK)
			return false;
	}
	else {
		
		result = sdPrvSimpleCommand(9, 0, false, SdRespTypeR1, respBuf, SdHwDataRead, 16, 1);
		if (result != SdHwCmdResultOK || respBuf[0])
			return false;
		dataRxed = sdHwReadData(respBuf, 16);
		sdHwChipDeselect();
		if (!dataRxed)
			return false;
	}
	
	if (VERBOSE) {
		printf("CSD:");
		for (i = 0; i < 16; i++)
			printf(" %02x", respBuf[i]);
		printf("\n\r\n");
	}
	
	if (VERBOSE)
		printf("CSD ver %u\r\n", sdPrvGetBits(respBuf, 16, 126, 2) + 1);

	//calc capacity & figure out timeouts
	if (mSD.flags.SD && sdPrvGetBits(respBuf, 16, 126, 2)) {	//v2 SD csd
		
		mSD.numSec = 1024 * (1 + sdPrvGetBits(respBuf, 16, 48, 22));
		
		if (VERBOSE)
			printf("c_size: 0x%08x\r\n", sdPrvGetBits(respBuf, 16, 48, 22));
		
		readToTicks = (TICKS_PER_SECOND + 9) / 10;	//use 100ms
		writeToTicks = (TICKS_PER_SECOND + 1) / 2;	//use 500ms (sometimes 25, but this is simpler)
		toBytes = 0;
	}
	else {
		
		static const uint8_t taacMant[] = {0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, };
		uint_fast8_t r2wFactor = sdPrvGetBits(respBuf, 16, 26, 3);
		uint32_t cSizeMult = sdPrvGetBits(respBuf, 16, 47, 3);
		uint32_t readBlLen = sdPrvGetBits(respBuf, 16, 80, 4);
		uint_fast8_t taac = sdPrvGetBits(respBuf, 16, 112, 8);
		uint_fast8_t nsac = sdPrvGetBits(respBuf, 16, 104, 8);
		uint32_t cSize = sdPrvGetBits(respBuf, 16, 62, 12);
		uint32_t shiftBy = readBlLen + cSizeMult + 2;
		uint_fast8_t i;
		
		
		if (!mSD.flags.SD && sdPrvGetBits(respBuf, 16, 126, 2) >= 2 && sdPrvGetBits(respBuf, 16, 122, 4) >= 4) {
			
			if (VERBOSE)
				printf("MMC+ over 4GB not supported due to lack of testing\r\n");
			
			return false;
		}
		
		if (shiftBy < 9) {	//fatal
			if (VERBOSE)
				printf("card shift size invalid\r\n");
			return false;
		}
		shiftBy -= 9;
		
		mSD.numSec = (cSize + 1) << shiftBy;
		
		//taac is now the "typical" access time in units of 0.1ns
		//it is expressed as a mantissa from this array: {0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, }
		// and an exponent. this is "typical" time
		// and real timeout is 100x that, or 100ms (whichever is smaller)
		// to save on math we use 100ms!!!
		readToTicks = (TICKS_PER_SECOND + 9) / 10;	//use 100ms
		
		if (r2wFactor > 5)
			r2wFactor = 5;
		
		//250ms max
		writeToTicks = readToTicks << r2wFactor;
		if (writeToTicks > (TICKS_PER_SECOND + 3) / 4)
			writeToTicks = (TICKS_PER_SECOND + 3) / 4;
		
		//bytes
		toBytes = 256 / 8 * nsac;
	}
	
	if (VERBOSE)
		printf("read timeout: %u ticks, write %u + %u bytes\r\n", readToTicks, writeToTicks, toBytes);
	
	sdHwSetTimeouts(toBytes, readToTicks, writeToTicks);

	mSD.flags.RO = false;
	
	if (sdPrvGetBits(respBuf, 16, 12, 1)) {
		
		if (VERBOSE)
			printf("Card temporarily write protected\r\n");
		mSD.flags.RO = true;
	}
	
	if (sdPrvGetBits(respBuf, 16, 13, 1)) {
		
		if (VERBOSE)
			printf("Card permanently write protected\r\n");
		mSD.flags.RO = true;
	}
	
	if (VERBOSE)
		printf("CARD capacity: %u 512-byte blocks\r\n", mSD.numSec);
	
	if (mSD.flags.sdioIface) {
		
		//select the card
		result = sdPrvSimpleCommand(7, ((uint32_t)mSD.rca) << 16, false, SdRespTypeR1withBusy, respBuf, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK || respBuf[0])
			return false;
	}
	
	if (mSD.flags.SD) {		//some SD-specific things
		
		result = sdPrvACMD(51, 0, false, SdRespTypeR1, respBuf, SdHwDataRead, 8, 1);
		if (result != SdHwCmdResultOK || respBuf[0])
			return false;
		dataRxed = sdHwReadData(respBuf, 8);
		sdHwChipDeselect();
		if (!dataRxed)
			return false;
		
		if (VERBOSE) {
			printf("SCR:");
			for (i = 0; i < 8; i++)
				printf(" %02x", respBuf[i]);
			printf("\n\r\n");
		}
		
		if (!(respBuf[1] & 1)) {
			if (VERBOSE)
				printf("card claims to not support 1-bit mode...clearly SCR is wrong\r\n");
			return false;
		}

/*  -- not aplicable for this project 
		if ((hwFlags & SD_HW_FLAG_SUPPORT_4BIT) && (respBuf[1] & 4)) {
			
			uint8_t resp;
			
			if (VERBOSE)
				printf("card support 4-wide bus. we'll do that\r\n");
			
			if (!sdHwSetBusWidth(true))
				return false;
			
			//disable pullup
			result = sdPrvACMD(42, 0, false, SdRespTypeR1, &resp, SdHwDataNone, 0, 0);
			if (result != SdHwCmdResultOK || resp)
				return false;
			
			//go to 4-bit-wide
			result = sdPrvACMD(6, 2, false, SdRespTypeR1, &resp, SdHwDataNone, 0, 0);
			if (result != SdHwCmdResultOK || resp)
				return false;
			
			if (VERBOSE)
				printf("now in 4-bit mode\r\n");
		}
*/
		mSD.flags.cmd23supported = !!(respBuf[3] & 2);
		if (VERBOSE)
			printf("CMD23 support: %d\r\n", mSD.flags.cmd23supported);
		
		if (respBuf[0] & 0x0f) {	//at least spec 1.1 needed to use CMD6, no point bothering if host cnanot go fast enough
			
			//SD v1.1 supports 25MHz
			maxSpeed = 25000000;

/* -- not aplicable for this project

			if (!sdPrvCmd6(0x00000001, respBuf))
				return false;
			
			if ((respBuf[16] & 0x0f) == 1) {
				
				if (VERBOSE)
					printf("SD high speed mode supported\n sending cmd6 (to set HS mode)\r\n");
				
				if (!sdPrvCmd6(0x80000001, respBuf))
					return false;
				if ((respBuf[16] & 0x0f) == 1) {
					if (VERBOSE)
						printf("switch to high speed mode was a success\r\n");
					maxSpeed = 50000000;
				}
			}
			else {				//high speed mode not supported - falling back to 12.5MHz
				if (VERBOSE)
					printf("high speed mode not supported\r\n");
			}
*/
		}
		else {				//high speed mode not supported - falling back to 12.5MHz
			if (VERBOSE)
				printf("SD v1.0: high speed mode not known yet\r\n");
			maxSpeed = 12500000;
		}
		
		result = sdPrvACMD(13, 0, false, mSD.flags.sdioIface ? SdRespTypeR1 : SdRespTypeSpiR2, respBuf, SdHwDataRead, 64, 1);
		if (result != SdHwCmdResultOK || respBuf[0])
			return false;

		dataRxed = sdHwReadData(respBuf, 64);
		sdHwChipDeselect();
		if (!dataRxed)
			return false;
		
		if (VERBOSE) {
			printf("SD_STATUS:");
			for (i = 0; i < 64; i++)
				printf(" %02x", respBuf[i]);
			printf("\n\r\n");
		}
		
		mSD.flags.hasDiscard = !!(respBuf[24] & 2);
	}
	
	//speed the clock up
	sdHwSetSpeed(maxSpeed);
	
	//set block size
	result = sdPrvSimpleCommand(16, SD_BLOCK_SIZE, false, SdRespTypeR1, respBuf, SdHwDataNone, 0, 0);
	if (result != SdHwCmdResultOK || respBuf[0])
		return false;
	
	mSD.flags.inited = true;

	return true;
}

uint32_t sdGetNumSecs(void)
{
	return mSD.numSec;
}

bool sdSecRead(uint32_t sec, uint8_t *dst)
{
	enum SdHwCmdResult result;
	bool dataRxed;
	uint8_t rsp;
	
	result = sdPrvSimpleCommand(17, mSD.flags.HC ? sec : sec << 9, false, SdRespTypeR1, &rsp, SdHwDataRead, SD_BLOCK_SIZE, 1);
	if (result != SdHwCmdResultOK || rsp) {
		
		sdPrvErrHappened(ERR_READ_CMD17_REPLY, sec, rsp);
		return false;
	}

	dataRxed = sdHwReadData(dst, SD_BLOCK_SIZE);
	sdHwChipDeselect();
	if (!dataRxed) {
		sdPrvErrHappened(ERR_READ_DATA_TIMEOUT, sec, 0);
		return false;
	}

	return true;
}

bool sdReadStart(uint32_t sec, uint32_t numSec)
{
	enum SdHwCmdResult result;
	uint8_t rsp = 0;

	if (!numSec)
		numSec = 0xffffffff;
	else if (mSD.flags.cmd23supported || !mSD.flags.SD) {	//all MMC cards support CMD23, only some SD cards do
	
		result = sdPrvSimpleCommand(23, numSec, false, SdRespTypeR1, &rsp, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK || rsp) {
			
			sdPrvErrHappened(ERR_READ_CMD23_ERROR, sec, rsp);
			return false;
		}
	}
	
	result = sdPrvSimpleCommand(18, mSD.flags.HC ? sec : sec << 9, false, SdRespTypeR1, &rsp, SdHwDataRead, SD_BLOCK_SIZE, numSec);
	if (result != SdHwCmdResultOK || rsp) {
		
		sdPrvErrHappened(ERR_READ_CMD18_ERROR, sec, rsp);
		return false;
	}
	
	mCurBlock = sec;
	return true;
}

bool sdReadNext(uint8_t *dst)
{
	bool dataRxed;
	
	dataRxed = sdHwReadData(dst, SD_BLOCK_SIZE);
	mCurBlock++;
	
	if (dataRxed)
		return true;
	
	sdPrvErrHappened(ERR_READ_DATA_TIMEOUT, mCurBlock - 1, 0);
	return false;
}

bool sdReadStop(void)
{
	bool readEndOk;
	uint8_t rsp;
	
	readEndOk = sdHwMultiBlockReadSignalEnd();
	sdHwChipDeselect();
	
	(void)sdPrvSimpleCommand(12, 0, false, SdRespTypeR1withBusy, &rsp, SdHwDataNone, 0, 0);
	
	if (!mSD.flags.sdioIface) {
		//some card keep sending data a few more cycles on SPI...ignore it
		sdHwRxRawBytes(NULL, 8);
		sdHwChipDeselect();
	}
	
	return readEndOk;
}

bool sdWriteStart(uint32_t sec, uint32_t numSec)
{
	enum SdHwCmdResult result;
	uint8_t rsp;
	
	if (!numSec)
		numSec = 0xffffffff;
	else {
		//SD: ACMD23 is required
		//MMC: CMD23 is required
		result = (mSD.flags.SD ? sdPrvACMD : sdPrvSimpleCommand)(23, numSec, false, SdRespTypeR1, &rsp, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK || rsp) {
			
			sdPrvErrHappened(mSD.flags.SD ? ERR_WRITE_ACMD23_ERROR : ERR_WRITE_CMD23_ERROR, sec, rsp);
			return false;
		}
	}
	
	result = sdPrvSimpleCommand(25, mSD.flags.HC ? sec : sec << 9, false, SdRespTypeR1, &rsp, SdHwDataWrite, SD_BLOCK_SIZE, numSec);
	if (result != SdHwCmdResultOK || rsp) {
		
		sdPrvErrHappened(ERR_WRITE_CMD25_ERROR, sec, rsp);
		return false;
	}
	
	mCurBlock = sec;
	return true;
}

bool sdWriteNext(const uint8_t *src)
{
	enum SdHwWriteReply wr;
	bool busyDone;
	
	wr = sdHwWriteData(src, SD_BLOCK_SIZE, true);
	busyDone = sdHwPrgBusyWait();
	
	if (wr == SdHwTimeout) {
		
		sdPrvErrHappened(ERR_WRITE_REPLY_TIMEOUT, mCurBlock, wr);
		return false;
	}
	
	if (wr != SdHwWriteAccepted) {
		
		sdPrvErrHappened(ERR_WRITE_DATA_REPLY_ERROR, mCurBlock, wr);
		return false;
	}
	
	if (!busyDone) {
		
		sdPrvErrHappened(ERR_WRITE_BUSY_WAIT_TIMEOUT, mCurBlock, wr);
		return false;
	}
	
	mCurBlock++;
	return true;
}

bool sdWriteStop(void)
{
	enum SdHwCmdResult result;
	bool writeEndOk, busyDone;
	uint8_t rsp;

	writeEndOk = sdHwMultiBlockWriteSignalEnd();
	busyDone = sdHwPrgBusyWait();
	
	sdHwChipDeselect();

	if (mSD.flags.sdioIface) {
		result = sdPrvSimpleCommand(12, 0, false, SdRespTypeR1withBusy, &rsp, SdHwDataNone, 0, 0);
		if (result != SdHwCmdResultOK || rsp)
			return false;
	}
	
	return writeEndOk && busyDone;
}

bool sdSecWrite(uint32_t sec, const uint8_t *src)
{
	enum SdHwCmdResult result;
	enum SdHwWriteReply wr;
	uint8_t rsp = 0;
	bool busyDone;
	
	result = sdPrvSimpleCommand(24, mSD.flags.HC ? sec : sec << 9, false,  SdRespTypeR1, &rsp, SdHwDataWrite, SD_BLOCK_SIZE, 1);
	if (result != SdHwCmdResultOK || rsp) {
		
		sdPrvErrHappened(ERR_WRITE_CMD24_ERROR, sec, rsp);
		return false;
	}
	
	wr = sdHwWriteData(src, SD_BLOCK_SIZE, false);
	busyDone = sdHwPrgBusyWait();
	
	sdHwChipDeselect();
	
	if (wr == SdHwTimeout) {
		
		sdPrvErrHappened(ERR_WRITE_REPLY_TIMEOUT, mCurBlock, wr);
		return false;
	}
	
	if (wr != SdHwWriteAccepted) {
		
		sdPrvErrHappened(ERR_WRITE_DATA_REPLY_ERROR, mCurBlock, wr);
		return false;
	}
	
	if (!busyDone) {
		
		sdPrvErrHappened(ERR_WRITE_BUSY_WAIT_TIMEOUT, mCurBlock, wr);
		return false;
	}
	
	return true;
}

void sdGetInfo(uint8_t *midP, uint16_t *oidP, uint32_t *snumP)
{
	if (midP)
		*midP = mSD.mid;
	
	if (oidP)
		*oidP = mSD.oid;
	
	if (snumP)
		*snumP = mSD.snum;
}

uint8_t sdGetFlags(void)
{
	return mSD.flags.value;
}






