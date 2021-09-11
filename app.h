/*
 *  app.h
 *
 *  P/EMU - P/ECE Emulator
 *  Copyright (C) 2003 Naoyuki Sawa
 *
 *  * Mon Apr 14 00:00:00 JST 2003 Naoyuki Sawa
 *  - �쐬�J�n�B
 */
#ifndef __APP_H__
#define __APP_H__

#ifdef PSP
#include <pspkernel.h>
#include <psppower.h>
#include <pspthreadman.h>
#endif

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "sleep.h"

//#include "resource.h"

#define APPNAME   "P/EMU"
#define VERSION   "20030430"

#define ABOUT_CAPTION "P/EMU�̃o�[�W�������"
#define ABOUT_TEXT  "P/EMU - P/ECE Emulator (" VERSION ")\n" \
      "Copyright(C) 2003 Naoyuki Sawa"

#ifdef WIN32
typedef          __int64   int64;
typedef unsigned __int64  uint64;
#define INLINE      static __inline
#else
#ifndef PSP
typedef          long long  int64;
#endif
typedef unsigned long long  uint64;
#define INLINE      static __inline__
#endif

/* /usr/PIECE/include/piece.h���c */
typedef struct tagSYSTEMINFO {    // (piece.h���)
  unsigned short size;    //  0 ���̍\���̂̃T�C�Y
  unsigned short hard_ver;  //  2 �n�[�h�E�G�A�E�o�[�W����
  unsigned short bios_ver;  //  4 BIOS�o�[�W����
  unsigned short bios_date; //  6 BIOS�X�V�� YY(7):MM(4):DD(5)
  unsigned long sys_clock;  //  8 �V�X�e���E�N���b�N(Hz)
  unsigned short vdde_voltage;  // 12 VDDE(����)�d��(mV)
  unsigned short resv1;   // 14 �\��
  unsigned char *sram_top;  // 16 SRAM �J�n�A�h���X
  unsigned char *sram_end;  // 20 SRAM �I���A�h���X+1
  unsigned char *pffs_top;  // 24 pffs �J�n�A�h���X
  unsigned char *pffs_end;  // 28 pffs �I���A�h���X
} SYSTEMINFO;

struct tagPIEMU_CONTEXT;

#include "emu.h"
//
#include "fram.h"
#include "iomem.h"
#include "sram.h"
#include "usbc.h"
#include "flash.h"
#include "lcdc.h"
//
#include "bcu.h"
#include "core.h"
//
#include "c33209e.h"
#include "vector.h"
//#include "iomap.h"

#include "piemu.h"

/* main.c */
//
//void dbg(const char* fmt, ...);
//void die(const char* fmt, ...);
//#define DIE() die(__FILE__ "(%d)", __LINE__)

#ifdef NDEBUG
#define DIE()  ((void)0)
#define dbg()  ((void)0)
#else
#define DIE()  exit(-1)
#define dbg()  ((void)0)
#endif

#define KEY_UP    SDLK_UP
#define KEY_DOWN  SDLK_DOWN
#define KEY_LEFT  SDLK_LEFT
#define KEY_RIGHT SDLK_RIGHT
#define KEY_A   SDLK_z
#define KEY_B   SDLK_x
#define KEY_START SDLK_a
#define KEY_SELECT  SDLK_s

#ifdef PSP
#define JOY_CIRCLE  1  // as A
#define JOY_CROSS   2  // as B
#define JOY_START  11  // as START
#define JOY_SELECT 10  // as SELECT
#define JOY_LEFT    7  // as LEFT
#define JOY_RIGHT   9  // as RIGHT
#define JOY_UP      8  // as UP
#define JOY_DOWN    6  // as DOWN
#endif // PSP

// �ėp���������[�h�B�p�[�V�������W�X�^�X�g�[����h���邵�A
// �G���f�B�A���l�X�̈Ⴄ�}�V���ֈڐA����Ƃ��͂�����������������΂悢
// �߂�l�͂��ׂ� 32bit int, �v���g�^�C�v���Ђ��[����ύX�̕K�v����

// little endian �p
#define MEM_MASK(type) ((1 << (sizeof(type) << 3)) - 1)
#define READ_MEM(mem, type) ((*(int*)(mem)) & MEM_MASK(type))
#define READ_MEM_B(mem) READ_MEM((mem), char)
#define READ_MEM_H(mem) READ_MEM((mem), short)
#define READ_MEM_W(mem) (*(int*)(mem))

#endif /*__APP_H__*/
