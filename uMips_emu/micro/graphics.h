/*
	(c) 2021 Dmitry Grinberg   https://dmitry.gr
	Non-commercial use only OR licensing@dmitry.gr
*/

#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include <stdbool.h>
#include <stdint.h>


#define SCREEN_WIDTH				(1024)
#define SCREEN_HEIGHT				(864)


#if defined(COLOR_FRAMEBUFFER)
	#define SCREEN_STRIDE			(1024)
	#define SCREEN_BYTES			(0x00100000)
	#define SCREEN_PALETTE_BYTES	(4 * (256 + 16))
#elif defined(MONO_FRAMEBUFFER)
	#define SCREEN_STRIDE			(256)
	#define SCREEN_BYTES			(0x00040000)
	#define SCREEN_PALETTE_BYTES	(4 * (256 + 16))
#else
	#define SCREEN_BYTES			(0)
	#define SCREEN_PALETTE_BYTES	(0)
#endif



bool graphicsInit(void);
void graphicsPeriodic(void);



#endif

