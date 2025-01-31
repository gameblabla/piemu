/*	
 *	lcdc.h
 *
 *	P/EMU - P/ECE Emulator
 *	Copyright (C) 2003 Naoyuki Sawa
 *
 *	* Mon Apr 16 05:06:00 JST 2003 Naoyuki Sawa
 *	- �쐬�J�n�B
 */
#ifndef __LCDC_H__
#define __LCDC_H__

/****************************************************************************
 *
 ****************************************************************************/

#define DISP_X		128
#define DISP_Y		 88

/****************************************************************************
 *
 ****************************************************************************/

typedef struct _LCDC {
	unsigned char vram[16][256];
	int page;	/* 0�` 15 */
	int col2;	/* 0�`255 */
} LCDC;

/****************************************************************************
 *
 ****************************************************************************/

void lcdc_init(struct tagPIEMU_CONTEXT* context);
#define lcdc_work	NULL

void lcdc_write(struct tagPIEMU_CONTEXT* context, unsigned char data);

void lcdc_conv(struct tagPIEMU_CONTEXT* context, unsigned char bits[DISP_Y][DISP_X]);

#endif /*__LCDC_H__*/
