/*
 *  core.c
 *
 *  P/EMU - P/ECE Emulator
 *  Copyright (C) 2003 Naoyuki Sawa
 *
 *  * Mon Apr 14 00:00:00 JST 2003 Naoyuki Sawa
 *  - �쐬�J�n�B
 *  * Sat Apr 19 12:30:00 JST 2003 Naoyuki Sawa
 *  - scan0/scan1�̃o�O�C���B�i�����������t�ł����j
 *
 *  * Aug 23 ... 30, 2005.
 *  * ����Ȃ���S���}�N������[
 */
#include "app.h"

/****************************************************************************
 *  �O���[�o���ϐ�
 ****************************************************************************/

//CORE core;

/****************************************************************************
 *  �}�N���E�⏕�֐�
 ****************************************************************************/

// (1 << NOP_WAIT) �� nop ���Ă΂ꂽ�� SDL_Delay(1)
#define NOP_WAIT 9

#define NOP_CLK_MULTIPLY      2
#define HALT_CLK_MULTIPLY   240
#define HALT_INT_CLK      24000

/* Gameblabla - Some cycle accurate stuff could require this 
 * but i've used it without it and so far, it seems good.
 * */
#ifdef EXTRA_ACCURACY
#define STD_NOP \
{ \
  context->core.nop_count++; \
  context->core.nop_count &= (1 << NOP_WAIT) - 1; \
  if(!context->core.nop_count) \
  { \
    SDL_Delay(1); \
  } \
}
#else // 0
#define STD_NOP OS_YIELD() //__asm { nop }
#endif

/* �֐��̐擪�Ŏg����悤�ɁA�_�~�[�ϐ������������܂��B */
// #ifdef CORE_DEBUG
// static int no_ext()  { if(EXT1.s) DIE(); return 0; }
// static int no_delay()  { if(core.d) DIE(); return 0; }
// #define NO_EXT   int __no_ext__ = no_ext();
// #define NO_DELAY int __no_delay__ = no_delay();
// #else /*CORE_DEBUG*/
#define NO_EXT
#define NO_DELAY
// #endif /*CORE_DEBUG*/

/* �C���f�N�X�����t���̔ėp/�V�X�e�����W�X�^�A�N�Z�X�@*/
// #ifdef CORE_DEBUG
// static int check_r(int n) { if(n < 0 || n > 15) DIE(); return n; }
// static int check_s(int n) { if(n < 0 || n >  3) DIE(); return n; }
// #define R(n) context->core.r[check_r(n)]
// #define S(n) context->core.s[check_s(n)]
// #else /*CORE_DEBUG*/
#define R(n)  context->core.r[n]
#define S(n)  context->core.s[n]

#define RZ context->core.rZ
// #endif /*CORE_DEBUG*/

/* �ldata�̃r�b�g(bits-1)���A�r�b�g31�`bits�ɕ����g�����܂��B */
INLINE int
sign_ext(int data, int bits)
{
  data <<= 32 - bits;
  data >>= 32 - bits;
  return data;
}

/* �f�B���C�h���򖽗߂̒�����g�p�B����d��1���w�肳�ꂽ��A�f�B���C�h���߂����s���܂��B */
#define exec_delay(dflag) { \
  if(dflag) { \
    if(context->core.d) DIE();  /* �O�̂��߁A�f�B���C�h����d�ɂȂ��Ă��Ȃ����Ƃ����� */ \
    context->core.d = 1;    /* �f�B���C�h�J�n */ \
    d_inst.s = mem_readH(context, PC + 2); \
    core_inst(context, d_inst); \
    if(!context->core.d) DIE(); /* �O�̂��߁A�\�����Ȃ��f�B���C�h�������Ȃ����Ƃ����� */ \
    context->core.d = 0;    /* �f�B���C�h�I�� */ \
  } \
}

/****************************************************************************
 *  ���l�g��
 ****************************************************************************/

INLINE int
ext_imm6(PIEMU_CONTEXT* context, int imm6)
{
  int data;
  imm6 &= (1 << 6) - 1;
  if(EXT2.s) {
    data = imm6 | (EXT2.c6.imm13 << 6) | (EXT1.c6.imm13 << 19);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    data = imm6 | (EXT1.c6.imm13 << 6);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    data = imm6;
  }
  return data;
}

INLINE int
ext_sign6(PIEMU_CONTEXT* context, int sign6)
{
  int data, bits;
  sign6 &= (1 << 6) - 1;
  if(EXT2.s) {
    data = sign6 | (EXT2.c6.imm13 << 6) | (EXT1.c6.imm13 << 19);
    bits = 6 + 13 + 13;
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    data = sign6 | (EXT1.c6.imm13 << 6);
    bits = 6 + 13;
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    data = sign6;
    bits = 6;
  }
  return sign_ext(data, bits);
}

INLINE int
ext_RB(PIEMU_CONTEXT* context, int rb)
{
  int disp;
  if(EXT2.s) {
    disp = (EXT2.c6.imm13 << 0) | (EXT1.c6.imm13 << 13);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    disp = (EXT1.c6.imm13 << 0);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    disp = 0;
  }
  return R(rb) + disp;
}

INLINE int
ext_SPxIMM6(PIEMU_CONTEXT* context, int imm6, int size)
{
  int disp;
  imm6 &= (1 << 6) - 1;
  if(EXT2.s) {
    disp = imm6 | (EXT2.c6.imm13 << 6) | (EXT1.c6.imm13 << 19);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    disp = imm6 | (EXT1.c6.imm13 << 6);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    disp = imm6 * size;
/*
    switch(size) {
    case 1: disp = imm6 * 1; break;
    case 2: disp = imm6 * 2; break;
    case 4: disp = imm6 * 4; break;
    default: DIE();
    }
*/
  }
  return SP + disp;
}

INLINE int
ext_3op(PIEMU_CONTEXT* context)
{
  /* NOTE1: ���Ȃ��Ƃ�EXT1�����݂��邱�Ƃ��Ăяo�����Ŋm�F���Ă��������B */
  /* NOTE2: cmp/and/or/xor/not���A3op�g������sign�ł͂Ȃ�imm�ƂȂ�܂��B */
  int data;
  if(EXT2.s) {
    data = (EXT2.c6.imm13 << 0) | (EXT1.c6.imm13 << 13);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    data = (EXT1.c6.imm13 << 0);
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    DIE();
  }
  return data;
}

INLINE int
ext_PCxSIGN8(PIEMU_CONTEXT* context, int sign8)
{
  int disp, bits;
  sign8 &= (1 << 8) - 1;
  if(EXT2.s) {
    disp = sign8 | (EXT2.c6.imm13 << 8) | (EXT1.c6.imm13 >> 3 << 21);
    bits = 8 + 13 + 10;
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else if(EXT1.s) {
    disp = sign8 | (EXT1.c6.imm13 << 8);
    bits = 8 + 13;
    context->core.ext[0].s = context->core.ext[1].s = 0;
  } else {
    disp = sign8;
    bits = 8;
  }
  return PC + sign_ext(disp, bits) * 2;
}

// yui: 2005.09.21: �ȉ� core_*() �ȊO�͑S���}�N��(;�L�D�M)

/****************************************************************************
 *  ���ʉ��Z��PSR�ݒ�
 ****************************************************************************/

#define add(a, b, r) \
{ \
  c = (int)a + (int)b; \
  PSR.n = c < 0; \
  PSR.z = !c; \
  PSR.v = ((int)a < 0  && (int)b <  0 && c >= 0) || \
          ((int)a >= 0 && (int)b >= 0 && c <  0); \
  PSR.c = (unsigned)c < (unsigned)a; \
  (r) = c; \
}

#define adc(a, b, r) \
{ \
  c = (int)a + (int)b; \
  d = c + PSR.c; \
  PSR.n = d < 0; \
  PSR.z = !d; \
  PSR.v = ((int)a < 0  && (int)b <  0 && d >= 0) || \
    ((int)a >= 0 && (int)b >= 0 && d <  0); \
  PSR.c = ((unsigned)c < (unsigned)a) || \
    ((unsigned)d < (unsigned)c); \
  (r) = d; \
}

#define sub(a, b, r) \
{ \
  c = (int)a - (int)b; \
  PSR.n = c < 0; \
  PSR.z = !c; \
  PSR.v = ((int)a >= 0 && (int)b <  0 && c <  0) || \
          ((int)a <  0 && (int)b >= 0 && c >= 0); \
  PSR.c = (unsigned)c > (unsigned)a; \
  (r) = c; \
}

#define sbc(a, b, r) \
{ \
  c = (int)a - (int)b; \
  d = c - PSR.c; \
  PSR.n = d < 0; \
  PSR.z = !d; \
  PSR.v = ((int)a >= 0 && (int)b <  0 && d <  0) || \
          ((int)a <  0 && (int)b >= 0 && d >= 0); \
  PSR.c = ((unsigned)c > (unsigned)a) || \
    ((unsigned)d > (unsigned)c); \
  (r) = d; \
}

#define and(a, b, r) \
{ \
  ACC = (int)a & (int)b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define or(a, b, r) \
{ \
  ACC = (int)a | (int)b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define xor(a, b, r) \
{ \
  ACC = (int)a ^ (int)b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define not(a, r) \
{ \
  ACC = ~(int)a; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define srl(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = (unsigned)a >> b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define sll(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = (unsigned)a << b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define sra(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = (int)a >> b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define sla(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = (int)a << b; \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define rr(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = ((unsigned)a >> b) | ((unsigned)a << (32 - b)); \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

#define rl(a, b, r) \
{ \
  if(b < 0 || b > 8) DIE(); \
  ACC = ((unsigned)a << b) | ((unsigned)a >> (32 - b)); \
  PSR.n = ACC < 0; \
  PSR.z = !ACC; \
  (r) = ACC; \
}

/****************************************************************************
 *  CLASS 0A
 ****************************************************************************/

#define exec_nop(inst) { NO_EXT NO_DELAY \
  STD_NOP; \
  PC += 2; \
  CLK += 1 * NOP_CLK_MULTIPLY; \
}

// �Ă΂�ĂȂ��炵���H
#define exec_slp(inst) { NO_EXT NO_DELAY \
  STD_NOP; \
  /* ��TODO: */ \
  context->core.in_halt = 1; \
  PC += 2; \
  CLK += 1 * NOP_CLK_MULTIPLY; \
}
#define exec_halt(inst) { NO_EXT NO_DELAY       \
      STD_NOP;                                  \
    /* ��TODO: */                               \
    /*context->core.in_halt = 1;*/              \
    PC += 2;                                    \
    CLK += 1 * HALT_CLK_MULTIPLY;               \
  }
#define exec_pushn_rs(inst) { NO_EXT NO_DELAY \
  int rs; \
  for(rs = inst.imm2_rd_rs; rs >= 0; rs--) { \
    SP -= 4; \
    mem_writeW(context, SP, R(rs)); \
    CLK += 1; \
  } \
  PC += 2; \
}
#define exec_popn_rd(inst) { NO_EXT NO_DELAY \
  int rd; \
  for(rd = 0; rd <= inst.imm2_rd_rs; rd++) { \
    R(rd) = mem_readW(context, SP); \
    SP += 4; \
    CLK += 1; \
  } \
  PC += 2; \
}
#define exec_brk(inst) { DIE(); }
#define exec_retd(inst) { DIE(); }
#define exec_int_imm2(inst) { NO_EXT NO_DELAY \
  if(inst.imm2_rd_rs < 0 || inst.imm2_rd_rs > 3) DIE(); \
  PC += 2; \
  CLK += 10; \
  core_trap(context, TRAP_SOFTINT0 + inst.imm2_rd_rs, 0); \
}
#define exec_reti(inst) { NO_EXT NO_DELAY \
  S(0) = mem_readW(context, SP); \
  SP += 4; \
  PC = mem_readW(context, SP); \
  SP += 4; \
  CLK += 5; \
}
#define exec_call_rb(inst) { NO_EXT NO_DELAY \
  addr = R(inst.imm2_rd_rs); \
  exec_delay(inst.d); \
  SP -= 4; \
  mem_writeW(context, SP, PC + 2); \
  PC = addr; \
  CLK += !inst.d ? 3 : 2; \
}
#define exec_ret(inst) { NO_EXT NO_DELAY \
  addr = mem_readW(context, SP); \
  SP += 4; \
  exec_delay(inst.d); \
  PC = addr; \
  CLK += !inst.d ? 4 : 3; \
}
#define exec_jp_rb(inst) { NO_EXT NO_DELAY \
  addr = R(inst.imm2_rd_rs); \
  exec_delay(inst.d); \
  PC = addr; \
  CLK += !inst.d ? 2 : 1; \
}

/****************************************************************************
 *  CLASS 0B
 ****************************************************************************/

#define exec_jrgt_sign8(inst) { NO_DELAY \
  cc = !(PSR.n ^ PSR.v) && !PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrge_sign8(inst) { NO_DELAY \
  cc = !(PSR.n ^ PSR.v); \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrlt_sign8(inst) { NO_DELAY \
  cc = (PSR.n ^ PSR.v); \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrle_sign8(inst) { NO_DELAY \
  cc = (PSR.n ^ PSR.v) || PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrugt_sign8(inst) { NO_DELAY \
  cc = !PSR.c && !PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jruge_sign8(inst) { NO_DELAY \
  cc = !PSR.c; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrult_sign8(inst) { NO_DELAY \
  cc = PSR.c; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrule_sign8(inst) { NO_DELAY \
  cc = PSR.c || PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jreq_sign8(inst) { NO_DELAY \
  cc = PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_jrne_sign8(inst) { NO_DELAY \
  cc = !PSR.z; \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = cc ? addr : PC + 2; \
  CLK += cc && !inst.d ? 2 : 1; \
}
#define exec_call_sign8(inst) { NO_DELAY \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  SP -= 4; \
  mem_writeW(context, SP, PC + 2); \
  PC = addr; \
  CLK += !inst.d ? 3 : 2; \
}
#define exec_jp_sign8(inst) { NO_DELAY \
  addr = ext_PCxSIGN8(context, inst.sign8); \
  exec_delay(inst.d); \
  PC = addr; \
  CLK += !inst.d ? 2 : 1; \
}

/****************************************************************************
 *  CLASS 1A
 ****************************************************************************/

#define exec_ld_b_rd_RB(inst) { NO_DELAY \
  R(inst.rs_rd) = (char)mem_readB(context, ext_RB(context, inst.rb)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_ub_rd_RB(inst) { NO_DELAY \
  R(inst.rs_rd) = (unsigned char)mem_readB(context, ext_RB(context, inst.rb)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_h_rd_RB(inst) { NO_DELAY \
  R(inst.rs_rd) = (short)mem_readH(context, ext_RB(context, inst.rb)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_uh_rd_RB(inst) { NO_DELAY \
  R(inst.rs_rd) = (unsigned short)mem_readH(context, ext_RB(context, inst.rb)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_rd_RB(inst) { NO_DELAY \
  R(inst.rs_rd) = (int)mem_readW(context, ext_RB(context, inst.rb)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_b_RB_rs(inst) { NO_DELAY \
  mem_writeB(context, ext_RB(context, inst.rb), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_h_RB_rs(inst) { NO_DELAY \
  mem_writeH(context, ext_RB(context, inst.rb), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_RB_rs(inst) { NO_DELAY \
  mem_writeW(context, ext_RB(context, inst.rb), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_b_rd_RBx(inst) { NO_EXT NO_DELAY \
  R(inst.rs_rd) = (char)mem_readB(context, R(inst.rb)); \
  R(inst.rb) += 1; \
  PC += 2; \
  CLK += 2; \
}
#define exec_ld_ub_rd_RBx(inst) { NO_EXT NO_DELAY \
  R(inst.rs_rd) = (unsigned char)mem_readB(context, R(inst.rb)); \
  R(inst.rb) += 1; \
  PC += 2; \
  CLK += 2; /* �ǂݍ��ݑ���[%rb]+��2�N���b�N�v���� */ \
}
#define exec_ld_h_rd_RBx(inst) { NO_EXT NO_DELAY \
  R(inst.rs_rd) = (short)mem_readH(context, R(inst.rb)); \
  R(inst.rb) += 2; \
  PC += 2; \
  CLK += 2; /* �ǂݍ��ݑ���[%rb]+��2�N���b�N�v���� */ \
}
#define exec_ld_uh_rd_RBx(inst) { NO_EXT NO_DELAY \
  R(inst.rs_rd) = (unsigned short)mem_readH(context, R(inst.rb)); \
  R(inst.rb) += 2; \
  PC += 2; \
  CLK += 2; /* �ǂݍ��ݑ���[%rb]+��2�N���b�N�v���� */ \
}
#define exec_ld_w_rd_RBx(inst) { NO_EXT NO_DELAY \
  R(inst.rs_rd) = mem_readW(context, R(inst.rb)); \
  R(inst.rb) += 4; \
  PC += 2; \
  CLK += 2; /* �ǂݍ��ݑ���[%rb]+��2�N���b�N�v���� */ \
}
#define exec_ld_b_RBx_rs(inst) { NO_EXT NO_DELAY \
  mem_writeB(context, R(inst.rb), R(inst.rs_rd)); \
  R(inst.rb) += 1; \
  PC += 2; \
  CLK += 1; /* �������ݑ���[%rb]+��1�N���b�N�ōς� */ \
}
#define exec_ld_h_RBx_rs(inst) { NO_EXT NO_DELAY \
  mem_writeH(context, R(inst.rb), R(inst.rs_rd)); \
  R(inst.rb) += 2; \
  PC += 2; \
  CLK += 1; /* �������ݑ���[%rb]+��1�N���b�N�ōς� */ \
}
#define exec_ld_w_RBx_rs(inst) { NO_EXT NO_DELAY \
  mem_writeW(context, R(inst.rb), R(inst.rs_rd)); \
  R(inst.rb) += 4; \
  PC += 2; \
  CLK += 1; /* �������ݑ���[%rb]+��1�N���b�N�ōς� */ \
}

/****************************************************************************
 *  CLASS 1B
 ****************************************************************************/

#define exec_add_rd_rs(inst) { \
  if(!EXT1.s) { \
    add(R(inst.rd), R(inst.rs), R(inst.rd)); \
  } else { \
    un = ext_3op(context); \
    add(R(inst.rs), un, R(inst.rd)); \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_sub_rd_rs(inst) { \
  if(!EXT1.s) { \
    sub(R(inst.rd), R(inst.rs), R(inst.rd)); \
  } else { \
    un = ext_3op(context); \
    sub(R(inst.rs), un, R(inst.rd)); \
  } \
  PC += 2; \
  CLK += 1; \
}

// RZ �̓_�~�[���W�X�^
#define exec_cmp_rd_rs(inst) { \
  if(!EXT1.s) { \
    sub(R(inst.rd), R(inst.rs), RZ); \
  } else { \
    un = ext_3op(context); \
    sub(R(inst.rs), un, RZ); \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_rd_rs(inst) { NO_EXT \
  R(inst.rd) = R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_and_rd_rs(inst) { \
  if(!EXT1.s) { \
    and(R(inst.rd), R(inst.rs), R(inst.rd)); \
  } else { \
    un = ext_3op(context); \
    and(R(inst.rs), un, R(inst.rd)); \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_or_rd_rs(inst) { \
  if(!EXT1.s) { \
    or(R(inst.rd), R(inst.rs), R(inst.rd)); \
  } else { \
    un = ext_3op(context); \
    or(R(inst.rs), un, R(inst.rd)); \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_xor_rd_rs(inst) { \
  if(!EXT1.s) { \
    xor(R(inst.rd), R(inst.rs), R(inst.rd)); \
  } else { \
    un = ext_3op(context); \
    xor(R(inst.rs), un, R(inst.rd)); \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_not_rd_rs(inst) { NO_EXT \
  not(R(inst.rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 2
 ****************************************************************************/

#define exec_ld_b_rd_SPxIMM6(inst) { NO_DELAY \
  R(inst.rs_rd) = (char)mem_readB(context, ext_SPxIMM6(context, inst.imm6, 1)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_ub_rd_SPxIMM6(inst) { NO_DELAY \
  R(inst.rs_rd) = (unsigned char)mem_readB(context, ext_SPxIMM6(context, inst.imm6, 1)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_h_rd_SPxIMM6(inst) { NO_DELAY \
  R(inst.rs_rd) = (short)mem_readH(context, ext_SPxIMM6(context, inst.imm6, 2)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_uh_rd_SPxIMM6(inst) { NO_DELAY \
  R(inst.rs_rd) = (unsigned short)mem_readH(context, ext_SPxIMM6(context, inst.imm6, 2)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_rd_SPxIMM6(inst) { NO_DELAY \
  R(inst.rs_rd) = mem_readW(context, ext_SPxIMM6(context, inst.imm6, 4)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_b_SPxIMM6_rs(inst) { NO_DELAY \
  mem_writeB(context, ext_SPxIMM6(context, inst.imm6, 1), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_h_SPxIMM6_rs(inst) { NO_DELAY \
  mem_writeH(context, ext_SPxIMM6(context, inst.imm6, 2), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_SPxIMM6_rs(inst) { NO_DELAY \
  mem_writeW(context, ext_SPxIMM6(context, inst.imm6, 4), R(inst.rs_rd)); \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 3
 ****************************************************************************/

#define exec_add_rd_imm6(inst) { \
  un = ext_imm6(context, inst.imm6_sign6); \
  add(R(inst.rd), un, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sub_rd_imm6(inst) { \
  un = ext_imm6(context, inst.imm6_sign6); \
  sub(R(inst.rd), un, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
// RZ �̓_�~�[���W�X�^
#define exec_cmp_rd_sign6(inst) { \
  sn = ext_sign6(context, inst.imm6_sign6); \
  sub(R(inst.rd), sn, RZ); /* �v���ӁIimm6�ł͂Ȃ�sign6�ł��I */ \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_rd_sign6(inst) { \
  R(inst.rd) = ext_sign6(context, inst.imm6_sign6); \
  PC += 2; \
  CLK += 1; \
}
#define exec_and_rd_sign6(inst) { \
  sn = ext_sign6(context, inst.imm6_sign6); \
  and(R(inst.rd), sn, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_or_rd_sign6(inst) { \
  sn = ext_sign6(context, inst.imm6_sign6); \
  or(R(inst.rd), sn, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_xor_rd_sign6(inst) { \
  sn = ext_sign6(context, inst.imm6_sign6); \
  xor(R(inst.rd), sn, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_not_rd_sign6(inst) { \
  sn = ext_sign6(context, inst.imm6_sign6); \
  not(sn, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 4A
 ****************************************************************************/

#define exec_add_sp_imm10(inst) { NO_EXT \
  SP += inst.imm10 * 4; \
  PC += 2; \
  CLK += 1; \
}
#define exec_sub_sp_imm10(inst) { NO_EXT \
  SP -= inst.imm10 * 4; \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 4B
 ****************************************************************************/

#define exec_srl_rd_imm4(inst) { NO_EXT \
  srl(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sll_rd_imm4(inst) { NO_EXT \
  sll(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sra_rd_imm4(inst) { NO_EXT \
  sra(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sla_rd_imm4(inst) { NO_EXT \
  sla(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_rr_rd_imm4(inst) { NO_EXT \
  rr(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_rl_rd_imm4(inst) { NO_EXT \
  rl(R(inst.rd), inst.imm4_rs, R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_srl_rd_rs(inst) { NO_EXT \
  srl(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sll_rd_rs(inst) { NO_EXT \
  sll(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sra_rd_rs(inst) { NO_EXT \
  sra(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sla_rd_rs(inst) { NO_EXT \
  sla(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_rr_rd_rs(inst) { NO_EXT \
  rr(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_rl_rd_rs(inst) { NO_EXT \
  rl(R(inst.rd), R(inst.imm4_rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 4C
 ****************************************************************************/

#define exec_scan0_rd_rs(inst) { NO_EXT \
  ua = R(inst.rs); \
  for(ub = 0; ub < 8; ub++) { \
    if(!(ua & (1 << 31))) break; \
    ua <<= 1; \
  } \
  R(inst.rd) = ub; \
  PSR.z = ub == 0; \
  PSR.c = ub == 8; \
  PC += 2; \
  CLK += 1; \
}
#define exec_scan1_rd_rs(inst) { NO_EXT \
  ua = R(inst.rs); \
  for(ub = 0; ub < 8; ub++) { \
    if(ua & (1 << 31)) break; \
    ua <<= 1; \
  } \
  R(inst.rd) = ub; \
  PSR.z = ub == 0; \
  PSR.c = ub == 8; \
  PC += 2; \
  CLK += 1; \
}
#define exec_swap_rd_rs(inst) { NO_EXT \
  ua = R(inst.rs); \
  R(inst.rd) = ((ua & 0x000000ff) << 24) | \
         ((ua & 0x0000ff00) <<  8) | \
         ((ua & 0x00ff0000) >>  8) | \
         ((ua & 0xff000000) >> 24); \
  PC += 2; \
  CLK += 1; \
}
#define exec_mirror_rd_rs(inst) { NO_EXT \
  ua = R(inst.rs); \
  R(inst.rd) = ((ua & 0x01010101) << 7) | \
         ((ua & 0x02020202) << 5) | \
         ((ua & 0x04040404) << 3) | \
         ((ua & 0x08080808) << 1) | \
         ((ua & 0x10101010) >> 1) | \
         ((ua & 0x20202020) >> 3) | \
         ((ua & 0x40404040) >> 5) | \
         ((ua & 0x80808080) >> 7); \
  PC += 2; \
  CLK += 1; \
}
#define exec_div0s_rs(inst) { NO_EXT NO_DELAY \
  if(!R(inst.rs)) DIE()/*core_trap(TRAP_ZERODIV, 0)*/; \
  AHR = (int)ALR >> 31; \
  PSR.ds = ALR >> 31; \
  PSR.n = R(inst.rs) >> 31; \
  PC += 2; \
  CLK += 1; \
}
#define exec_div0u_rs(inst) { NO_EXT NO_DELAY \
  if(!R(inst.rs)) DIE()/*core_trap(TRAP_ZERODIV, 0)*/; \
  AHR = 0; \
  PSR.ds = 0; \
  PSR.n = 0; \
  PC += 2; \
  CLK += 1; \
}
#define exec_div1_rs(inst) { NO_EXT NO_DELAY \
  /* div0x�ȊO�ł́A�[�����Z��O�͔������܂���B */ \
  AR <<= 1; \
  if(!PSR.ds) { \
    if(!PSR.n) { /* ������ */ \
      tmp = AHR - R(inst.rs); \
      if(tmp <= AHR) { /* !C */ \
        AHR = tmp; \
        ALR |= 1; \
      } \
    } else { /* ������ */ \
      tmp = AHR + R(inst.rs); \
      if(tmp < AHR) { /* C */ \
        AHR = tmp; \
        ALR |= 1; \
      } \
    } \
  } else { \
    if(!PSR.n) { /* ������ */ \
      tmp = AHR + R(inst.rs); \
      if(tmp >= AHR) { /* !C */ \
        AHR = tmp; \
        ALR |= 1; \
      } \
    } else { /* ������ */ \
      tmp = AHR - R(inst.rs); \
      if(tmp > AHR) { /* !C */ \
        AHR = tmp; \
        ALR |= 1; \
      } \
    } \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_div2s_rs(inst) { NO_EXT NO_DELAY \
  /* div0x�ȊO�ł́A�[�����Z��O�͔������܂���B */ \
  if(PSR.ds) { \
    if(!PSR.n) { \
      tmp = AHR + R(inst.rs); \
    } else { \
      tmp = AHR - R(inst.rs); \
    } \
    if(!tmp) { \
      AHR = tmp; \
      ALR += 1; \
    } \
  } \
  PC += 2; \
  CLK += 1; \
}
#define exec_div3s(inst) { NO_EXT NO_DELAY \
  /* div0x�ȊO�ł́A�[�����Z��O�͔������܂���B */ \
  if(PSR.ds != PSR.n) { \
    ALR = 0 - ALR;  /* ALR = -ALR �ł͌x���ɂȂ�̂Łc */ \
  } \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 5A
 ****************************************************************************/

#define exec_ld_w_sd_rs(inst) { NO_EXT /*NO_DELAY ���̓f�B���C�h�\�IEPSON���C�u�����̏��Z���[�`�����g���Ă�*/ \
  S(inst.sd_rd) = R(inst.rs_ss); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_w_rd_ss(inst) { NO_EXT /*NO_DELAY ���̓f�B���C�h�\�IEPSON���C�u�����̏��Z���[�`�����g���Ă�*/ \
  R(inst.sd_rd) = S(inst.rs_ss); \
  PC += 2; \
  CLK += 1; \
}

/****************************************************************************
 *  CLASS 5B
 ****************************************************************************/

#define exec_btst_RB_imm3(inst) { NO_DELAY \
  PSR.z = !((mem_readB(context, ext_RB(context, inst.rb)) >> inst.imm3) & 1); \
  PC += 2; \
  CLK += 3; \
}
#define exec_bclr_RB_imm3(inst) { NO_DELAY \
  addr = ext_RB(context, inst.rb); \
  mem_writeB(context, addr, mem_readB(context, addr) & ~(1 << inst.imm3)); \
  PC += 2; \
  CLK += 3; \
}
#define exec_bset_RB_imm3(inst) { NO_DELAY \
  addr = ext_RB(context, inst.rb); \
  mem_writeB(context, addr, mem_readB(context, addr) | (1 << inst.imm3)); \
  PC += 2; \
  CLK += 3; \
}
#define exec_bnot_RB_imm3(inst) { NO_DELAY \
  addr = ext_RB(context, inst.rb); \
  mem_writeB(context, addr, mem_readB(context, addr) ^ (1 << inst.imm3)); \
  PC += 2; \
  CLK += 3; \
}

/****************************************************************************
 *  CLASS 5C
 ****************************************************************************/

#define exec_adc_rd_rs(inst) { NO_EXT \
  adc(R(inst.rd), R(inst.rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_sbc_rd_rs(inst) { NO_EXT \
  sbc(R(inst.rd), R(inst.rs), R(inst.rd)); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_b_rd_rs(inst) { NO_EXT NO_DELAY /*�v���ӁI�f�B���C�h�s�I*/ \
  R(inst.rd) = (char)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_ub_rd_rs(inst) {  NO_EXT NO_DELAY /*�v���ӁI�f�B���C�h�s�I*/ \
  R(inst.rd) = (unsigned char)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_h_rd_rs(inst) {  NO_EXT NO_DELAY /*�v���ӁI�f�B���C�h�s�I*/ \
  R(inst.rd) = (short)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_ld_uh_rd_rs(inst) {  NO_EXT NO_DELAY /*�v���ӁI�f�B���C�h�s�I*/ \
  R(inst.rd) = (unsigned short)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_mlt_h_rd_rs(inst) { NO_EXT \
  ALR = (short)R(inst.rd) * (short)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_mltu_h_rd_rs(inst) { NO_EXT \
  ALR = (unsigned short)R(inst.rd) * (unsigned short)R(inst.rs); \
  PC += 2; \
  CLK += 1; \
}
#define exec_mlt_w_rd_rs(inst) { NO_EXT NO_DELAY \
  AR = (int64)(int)R(inst.rd) * (int64)(int)R(inst.rs); \
  PC += 2; \
  CLK += 5; \
}
#define exec_mltu_w_rd_rs(inst) { NO_EXT NO_DELAY \
  AR = (uint64)(unsigned)R(inst.rd) * (uint64)(unsigned)R(inst.rs); \
  PC += 2; \
  CLK += 5; \
}
  //int64 a, b, c;
  //while(R(inst.rs) != 0) {
  //  a = AR;
  //  b = (short)mem_read(R(inst.rs + 1), 2) * (short)mem_read(R(inst.rs + 2), 2);
  //  c = a + b;
  //  AR = a;
  //  if(!PSR.mo) { /* 1��0�ɂ͕ω����Ȃ� */
  //    PSR.mo = (a < 0  && b <  0 && c >= 0) ||
  //             (a >= 0 && b >= 0 && c <  0);
  //  }
  //  R(inst.rs)--;
  //  R(inst.rs + 1) += 2;
  //  R(inst.rs + 2) += 2;
  //  CLK += 2;
  //}
  //PC += 2;
  //CLK += 4;
  //
  //���{���͂����ł����Amac���ߎ��s���Ɋ��荞�݂��󂯕t���邽�߂ɁA���̂悤�ɕύX���܂����B
  //�����@�ƈႢ����mac���߂��t�F�b�`���Ă��܂����߁A���@�������s�T�C�N���������Ă��܂��B
  //

#define exec_mac_rs(inst) { NO_EXT NO_DELAY \
  if(R(inst.rs)) { \
    a64 = AR; \
    b64 = mem_readH(context, R(inst.rs + 1)) * mem_readH(context, R(inst.rs + 2)); \
    c64 = a64 + b64; \
    AR = a64; \
    if(!PSR.mo) { /* 1��0�ɂ͕ω����Ȃ� */ \
      PSR.mo = (a64 < 0  && b64 <  0 && c64 >= 0) || \
               (a64 >= 0 && b64 >= 0 && c64 <  0); \
    } \
    R(inst.rs)--; \
    R(inst.rs + 1) += 2; \
    R(inst.rs + 2) += 2; \
    /* PC�͂��̂܂܁B���������mac���߂����s���܂��B */ \
    CLK += 2; \
  } else { \
    PC += 2; \
    CLK += 4; \
  } \
}

/****************************************************************************
 *  CLASS 6
 ****************************************************************************/

#define exec_ext_imm13(inst) { NO_DELAY \
 \
  if(!EXT1.s) { \
    EXT1.c6 = inst; \
  } else if(!EXT2.s) { \
    EXT2.c6 = inst; \
  } else { \
    DIE(); \
  } \
  PC += 2; \
  CLK += 1; \
 \
  /* �g������閽�߂����s�B�i���̊Ԃ̊��荞�݂��֎~���邽�߁j */ \
  inst2.s = mem_readH(context, PC); \
  core_inst(context, inst2); \
  if(EXT1.s) DIE(); /* �m����ext�������Ă��邱�� */ \
}

/****************************************************************************
 *  �O���[�o���֐�
 ****************************************************************************/

void
core_init(PIEMU_CONTEXT* context)
{
  memset(&context->core, 0, sizeof context->core);
  PC = mem_readW(context, 0x0c00000);
}

void
core_work(PIEMU_CONTEXT* context)
{
  INST inst;

  if(context->core.in_halt)
  {
    STD_NOP;
    CLK += HALT_INT_CLK; //1 * NOP_CLK_MULTIPLY;
    return;
  }

  inst.s = mem_readH(context, PC);
  core_inst(context, inst);
}

unsigned
core_workex(PIEMU_CONTEXT* context, unsigned mils_org, unsigned nClocksDivBy1k)
{
  INST inst;
  unsigned insts = 0;
  do
  {
    inst.s = mem_readH(context, PC); core_inst(context, inst);
//    inst.s = mem_readH(context, PC); core_inst(context, inst);
//    inst.s = mem_readH(context, PC); core_inst(context, inst);
//    inst.s = mem_readH(context, PC); core_inst(context, inst);
    insts++;
  } while(!context->bEndFlag && (CLK - mils_org) < nClocksDivBy1k); /* 1�~���b���̏��� */
  return insts;
}

void
core_trap(PIEMU_CONTEXT* context,int no, int level)
{
  unsigned addr;

  /* �}�X�N�\�Ȋ��荞�݂̏ꍇ�̂݁A���荞�݉\�������������܂��B */
  if(no >= 16) {
    if(!PSR.ie) return;
    if((unsigned)level <= PSR.il) return;
  }

  if(context->core.in_halt)
    context->core.in_halt = 0;

  addr = mem_readW(context, pTTBR_REG + no * 4);  /* ���v����:�g���b�v�e�[�u�����ǂ݂��Ă܂� */
  SP -= 4;
  mem_writeW(context, SP, PC); /* �v���ӁIPC+2����Ȃ���I */
  SP -= 4;
  mem_writeW(context, SP, S(0));
  PC = addr;
  PSR.ie = 0;

  /* �}�X�N�\�Ȋ��荞�݂̏ꍇ�̂݁A���荞�݃��x����ω������܂��B */
  if(no >= 16) {
    PSR.il = level;
  }
}

/* Gameblabla - Why is this used ??? It was set to 1 but i disabled it for now
 * as the additional computing costs can cause extra cycles.
 * */
#ifdef EXTRA_ACCURACY
#define MASK(op, shr, and) ((inst.s >> (16 - (shr + and))) & ((1 << and) - 1))
#else
#define MASK(op, shr, and) (op)
#endif

void
core_inst(PIEMU_CONTEXT* context, INST inst)
{
  // �œK���ɂ���ă��W�X�^�Ŏg���񂵂Ă����
  int ACC; // �A�L�������[�^
  INST d_inst; // delayed inst.
  INST inst2; // extended inst.
  unsigned addr = 0; // �A�h���X
  int c = 0, d = 0;
  int cc = 0;
  unsigned ua = 0, ub = 0;
  int64 a64, b64, c64;
  unsigned tmp;
  unsigned un;
  int sn;

  // ������Ƀ��W�X�^���g�����W�����v�e�[�u���ɂȂ��Ă����
  switch(MASK(inst.c0a.cls, 0, 3))
  {
    case 0:
      switch(MASK(inst.c0a.op1, 3, 4))
      {
        /* CLASS 0A */
        case  0:
          switch(MASK(inst.c0a.op2, 8, 2))
          {
            case 0: exec_nop(inst.c0a); return;
            case 1: exec_slp(inst.c0a); return;
            case 2: exec_halt(inst.c0a); return;
            case 3: return;
          }
          break;
        case  1:
          switch(MASK(inst.c0a.op2, 8, 2))
          {
            case 0: exec_pushn_rs(inst.c0a); return;
            case 1: exec_popn_rd(inst.c0a); return;
            case 2: return;
            case 3: return;
          }
          break;
        case  2:
          switch(MASK(inst.c0a.op2, 8, 2))
          {
            case 0: exec_brk(inst.c0a); return;
            case 1: exec_retd(inst.c0a); return;
            case 2: exec_int_imm2(inst.c0a); return;
            case 3: exec_reti(inst.c0a); return;
          }
          break;
        case  3:
          switch(MASK(inst.c0a.op2, 8, 2))
          {
            case 0: exec_call_rb(inst.c0a); return;
            case 1: exec_ret(inst.c0a); return;
            case 2: exec_jp_rb(inst.c0a); return;
            case 3: return;
          }
          break;
        /* CLASS 0B */
        case  4: exec_jrgt_sign8(inst.c0b); return;
        case  5: exec_jrge_sign8(inst.c0b); return;
        case  6: exec_jrlt_sign8(inst.c0b); return;
        case  7: exec_jrle_sign8(inst.c0b); return;
        case  8: exec_jrugt_sign8(inst.c0b); return;
        case  9: exec_jruge_sign8(inst.c0b); return;
        case 10: exec_jrult_sign8(inst.c0b); return;
        case 11: exec_jrule_sign8(inst.c0b); return;
        case 12: exec_jreq_sign8(inst.c0b); return;
        case 13: exec_jrne_sign8(inst.c0b); return;
        case 14: exec_call_sign8(inst.c0b); return;
        case 15: exec_jp_sign8(inst.c0b); return;
      }
      break;
    case 1:
      switch(MASK(inst.c1a.op2, 6, 2))
      {
        /* CLASS 1A */
        case  0:
          switch(MASK(inst.c1a.op1, 3, 3))
          {
            case 0: exec_ld_b_rd_RB(inst.c1a); return;
            case 1: exec_ld_ub_rd_RB(inst.c1a); return;
            case 2: exec_ld_h_rd_RB(inst.c1a); return;
            case 3: exec_ld_uh_rd_RB(inst.c1a); return;
            case 4: exec_ld_w_rd_RB(inst.c1a); return;
            case 5: exec_ld_b_RB_rs(inst.c1a); return;
            case 6: exec_ld_h_RB_rs(inst.c1a); return;
            case 7: exec_ld_w_RB_rs(inst.c1a); return;
          }
          break;
        case  1:
          switch(MASK(inst.c1a.op1, 3, 3))
          {
            case 0: exec_ld_b_rd_RBx(inst.c1a); return;
            case 1: exec_ld_ub_rd_RBx(inst.c1a); return;
            case 2: exec_ld_h_rd_RBx(inst.c1a); return;
            case 3: exec_ld_uh_rd_RBx(inst.c1a); return;
            case 4: exec_ld_w_rd_RBx(inst.c1a); return;
            case 5: exec_ld_b_RBx_rs(inst.c1a); return;
            case 6: exec_ld_h_RBx_rs(inst.c1a); return;
            case 7: exec_ld_w_RBx_rs(inst.c1a); return;
          }
          break;
        /* CLASS 1B */
        case  2:
          switch(MASK(inst.c1a.op1, 3, 3))
          {
            case 0: exec_add_rd_rs(inst.c1b); return;
            case 1: exec_sub_rd_rs(inst.c1b); return;
            case 2: exec_cmp_rd_rs(inst.c1b); return;
            case 3: exec_ld_w_rd_rs(inst.c1b); return;
            case 4: exec_and_rd_rs(inst.c1b); return;
            case 5: exec_or_rd_rs(inst.c1b); return;
            case 6: exec_xor_rd_rs(inst.c1b); return;
            case 7: exec_not_rd_rs(inst.c1b); return;
          }
          break;
      }
      break;
    case 2:
      switch(MASK(inst.c2.op1, 3, 3))
      {
        /* CLASS 2 */
        case 0: exec_ld_b_rd_SPxIMM6(inst.c2); return;
        case 1: exec_ld_ub_rd_SPxIMM6(inst.c2); return;
        case 2: exec_ld_h_rd_SPxIMM6(inst.c2); return;
        case 3: exec_ld_uh_rd_SPxIMM6(inst.c2); return;
        case 4: exec_ld_w_rd_SPxIMM6(inst.c2); return;
        case 5: exec_ld_b_SPxIMM6_rs(inst.c2); return;
        case 6: exec_ld_h_SPxIMM6_rs(inst.c2); return;
        case 7: exec_ld_w_SPxIMM6_rs(inst.c2); return;
      }
      break;
    case 3:
      switch(MASK(inst.c3.op1, 3, 3))
      {
        /* CLASS 3 */
        case 0: exec_add_rd_imm6(inst.c3); return;
        case 1: exec_sub_rd_imm6(inst.c3); return;
        case 2: exec_cmp_rd_sign6(inst.c3); return;
        case 3: exec_ld_w_rd_sign6(inst.c3); return;
        case 4: exec_and_rd_sign6(inst.c3); return;
        case 5: exec_or_rd_sign6(inst.c3); return;
        case 6: exec_xor_rd_sign6(inst.c3); return;
        case 7: exec_not_rd_sign6(inst.c3); return;
      }
      break;
    case 4:
      switch(MASK(inst.c4a.op1, 3, 3))
      {
        /* CLASS 4A */
        case 0: exec_add_sp_imm10(inst.c4a); return;
        case 1: exec_sub_sp_imm10(inst.c4a); return;
        default:
          switch(MASK(inst.c4b.op2, 6, 2))
          {
            /* CLASS 4B */
            case 0:
              switch(MASK(inst.c4b.op1, 3, 3))
              {
                case 0: return;
                case 1: return;
                case 2: exec_srl_rd_imm4(inst.c4b); return;
                case 3: exec_sll_rd_imm4(inst.c4b); return;
                case 4: exec_sra_rd_imm4(inst.c4b); return;
                case 5: exec_sla_rd_imm4(inst.c4b); return;
                case 6: exec_rr_rd_imm4(inst.c4b); return;
                case 7: exec_rl_rd_imm4(inst.c4b); return;
              }
              break;
            case 1:
              switch(MASK(inst.c4b.op1, 3, 3))
              {
                case 0: return;
                case 1: return;
                case 2: exec_srl_rd_rs(inst.c4b); return;
                case 3: exec_sll_rd_rs(inst.c4b); return;
                case 4: exec_sra_rd_rs(inst.c4b); return;
                case 5: exec_sla_rd_rs(inst.c4b); return;
                case 6: exec_rr_rd_rs(inst.c4b); return;
                case 7: exec_rl_rd_rs(inst.c4b); return;
              }
              break;
            /* CLASS 4C */
            case 2:
              switch(MASK(inst.c4c.op1, 3, 3))
              {
                case 0: return;
                case 1: return;
                case 2: exec_scan0_rd_rs(inst.c4c); return;
                case 3: exec_scan1_rd_rs(inst.c4c); return;
                case 4: exec_swap_rd_rs(inst.c4c); return;
                case 5: exec_mirror_rd_rs(inst.c4c); return;
              }
              break;
            case 3:
              switch(MASK(inst.c4c.op1, 3, 3))
              {
                case 0: return;
                case 1: return;
                case 2: exec_div0s_rs(inst.c4c); return;
                case 3: exec_div0u_rs(inst.c4c); return;
                case 4: exec_div1_rs(inst.c4c); return;
                case 5: exec_div2s_rs(inst.c4c); return;
                case 6: exec_div3s(inst.c4c); return;
              }
              break;
          }
          break;
      }
      break;
    case 5:
      switch(MASK(inst.c5a.op2, 6, 2))
      {
        case 0:
          switch(MASK(inst.c5a.op1, 3, 3))
          {
            /* CLASS 5A */
            case 0: exec_ld_w_sd_rs(inst.c5a); return;
            case 1: exec_ld_w_rd_ss(inst.c5a); return;
            /* CLASS 5B */
            case 2: exec_btst_RB_imm3(inst.c5b); return;
            case 3: exec_bclr_RB_imm3(inst.c5b); return;
            case 4: exec_bset_RB_imm3(inst.c5b); return;
            case 5: exec_bnot_RB_imm3(inst.c5b); return;
            /* CLASS 5C */
            case 6: exec_adc_rd_rs(inst.c5c); return;
            case 7: exec_sbc_rd_rs(inst.c5c); return;
          }
          break;
        case 1:
          switch(MASK(inst.c5c.op1, 3, 3))
          {
            case 0: exec_ld_b_rd_rs(inst.c5c); return;
            case 1: exec_ld_ub_rd_rs(inst.c5c); return;
            case 2: exec_ld_h_rd_rs(inst.c5c); return;
            case 3: exec_ld_uh_rd_rs(inst.c5c); return;
            case 4: return;
            case 5: return;
            case 6: return;
            case 7: return;
          }
          break;
        case 2:
          switch(MASK(inst.c5c.op1, 3, 3))
          {
            case 0: exec_mlt_h_rd_rs(inst.c5c); return;
            case 1: exec_mltu_h_rd_rs(inst.c5c); return;
            case 2: exec_mlt_w_rd_rs(inst.c5c); return;
            case 3: exec_mltu_w_rd_rs(inst.c5c); return;
            case 4: exec_mac_rs(inst.c5c); return;
            case 5: return;
            case 6: return;
            case 7: return;
          }
          break;
      }
      break;
    case 6:
      /* CLASS 6 */
      exec_ext_imm13(inst.c6); return;
      break;
    case 7:
      return;
    case 8:
      return;
  }
  DIE();
}

