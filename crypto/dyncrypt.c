/*----------------------------------------------------------------------------*/
/* file: dyncrypt.c                                                           */
/*                                                                            */
/* Implementation of the z/Architecture crypto instructions described in      */
/* SA22-7832-02: z/Architecture Principles of Operation within the Hercules   */
/* z/Architecture emulator. This file may only be used with and within the    */
/* Hercules emulator for non-commercial use!                                  */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2003-2004 */
/*                              Noordwijkerhout, The Netherlands.             */
/*----------------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "des.h"
#include "sha1.h"
#include "sha256.h"

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST

/*----------------------------------------------------------------------------*/
/* Debugging options                                                          */
/*----------------------------------------------------------------------------*/
//#define OPTION_KM_DEBUG
//#define OPTION_KMC_DEBUG
//#define OPTION_KIMD_DEBUG
//#define OPTION_KLMD_DEBUG
//#define OPTION_KMAC_DEBUG

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* fc   : Function code                                                       */
/* m    : Modifier bit                                                        */
/*----------------------------------------------------------------------------*/
#define GR0_fc(regs)    ((regs)->GR_L(0) & 0x0000007F)
#define GR0_m(regs)     (((regs)->GR_L(0) & 0x00000080) ? TRUE : FALSE)

/*----------------------------------------------------------------------------*/
/* Function codes for compute intermediate message digest                     */
/*----------------------------------------------------------------------------*/
#define KIMD_QUERY      0
#define KIMD_SHA_1      1
#define KIMD_SHA_256    2
#define KIMD_MAX_FC     2
#define KIMD_BITS       { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*----------------------------------------------------------------------------*/
/* Function codes for compute last message digest                             */
/*----------------------------------------------------------------------------*/
#define KLMD_QUERY      0
#define KLMD_SHA_1      1
#define KLMD_SHA_256    2
#define KLMD_MAX_FC     2
#define KLMD_BITS       { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*----------------------------------------------------------------------------*/
/* Function codes for cipher message                                          */
/*----------------------------------------------------------------------------*/
#define KM_QUERY        0
#define KM_DEA          1
#define KM_TDEA_128     2
#define KM_TDEA_192     3
#define KM_MAX_FC       3
#define KM_BITS         { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*----------------------------------------------------------------------------*/
/* Function codes for compute message authentication code                     */
/*----------------------------------------------------------------------------*/
#define KMAC_QUERY      0
#define KMAC_DEA        1
#define KMAC_TDEA_128   2
#define KMAC_TDEA_192   3
#define KMAC_MAX_FC     3
#define KMAC_BITS       { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*----------------------------------------------------------------------------*/
/* Function codes for cipher message with chaining                            */
/*----------------------------------------------------------------------------*/
#define KMC_QUERY       0
#define KMC_DEA         1
#define KMC_TDEA_128    2
#define KMC_TDEA_192    3
#define KMC_MAX_FC      3
#define KMC_BITS        { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*----------------------------------------------------------------------------*/
/* Handy macro's                                                              */
/*----------------------------------------------------------------------------*/
#define SHA1M2C(ctx, md) \
{ \
  (ctx)->state[0] = ((md)[0] << 24) + ((md)[1] << 16) + ((md)[2] << 8) + (md)[3]; \
  (ctx)->state[1] = ((md)[4] << 24) + ((md)[5] << 16) + ((md)[6] << 8) + (md)[7]; \
  (ctx)->state[2] = ((md)[8] << 24) + ((md)[9] << 16) + ((md)[10] << 8) + (md)[11]; \
  (ctx)->state[3] = ((md)[12] << 24) + ((md)[13] << 16) + ((md)[14] << 8) + (md)[15]; \
  (ctx)->state[4] = ((md)[16] << 24) + ((md)[17] << 16) + ((md)[18] << 8) + (md)[19]; \
}
#define SHA256M2C(ctx, md) \
{ \
  (ctx)->state[0] = ((md)[0] << 24) + ((md)[1] << 16) + ((md)[2] << 8) + (md)[3]; \
  (ctx)->state[1] = ((md)[4] << 24) + ((md)[5] << 16) + ((md)[6] << 8) + (md)[7]; \
  (ctx)->state[2] = ((md)[8] << 24) + ((md)[9] << 16) + ((md)[10] << 8) + (md)[11]; \
  (ctx)->state[3] = ((md)[12] << 24) + ((md)[13] << 16) + ((md)[14] << 8) + (md)[15]; \
  (ctx)->state[4] = ((md)[16] << 24) + ((md)[17] << 16) + ((md)[18] << 8) + (md)[19]; \
  (ctx)->state[5] = ((md)[20] << 24) + ((md)[21] << 16) + ((md)[22] << 8) + (md)[23]; \
  (ctx)->state[6] = ((md)[24] << 24) + ((md)[25] << 16) + ((md)[26] << 8) + (md)[27]; \
  (ctx)->state[7] = ((md)[28] << 24) + ((md)[29] << 16) + ((md)[30] << 8) + (md)[31]; \
}
#define SHA1C2M(md, ctx) \
{ \
  (md)[0] = ((ctx)->state[0] & 0xff000000) >> 24; \
  (md)[1] = ((ctx)->state[0] & 0x00ff0000) >> 16; \
  (md)[2] = ((ctx)->state[0] & 0x0000ff00) >> 8; \
  (md)[3] = ((ctx)->state[0] & 0x000000ff); \
  (md)[4] = ((ctx)->state[1] & 0xff000000) >> 24; \
  (md)[5] = ((ctx)->state[1] & 0x00ff0000) >> 16; \
  (md)[6] = ((ctx)->state[1] & 0x0000ff00) >> 8; \
  (md)[7] = ((ctx)->state[1] & 0x000000ff); \
  (md)[8] = ((ctx)->state[2] & 0xff000000) >> 24; \
  (md)[9] = ((ctx)->state[2] & 0x00ff0000) >> 16; \
  (md)[10] = ((ctx)->state[2] & 0x0000ff00) >> 8; \
  (md)[11] = ((ctx)->state[2] & 0x000000ff); \
  (md)[12] = ((ctx)->state[3] & 0xff000000) >> 24; \
  (md)[13] = ((ctx)->state[3] & 0x00ff0000) >> 16; \
  (md)[14] = ((ctx)->state[3] & 0x0000ff00) >> 8; \
  (md)[15] = ((ctx)->state[3] & 0x000000ff); \
  (md)[16] = ((ctx)->state[4] & 0xff000000) >> 24; \
  (md)[17] = ((ctx)->state[4] & 0x00ff0000) >> 16; \
  (md)[18] = ((ctx)->state[4] & 0x0000ff00) >> 8; \
  (md)[19] = ((ctx)->state[4] & 0x000000ff); \
}
#define SHA256C2M(md, ctx) \
{ \
  (md)[0] = ((ctx)->state[0] & 0xff000000) >> 24; \
  (md)[1] = ((ctx)->state[0] & 0x00ff0000) >> 16; \
  (md)[2] = ((ctx)->state[0] & 0x0000ff00) >> 8; \
  (md)[3] = ((ctx)->state[0] & 0x000000ff); \
  (md)[4] = ((ctx)->state[1] & 0xff000000) >> 24; \
  (md)[5] = ((ctx)->state[1] & 0x00ff0000) >> 16; \
  (md)[6] = ((ctx)->state[1] & 0x0000ff00) >> 8; \
  (md)[7] = ((ctx)->state[1] & 0x000000ff); \
  (md)[8] = ((ctx)->state[2] & 0xff000000) >> 24; \
  (md)[9] = ((ctx)->state[2] & 0x00ff0000) >> 16; \
  (md)[10] = ((ctx)->state[2] & 0x0000ff00) >> 8; \
  (md)[11] = ((ctx)->state[2] & 0x000000ff); \
  (md)[12] = ((ctx)->state[3] & 0xff000000) >> 24; \
  (md)[13] = ((ctx)->state[3] & 0x00ff0000) >> 16; \
  (md)[14] = ((ctx)->state[3] & 0x0000ff00) >> 8; \
  (md)[15] = ((ctx)->state[3] & 0x000000ff); \
  (md)[16] = ((ctx)->state[4] & 0xff000000) >> 24; \
  (md)[17] = ((ctx)->state[4] & 0x00ff0000) >> 16; \
  (md)[18] = ((ctx)->state[4] & 0x0000ff00) >> 8; \
  (md)[19] = ((ctx)->state[4] & 0x000000ff); \
  (md)[20] = ((ctx)->state[5] & 0xff000000) >> 24; \
  (md)[21] = ((ctx)->state[5] & 0x00ff0000) >> 16; \
  (md)[22] = ((ctx)->state[5] & 0x0000ff00) >> 8; \
  (md)[23] = ((ctx)->state[5] & 0x000000ff); \
  (md)[24] = ((ctx)->state[6] & 0xff000000) >> 24; \
  (md)[25] = ((ctx)->state[6] & 0x00ff0000) >> 16; \
  (md)[26] = ((ctx)->state[6] & 0x0000ff00) >> 8; \
  (md)[27] = ((ctx)->state[6] & 0x000000ff); \
  (md)[28] = ((ctx)->state[7] & 0xff000000) >> 24; \
  (md)[29] = ((ctx)->state[7] & 0x00ff0000) >> 16; \
  (md)[30] = ((ctx)->state[7] & 0x0000ff00) >> 8; \
  (md)[31] = ((ctx)->state[7] & 0x000000ff); \
}
#define LOGBYTE(s, v, x) \
{ \
  int i; \
  \
  logmsg("  " s " "); \
  for(i = 0; i < (x); i++) \
    logmsg("%02X", (v)[i]); \
  logmsg("\n"); \
}
#define LOGBYTE2(s, v, x, y)    \
{  \
  int i; \
  int j; \
  \
  logmsg("  " s "\n"); \
  for(i = 0; i < (y); i++) \
  { \
    logmsg("      "); \
    for(j = 0; j < (x); j++) \
      logmsg("%02X", (v)[i * (y) + j]); \
    logmsg("\n"); \
  } \
}
#define PROCESS_MAX     4096
#define TRUEFALSE(boolean)  ((boolean) ? "True" : "False")
/*----------------------------------------------------------------------------*/
/* constants                                                                  */
/*----------------------------------------------------------------------------*/
#ifndef CONST
#define CONST
BYTE kimd_bits[] = KIMD_BITS;
BYTE klmd_bits[] = KLMD_BITS;
BYTE km_bits[] = KM_BITS;
BYTE kmac_bits[] = KMAC_BITS;
BYTE kmc_bits[] = KMC_BITS;
#endif

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD) FC 0                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_query)(int r1, int r2, REGS *regs)
{
  UNREFERENCED(r1);

#ifdef OPTION_KIMD_DEBUG
  logmsg("  KIMD: function 0: query\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Store the parameter block */
  ARCH_DEP(vstorec)(kimd_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD) FC 1                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_sha_1)(int r1, int r2, REGS *regs)
{
  BYTE buffer[64];
  int crypted;
  BYTE cv[20];
  sha1_context context;

  UNREFERENCED(r1);

#ifdef OPTION_KIMD_DEBUG
  logmsg("  KIMD: function 1: sha-1\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 64 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 19, ACCTYPE_WRITE, regs);

  /* Fetch and set initial chaining value */
  ARCH_DEP(vfetchc)(cv, 19, GR_A(1, regs), 1, regs);
  sha1_starts(&context);
  SHA1M2C(&context, cv);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("icv   :", cv, 20);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 64 < PROCESS_MAX)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(buffer, 63, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE2("  input :", buffer, 16, 4);
#endif

    sha1_update(&context, buffer, 64);

    /* Store the output chaining value */
    SHA1C2M(cv, &context);
    ARCH_DEP(vstorec)(cv, 19, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("ocv   :", cv, 20);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs) + 64);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 64);

#ifdef OPTION_KIMD_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD) FC 2                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_sha_256)(int r1, int r2, REGS *regs)
{
  BYTE buffer[64];
  int crypted;
  BYTE cv[32];
  sha256_context context;

  UNREFERENCED(r1);

#ifdef OPTION_KIMD_DEBUG
  logmsg("  KIMD: function 2: sha-256\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 64 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 31, ACCTYPE_WRITE, regs);

  /* Fetch and set initial chaining value */
  ARCH_DEP(vfetchc)(cv, 31, GR_A(1, regs), 1, regs);
  sha256_starts(&context);
  SHA256M2C(&context, cv);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("icv   :", cv, 32);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 64 < PROCESS_MAX)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(buffer, 63, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE2("  input :", buffer, 16, 4);
#endif

    sha256_update(&context, buffer, 64);

    /* Store the output chaining value */
    SHA256C2M(cv, &context);
    ARCH_DEP(vstorec)(cv, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("ocv   :", cv, 32);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs) + 64);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 64);

#ifdef OPTION_KIMD_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B93F Compute last message digest (KLMD) FC 0                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(klmd_query)(int r1, int r2, REGS *regs)
{
  UNREFERENCED(r1);

#ifdef OPTION_KLMD_DEBUG
  logmsg("  KLMD: function 0: query\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Store the parameter block */
  ARCH_DEP(vstorec)(klmd_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B93F Compute last message digest (KLMD) FC 1                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(klmd_sha_1)(int r1, int r2, REGS *regs)
{
  BYTE buffer[64];
  int crypted;
  BYTE cv[20];
  sha1_context context;
  int i;

  UNREFERENCED(r1);

#ifdef OPTION_KLMD_DEBUG
  logmsg("  KLMD: function 1: sha-1\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 19, ACCTYPE_WRITE, regs);

  /* Fetch and set initial chaining value */
  ARCH_DEP(vfetchc)(cv, 19, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("icv   :", cv, 20);
#endif

  sha1_starts(&context);
  SHA1M2C(&context, cv);

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 64 < PROCESS_MAX)
  {
    /* Check for last block */
    if(GR_A(r2 + 1, regs) < 64)
      break;

    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(buffer, 63, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
    LOGBYTE2("input :", buffer, 16, 4);
#endif

    sha1_update(&context, buffer, 64);

    /* Store the output chaining value */
    SHA1C2M(cv, &context);
    ARCH_DEP(vstorec)(cv, 19, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("ocv   :", cv, 20);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs) + 64);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 64);

#ifdef OPTION_KLMD_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

  }

  /* Check if cpu determined amount of data is processed */
  if(GR_A(r2 + 1, regs) >= 64)
  {
    regs->psw.cc = 3;
    return;
  }

  /* Fetch and process possible last block of data */
  if(GR_A(r2 + 1, regs))
  {
    ARCH_DEP(vfetchc)(buffer, GR_A(r2 + 1, regs) - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
    LOGBYTE("input :", buffer, GR_A(r2 + 1, regs));
#endif
  }

  /* Do the padding */
  i = GR_A(r2 + 1, regs);
  if(i > 55)
  {
    buffer[i++] = 0x80;
    while(i < 64)
     buffer[i++] = 0x00;
    sha1_update(&context, buffer, 64);
    for(i = 0; i < 56; i++)
      buffer[i] = 0;
  }
  else
  {
    buffer[i++] = 0x80;
    while(i < 56)
      buffer[i++] = 0x00;
  }

  /* Fetch and set the message bit length */
  ARCH_DEP(vfetchc)(&buffer[56], 7, GR_A(1, regs) + 20, 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("mbl   :", &buffer[56], 8);
#endif

  /* Calculate and store the message digest */
  sha1_update(&context, buffer, 64);
  SHA1C2M(cv, &context);
  ARCH_DEP(vstorec)(cv, 19, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("md    :", cv, 20);
#endif

  /* Update registers */
  SET_GR_A(r2, regs, GR_A(r2,regs) + GR_A(r2 + 1, regs));
  SET_GR_A(r2 + 1, regs, 0);

#ifdef OPTION_KLMD_DEBUG
  logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
  logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

  /* Set condition code */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B93F Compute last message digest (KLMD) FC 2                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(klmd_sha_256)(int r1, int r2, REGS *regs)
{
  BYTE buffer[64];
  int crypted;
  BYTE cv[32];
  sha256_context context;
  int i;

  UNREFERENCED(r1);

#ifdef OPTION_KLMD_DEBUG
  logmsg("  KLMD: function 2: sha-256\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 31, ACCTYPE_WRITE, regs);

  /* Fetch and set initial chaining value */
  ARCH_DEP(vfetchc)(cv, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("icv   :", cv, 32);
#endif

  sha256_starts(&context);
  SHA256M2C(&context, cv);

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 64 < PROCESS_MAX)
  {
    /* Check for last block */
    if(GR_A(r2 + 1, regs) < 64)
      break;

    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(buffer, 63, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
    LOGBYTE2("input :", buffer, 16, 4);
#endif

    sha256_update(&context, buffer, 64);

    /* Store the output chaining value */
    SHA256C2M(cv, &context);
    ARCH_DEP(vstorec)(cv, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("ocv   :", cv, 32);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs) + 64);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 64);

#ifdef OPTION_KLMD_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

  }

  /* Check if cpu determined amount of data is processed */
  if(GR_A(r2 + 1, regs) >= 64)
  {
    regs->psw.cc = 3;
    return;
  }

  /* Fetch and process possible last block of data */
  if(GR_A(r2 + 1, regs))
  {
    ARCH_DEP(vfetchc)(buffer, GR_A(r2 + 1, regs) - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
    LOGBYTE("input :", buffer, GR_A(r2 + 1, regs));
#endif
  }

  /* Do the padding */
  i = GR_A(r2 + 1, regs);
  if(i > 55)
  {
    buffer[i++] = 0x80;
    while(i < 64)
     buffer[i++] = 0x00;
    sha256_update(&context, buffer, 64);
    for(i = 0; i < 56; i++)
      buffer[i] = 0;
  }
  else
  {
    buffer[i++] = 0x80;
    while(i < 56)
      buffer[i++] = 0x00;
  }

  /* Fetch and set the message bit length */
  ARCH_DEP(vfetchc)(&buffer[56], 7, GR_A(1, regs) + 32, 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("mbl   :", &buffer[56], 8);
#endif

  /* Calculate and store the message digest */
  sha256_update(&context, buffer, 64);
  SHA256C2M(cv, &context);
  ARCH_DEP(vstorec)(cv, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  LOGBYTE("md    :", cv, 32);
#endif

  /* Update registers */
  SET_GR_A(r2, regs, GR_A(r2,regs) + GR_A(r2 + 1, regs));
  SET_GR_A(r2 + 1, regs, 0);

#ifdef OPTION_KLMD_DEBUG
  logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
  logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

  /* Set condition code */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 0                                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_query)(int r1, int r2, REGS *regs)
{
#ifdef OPTION_KM_DEBUG
  logmsg("  KM: function 0: query\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Store the parameter block */
  ARCH_DEP(vstorec)(km_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 and return */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 1                                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_dea)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  des_context context;
  BYTE k[8];

#ifdef OPTION_KM_DEBUG
  logmsg("  KM: function 1: dea\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Fetch and set the cryptographic key */
  ARCH_DEP(vfetchc)(k, 7, GR_A(1, regs), 1, regs);
  des_set_key(&context, k);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", k, 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
      des_decrypt(&context, buffer, buffer); 
    else
      des_encrypt(&context, buffer, buffer);

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1,regs) + 8);
    if(r1 != r2)
      SET_GR_A(r2, regs, GR_A(r2,regs) + 8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 8);

#ifdef OPTION_KM_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 2                                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_tdea_128)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  des3_context context;
  BYTE k[16];

#ifdef OPTION_KM_DEBUG
  logmsg("  KM: function 2: tdea-128\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 15, GR_A(1, regs), 1, regs);
  des3_set_2keys(&context, &k[0], &k[8]);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
      des3_decrypt(&context, buffer, buffer);
    else
      des3_encrypt(&context, buffer, buffer);

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs,GR_A(r1,regs) + 8);
    if(r1 != r2)
      SET_GR_A(r2, regs,GR_A(r2,regs) + 8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 8);

#ifdef OPTION_KM_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 3                                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_tdea_192)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  des3_context context;
  BYTE k[24];

#ifdef OPTION_KM_DEBUG
  logmsg("  KM: function 3: tdea-192\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 23, GR_A(1, regs), 1, regs);
  des3_set_3keys(&context, &k[0], &k[8], &k[16]);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
  LOGBYTE("k3    :", &k[16], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
      des3_decrypt(&context, buffer, buffer);
    else
      des3_encrypt(&context, buffer, buffer);

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs,GR_A(r1,regs) + 8);
    if(r1 != r2)
      SET_GR_A(r2, regs,GR_A(r2,regs)+8);
    SET_GR_A(r2 + 1, regs, GR_A(r2+1, regs) - 8);

#ifdef OPTION_KM_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 0                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_query)(int r1, int r2, REGS *regs)
{
  UNREFERENCED(r1);

#ifdef OPTION_KMAC_DEBUG
  logmsg("  KMAC: function 0: query\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Store the parameter block */
  ARCH_DEP(vstorec)(kmac_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 1                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_dea)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  BYTE cv[8];
  int i;
  des_context context;
  BYTE k[8];

  UNREFERENCED(r1);

#ifdef OPTION_KMAC_DEBUG
  logmsg("  KMAC: function 1: dea\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic key */
  ARCH_DEP(vfetchc)(k, 7, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context, k);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k     :", k, 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* XOR the message with chaining value */
    for(i = 0; i < 8; i++)
      buffer[i] ^= cv[i];

    /* Calculate the output chaining value */
    des_encrypt(&context, buffer, cv);
    
    /* Store the output chaining value */
    ARCH_DEP(vstorec)(cv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", cv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2,regs) + 8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs) - 8);

#ifdef OPTION_KMAC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 2                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_tdea_128)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  BYTE cv[8];
  des_context context1;
  des_context context2;
  int i;
  BYTE k[16];

  UNREFERENCED(r1);

#ifdef OPTION_KMAC_DEBUG
  logmsg("  KMAC: function 2: tdea-128\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 15, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context1, &k[0]);
  des_set_key(&context2, &k[8]);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* XOR the message with chaining value */
    for(i = 0; i < 8; i++)
      buffer[i] ^= cv[i];  

    /* Calculate the output chaining value */
    des_encrypt(&context1, buffer, cv);
    des_decrypt(&context2, cv, cv);
    des_encrypt(&context1, cv, cv);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(cv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", cv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs)+8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs)-8);

#ifdef OPTION_KMAC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 3                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_tdea_192)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  BYTE cv[8];
  des_context context1;
  des_context context2;
  des_context context3;
  int i;
  BYTE k[24];

  UNREFERENCED(r1);

#ifdef OPTION_KMAC_DEBUG
  logmsg("  KMAC: function 3: tdea-192\n");
#endif

  /* Check special conditions */
  if(!r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8 || GR0_m(regs))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 23, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context1, &k[0]);
  des_set_key(&context2, &k[8]);
  des_set_key(&context3, &k[16]);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
  LOGBYTE("k3    :", &k[16], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* XOR the message with chaining value */
    for(i = 0; i < 8; i++)
      buffer[i] ^= cv[i];

    /* Calculate the output chaining value */
    des_encrypt(&context1, buffer, cv);
    des_decrypt(&context2, cv, cv);
    des_encrypt(&context3, cv, cv);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(cv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", cv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs,GR_A(r2,regs)+8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs)-8);

#ifdef OPTION_KMAC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 0                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_query)(int r1, int r2, REGS *regs)
{
#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 0: query\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Store the parameter block */
  ARCH_DEP(vstorec)(kmc_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 1                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_dea)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  BYTE cv[8];
  BYTE ocv[8];
  des_context context;
  int i;
  BYTE k[8];

#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 1: dea\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic key */
  ARCH_DEP(vfetchc)(k, 7, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context, k);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k     :", k, 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
    {

      /* Save the ovc */
      memcpy(ocv, buffer, 8);

      /* Decrypt and XOR */
      des_decrypt(&context, buffer, buffer);
      for(i = 0; i < 8; i++)
        buffer[i] ^= cv[i];
    }
    else
    {
      /* XOR and encrypt */
      for(i = 0; i < 8; i++)
        buffer[i] ^= cv[i];
      des_encrypt(&context, buffer, buffer);

      /* Save the ovc */
      memcpy(ocv, buffer, 8);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs,GR_A(r1,regs)+ 8);
    if(r1 != r2)
      SET_GR_A(r2, regs, GR_A(r2,regs)+8);
    SET_GR_A(r2 + 1, regs,GR_A(r2+1,regs)-8);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 2                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_tdea_128)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  int i;
  BYTE cv[8];
  BYTE ocv[8];
  des_context context1;
  des_context context2;
  BYTE k[16];

#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 2: tdea-128\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 15, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context1, &k[0]);
  des_set_key(&context2, &k[8]);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
    {

      /* Save the ovc */
      memcpy(ocv, buffer, 8);

      /* Decrypt and XOR */
      des_decrypt(&context1, buffer, buffer);
      des_encrypt(&context2, buffer, buffer);
      des_decrypt(&context1, buffer, buffer);
      for(i = 0; i < 8; i++)
        buffer[i] ^= cv[i];
    }
    else
    {
      /* XOR and encrypt */
      for(i = 0 ; i < 8; i++)
        buffer[i] ^= cv[i];
      des_encrypt(&context1, buffer, buffer);
      des_decrypt(&context2, buffer, buffer);
      des_encrypt(&context1, buffer, buffer);

      /* Save the ovc */
      memcpy(ocv, buffer, 8);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs,GR_A(r1,regs)+8);
    if(r1 != r2)
      SET_GR_A(r2, regs, GR_A(r2, regs)+8);
    SET_GR_A(r2 + 1, regs, GR_A(r2+1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 3                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_tdea_192)(int r1, int r2, REGS *regs)
{
  BYTE buffer[8];
  int crypted;
  int i;
  BYTE cv[8];
  BYTE ocv[8];
  des_context context1;
  des_context context2;
  des_context context3;
  BYTE k[24];

#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 3: tdea-192\n");
#endif

  /* Check special conditions */
  if(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || GR_A(r2 + 1, regs) % 8)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(!GR_A(r2 + 1, regs))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1,regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the initial chaining value */
  ARCH_DEP(vfetchc)(cv, 7, GR_A(1, regs), 1, regs);

  /* Fetch and set the cryptographic keys */
  ARCH_DEP(vfetchc)(k, 23, GR_A(1, regs) + 8, 1, regs);
  des_set_key(&context1, &k[0]);
  des_set_key(&context2, &k[8]);
  des_set_key(&context3, &k[16]);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", cv, 8);
  LOGBYTE("k1    :", &k[0], 8);
  LOGBYTE("k2    :", &k[8], 8);
  LOGBYTE("k3    :", &k[16], 8);
#endif

  /* Try to process the CPU-determined amount of data */
  crypted = 0;
  while(crypted += 8 < PROCESS_MAX)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(buffer, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", buffer, 8);
#endif

    /* Do the job */
    if(GR0_m(regs))
    {

      /* Save the ovc */
      memcpy(ocv, buffer, 8);

      /* Decrypt and XOR */
      des_decrypt(&context3, buffer, buffer);
      des_encrypt(&context2, buffer, buffer);
      des_decrypt(&context1, buffer, buffer);
      for(i = 0; i < 8; i++)
        buffer[i] ^= cv[i];
    }
    else
    {
      /* XOR and encrypt */
      for(i = 0; i < 8; i++)
        buffer[i] ^= cv[i];
      des_encrypt(&context1, buffer, buffer);
      des_decrypt(&context2, buffer, buffer);
      des_encrypt(&context3, buffer, buffer);

      /* Save the ovc */
      memcpy(ocv, buffer, 8);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(buffer, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", buffer, 8);
#endif

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs,GR_A(r1,regs)+8);
    if(r1 != r2)
      SET_GR_A(r2, regs, GR_A(r2, regs)+8);
    SET_GR_A(r2 + 1, regs, GR_A(r2+1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(!GR_A(r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return;
    }
  }
  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Function arrays                                                            */
/*----------------------------------------------------------------------------*/
static void (*ARCH_DEP(kimd)[KIMD_MAX_FC + 1])(int r1, int r2, REGS *regs) =
{
  ARCH_DEP(kimd_query),
  ARCH_DEP(kimd_sha_1),
  ARCH_DEP(kimd_sha_256)
};

static void (*ARCH_DEP(klmd)[KLMD_MAX_FC + 1])(int r1, int r2, REGS *regs) =
{
  ARCH_DEP(klmd_query),
  ARCH_DEP(klmd_sha_1),
  ARCH_DEP(klmd_sha_256)
};

static void (*ARCH_DEP(km)[KM_MAX_FC + 1])(int r1, int r2, REGS *regs) =
{
  ARCH_DEP(km_query),
  ARCH_DEP(km_dea),
  ARCH_DEP(km_tdea_128),
  ARCH_DEP(km_tdea_192)
};

static void (*ARCH_DEP(kmac)[KMAC_MAX_FC + 1])(int r1, int r2, REGS *regs) =
{
  ARCH_DEP(kmac_query),
  ARCH_DEP(kmac_dea),
  ARCH_DEP(kmac_tdea_128),
  ARCH_DEP(kmac_tdea_192)
};

static void (*ARCH_DEP(kmc)[KMC_MAX_FC + 1])(int r1, int r2, REGS *regs) =
{
  ARCH_DEP(kmc_query),
  ARCH_DEP(kmc_dea),
  ARCH_DEP(kmc_tdea_128),
  ARCH_DEP(kmc_tdea_192)
};

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC)                            */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KMAC_DEBUG
  logmsg("KMAC: compute message authentication code\n");
  logmsg("  r2        : GR%02d\n", r2);
  logmsg("    address : " F_VADR "\n", regs->GR(r2));
  logmsg("    length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg("  GR00      : " F_GREG "\n", regs->GR(0));
  logmsg("    bit 56  : %s\n", TRUEFALSE(GR0_m(regs)));
  logmsg("    fc      : %d\n", GR0_fc(regs));
  logmsg("  GR01      : " F_GREG "\n", regs->GR(1));
#endif

  /* Check for valid function code */
  if(GR0_fc(regs) > KMAC_MAX_FC)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Go to the requested function */
  ARCH_DEP(kmac)[GR0_fc(regs)](r1, r2, regs);
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM)                                                   */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KM_DEBUG
  logmsg("KM: cipher message\n");
  logmsg("  r1        : GR%02d\n", r1);
  logmsg("    address : " F_VADR "\n", regs->GR(r1));
  logmsg("  r2        : GR%02d\n", r2);
  logmsg("    address : " F_VADR "\n", regs->GR(r2));
  logmsg("    length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg("  GR00      : " F_GREG "\n", regs->GR(0));
  logmsg("    m       : %s\n", TRUEFALSE(GR0_m(regs)));
  logmsg("    fc      : %d\n", GR0_fc(regs));
  logmsg("  GR01      : " F_GREG "\n", regs->GR(1));
#endif

  /* Check valid function code */
  if(GR0_fc(regs) > KM_MAX_FC)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Go to the requested function */
  ARCH_DEP(km)[GR0_fc(regs)](r1, r2, regs);
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC)                                    */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_chaining_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KMC_DEBUG
  logmsg("KMC: cipher message with chaining\n");
  logmsg("  r1        : GR%02d\n", r1);
  logmsg("    address : " F_VADR "\n", regs->GR(r1));
  logmsg("  r2        : GR%02d\n", r2);
  logmsg("    address : " F_VADR "\n", regs->GR(r2));
  logmsg("    length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg("  GR00      : " F_GREG "\n", regs->GR(0));
  logmsg("    m       : %s\n", TRUEFALSE(GR0_m(regs)));
  logmsg("    fc      : %d\n", GR0_fc(regs));
  logmsg("  GR01      : " F_GREG "\n", regs->GR(1));
#endif

  /* Check for valid function code */
  if(GR0_fc(regs) > KMC_MAX_FC)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Go to the requested function */
  ARCH_DEP(kmc)[GR0_fc(regs)](r1, r2, regs);
}

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD)                            */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KIMD_DEBUG
  logmsg("KIMD: compute intermediate message digest\n");
  logmsg("  r1        : GR%02d\n", r1);
  logmsg("    address : " F_VADR "\n", regs->GR(r1));
  logmsg("  r2        : GR%02d\n", r2);
  logmsg("    address : " F_VADR "\n", regs->GR(r2));
  logmsg("    length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg("  GR00      : " F_GREG "\n", regs->GR(0));
  logmsg("    bit 56  : %s\n", TRUEFALSE(GR0_m(regs)));
  logmsg("    fc      : %d\n", GR0_fc(regs));
  logmsg("  GR01      : " F_GREG "\n", regs->GR(1));
#endif

  /* Check for valid function code */
  if(GR0_fc(regs) > KIMD_MAX_FC)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Go to the requested function */
  ARCH_DEP(kimd)[GR0_fc(regs)](r1, r2, regs);
}

/*----------------------------------------------------------------------------*/
/* B93F Compute last message digest (KLMD)                                    */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KLMD_DEBUG
  logmsg("KLMD: compute last message digest\n");
  logmsg("  r1        : GR%02d\n", r1);
  logmsg("    address : " F_VADR "\n", regs->GR(r1));
  logmsg("  r2        : GR%02d\n", r2);
  logmsg("    address : " F_VADR "\n", regs->GR(r2));
  logmsg("    length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg("  GR00      : " F_GREG "\n", regs->GR(0));
  logmsg("    bit 56  : %s\n", TRUEFALSE(GR0_m(regs)));
  logmsg("    fc      : %d\n", GR0_fc(regs));
  logmsg("  GR01      : " F_GREG "\n", regs->GR(1));
#endif

  /* Check for valid function code */
  if(GR0_fc(regs) > KLMD_MAX_FC)
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Go to the requested function */
  ARCH_DEP(klmd)[GR0_fc(regs)](r1, r2, regs);
}

#endif /*defined(FEATURE_MESSAGE_SECURITY_ASSIST)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "dyncrypt.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "dyncrypt.c"
#endif

HDL_DEPENDENCY_SECTION;
{
   HDL_DEPENDENCY (HERCULES);
   HDL_DEPENDENCY (REGS);
   HDL_DEPENDENCY (DEVBLK);
// HDL_DEPENDENCY (SYSBLK);
// HDL_DEPENDENCY (WEBBLK);
}
END_DEPENDENCY_SECTION;

HDL_REGISTER_SECTION;
{
#if defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)
  HDL_REGISTER(s390_cipher_message, s390_cipher_message_d);
  HDL_REGISTER(s390_cipher_message_with_chaining, s390_cipher_message_with_chaining_d);
  HDL_REGISTER(s390_compute_intermediate_message_digest, s390_compute_intermediate_message_digest_d);
  HDL_REGISTER(s390_compute_last_message_digest, s390_compute_last_message_digest_d);
  HDL_REGISTER(s390_compute_message_authentication_code, s390_compute_message_authentication_code_d);
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/

#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
  HDL_REGISTER(z900_cipher_message, z900_cipher_message_d);
  HDL_REGISTER(z900_cipher_message_with_chaining, z900_cipher_message_with_chaining_d);
  HDL_REGISTER(z900_compute_intermediate_message_digest, z900_compute_intermediate_message_digest_d);
  HDL_REGISTER(z900_compute_last_message_digest, z900_compute_last_message_digest_d);
  HDL_REGISTER(z900_compute_message_authentication_code, z900_compute_message_authentication_code_d);
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/

}
END_REGISTER_SECTION;

#endif /*!defined(_GEN_ARCH)*/
