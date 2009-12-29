/*----------------------------------------------------------------------------*/
/* file: dyncrypt.c                                                           */
/*                                                                            */
/* Implementation of the z/Architecture crypto instructions described in      */
/* SA22-7832-04: z/Architecture Principles of Operation within the Hercules   */
/* z/Architecture emulator.                                                   */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2003-2010 */
/*                              Noordwijkerhout, The Netherlands.             */
/*----------------------------------------------------------------------------*/

// $Id$

#include "hstdinc.h"

#ifndef _DYNCRYPT_C_
#define _DYNCRYPT_C_
#endif 

#ifndef _DYNCRYPT_DLL_
#define _DYNCRYPT_DLL_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "aes.h"
#include "des.h"
#include "sha1.h"
#include "sha256.h"

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST

/*----------------------------------------------------------------------------*/
/* Debugging options                                                          */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_KxMD_DEBUG
#define OPTION_KM_DEBUG
#define OPTION_KMAC_DEBUG
#define OPTION_KMC_DEBUG
#endif

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* fc   : Function code                                                       */
/* m    : Modifier bit                                                        */
/*----------------------------------------------------------------------------*/
#define GR0_fc(regs)    ((regs)->GR_L(0) & 0x0000007F)
#define GR0_m(regs)     (((regs)->GR_L(0) & 0x00000080) ? TRUE : FALSE)

/*----------------------------------------------------------------------------*/
/* Bit strings for query functions                                            */
/*----------------------------------------------------------------------------*/
#undef KxMD_BITS
#undef KM_BITS
#undef KMAC_BITS
#undef KMC_BITS

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  #define KxMD_BITS     { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    #define KxMD_BITS   { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #define KxMD_BITS   { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #endif
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  #define KM_BITS       { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    #define KM_BITS     { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #define KM_BITS     { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #endif
#endif

#define KMAC_BITS       { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  #define KMC_BITS      { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    #define KMC_BITS    { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #define KMC_BITS    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #endif
#endif

/*----------------------------------------------------------------------------*/
/* Write bytes on one line                                                    */
/*----------------------------------------------------------------------------*/
#define LOGBYTE(s, v, x) \
{ \
  int i; \
  \
  logmsg("  " s " "); \
  for(i = 0; i < (x); i++) \
    logmsg("%02X", (v)[i]); \
  logmsg(" | "); \
  for(i = 0; i < (x); i++) \
  { \
    if(isprint(guest_to_host((v)[i]))) \
      logmsg("%c", guest_to_host((v)[i])); \
    else \
      logmsg("."); \
  } \
  logmsg(" |\n"); \
}

/*----------------------------------------------------------------------------*/
/* Write bytes on multiple lines                                              */
/*----------------------------------------------------------------------------*/
#define LOGBYTE2(s, v, x, y) \
{ \
  int i; \
  int j; \
  \
  logmsg("  " s "\n"); \
  for(i = 0; i < (y); i++) \
  { \
    logmsg("      "); \
    for(j = 0; j < (x); j++) \
      logmsg("%02X", (v)[i * (x) + j]); \
    logmsg(" | "); \
    for(j = 0; j < (x); j++) \
    { \
      if(isprint(guest_to_host((v)[i * (x) + j]))) \
        logmsg("%c", guest_to_host((v)[i * (x) + j])); \
      else \
        logmsg("."); \
    } \
    logmsg(" |\n"); \
  } \
}

/*----------------------------------------------------------------------------*/
/* CPU determined amount of data (processed in one go)                        */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX        16384

/*----------------------------------------------------------------------------*/
/* Used for printing debugging info                                           */
/*----------------------------------------------------------------------------*/
#define TRUEFALSE(boolean)  ((boolean) ? "True" : "False")

#ifndef __SHA1_COMPILE__
#define __SHA1_COMPILE__
/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha1_getcv(sha1_context *ctx, BYTE icv[20])
{
  int i, j;

  for(i = 0, j = 0; i < 5; i++)
  { 
    icv[j++] = (ctx->state[i] & 0xff000000) >> 24;
    icv[j++] = (ctx->state[i] & 0x00ff0000) >> 16;
    icv[j++] = (ctx->state[i] & 0x0000ff00) >> 8;
    icv[j++] = (ctx->state[i] & 0x000000ff);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha1_seticv(sha1_context *ctx, BYTE icv[20])
{
  int i, j;

  for(i = 0, j = 0; i < 5; i++)
  {
    ctx->state[i] = icv[j++] << 24;
    ctx->state[i] |= icv[j++] << 16;
    ctx->state[i] |= icv[j++] << 8;
    ctx->state[i] |= icv[j++];
  }
}
#endif /* __SHA1_COMPILE__ */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
#ifndef __SHA256_COMPILE__
#define __SHA256_COMPILE__
/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha256_getcv(sha256_context *ctx, BYTE icv[32])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    icv[j++] = (ctx->state[i] & 0xff000000) >> 24;
    icv[j++] = (ctx->state[i] & 0x00ff0000) >> 16;
    icv[j++] = (ctx->state[i] & 0x0000ff00) >> 8;
    icv[j++] = (ctx->state[i] & 0x000000ff);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha256_seticv(sha256_context *ctx, BYTE icv[32])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    ctx->state[i] = icv[j++] << 24;
    ctx->state[i] |= icv[j++] << 16;
    ctx->state[i] |= icv[j++] << 8;
    ctx->state[i] |= icv[j++];
  }
}
#endif /* __SHA256_COMPILE__ */
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
#ifndef __SHA512_COMPILE__
#define __SHA512_COMPILE__
/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha512_getcv(sha512_context *ctx, BYTE icv[64])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    icv[j++] = (ctx->state[i] & 0xff00000000000000LL) >> 56;
    icv[j++] = (ctx->state[i] & 0x00ff000000000000LL) >> 48;
    icv[j++] = (ctx->state[i] & 0x0000ff0000000000LL) >> 40;
    icv[j++] = (ctx->state[i] & 0x000000ff00000000LL) >> 32;
    icv[j++] = (ctx->state[i] & 0x00000000ff000000LL) >> 24;
    icv[j++] = (ctx->state[i] & 0x0000000000ff0000LL) >> 16;
    icv[j++] = (ctx->state[i] & 0x000000000000ff00LL) >> 8;
    icv[j++] = (ctx->state[i] & 0x00000000000000ffLL);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha512_seticv(sha512_context *ctx, BYTE icv[64])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    ctx->state[i] = (U64) icv[j++] << 56;
    ctx->state[i] |= (U64) icv[j++] << 48;
    ctx->state[i] |= (U64) icv[j++] << 40;
    ctx->state[i] |= (U64) icv[j++] << 32;
    ctx->state[i] |= (U64) icv[j++] << 24;
    ctx->state[i] |= (U64) icv[j++] << 16;
    ctx->state[i] |= (U64) icv[j++] << 8;
    ctx->state[i] |= (U64) icv[j++];
  }
}
#endif /* __SHA512_COMPILE__ */
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

/*----------------------------------------------------------------------------*/
/* Needed functions from sha1.c and sha256.c.                                 */
/* We do our own counting and padding, we only need the hashing.              */
/*----------------------------------------------------------------------------*/
void sha1_process(sha1_context *ctx, BYTE data[64]);

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
void sha256_process(sha256_context *ctx, BYTE data[64]);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
void sha512_process(sha512_context *ctx, BYTE data[128]);
#endif

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD) FC 1, 2 and 3              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_sha)(int r1, int r2, REGS *regs, int klmd)
{
  sha1_context sha1_ctx;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  sha256_context sha256_ctx;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  sha512_context sha512_ctx;
#endif

  int crypted;
  int fc;
  BYTE message_block[128];
  int message_blocklen = 0;
  BYTE parameter_block[64];
  int parameter_blocklen = 0;

  UNREFERENCED(r1);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 1:
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2:
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3:
      message_blocklen = 128;
      parameter_blocklen = 64;
#endif

  }

  /* Check special conditions */
  if(unlikely(!klmd && (GR_A(r2 + 1, regs) % message_blocklen)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KxMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("icv   :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("icv   :", parameter_block, parameter_blocklen);
  }
#endif

  /* Set initial chaining value */
  switch(fc)
  {
    case 1:
      sha1_seticv(&sha1_ctx, parameter_block);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2:
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3:
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
#endif

  }

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += message_blocklen)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(message_block, message_blocklen - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KxMD_DEBUG
    LOGBYTE2("input :", message_block, 16, message_blocklen / 16);
#endif

    switch(fc)
    {
      case 1:
        sha1_process(&sha1_ctx, message_block);
        sha1_getcv(&sha1_ctx, parameter_block);
        break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2:
        sha256_process(&sha256_ctx, message_block);
        sha256_getcv(&sha256_ctx, parameter_block);
        break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3:
        sha512_process(&sha512_ctx, message_block);
        sha512_getcv(&sha512_ctx, parameter_block);
        break;
#endif

    }

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KxMD_DEBUG
    if(parameter_blocklen > 32)
    {
      LOGBYTE2("ocv   :", parameter_block, 16, parameter_blocklen / 16);
    }
    else
    {
      LOGBYTE("ocv   :", parameter_block, parameter_blocklen);
    }
#endif

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + message_blocklen);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - message_blocklen);

#ifdef OPTION_KxMD_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(GR_A(r2 + 1, regs) < 64))
    {
      if(unlikely(klmd))
        return;
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B93F Compute last message digest (KLMD) FC 1, 2 and 3                      */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(klmd_sha)(int r1, int r2, REGS *regs)
{
  sha1_context sha1_ctx;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  sha256_context sha256_ctx;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  sha512_context sha512_ctx;
#endif

  int fc;
  int i;
  int mbllen = 0;
  BYTE message_block[128];
  int message_blocklen = 0;
  BYTE parameter_block[80];
  int parameter_blocklen = 0;

  UNREFERENCED(r1);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 1:
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2:
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3:
      mbllen = 16;
      message_blocklen = 128;
      parameter_blocklen = 64;
#endif

  }

  /* Process intermediate message blocks */
  if(unlikely(GR_A(r2 + 1, regs) >= (unsigned) message_blocklen))
  {
    ARCH_DEP(kimd_sha)(r1, r2, regs, 1);
    if(regs->psw.cc == 3)
      return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen + mbllen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KxMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("icv   :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("icv   :", parameter_block, parameter_blocklen);
  }
  LOGBYTE("mbl   :", &parameter_block[parameter_blocklen], mbllen);
#endif

  /* Set initial chaining value */
  switch(fc)
  {
    case 1:
      sha1_seticv(&sha1_ctx, parameter_block);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2:
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3:
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
#endif

  }

  /* Fetch and process possible last block of data */
  if(likely(GR_A(r2 + 1, regs)))
  {
    ARCH_DEP(vfetchc)(message_block, GR_A(r2 + 1, regs) - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KxMD_DEBUG
    if(GR_A(r2 + 1, regs) > 32)
    {
      LOGBYTE("input :", message_block, 32);
      LOGBYTE("       ", &message_block[32], (int) GR_A(r2 + 1, regs) - 32);
    }
    else
      LOGBYTE("input :", message_block, (int) GR_A(r2 + 1, regs));
#endif

  }

  /* Do the padding */
  i = GR_A(r2 + 1, regs);
  if(unlikely(i >= (message_blocklen - mbllen)))
  {
    message_block[i++] = 0x80;
    while(i < message_blocklen)
      message_block[i++] = 0x00;
    switch(fc)
    {
      case 1:
        sha1_process(&sha1_ctx, message_block);
        break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2:
        sha256_process(&sha256_ctx, message_block);
        break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3:
        sha512_process(&sha512_ctx, message_block);
        break;
#endif

    }
    for(i = 0; i < message_blocklen - mbllen; i++)
      message_block[i] = 0x00;
  }
  else
  {
    message_block[i++] = 0x80;
    while(i < message_blocklen - mbllen)
      message_block[i++] = 0x00;
  }

  /* Set the message bit length */
  memcpy(&message_block[message_blocklen - mbllen], &parameter_block[parameter_blocklen], mbllen);

  /* Calculate and store the message digest */
  switch(fc)
  {
    case 1:
      sha1_process(&sha1_ctx, message_block);
      sha1_getcv(&sha1_ctx, parameter_block);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2:
      sha256_process(&sha256_ctx, message_block);
      sha256_getcv(&sha256_ctx, parameter_block);
      break;      
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3:
      sha512_process(&sha512_ctx, message_block);
      sha512_getcv(&sha512_ctx, parameter_block);
      break;
#endif

  }
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KxMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("md    :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("md    :", parameter_block, parameter_blocklen);
  }
#endif

  /* Update registers */
  SET_GR_A(r2, regs, GR_A(r2, regs) + GR_A(r2 + 1, regs));
  SET_GR_A(r2 + 1, regs, 0);

#ifdef OPTION_KxMD_DEBUG
  logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
  logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

  /* Set condition code */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 1, 2 and 3                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_dea)(int r1, int r2, REGS *regs)
{
  des_context context;
  des3_context des3_ctx;
  int crypted;
  int fc;
  BYTE message_block[8];
  int modifier_bit;
  BYTE parameter_block[24];
  int parameter_blocklen;
  int r1_is_not_r2;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  fc = GR0_fc(regs);
  parameter_blocklen = fc * 8;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  switch(fc)
  {
    case 1:
      LOGBYTE("k     :", parameter_block, 8);
      break;

    case 2:
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      break;

    case 3:
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      LOGBYTE("k3    :", &parameter_block[16], 8);
      break;
  }
#endif

  /* Set the cryptographic key */
  switch(fc)
  {
    case 1:
      des_set_key(&context, parameter_block);
      break;
   
    case 2:
      des3_set_2keys(&des3_ctx, parameter_block, &parameter_block[8]);
      break;

    case 3:
      des3_set_3keys(&des3_ctx, parameter_block, &parameter_block[8], &parameter_block[16]);
      break;
  }   

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif

    /* Do the job */
    switch(fc)
    {
      case 1:
        if(modifier_bit)
          des_decrypt(&context, message_block, message_block); 
        else
          des_encrypt(&context, message_block, message_block);
        break;

      case 2:
        if(modifier_bit)
          des3_decrypt(&des3_ctx, message_block, message_block);
        else
          des3_encrypt(&des3_ctx, message_block, message_block);
        break;

      case 3:
        if(modifier_bit)
          des3_decrypt(&des3_ctx, message_block, message_block);
        else
          des3_encrypt(&des3_ctx, message_block, message_block);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KM_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 18, 19 and 20                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int fc;
  BYTE message_block[16];
  int modifier_bit;
  BYTE parameter_block[16];
  int parameter_blocklen;
  int r1_is_not_r2;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  fc = GR0_fc(regs) - 17;
  parameter_blocklen = fc * 8 + 8;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", parameter_block, parameter_blocklen);
#endif

  /* Set the cryptographic keys */
  aes_set_key(&context, parameter_block, 64 * (fc + 1));

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif

    /* Do the job */
    if(modifier_bit)
      aes_decrypt(&context, message_block, message_block);
    else
      aes_encrypt(&context, message_block, message_block);

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KM_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

/*----------------------------------------------------------------------------*/
/* B91E KMAC  - Compute message authentication code                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code_d)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int fc;
  int i;
  BYTE message_block[8];
  BYTE parameter_block[32];
  int parameter_blocklen;
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

  UNREFERENCED(r1);

  /* Initialize values */
  fc = GR0_fc(regs);
  parameter_blocklen = fc * 8 + 8;

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Check for query function */
  if(!fc)
  {
    BYTE parameter_block[16] = KMAC_BITS;

    /* Store the parameter block */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

    /* Set condition code 0 */
    regs->psw.cc = 0;
    return;
  }

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(fc)
  {
    case 1: /* dea */
      LOGBYTE("k1    :", &parameter_block[8], 8);
      break;
    
    case 2: /* tdea-128 */
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    
    case 3: /* tdea-192 */
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
  }
#endif

  /* Set the cryptographic key */
  switch(fc)
  {
    case 1: /* dea */
      des_set_key(&context1, &parameter_block[8]);
      break;
    
    case 2: /* tdea-128 */
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    
    case 3: /* tdea-192 */
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
  }

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif

    /* XOR the message with chaining value */
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    /* Calculate the output chaining value */
    switch(fc)
    {
      case 1: /* dea */
        des_encrypt(&context1, message_block, parameter_block);
        break;
      
      case 2: /* tdea-128 */
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      
      case 3: /* tdea-192 */
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context3, parameter_block, parameter_block);
        break;
    }

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", parameter_block, 8);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMAC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 1, 2 and 3                      */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int fc;
  int i;
  BYTE message_block[8];
  int modifier_bit;
  BYTE ocv[8];
  BYTE parameter_block[16];
  int parameter_blocklen;
  int r1_is_not_r2;

#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 1: dea\n");
#endif

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  fc = GR0_fc(regs);
  parameter_blocklen = fc * 8 + 8;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  switch(fc)
  {
    case 1:
      LOGBYTE("icv   :", parameter_block, 8);
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;

    case 2:
      LOGBYTE("icv   :", parameter_block, 8);
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;

    case 3:
      LOGBYTE("icv   :", parameter_block, 8);
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
  }
#endif

  /* Set the cryptographic key */
  switch(fc)
  {
    case 1:
      des_set_key(&context1, &parameter_block[8]);
      break;

    case 2:
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;

    case 3:
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
  }

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif

    /* Do the job */
    switch(fc)
    {
      case 1:
        if(modifier_bit)
        {
          /* Save the ocv */
          memcpy(ocv, message_block, 8);

          /* Decrypt and XOR */
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR and encrypt */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);

          /* Save the ocv */
          memcpy(ocv, message_block, 8);
        }
        break;

      case 2:
        if(modifier_bit)
        {
          /* Save the ocv */
          memcpy(ocv, message_block, 8);

          /* Decrypt and XOR */
          des_decrypt(&context1, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR and encrypt */
          for(i = 0 ; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context1, message_block, message_block);

          /* Save the ocv */
          memcpy(ocv, message_block, 8);
        }
        break;

      case 3:
        if(modifier_bit)
        {
          /* Save the ocv */
          memcpy(ocv, message_block, 8);

          /* Decrypt and XOR */
          des_decrypt(&context3, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR and encrypt */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context3, message_block, message_block);

          /* Save the ocv */
          memcpy(ocv, message_block, 8);
        }
        break;
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 8 bytes */
    memcpy(parameter_block, ocv, 8);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 18, 19 and 20                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int fc;
  int i;
  BYTE message_block[16];
  int modifier_bit;
  BYTE ocv[16];
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  fc = GR0_fc(regs) - 17;
  parameter_blocklen = fc * 8 + 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], parameter_blocklen - 16);
#endif

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], 64 * (fc + 1));

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif

    /* Do the job */
    if(modifier_bit)
    {

      /* Save the ovc */
      memcpy(ocv, message_block, 16);

      /* Decrypt and XOR */
      aes_decrypt(&context, message_block, message_block);
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
    }
    else
    {
      /* XOR and encrypt */
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
      aes_encrypt(&context, message_block, message_block);

      /* Save the ovc */
      memcpy(ocv, message_block, 16);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 16);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 16 bytes */
    memcpy(parameter_block, ocv, 16);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 67                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_prng)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int i;
  BYTE message_block[8];
  BYTE parameter_block[32];
  BYTE ocv[8];
  BYTE tcv[8];
  int r1_is_not_r2;

#ifdef OPTION_KMC_DEBUG
  logmsg("  KMC: function 67: prng\n");
#endif

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  LOGBYTE("k1    :", &parameter_block[8], 8);
  LOGBYTE("k2    :", &parameter_block[16], 8);
  LOGBYTE("k3    :", &parameter_block[24], 8);
#endif

  /* Set the cryptographic keys */
  des_set_key(&context1, &parameter_block[8]);
  des_set_key(&context2, &parameter_block[16]);
  des_set_key(&context3, &parameter_block[24]);

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif

    /* Do the job */
    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Save the temporary cv */
    memcpy(tcv, message_block, 8);

    /* XOR */
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif

    /* XOR */
    for(i = 0; i < 8; i++)
      message_block[i] ^= tcv[i];

    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Save the ocv */
    memcpy(ocv, message_block, 8);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    logmsg("  GR%02d  : " F_GREG "\n", r1, (regs)->GR(r1));
    logmsg("  GR%02d  : " F_GREG "\n", r2, (regs)->GR(r2));
    logmsg("  GR%02d  : " F_GREG "\n", r2 + 1, (regs)->GR(r2 + 1));
#endif

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 8 bytes */
    memcpy(parameter_block, ocv, 8);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

/*----------------------------------------------------------------------------*/
/* B93E/B93F KIMD/KLMD  - Compute intermediate/last message digest      [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_digest_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KxMD_DEBUG
  if(inst[1] == 0x3e)
    logmsg("KIMD: compute intermediate message digest\n");
  else
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

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KxMD_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KxMD_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* sha-1 */
      if(likely(inst[1] == 0x3e))
        ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      else
        ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
      if(likely(inst[1] == 0x3e))
        ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      else
        ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
      if(likely(inst[1] == 0x3e))
        ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      else
        ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
#endif

    default:
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
  }
}

/*----------------------------------------------------------------------------*/
/* B92E KM    - Cipher message                                          [RRE] */
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

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KM_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
      ARCH_DEP(km_dea)(r1, r2, regs);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
      ARCH_DEP(km_aes)(r1, r2, regs);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
      ARCH_DEP(km_aes)(r1, r2, regs);
      break;
#endif

    default:
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
  }
}

/*----------------------------------------------------------------------------*/
/* B92F KMC   - Cipher message with chaining                            [RRE] */
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

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KMC_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
      ARCH_DEP(kmc_dea)(r1, r2, regs);
      break;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
      ARCH_DEP(kmc_aes)(r1, r2, regs);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
      ARCH_DEP(kmc_aes)(r1, r2, regs);
      break;
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 67:
      ARCH_DEP(kmc_prng)(r1, r2, regs);
      break;
#endif

    default:
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
  }
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
  HDL_REGISTER(s390_compute_message_digest, s390_compute_message_digest_d);
  HDL_REGISTER(s390_compute_message_authentication_code, s390_compute_message_authentication_code_d);
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/

#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
  HDL_REGISTER(z900_cipher_message, z900_cipher_message_d);
  HDL_REGISTER(z900_cipher_message_with_chaining, z900_cipher_message_with_chaining_d);
  HDL_REGISTER(z900_compute_message_digest, z900_compute_message_digest_d);
  HDL_REGISTER(z900_compute_message_authentication_code, z900_compute_message_authentication_code_d);
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/

  logmsg("Crypto module loaded (c) Copyright Bernard van der Helm, 2003-2010\n");
  logmsg("  Active: Message Security Assist\n");
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  logmsg("          Message Security Assist Extension 1\n");
#endif
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  logmsg("          Message Security Assist Extension 2\n");
#endif

}
END_REGISTER_SECTION;

#endif /*!defined(_GEN_ARCH)*/
