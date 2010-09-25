/* DYNCRYPT.C   (c) Bernard van der Helm, 2003-2010                  */
/*              z/Architecture crypto instructions                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*----------------------------------------------------------------------------*/
/*                                                                            */
/* Implementation of the z/Architecture crypto instructions described in      */
/* SA22-7832-04: z/Architecture Principles of Operation within the Hercules   */
/* z/Architecture emulator.                                                   */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2003-2010 */
/*                              Noordwijkerhout, The Netherlands.             */
/*----------------------------------------------------------------------------*/

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

#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST)
  #error You cannot have "Message Security Assist extension 1" without having "Message Security Assist"
#endif
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1)
  #error You cannot have "Message Security Assist extension 2" without having "Message Security Assist extension 1"
#endif
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2)
  #error You cannot have "Message Security Assist extension 3" without having "Message Security Assist extension 2"
#endif
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
  #error You cannot have "Message Security Assist extension 4" without having "Message Security Assist extension 3"
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST

/*----------------------------------------------------------------------------*/
/* Debugging options                                                          */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_KIMD_DEBUG
#define OPTION_KLMD_DEBUG
#define OPTION_KM_DEBUG
#define OPTION_KMAC_DEBUG
#define OPTION_KMC_DEBUG
#define OPTION_KMCTR_DEBUG
#define OPTION_KMF_DEBUG
#define OPTION_KMO_DEBUG
#define OPTION_PCC_DEBUG
#define OPTION_PCKMO_DEBUG
#endif

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* fc   : Function code                                                       */
/* m    : Modifier bit                                                        */
/* lcfb : Length of cipher feedback                                           */
/* wrap : Indication if key is wrapped                                        */
/* tfc  : Function code without wrap indication                               */
/*----------------------------------------------------------------------------*/
#define GR0_fc(regs)    ((regs)->GR_L(0) & 0x0000007F)
#define GR0_m(regs)     (((regs)->GR_L(0) & 0x00000080) ? TRUE : FALSE)
#define GR0_lcfb(regs)  ((regs)->GR_L(0) >> 24)
#define GR0_wrap(egs)   (((regs)->GR_L(0) & 0x08) ? TRUE : FALSE)
#define GR0_tfc(regs)   (GR0_fc(regs) & 0x77)

/*----------------------------------------------------------------------------*/
/* Bit strings for query functions                                            */
/*----------------------------------------------------------------------------*/
#undef KIMD_BITS
#undef KLMD_BITS
#undef KM_BITS
#undef KMAC_BITS
#undef KMC_BITS
#undef KMCTR_BITS
#undef KMF_BITS
#undef KMO_BITS
#undef PCC_BITS
#undef PCKMO_BITS

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  #define KIMD_BITS     { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    #define KIMD_BITS   { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      #define KIMD_BITS { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    #else
      #define KIMD_BITS { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    #endif
  #endif
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  #define KLMD_BITS     { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    #define KLMD_BITS   { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #define KLMD_BITS   { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #endif
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  #define KM_BITS       { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    #define KM_BITS     { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      #define KM_BITS   { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    #else
      #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
        #define KM_BITS { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
      #else
        #define KM_BITS { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
      #endif
    #endif
  #endif
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  #define KMAC_BITS     { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    #define KMAC_BITS   { 0xf0, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #define KMAC_BITS   { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #endif
#endif
  
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  #define KMC_BITS      { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    #define KMC_BITS    { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #else
    #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      #define KMC_BITS  { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    #else
      #define KMC_BITS  { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    #endif
  #endif
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  #define PCKMO_BITS    { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  #define KMCTR_BITS    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #define KMF_BITS      { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #define KMO_BITS      { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  #define PCC_BITS      { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#endif

/*----------------------------------------------------------------------------*/
/* Write bytes on one line                                                    */
/*----------------------------------------------------------------------------*/
#define LOGBYTE(s, v, x) \
{ \
  char buf[128]; \
  int i; \
  \
  buf[0] = 0; \
  for(i = 0; i < (x); i++) \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", (v)[i]); \
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " | "); \
  for(i = 0; i < (x); i++) \
  { \
    if(isprint(guest_to_host((v)[i]))) \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", guest_to_host((v)[i])); \
    else \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "."); \
  } \
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " |"); \
  WRMSG(HHC90109, "D", s, buf); \
}

/*----------------------------------------------------------------------------*/
/* Write bytes on multiple lines                                              */
/*----------------------------------------------------------------------------*/
#define LOGBYTE2(s, v, x, y) \
{ \
  char buf[128]; \
  int i; \
  int j; \
  \
  buf[0] = 0; \
  WRGMSG_ON; \
  WRGMSG(HHC90109, "D", s, ""); \
  for(i = 0; i < (y); i++) \
  { \
    for(j = 0; j < (x); j++) \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", (v)[i * (x) + j]); \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " | "); \
    for(j = 0; j < (x); j++) \
    { \
      if(isprint(guest_to_host((v)[i * (x) + j]))) \
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", guest_to_host((v)[i * (x) + j])); \
      else \
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "."); \
    } \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " |"); \
    WRGMSG(HHC90110, "D", buf); \
  } \
  WRGMSG_OFF; \
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

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
#ifndef __GF_COMPILE__
#define __GF_COMPILE__
/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@..., http://libtomcrypt.org
*/

/* Remarks Bernard van der Helm: Strongly adjusted for
 * Hercules-390. We need the internal function gcm_gf_mult.
 * The rest of of the code is deleted.
 *
 * Thanks Tom!
*/

/* Hercules adjustments */
#define zeromem(dst, len)    memset((dst), 0, (len))
#define GF(X, Y, Z)          gcm_gf_mult((X), (Y), (Z))
#define XMEMCPY              memcpy

/* Original code from gcm_gf_mult.c */
/* right shift */
static void gcm_rightshift(unsigned char *a)
{
  int x;
  
  for(x = 15; x > 0; x--) 
    a[x] = (a[x] >> 1) | ((a[x-1] << 7) & 0x80);
  a[0] >>= 1;
}

/* c = b*a */
static const unsigned char mask[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
static const unsigned char poly[] = { 0x00, 0xE1 };

void gcm_gf_mult(const unsigned char *a, const unsigned char *b, unsigned char *c)
{
  unsigned char Z[16], V[16];
  unsigned x, y, z;

  zeromem(Z, 16);
  XMEMCPY(V, a, 16);
  for (x = 0; x < 128; x++) 
  {
    if(b[x>>3] & mask[x&7]) 
    {
      for(y = 0; y < 16; y++) 
        Z[y] ^= V[y];
    }
    z = V[15] & 0x01;
    gcm_rightshift(V);
    V[0] ^= poly[z];
  }
  XMEMCPY(c, Z, 16);
}
#endif /* __GF_COMPILE__ */
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

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

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/* Wrapping-key registers */
/* Each time a clear reset is performed, a new set of wrapping keys and */
/* their associated verification patterns are generated. */

#ifndef __KEY_WRAP__
#define __KEY_WRAP__
/*----------------------------------------------------------------------------*/
/* Machine generated Wrapping-Key Registers                                   */
/*----------------------------------------------------------------------------*/
/* For now they are fixed */   /*01234567890123456789012345678901*/
static BYTE wk_regs_aes[32] =   "W#$%Y%NY$^HNNGFdghk57MGF%^hmghf4"; 
static BYTE wk_regs_dea[24] =   "TRHsdgh%&%^GFNTUK%&GFNf(";        

/*----------------------------------------------------------------------------*/
/* Wrapping-Key Verification-Pattern Registers                                */
/*----------------------------------------------------------------------------*/
static BYTE wkvp_regs_aes[32] = "asdfsgtrnFNH57N57%J&5jfnh56w46gf";
static BYTE wkvp_regs_dea[24] = "GFN5u46y%^%&mju%&$%tymty";

/*----------------------------------------------------------------------------*/
/* Wrap key using aes                                                         */
/*----------------------------------------------------------------------------*/
static void wrap_aes(BYTE *key, int keylen)
{
  BYTE buf[16];
  int i;
  aes_context context;
  BYTE cv[16];
  
  aes_set_key(&context, wk_regs_aes, 256);
  switch(keylen)
  {
    case 16:
    {
      aes_encrypt(&context, key, key);
      break;
    } 
    case 24:
    {
      aes_encrypt(&context, key, cv);
      memcpy(buf, &key[16], 8);
      memset(&buf[8], 0, 8);
      for(i = 0; i < 16; i++)
        buf[i] ^= cv[i];
      aes_encrypt(&context, buf, buf);
      memcpy(key, cv, 8);
      memcpy(&key[8], buf, 16);
      break;
    }  
    case 32:
    {
      aes_encrypt(&context, key, key);
      for(i = 0; i < 16; i++)
        key[i + 16] ^= key[i];
      aes_encrypt(&context, &key[16], &key[16]);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* Wrap key using dea                                                         */
/*----------------------------------------------------------------------------*/
static void wrap_dea(BYTE *key, int keylen)
{
  des3_context context;
  int i;
  int j;

  des3_set_3keys(&context, wk_regs_dea, &wk_regs_dea[8], &wk_regs_dea[16]);
  for(i = 0; i < keylen; i += 8)
  {
    if(i)
    {
      /* XOR */
      for(j = 0; j < 8; j++)
        key[i + j] ^= key[i + j - 8];
    }
    des3_encrypt(&context, &key[i], &key[i]);
    des3_decrypt(&context, &key[i], &key[i]);    
    des3_encrypt(&context, &key[i], &key[i]);
  }
}

/*----------------------------------------------------------------------------*/
/* Unwrap key using aes                                                       */
/*----------------------------------------------------------------------------*/
static int unwrap_aes(BYTE *key, int keylen, BYTE *wkvp)
{
  BYTE buf[16];
  aes_context context;
  BYTE cv[16];
  int i;
  
  /* Verify verification pattern */
  if(unlikely(memcmp(wkvp, wkvp_regs_aes, 32)))
    return(1);
  
  aes_set_key(&context, wk_regs_aes, 256);
  switch(keylen)
  {
    case 16:
    {
      aes_decrypt(&context, key, key);
      break;
    }  
    case 24:
    {
      aes_decrypt(&context, &key[8], buf);
      memcpy(&key[8], &buf[8], 8);
      memcpy(cv, key, 8);
      aes_decrypt(&context, key, key);
      for(i = 0; i < 8; i++)
        key[16 + i] = buf[i] ^ cv[i];
      break;
    }  
    case 32:
    {
      memcpy(cv, key, 16);
      aes_decrypt(&context, key, key);
      aes_decrypt(&context, &key[16], &key[16]);
      for(i = 0; i < 16; i++)
        key[i + 16] ^= cv[i];
      break;
    }
  }
  return(0);
}

/*----------------------------------------------------------------------------*/
/* Unwrap key using dea                                                       */
/*----------------------------------------------------------------------------*/
static int unwrap_dea(BYTE *key, int keylen, BYTE *wkvp)
{
  BYTE cv[16];
  des3_context context;
  int i;
  int j;
  
  /* Verify verification pattern */
  if(unlikely(memcmp(wkvp, wkvp_regs_aes, 32)))
    return(1);
  
  des3_set_3keys(&context, wk_regs_dea, &wk_regs_dea[8], &wk_regs_dea[16]);
  for(i = 0; i < keylen; i += 8)
  {
    /* Save cv */
    memcpy(cv, &cv[8], 8);
    memcpy(&cv[8], &key[i], 8);
    
    des3_decrypt(&context, &key[i], &key[i]);
    des3_encrypt(&context, &key[i], &key[i]);    
    des3_decrypt(&context, &key[i], &key[i]);
    if(i)
    {
      /* XOR */
      for(j = 0; j < 8; j++)
        key[i + j] ^= cv[j];
    }
  }
  return(0);
}
#endif /* __KEY_WRAP__ */
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

/*----------------------------------------------------------------------------*/
/* B93E Compute intermediate message digest (KIMD) FC 1-3                     */
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
    case 1: /* sha-1 */
    {
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      message_blocklen = 128;
      parameter_blocklen = 64;
    }
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

#ifdef OPTION_KIMD_DEBUG
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
    case 1: /* sha-1 */
    {
      sha1_seticv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
    }
#endif

  }

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += message_blocklen)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(message_block, message_blocklen - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE2("input :", message_block, 16, message_blocklen / 16);
#endif

    switch(fc)
    {
      case 1: /* sha-1 */
      {
        sha1_process(&sha1_ctx, message_block);
        sha1_getcv(&sha1_ctx, parameter_block);
        break;
      }
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2: /* sha-256 */
      {
        sha256_process(&sha256_ctx, message_block);
        sha256_getcv(&sha256_ctx, parameter_block);
        break;
      }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3: /* sha-512 */
      {
        sha512_process(&sha512_ctx, message_block);
        sha512_getcv(&sha512_ctx, parameter_block);
        break;
      }
#endif

    }

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
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

#ifdef OPTION_KIMD_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B93E Compute intermediate message digest (KIMD) FC 65                      */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_ghash)(int r1, int r2, REGS *regs)
{
  int crypted;
  int i;
  BYTE message_block[16];
  BYTE parameter_block[32];

  UNREFERENCED(r1);

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("h     :", &parameter_block[16], 16);
#endif

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif

    /* XOR and multiply */
    for(i = 0; i < 16; i++)
      parameter_block[i] ^= message_block[i];
    GF(parameter_block, &parameter_block[16], parameter_block);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE("ocv   :", parameter_block, 16);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KIMD_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B93F Compute last message digest (KLMD) FC 1-3                             */
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
    case 1: /* sha-1 */
    {
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      mbllen = 16;
      message_blocklen = 128;
      parameter_blocklen = 64;
    }
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

#ifdef OPTION_KLMD_DEBUG
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
    case 1: /* sha-1 */
    {
      sha1_seticv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
    }
#endif

  }

  /* Fetch and process possible last block of data */
  if(likely(GR_A(r2 + 1, regs)))
  {
    ARCH_DEP(vfetchc)(message_block, GR_A(r2 + 1, regs) - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
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
      case 1: /* sha-1 */
      {
        sha1_process(&sha1_ctx, message_block);
        break;
      }
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2: /* sha-256 */
      {
        sha256_process(&sha256_ctx, message_block);
        break;
      }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3: /* sha-512 */
      {
        sha512_process(&sha512_ctx, message_block);
        break;
      }
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
    case 1: /* sha-1 */
    {
      sha1_process(&sha1_ctx, message_block);
      sha1_getcv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_process(&sha256_ctx, message_block);
      sha256_getcv(&sha256_ctx, parameter_block);
      break;      
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_process(&sha512_ctx, message_block);
      sha512_getcv(&sha512_ctx, parameter_block);
      break;
    }
#endif

  }
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
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

#ifdef OPTION_KLMD_DEBUG
  WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
  WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif

  /* Set condition code */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 1-3 and 9-11                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_dea)(int r1, int r2, REGS *regs)
{
  int crypted;
  des_context des_ctx;
  des3_context des3_ctx;
  int keylen;
  BYTE message_block[8];
  int modifier_bit;
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 24;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", parameter_block, 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      LOGBYTE("k3    :", &parameter_block[16], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen], 24);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(parameter_block, keylen, &parameter_block[keylen]))
  {
    regs->psw.cc = 1;
    return;
  }
#endif  

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&des_ctx, parameter_block);
      break;
    }
    case 2: /* tdea-128 */
    {
      des3_set_2keys(&des3_ctx, parameter_block, &parameter_block[8]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des3_set_3keys(&des3_ctx, parameter_block, &parameter_block[8], &parameter_block[16]);
      break;
    }
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
    switch(tfc)
    {
      case 1: /* dea */
      {
        if(modifier_bit)
          des_decrypt(&des_ctx, message_block, message_block); 
        else
          des_encrypt(&des_ctx, message_block, message_block);
        break;
      }
      case 2: /* tdea-128 */
      case 3: /* tdea-192 */
      {
        if(modifier_bit)
          des3_decrypt(&des3_ctx, message_block, message_block);
        else
          des3_encrypt(&des3_ctx, message_block, message_block);
        break;
      }
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
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B92E Cipher message (KM) FC 18-20 and 26-28                                */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  BYTE message_block[16];
  int modifier_bit;
  BYTE parameter_block[64];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 32;
  
  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", parameter_block, keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen], 32);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_aes(parameter_block, keylen, &parameter_block[keylen]))
  {
    regs->psw.cc = 1;
    return;
  }
#endif  

  /* Set the cryptographic keys */
  aes_set_key(&context, parameter_block, keylen * 8);

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
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", (regs)->GR(r2 + 1));
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

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B92E Cipher message (KM) FC 50, 52, 58 and 60                              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_xts_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  int modifier_bit;
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  BYTE two[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 49) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;
  
  /* Test writeability output chaining value */ 
  ARCH_DEP(validate_operand)((GR_A(1, regs) + parameter_blocklen - 16), 1, 15, ACCTYPE_WRITE, regs);
  
  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", parameter_block, keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen], 32);
  LOGBYTE("xtsp  :", &parameter_block[parameter_blocklen - 16], 32);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_aes(parameter_block, keylen, &parameter_block[keylen]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic keys */
  aes_set_key(&context, parameter_block, keylen * 8);

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

    /* XOR, decrypt/encrypt and XOR again*/
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[parameter_blocklen - 16 + i];
    if(modifier_bit)
      aes_decrypt(&context, message_block, message_block);
    else
      aes_encrypt(&context, message_block, message_block);
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[parameter_blocklen - 16 + i];

    /* Calculate output XTSP */
    GF(&parameter_block[parameter_blocklen - 16], two, &parameter_block[parameter_blocklen - 16]);
    
    /* Store the output and XTSP */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);
    ARCH_DEP(vstorec)(&parameter_block[parameter_blocklen - 16], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 16);
    LOGBYTE("xtsp  :", &parameter_block[parameter_bloklen - 16]);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", (regs)->GR(r2 + 1));
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
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 1-3 and 9-11            */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int tfc;
  int wrap;

  UNREFERENCED(r1);

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen, &parameter_block[keylen + 8]))
  {
    regs->psw.cc = 1;
    return;
  }
#endif

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
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
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, message_block, parameter_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context3, parameter_block, parameter_block);
        break;
      }
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
    WRMSG(HHC90108, "D", (regs)->GR(r2));
    WRMSG(HHC90108, "D", (regs)->GR(r2 + 1));
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

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B91E Compute message authentication code (KMAC) FC 18-20 and 26-28         */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int tfc;
  int wrap;

  UNREFERENCED(r1);

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen, &parameter_block[keylen + 16]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", message_block, 15);
#endif

    /* XOR the message with chaining value */
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[i];

    /* Calculate the output chaining value */
    aes_encrypt(&context, message_block, parameter_block);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", parameter_block, 15);
#endif

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMAC_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* B92F Cipher message with chaining (KMC) FC 1-3 and 9-11                    */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[8];
  int modifier_bit;
  BYTE ocv[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen, &parameter_block[keylen + 8]))
  {
    regs->psw.cc = 1;
    return;
  }
#endif

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
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
    switch(tfc)
    {
      case 1: /* dea */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
      case 2: /* tdea-128 */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context1, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0 ; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context1, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
      case 3: /* tdea-192 */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context3, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context3, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
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
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B92F Cipher message with chaining (KMC) FC 18-20 and 26-28                 */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[16];
  int modifier_bit;
  BYTE ocv[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen, &parameter_block[keylen + 16]))
  {
    regs->psw.cc = 1;
    return;
  }
#endif

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);

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

      /* Save, decrypt and XOR */
      memcpy(ocv, message_block, 16);
      aes_decrypt(&context, message_block, message_block);
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
    }
    else
    {
      /* XOR, encrypt and save */
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
      aes_encrypt(&context, message_block, message_block);
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
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B92D Cipher message with counter (KMCTR) FC 1-3 and 9-11                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmctr_dea)(int r1, int r2, int r3, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  BYTE countervalue_block[8];
  int crypted;
  int i;
  int keylen;
  BYTE message_block[8];
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int r1_is_not_r3;
  int r2_is_not_r3;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 24;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[24], 24);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_dea(parameter_block, keylen, &parameter_block[keylen]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  r1_is_not_r3 = r1 != r3;
  r2_is_not_r3 = r2 != r3;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data and counter-value */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);
    ARCH_DEP(vfetchc)(countervalue_block, 7, GR_A(r3, regs), r3, regs);
    
#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("input :", message_block, 8);
    LOGBYTE("cv    :", countervalue_block, 8);
#endif

    /* Do the job */
    switch(tfc)
    {
      /* Encrypt */
      case 1: /* dea */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        des_decrypt(&context2, countervalue_block, countervalue_block);
        des_encrypt(&context1, countervalue_block, countervalue_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        des_decrypt(&context2, countervalue_block, countervalue_block);
        des_encrypt(&context3, countervalue_block, countervalue_block);
        break;
      }
    }

    /* XOR */
    for(i = 0; i < 8; i++)
      countervalue_block[i] ^= message_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(countervalue_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("output:", countervalue_block, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);
    if(likely(r1_is_not_r3 && r2_is_not_r3))
      SET_GR_A(r3, regs, GR_A(r3, regs) + 8);

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
    WRMSG(HHC90108, "D", r3, (regs)->GR(r3));
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
/* B92D Cipher message with counter (KMCTR) FC 18-20 and 26-28                */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmctr_aes)(int r1, int r2, int r3, REGS *regs)
{
  aes_context context;
  BYTE countervalue_block[16];  
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int r1_is_not_r3;
  int r2_is_not_r3;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 32;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], parameter_blocklen - 16);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif

  if(wrap && unwrap_aes(parameter_block, keylen, &parameter_block[keylen]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  r1_is_not_r3 = r1 != r3;
  r2_is_not_r3 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data and counter-value */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);
    ARCH_DEP(vfetchc)(countervalue_block, 15, GR_A(r3, regs), r3, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("input :", message_block, 16);
    LOGBYTE("cv    :", countervalue_block, 16);
#endif

    /* Do the job */
    /* Encrypt and XOR */
    aes_encrypt(&context, countervalue_block, countervalue_block);
    for(i = 0; i < 16; i++)
      countervalue_block[i] ^= message_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(countervalue_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("output:", countervalue_block, 16);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);
    if(likely(r1_is_not_r3 && r2_is_not_r3))
      SET_GR_A(r3, regs, GR_A(r3, regs) + 16);

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
    WRMSG(HHC90108, "D", r3, (regs)->GR(r3));
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
/* B92D Cipher message with cipher feedback (KMF) FC 1-3 and 9-11             */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmf_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int keylen;
  int i;
  int lcfb;
  BYTE message_block[8];
  int modifier_bit;
  BYTE output_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Initialize values */
  lcfb = GR0_lcfb(regs);
  
  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % lcfb || !lcfb || lcfb > 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
  LOGBYTE("cv    :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen, &parameter_block[keylen + 8]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += lcfb)
  {
    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, parameter_block, output_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, parameter_block, output_block);
        des_decrypt(&context2, output_block, output_block);
        des_encrypt(&context1, output_block, output_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, parameter_block, output_block);
        des_decrypt(&context2, output_block, output_block);
        des_encrypt(&context3, output_block, output_block);
	break;
      }
    }
    ARCH_DEP(vfetchc)(message_block, lcfb - 1, GR_A(r2, regs), r2, regs);
    if(modifier_bit)
    {
      /* Decipher */
      for(i = 0; i < 8 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[8 - lcfb + i] = message_block[i];
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
    }
    else
    {
      /* Encipher */
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
      for(i = 0; i < 8 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[8 - lcfb + i] = output_block[i];
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, lcfb - 1, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("output:", message_block, lcfb);
#endif

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("cv    :", parameter_block, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + lcfb);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + lcfb);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - lcfb);

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B92A Cipher message with cipher feedback (KMF) FC 18-20 and 26-28          */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmf_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  int i;
  int lcfb;
  BYTE message_block[16];
  int modifier_bit;
  BYTE output_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Initialize values */
  lcfb = GR0_lcfb(regs);
  
  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % lcfb || !lcfb || lcfb > 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
  LOGBYTE("cv    :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen, &parameter_block[keylen + 16]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += lcfb)
  {
    aes_encrypt(&context, parameter_block, output_block);
    ARCH_DEP(vfetchc)(message_block, lcfb - 1, GR_A(r2, regs), r2, regs);      
    if(modifier_bit)
    {
      /* Decipher */
      for(i = 0; i < 16 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[16 - lcfb + i] = message_block[i];
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
    }
    else
    {
      /* Encipher */
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
      for(i = 0; i < 16 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[16 - lcfb + i] = output_block[i];
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, lcfb - 1, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("output:", message_block, lcfb);
#endif

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("cv    :", parameter_block, 16);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + lcfb);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + lcfb);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - lcfb);

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B92B Cipher message with output feedback (KMO) FC 1-3 and 9-11             */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmo_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
  LOGBYTE("cv    :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen, &parameter_block[keylen + 8]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context3, parameter_block, parameter_block);
        break;
      }
    }
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("output:", message_block, 7);
#endif

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("cv    :", parameter_block, 8);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMO_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
/* B92B Cipher message with output feedback (KMO) FC 18-20 and 26-28          */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmo_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  int i;
  BYTE message_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

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
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
  LOGBYTE("cv    :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen, &parameter_block[keylen + 16]))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    aes_encrypt(&context, parameter_block, parameter_block);
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);                    
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("cv    :", parameter_block, 16);
#endif

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
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
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* B93E KIMD  - Compute intermediate message digest                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KIMD_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KIMD: compute intermediate message digest");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KIMD_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* sha-1 */
    {
      ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 65: /* ghash */
    {
      ARCH_DEP(kimd_ghash)(r1, r2, regs);
      break;
    }
#endif

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B93F KLMD  - Compute last message digest                             [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KLMD_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KLMD: compute last message digest");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KLMD_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* sha-1 */
    {
      ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
    }
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
    }
#endif

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
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
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KM: cipher message");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
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
    {
      ARCH_DEP(km_dea)(r1, r2, regs);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(km_dea)(r1, r2, regs);
      break;
    }
#endif
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
    {
      ARCH_DEP(km_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    {
      ARCH_DEP(km_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(km_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 50: /* xts aes-128 */
    case 52: /* xts aes-256 */
    case 58: /* encrypted xts aes-128 */
    case 60: /* encrypted xts aes-256 */
    {
      ARCH_DEP(km_xts_aes)(r1, r2, regs);
      break;
    }
#endif

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B91E KMAC  - Compute message authentication code                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KMAC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMAC: compute message authentication code");
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KMAC_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    {
      ARCH_DEP(kmac_dea)(r1, r2, regs);
      break;
    }

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(kmac_dea)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 18: /* aes */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(kmac_aes)(r1, r2, regs);
      break;
    }
#endif

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
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
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMC: cipher message with chaining");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
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
    {
      ARCH_DEP(kmc_dea)(r1, r2, regs);
      break;
    } 
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(kmc_dea)(r1, r2, regs);
      break;
    }
#endif
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
    {
      ARCH_DEP(kmc_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    {
      ARCH_DEP(kmc_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(kmc_aes)(r1, r2, regs);
      break;
    }
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 67: /* prng */
    {
      ARCH_DEP(kmc_prng)(r1, r2, regs);
      break;
    }
#endif

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B92D KMCTR - Cipher message with counter                             [RRF] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_counter_d)
{
  int r1;
  int r2;
  int r3;

  RRF_M(inst, regs, r1, r2, r3);

#ifdef OPTION_KMCTR_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMCTR: cipher message with counter");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90101, "D", 3, r3);
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || !r3 || r3 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
  
  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KMCTR_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }

    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(kmctr_dea)(r1, r2, r3, regs);
      break;
    }
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(kmctr_aes)(r1, r2, r3, regs);
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92A KMF   - Cipher message with cipher feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_cipher_feedback_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KMF_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMF: cipher message with cipher feedback");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KMF_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }

    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(kmf_dea)(r1, r2, regs);
      break;      
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(kmf_aes)(r1, r2, regs);
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92B KMO   - Cipher message with output feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_output_feedback_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_KMO_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMO: cipher message with output feedback");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = KMO_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      ARCH_DEP(kmo_dea)(r1, r2, regs);
      break;      
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      ARCH_DEP(kmo_aes)(r1, r2, regs);
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92C PCC   - Perform cryptographic computation                       [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_computation_d)
{
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);
  
#ifdef OPTION_PCC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "PCC: perform cryptographic computation\n");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = PCC_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCC_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case  1: /* dea */
    case  2: /* tdea-128 */
    case  3: /* tdea-192 */
    case  9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      //ARCH_DEP(pcc_cmac_dea)(r1, r2, regs);
      break;
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      //ARCH_DEP(pcc_cmac_aes)(r1, r2, regs);
      break;
    }
    case 50: /* aes-128 */
    case 52: /* aes-256 */
    case 58: /* encrypted aes-128 */
    case 60: /* encrypted aes-256 */
    {
      //ARCH_DEP(pcc_xts_aes)(r1, r2, regs);
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }

}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/*----------------------------------------------------------------------------*/
/* B928 PCKMO - Perform cryptographic key management operations         [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_key_management_operations_d)
{
  int fc;
  int keylen = 0;
  BYTE parameter_block[64];
  int parameter_blocklen = 0;
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_PCKMO_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "PCKMO: perform cryptographic key management operations");
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif

  /* Privileged operation */
  PRIV_CHECK(regs);

  /* Check special conditions */
  if(unlikely(GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 0: /* Query */
    {
      BYTE parameter_block[16] = PCKMO_BITS;

      /* Store the parameter block */
      ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
      LOGBYTE("output:", parameter_block, 16);
#endif

      return;
    }
    case 1: /* encrypt-dea */
    case 2: /* encrypt-tdea-128 */
    case 3: /* encrypt-tdea-192 */
    {
      keylen = fc * 8;
      parameter_blocklen = keylen + 24;
      break;
    }  
    case 18: /* encrypt-aes-128 */
    case 19: /* encrypt-aes-192 */
    case 20: /* encrypt-aes-256 */
    {
      keylen = (fc - 16) * 8;
      parameter_blocklen = keylen + 32;
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }      
      
  /* Test writeability */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("input :", parameter_block, parameter_blocklen);
#endif
      
  /* Encrypt the key and fill the wrapping key verification pattern */
  switch(fc)
  {
    case 1: /* encrypt-dea */
    case 2: /* encrypt-tdea-128 */
    case 3: /* encrypt-tdea-192 */
    {
      wrap_dea(parameter_block, keylen);
      memcpy(&parameter_block[keylen], wkvp_regs_dea, 24);
      break;
    }
    case 18: /* encrypt-aes-128 */
    case 19: /* encrypt-aes-192 */
    case 20: /* encrypt-aes-256 */
    {
      wrap_aes(parameter_block, keylen);
      memcpy(&parameter_block[keylen], wkvp_regs_aes, 32);
      break;
    }
  }
        
  /* Store the parameterblock */
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("output:", parameter_block, parameter_blocklen);
#endif
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

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
//  HDL_REGISTER(s390_cipher_message_with_cipher_feedback, s390_cipher_message_with_cipher_feedback_d);
//  HDL_REGISTER(s390_cipher_message_with_counter, s390_cipher_message_with_counter_d);
//  HDL_REGISTER(s390_cipher_message_with_output_feedback, s390_cipher_message_with_output_feedback_d);
  HDL_REGISTER(s390_compute_intermediate_message_digest, s390_compute_intermediate_message_digest_d);
  HDL_REGISTER(s390_compute_last_message_digest, s390_compute_last_message_digest_d);
  HDL_REGISTER(s390_compute_message_authentication_code, s390_compute_message_authentication_code_d);
//  HDL_REGISTER(s390_perform_cryptographic_computation, s390_perform_cryptographic_computation_d);
  HDL_REGISTER(s390_perform_cryptographic_key_management_operations, s390_perform_cryptographic_key_management_operations_d);
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/

#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
  HDL_REGISTER(z900_cipher_message, z900_cipher_message_d);
  HDL_REGISTER(z900_cipher_message_with_chaining, z900_cipher_message_with_chaining_d);
//  HDL_REGISTER(z900_cipher_message_with_cipher_feedback, z900_cipher_message_with_cipher_feedback_d);
//  HDL_REGISTER(z900_cipher_message_with_counter, z900_cipher_message_with_counter_d);
//  HDL_REGISTER(z900_cipher_message_with_output_feedback, z900_cipher_message_with_output_feedback_d);
  HDL_REGISTER(z900_compute_intermediate_message_digest, z900_compute_intermediate_message_digest_d);
  HDL_REGISTER(z900_compute_last_message_digest, z900_compute_last_message_digest_d);
  HDL_REGISTER(z900_compute_message_authentication_code, z900_compute_message_authentication_code_d);
//  HDL_REGISTER(z900_perform_cryptographic_computation, z900_perform_cryptographic_computation_d);
  HDL_REGISTER(z900_perform_cryptographic_key_management_operations, z900_perform_cryptographic_key_management_operations_d);
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/

  WRMSG(HHC00150, "I", "Crypto", " (c) Copyright 2003-2010 by Bernard van der Helm"); // Copyright notice
  WRMSG(HHC00151, "I", "Message Security Assist"); // Feature notice
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  WRMSG(HHC00151, "I", "Message Security Assist Extension 1"); // Feature notice
#endif
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  WRMSG(HHC00151, "I", "Message Security Assist Extension 2"); // Feature notice
#endif
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  WRMSG(HHC00151, "I", "Message Security Assist Extension 3"); // Feature notice
#endif
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  WRMSG(HHC00151, "I", "Message Security Assist Extension 4 (excluding PCC)"); // Feature notice
#endif
}
END_REGISTER_SECTION;

#endif /*!defined(_GEN_ARCH)*/

#if 0
Remarks POP 
Page 1-16, 10-66, X-40, X41: "PERFORM CRYPTOGRAPHIC KEY MANAGEMENT OPERATION"
Suggested change: "PERFORM CRYPTOGRAPHIC KEY MANAGEMENT OPERATIONS"

Page 4-55
Action 6 should be added that denotes the creation of a new set of wrapping keys.

Figure 7-15, 7-19, 7-23, 7-27, 7-31, 7-35, 7-39, 7-43, 7-48, 7-52, 7-56, 7-60, 7-64, 
       7-68, 7-73, 7-83, 7-87, 7-91, 7-99, 7-103, 7-106, 7-116, 7-120, 7-124, 7-128, 
       7-132, 7-136, 7-139, 7-149, 7-153, 7-157, 7-165, 7-169, 7-172, 7-227, 7-230,
       7-233, 7-236, 7-239, 7-242, 7-244, 7-266, 7-269, 7-272, 7-275, 7-278, 7-281,
       7-284, 7-287, 7-289, 10-36, 10-37, 10-38, 10-39, 10-40, 10-41 : "Verification Pattern"
Page 7-70, 7-85, 7-98, 7-111, 7-180, 7-269: "Verification pattern"
Suggested change: "Verification-Pattern"

Page 7-90 and further: Description of KMCTR FC 1-3, 9-11, 18-20 and 26-28
Suggested change: Provide one description for cipher and decipher. The algoritm is the same!

Page 7-73 and further
Remark: It is not clearly stated that the LCFB is the "s" value of s-byte.

Page 7-98 and further: Description of KMO FC 1-3, 9-11, 18-20 and 26-28
Suggested change: Provide one description for cipher and decipher. The algoritm is the same!
#endif
