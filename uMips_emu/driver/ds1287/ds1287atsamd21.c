/*
	(c) 2021 Dmitry Grinberg   https://dmitry.gr
	Non-commercial use only OR licensing@dmitry.gr
*/



#include <stdint.h>
#include <timebase/timebase.h>
#include "ds1287.h"
#include "esar.h"
#include "mem.h"
#include "soc.h"
#include "cpu.h"
#include <stm32f1xx_ll_tim.h>
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_rcc.h>

#pragma GCC optimize ("Os")

//https://pdfserv.maximintegrated.com/en/ds/DS12885-DS12C887A.pdf

//XXX: dec PROM fucks with year, so linux works around it by storing year offset in 0x3F (ake ram[0x31]) and keeping "year" offset from 72 - 2000
#define DEC_REAL_YEAR_LOC		0x31


#define RTC_CTRLA_UIP		0x80
#define RTC_CTRLA_DV_MASK	0x70
#define RTC_CTRLA_DV_SHIFT	4
#define RTC_CTRLA_RS_MASK	0x0f
#define RTC_CTRLA_RS_SHIFT	0

#define RTC_CTRLB_SET		0x80
#define RTC_CTRLB_PIE		0x40
#define RTC_CTRLB_AIE		0x20
#define RTC_CTRLB_UIE		0x10
#define RTC_CTRLB_SQWE		0x08
#define RTC_CTRLB_DM		0x04
#define RTC_CTRLB_2412		0x02
#define RTC_CTRLB_DSE		0x01

#define RTC_CTRLC_IRQF		0x80
#define RTC_CTRLC_PF		0x40
#define RTC_CTRLC_AF		0x20
#define RTC_CTRLC_UF		0x10

#define RTC_CTRLD_VRT		0x80


struct Config {
	
	//irq flags (bytes to avoid RMW issues)
	volatile uint8_t pf;
	volatile uint8_t af;
	volatile uint8_t uf;
	volatile uint8_t irqf;
	
	//irq enables
	uint8_t pie			: 1;
	uint8_t aie			: 1;
	uint8_t uie			: 1;
	
	//misc & config
	uint8_t set			: 1;
	uint8_t m2412		: 1;	//1 for 24h mode, 0 for 12h
	uint8_t dse			: 1;
	uint8_t rs			: 4;
	uint8_t dv			: 3;
	uint8_t sqwe		: 1;
	uint8_t dm			: 1;	//1 for binary, 0 for BCD
	
	uint8_t ram[0x72];
	
} mDS1287;

static const uint8_t mMonthDaysNormal[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint8_t mMonthDaysLeap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct Time {
	uint8_t h, m, s;
};

struct Date {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t dow;
};

union DateTime {
	struct {
		struct Date date;
		struct Time time;
		uint8_t sbz;
	};
	uint64_t raw64;
};

static struct Time mAlarmTime;
static union DateTime mCurTime = {
	.date = {
		.year = 72,		//see note above
		.month = 6,		//june
		.day = 17,		//17th	
		.dow = 6,		//friday
	},
	.time = {
		.h = 1,
		.m = 0,
		.s = 0,
	},
};

static uint_fast8_t ds1287prvToBcdIfNeeded(uint_fast8_t val, bool allowSpecialIgnoreVals)
{
	if (val >= 0xc0 && allowSpecialIgnoreVals)
		return val;
	
	if (!mDS1287.dm)
		val = (val / 10) * 16 + val % 10;
	
	return val;
}

static uint_fast8_t ds1287prvFromBcdIfNeeded(uint_fast8_t val, bool allowSpecialIgnoreVals)
{
	if (val >= 0xc0 && allowSpecialIgnoreVals)
		return val;
	
	if (!mDS1287.dm)
		val = (val / 16) * 10 + val % 16;
	
	return val;
}

static void ds1287prvSimpleAccess8(volatile uint8_t *reg, uint8_t *buf, bool write, bool allowSpecialIgnoreVals)
{
	if (write) {
		*reg = ds1287prvFromBcdIfNeeded(*buf, allowSpecialIgnoreVals);
	}
	else
		*buf = ds1287prvToBcdIfNeeded(*reg, allowSpecialIgnoreVals);
}

static void ds1287prvHoursAccess(volatile uint8_t *reg, uint8_t *buf, bool write, bool allowSpecialIgnoreVals)
{
	uint_fast8_t rv, t;
	
	if (write) {
		
		rv = *buf;
		
		if (allowSpecialIgnoreVals && rv >= 0xc0)
			t = rv;
		else {
		
			t = ds1287prvFromBcdIfNeeded(rv & 0x7f, false /* already handled */);

			if (!mDS1287.m2412) {
				
				if (t == 12)
					t = 0;
				if (rv & 0x80)
					t += 12;
			}
		}
		
		*reg = t;
	}
	else {
		
		rv = *reg;
		
		if (allowSpecialIgnoreVals && rv >= 0xc0)
			t = rv;
		else {
			
			t = rv;
			
			if (!mDS1287.m2412) {
				
				if (t == 0)
					t = 12;
				else if (t == 12)
					t = 0x8c;
				else if (t > 12)
					t = 0x80 + t - 12;
			}
			
			t = (t & 0x80) | ds1287prvToBcdIfNeeded(t & 0x7f, false /* already handled */);
		}
		
		*buf = t;
	}
}

static bool ds1287prvMemAccess(uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	uint_fast8_t val;
		
	pa &= 0x00ffffff;
	
	//check for ESAR (Ethernet Station Address ROM) access
	if (esarMemAccess(pa, size, write, buf))
		return true;
	
	if (size != 1 || (pa & 3))
		return false;
	pa /= 4;
	if (pa >= 0x80)
		return false;
	
	switch (pa) {
		case 0x00:		//seconds
			ds1287prvSimpleAccess8(&mCurTime.time.s, buf, write, false);
			break;
			
		case 0x01:		//alarm seconds
			ds1287prvSimpleAccess8(&mAlarmTime.s, buf, write, true);
			break;
		
		case 0x02:		//minutes
			ds1287prvSimpleAccess8(&mCurTime.time.m, buf, write, false);
			break;
			
		case 0x03:		//alarm minutes
			ds1287prvSimpleAccess8(&mAlarmTime.m, buf, write, true);
			break;
	
		case 0x04:		//hours
			ds1287prvHoursAccess(&mCurTime.time.h, buf, write, false);
			break;
		
		case 0x05:		//alarm hours
			ds1287prvHoursAccess(&mAlarmTime.h, buf, write, true);
			break;
	
		case 0x06:		//DoW
			ds1287prvSimpleAccess8(&mCurTime.date.dow, buf, write, false);
			break;
		
		case 0x07:		//date
			ds1287prvSimpleAccess8(&mCurTime.date.day, buf, write, false);
			break;
	
		case 0x08:		//month
			ds1287prvSimpleAccess8(&mCurTime.date.month, buf, write, false);
			break;
		
		case 0x09:		//year
			ds1287prvSimpleAccess8(&mCurTime.date.year, buf, write, false);
			break;
	
		case 0x0a:		//CTRLA
			if (write) 
			{
				// None,256Hz,128Hz,8.192kHz,4.096kHz,2.048kHz,1.024kHz,512Hz,256Hz,128Hz,64Hz,32Hz,16Hz,8Hz,4Hz,2Hz
				static const uint16_t mCntTops[] = {0xffff /* as rare as possible */, 487, 976, 14, 30, 60, 121, 243, 487, 976, 1952, 3905, 7811, 15624, 31249, 62499, };
				
				val = *(const uint8_t*)buf;
				
				mDS1287.dv = (val & 0x70) >> 4;
				mDS1287.rs = val = val & 0x0f;
				//TODO- FUCK
				//TC5->COUNT16.CC[0].reg = mCntTops[val];
				LL_TIM_SetAutoReload(TIM2,mCntTops[val]);
			}
			else
				*(uint8_t*)buf = (mDS1287.dv << 4) | mDS1287.rs;	//uip is clear always for us
			break;
	
		case 0x0b:		//CTRLB
			if (write) {
				
				val = *(const uint8_t*)buf;
				
				if ((val & RTC_CTRLB_SET) && !mDS1287.set) {		//go to set mode
					
					mDS1287.set = 1;
					NVIC_DisableIRQ(TIM1_UP_IRQn);
				}
				else if (!(val & RTC_CTRLB_SET) && mDS1287.set) {	//leave set mode
					
					mDS1287.set = 0;
					NVIC_EnableIRQ(TIM1_UP_IRQn);
				}
				
				mDS1287.pie = !!(val & RTC_CTRLB_PIE);
				mDS1287.aie = !!(val & RTC_CTRLB_AIE);
				mDS1287.uie = !!(val & RTC_CTRLB_UIE);
				mDS1287.sqwe = !!(val & RTC_CTRLB_SQWE);
				mDS1287.sqwe = !!(val & RTC_CTRLB_SQWE);
				mDS1287.dm = !!(val & RTC_CTRLB_DM);
				mDS1287.m2412 = !!(val & RTC_CTRLB_2412);
				mDS1287.dse = !!(val & RTC_CTRLB_DSE);
				
				//Bit 6: Periodic Interrupt Enable (PIE). The PIE bit is a
				// read/write bit that allows the periodic interrupt flag (PF) bit
				// in Register C to drive the IRQ pin low. When the PIE bit is
				// set to 1, periodic interrupts are generated by driving the
				// IRQ pin low at a rate specified by the RS3–RS0 bits of
				// Register A. A 0 in the PIE bit blocks the IRQ output from
				// being driven by a periodic interrupt, but the PF bit is still
				// set at the periodic rate. PIE is not modified by any internal
				// device functions, but is cleared to 0 on RESET.
				//TODO- FUCK
				// if (mDS1287.pie)
				// 	TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
				// else
				// 	TC5->COUNT16.INTENCLR.reg = TC_INTENSET_MC0;

				if (mDS1287.pie)
					LL_TIM_EnableIT_UPDATE(TIM2);
				else
					LL_TIM_DisableIT_UPDATE(TIM2);
			}
			else {
				
				val = 0;
				
				if (mDS1287.set)
					val |= RTC_CTRLB_SET;
				if (mDS1287.pie)
					val |= RTC_CTRLB_PIE;
				if (mDS1287.aie)
					val |= RTC_CTRLB_AIE;
				if (mDS1287.uie)
					val |= RTC_CTRLB_UIE;
				if (mDS1287.sqwe)
					val |= RTC_CTRLB_SQWE;
				if (mDS1287.dm)
					val |= RTC_CTRLB_DM;
				if (mDS1287.m2412)
					val |= RTC_CTRLB_2412;
				if (mDS1287.dse)
					val |= RTC_CTRLB_DSE;
				
				*(uint8_t*)buf = val;
			}
			break;

		case 0x0c:		//CTRLC
			if (!write) {
				
				val = 0;
				asm volatile("cpsid i");
				if (mDS1287.irqf)
					val |= RTC_CTRLC_IRQF;
				if (mDS1287.uf)
					val |= RTC_CTRLC_UF;
				if (mDS1287.af)
					val |= RTC_CTRLC_AF;
				if (mDS1287.pf)
					val |= RTC_CTRLC_PF;
				
				mDS1287.irqf = 0;
				mDS1287.uf = 0;
				mDS1287.af = 0;
				mDS1287.pf = 0;
				
				cpuIrq(SOC_IRQNO_RTC, false);
				asm volatile("cpsie i");
				
				*(uint8_t*)buf = val;
			}
			break;

		case 0x0d:		//CTRLD
			if (!write)
				*(uint8_t*)buf = RTC_CTRLD_VRT;
			break;
		
		default:
			if (write)
				mDS1287.ram[pa - 0x0e] = *(const uint8_t*)buf;
			else
				*(uint8_t*)buf = mDS1287.ram[pa - 0x0e];
			break;
	}
	
	return true;
}

static uint_fast8_t rtcPrvDaysPerMonth(uint_fast16_t year, uint_fast8_t month)
{
	const uint8_t *mdays;
	bool isLeap;
	
	if (year % 4)
		isLeap = false;
	else if (year % 100)
		isLeap = true;
	else
		isLeap = !(year % 400);
	
	mdays = isLeap ? mMonthDaysLeap : mMonthDaysNormal;

	return mdays[month - 1];
}

void TIM1_UP_IRQHandler(void)
{
	LL_TIM_ClearFlag_UPDATE(TIM1);
	LL_GPIO_TogglePin(GPIOC,LL_GPIO_PIN_13);

	bool newIrq = false, doAlarm = false;
		
	if (++mCurTime.time.s == 60) {
		
		mCurTime.time.s = 0;
		if (++mCurTime.time.m == 60) {
			
			mCurTime.time.m = 0;
			if (++mCurTime.time.h == 24) {
				
				uint_fast8_t mDays = rtcPrvDaysPerMonth(mCurTime.date.year, mCurTime.date.month);
				mCurTime.time.h = 0;
				
				if (mCurTime.date.dow++ == 7)
					mCurTime.date.dow = 1;
				
				if (mCurTime.date.day++ == mDays) {
					
					mCurTime.date.day = 1;
					if (mCurTime.date.month++ == 12) {
						
						mCurTime.date.month = 1;
						mCurTime.date.year++;
					}
				}
			}
		}
	}
	
	if (!mDS1287.uf) {	//update irq
			
		mDS1287.uf = 1;
		if (mDS1287.uie)
			newIrq = true;
	}
	
	if (mAlarmTime.h >= 0xc0) {	//do not care for hours - alarm every hour
		if (!mCurTime.time.m)
			doAlarm = true;
		if (mAlarmTime.m >= 0xc0) {
			if (!mCurTime.time.s)
				doAlarm = true;
			if (mAlarmTime.s >= 0xc0)
				doAlarm = true;
		}
	}
	else if (mCurTime.time.h == mAlarmTime.h && mCurTime.time.m == mAlarmTime.m && mCurTime.time.s == mAlarmTime.s)
		doAlarm = true;
	
	if (doAlarm && !mDS1287.af) {
			
		mDS1287.af = 1;
		if (mDS1287.aie)
			newIrq = true;
	}
	
	if (newIrq && !mDS1287.irqf) {
		
		mDS1287.irqf = 1;
		cpuIrq(SOC_IRQNO_RTC, true);
	}
}

void TIM2_IRQHandler(void)
{
	LL_TIM_ClearFlag_UPDATE(TIM2);
	
	//perodic
	if (!mDS1287.pf) {
		
		mDS1287.pf = 1;
		if (mDS1287.pie && !mDS1287.irqf) {
			mDS1287.irqf = 1;
			cpuIrq(SOC_IRQNO_RTC, true);
		}
	}
}

bool ds1287init(void)
{
	//init data
	mDS1287.dm = 1;
	mDS1287.m2412 = 1;
	mDS1287.ram[DEC_REAL_YEAR_LOC] = 22;	//2022
	
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  	LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  	LL_GPIO_ResetOutputPin(GPIOC,LL_GPIO_PIN_13);

	LL_TIM_InitTypeDef TIM_InitStruct = {0};

	/* Peripheral clock enable */
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

	/* TIM1 interrupt Init */
	NVIC_SetPriority(TIM1_UP_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(TIM1_UP_IRQn);
	
	LL_RCC_ClocksTypeDef RCC_Clocks;
	LL_RCC_GetSystemClocksFreq(&RCC_Clocks);
	uint32_t SystemClock = RCC_Clocks.HCLK_Frequency;

	//irq at 1Hz
	TIM_InitStruct.Prescaler = SystemClock/10000; //7200;
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
	TIM_InitStruct.Autoreload = 10000-1;
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	TIM_InitStruct.RepetitionCounter = 0;
	LL_TIM_Init(TIM1, &TIM_InitStruct);
	LL_TIM_DisableARRPreload(TIM1);
	LL_TIM_SetClockSource(TIM1, LL_TIM_CLOCKSOURCE_INTERNAL);
	LL_TIM_SetTriggerOutput(TIM1, LL_TIM_TRGO_RESET);
	LL_TIM_DisableMasterSlaveMode(TIM1);
	LL_TIM_EnableCounter(TIM1);
	LL_TIM_EnableIT_UPDATE(TIM1);


	/********Timer2 ---------> 125khz*/
	  /* TIM2 interrupt Init */
  	NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  	NVIC_EnableIRQ(TIM2_IRQn);

	TIM_InitStruct.Prescaler = (SystemClock/125000UL)-1; /*Set timer to 125khz*/
  	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  	TIM_InitStruct.Autoreload = 0xF000;
  	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  	LL_TIM_Init(TIM2, &TIM_InitStruct);
  	LL_TIM_DisableARRPreload(TIM2);
  	LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
  	LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
  	LL_TIM_DisableMasterSlaveMode(TIM2);
	LL_TIM_EnableCounter(TIM2);
	//LL_TIM_EnableIT_UPDATE(TIM2);


// TODO - Timer Not WORK	
	// TC4->COUNT16.CTRLA.reg = TC_CTRLA_PRESCALER_DIV256 | TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ;	//counts at 31250 Hz
	// TC4->COUNT16.CC[0].reg = 31249;		//irq at 1Hz
	// TC4->COUNT16.CTRLA.bit.ENABLE = 1;
	// TC4->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
	// NVIC_EnableIRQ(TC4_IRQn);
	
	// #if 1	//realtime
	
	// 	TC5->COUNT16.CTRLA.reg = TC_CTRLA_PRESCALER_DIV64 | TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ;	//counts at 125000 Hz
	
	// #else	//4x slower
	
	// 	TC5->COUNT16.CTRLA.reg = TC_CTRLA_PRESCALER_DIV256 | TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ;	//counts at 31250 Hz
	
	// #endif
	// TC5->COUNT16.CC[0].reg = 0xf000;
	// TC5->COUNT16.CTRLA.bit.ENABLE = 1;
	// NVIC_EnableIRQ(TC5_IRQn);
	

	//add memory
	return memRegionAdd(0x1d000000, 0x01000000, ds1287prvMemAccess);
}






