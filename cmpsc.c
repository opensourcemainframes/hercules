/*----------------------------------------------------------------------------*/
/* file: cmpsc.c                                                              */
/*                                                                            */
/* Implementation of the S/390 compression call instruction described in      */
/* SA22-7208-01: Data Compression within the Hercules S/390 emulator.         */
/* This implementation couldn't be done without the test programs from        */
/* Mario Bezzi. Thanks Mario! Also special thanks to Greg Smith who           */
/* introduced iregs, needed when a page fault occurs.                         */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2000-2009 */
/*                              Noordwijkerhout, The Netherlands.             */
/*                                                                            */
/*----------------------------------------------------------------------------*/

// $Id$

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CMPSC_C_)
#define _CMPSC_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#ifdef FEATURE_COMPRESSION

/*============================================================================*/
/* Common                                                                     */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Debugging options:                                                         */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_CMPSC_DEBUG
#define OPTION_CMPSC_ECACHE_DEBUG
#define TRUEFALSE(boolean)   ((boolean) ? "True" : "False")
#endif

/*----------------------------------------------------------------------------*/
/* After a succesful compression of characters to an index symbol or a        */
/* succsful translation of an index symbol to characters, the registers must  */
/* be updated.                                                                */
/*----------------------------------------------------------------------------*/
#define ADJUSTREGS(r, regs, iregs, len) \
{\
  SET_GR_A((r), (iregs), (GR_A((r), (iregs)) + (len)) & ADDRESS_MAXWRAP((regs)));\
  SET_GR_A((r) + 1, (iregs), GR_A((r) + 1, (iregs)) - (len));\
}

/*----------------------------------------------------------------------------*/
/* Commit intermediate register                                               */
/*----------------------------------------------------------------------------*/
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2)) \
  logmsg("*** Regs committed\n");
#else
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2))
#endif
#define __COMMITREGS(regs, iregs, r1, r2) \
{\
  SET_GR_A(1, (regs), GR_A(1, (iregs)));\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

/*----------------------------------------------------------------------------*/
/* Commit intermediate register, except for GR1                               */
/*----------------------------------------------------------------------------*/
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2)) \
  logmsg("*** Regs committed\n");
#else
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2))
#endif
#define __COMMITREGS2(regs, iregs, r1, r2) \
{\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

/*----------------------------------------------------------------------------*/
/* Initialize intermediate registers                                          */
/*----------------------------------------------------------------------------*/
#define INITREGS(iregs, regs, r1, r2) \
{ \
  (iregs)->gr[1] = (regs)->gr[1]; \
  (iregs)->gr[(r1)] = (regs)->gr[(r1)]; \
  (iregs)->gr[(r1) + 1] = (regs)->gr[(r1) + 1]; \
  (iregs)->gr[(r2)] = (regs)->gr[(r2)]; \
  (iregs)->gr[(r2) + 1] = (regs)->gr[(r2) + 1]; \
}

#if (__GEN_ARCH == 900)
#undef INITREGS
#define INITREGS(iregs, regs, r1, r2) \
{ \
  (iregs)->gr[1] = (regs)->gr[1]; \
  (iregs)->gr[(r1)] = (regs)->gr[(r1)]; \
  (iregs)->gr[(r1) + 1] = (regs)->gr[(r1) + 1]; \
  (iregs)->gr[(r2)] = (regs)->gr[(r2)]; \
  (iregs)->gr[(r2) + 1] = (regs)->gr[(r2) + 1]; \
  (iregs)->psw.amode64 = (regs)->psw.amode64; \
}
#endif /* defined(__GEN_ARCH == 900) */

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* cdss  : compressed-data symbol size                                        */
/* e     : expansion operation                                                */
/* f1    : format-1 sibling descriptors                                       */
/* st    : symbol-translation option                                          */
/*----------------------------------------------------------------------------*/
#define GR0_cdss(regs)       (((regs)->GR_L(0) & 0x0000F000) >> 12)
#define GR0_e(regs)          ((regs)->GR_L(0) & 0x00000100)
#define GR0_f1(regs)         ((regs)->GR_L(0) & 0x00000200)
#define GR0_st(regs)         ((regs)->GR_L(0) & 0x00010000)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0) derived                           */
/*----------------------------------------------------------------------------*/
/* dcten      : # dictionary entries                                          */
/* dctsz      : dictionary size                                               */
/* smbsz      : symbol size                                                   */
/*----------------------------------------------------------------------------*/
#define GR0_dcten(regs)      (0x100 << GR0_cdss(regs))
#define GR0_dctsz(regs)      (0x800 << GR0_cdss((regs)))
#define GR0_smbsz(regs)      (GR0_cdss((regs)) + 8)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 1 macro's (GR1)                                   */
/*----------------------------------------------------------------------------*/
/* cbn   : compressed-data bit number                                         */
/* dictor: compression dictionary or expansion dictionary                     */
/* sttoff: symbol-translation-table offset                                    */
/*----------------------------------------------------------------------------*/
#define GR1_cbn(regs)        (((regs)->GR_L(1) & 0x00000007))
#define GR1_dictor(regs)     (GR_A(1, regs) & ((GREG) 0xFFFFFFFFFFFFF000ULL))
#define GR1_sttoff(regs)     (((regs)->GR_L(1) & 0x00000FF8) << 4)

/*----------------------------------------------------------------------------*/
/* Sets the compressed bit number in GR1                                      */
/*----------------------------------------------------------------------------*/
#define GR1_setcbn(regs, cbn) ((regs)->GR_L(1) = ((regs)->GR_L(1) & 0xFFFFFFF8) | ((cbn) & 0x00000007))

/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX          1048575   /* CPU-determined amount of data       */
#define ECACHE_SIZE          32768     /* Expanded iss cache size             */

/*----------------------------------------------------------------------------*/
/* Typedefs and prototypes                                                    */
/*----------------------------------------------------------------------------*/
#ifndef NO_2ND_COMPILE
struct cc                              /* Compress context                    */
{
  BYTE *cce;                           /* Character entry under investigation */
  BYTE *dict[32];                      /* Dictionary MADDR addresses          */
  BYTE *edict[32];                     /* Expansion dictionary MADDR addrs    */
  int f1;                              /* Indication format-1 sibling descr   */
};

struct ec                              /* Expand context                      */
{
  BYTE *dest;                          /* Destination MADDR page address      */
  BYTE *dict[32];                      /* Dictionary MADDR addresses          */
  BYTE ec[ECACHE_SIZE];                /* Expanded index symbol cache         */
  int eci[8192];                       /* Index within cache for is           */
  int ecl[8192];                       /* Size of expanded is                 */
  int ecwm;                            /* Water mark                          */
  BYTE oc[2080];                       /* Output cache                        */
  unsigned ocl;                        /* Output cache length                 */
  BYTE *src;                           /* Source MADDR page address           */
};
#endif /* #ifndef NO_2ND_COMPILE */

static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs);
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs);
static void ARCH_DEP(expand_is)(REGS *regs, struct ec *ec, U16 is);
static int  ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int ofst);
static int  ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *is);
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, struct ec *ec, U16 is[8]);
#ifdef OPTION_CMPSC_DEBUG
static void print_cce(BYTE *cce);
static void print_ece(BYTE *ece);
static void print_sd(int f1, BYTE *sd1, BYTE *sd2);
#endif
static int  ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, struct cc *cc, BYTE *ch, U16 *is);
static int  ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, struct cc *cc, BYTE *ch, U16 *is);
static int  ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is);
static int  ARCH_DEP(test_ec)(int r2, REGS *regs, REGS *iregs, BYTE *cce);
static int  ARCH_DEP(vstore)(int r1, REGS *regs, REGS *iregs, struct ec *ec, BYTE *buf, unsigned len);

/*----------------------------------------------------------------------------*/
/* B263 CMPSC - Compression Call                                        [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compression_call)
{
  REGS iregs;                          /* Intermediate registers              */
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_CMPSC_DEBUG 
  logmsg("CMPSC: compression call\n");
  logmsg(" r1      : GR%02d\n", r1);
  logmsg(" address : " F_VADR "\n", regs->GR(r1));
  logmsg(" length  : " F_GREG "\n", regs->GR(r1 + 1));
  logmsg(" r2      : GR%02d\n", r2);
  logmsg(" address : " F_VADR "\n", regs->GR(r2));
  logmsg(" length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg(" GR00    : " F_GREG "\n", regs->GR(0));
  logmsg("   st    : %s\n", TRUEFALSE(GR0_st(regs)));
  logmsg("   cdss  : %d\n", GR0_cdss(regs));
  logmsg("   f1    : %s\n", TRUEFALSE(GR0_f1(regs)));
  logmsg("   e     : %s\n", TRUEFALSE(GR0_e(regs)));
  logmsg(" GR01    : " F_GREG "\n", regs->GR(1));
  logmsg("   dictor: " F_GREG "\n", GR1_dictor(regs));
  logmsg("   sttoff: %08X\n", GR1_sttoff(regs));
  logmsg("   cbn   : %d\n", GR1_cbn(regs));
#endif 

  /* Check the registers on even-odd pairs and valid compression-data symbol size */
  if(unlikely(r1 & 0x01 || r2 & 0x01 || !GR0_cdss(regs) || GR0_cdss(regs) > 5))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Check for empty input */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Check for empty output */
  if(unlikely(!GR_A(r1 + 1, regs)))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set possible Data Exception code right away */
  regs->dxc = DXC_DECIMAL;     

  /* Initialize intermediate registers */
  INITREGS(&iregs, regs, r1, r2);

  /* Now go to the requested function */
  if(likely(GR0_e(regs)))
    ARCH_DEP(expand)(r1, r2, regs, &iregs);
  else
    ARCH_DEP(compress)(r1, r2, regs, &iregs);
}

/*============================================================================*/
/* Compress                                                                   */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE)                                  */
/*----------------------------------------------------------------------------*/
/* act    : additional-extension-character count                              */
/* cct    : child count                                                       */
/* cptr   : child pointer; index of first child                               */
/* d      : double-character entry                                            */
/* x(i)   : examine child bit for children 1 to 5                             */
/* y(i)   : examine child bit for 6th/13th and 7th/14th sibling               */
/*----------------------------------------------------------------------------*/
#define CCE_act(cce)         ((cce)[1] >> 5)
#define CCE_cct(cce)         ((cce)[0] >> 5)
#define CCE_cptr(cce)        ((((cce)[1] & 0x1f) << 8) | (cce)[2])
#define CCE_d(cce)           ((cce)[1] & 0x20)
#define CCE_x(cce, i)        ((cce)[0] & (0x10 >> (i)))
#define CCE_y(cce, i)        ((cce)[1] & (0x80 >> (i)))

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE) derived                          */
/*----------------------------------------------------------------------------*/
/* cc(i)  : child character                                                   */
/* ccc(i) : indication consecutive child character                            */
/* ccs    : number of child characters                                        */
/* ec(i)  : additional extension character                                    */
/* ecs    : number of additional extension characters                         */
/* mcc    : indication if siblings follow child characters                    */
/*----------------------------------------------------------------------------*/
#define CCE_cc(cce, i)       ((cce)[3 + CCE_ecs((cce)) + (i)])
#define CCE_ccc(cce, i)      (CCE_cc((cce), (i)) == CCE_cc((cce), 0))
#define CCE_ccs(cce)         (CCE_cct((cce)) - (CCE_mcc((cce)) ? 1 : 0))
#define CCE_ec(cce, i)       ((cce)[3 + (i)])
#define CCE_ecs(cce)         ((CCE_cct((cce)) <= 1) ? CCE_act((cce)) : (CCE_d((cce)) ? 1 : 0))
#define CCE_mcc(cce)         ((CCE_cct((cce)) + (CCE_d((cce)) ? 1 : 0) == 6))

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for siblings 1 to 5                             */
/*----------------------------------------------------------------------------*/
#define SD0_sct(sd0)         ((sd0)[0] >> 5)
#define SD0_y(sd0, i)        ((sd0)[0] & (0x10 >> (i)))

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0) derived                         */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 6th/7th fetched from parent          */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD0_ccc(sd0, i)      (SD0_sc((sd0), (i)) == SD0_sc((sd0), 0))
#define SD0_ecb(sd0, i, cce, y) (((i) < 5) ? SD0_y((sd0), (i)) : (y) ? CCE_y((cce), ((i) - 5)) : 1)
#define SD0_msc(sd0)         (!SD0_sct((sd0)))
#define SD0_sc(sd0, i)       ((sd0)[1 + (i)])
#define SD0_scs(sd0)         (SD0_msc((sd0)) ? 7 : SD0_sct((sd0)))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for sibling 1 to 12                             */
/*----------------------------------------------------------------------------*/
#define SD1_sct(sd1)         ((sd1)[0] >> 4)
#define SD1_y(sd1, i)        ((i) < 4 ? ((sd1)[0] & (0x08 >> (i))): ((sd1)[1] & (0x800 >> (i))))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1) derived                         */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 13th/14th fetched from parent        */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD1_ccc(sd1, sd2, i) (SD1_sc((sd1), (sd2), (i)) == SD1_sc((sd1), (sd2), 0))
#define SD1_ecb(sd1, i, cce, y) (((i) < 12) ? SD1_y((sd1), (i)) : (y) ? CCE_y((cce), ((i) - 12)) : 1)
#define SD1_msc(sd1)         ((SD1_sct((sd1)) == 15))
#define SD1_sc(sd1, sd2, i)  ((i) < 6 ? (sd1)[2 + (i)] : (sd2)[(i) - 6])
#define SD1_scs(sd1)         (SD1_msc((sd1)) ? 14 : SD1_sct((sd1)))

/*----------------------------------------------------------------------------*/
/* Format independent sibling descriptor macro's                              */
/*----------------------------------------------------------------------------*/
#define SD_ccc(f1, sd1, sd2, i) ((f1) ? SD1_ccc((sd1), (sd2), (i)) : SD0_ccc((sd1), (i)))
#define SD_ecb(f1, sd1, i, cce, y) ((f1) ? SD1_ecb((sd1), (i), (cce), (y)) : SD0_ecb((sd1), (i), (cce), (y)))
#define SD_msc(f1, sd1)      ((f1) ? SD1_msc((sd1)) : SD0_msc((sd1)))
#define SD_sc(f1, sd1, sd2, i) ((f1) ? SD1_sc((sd1), (sd2), (i)) : SD0_sc((sd1), (i)))
#define SD_scs(f1, sd1)      ((f1) ? SD1_scs((sd1)) : SD0_scs((sd1)))

/*----------------------------------------------------------------------------*/
/* Check character entry for data exception                                   */
/*----------------------------------------------------------------------------*/
#define CCE_check(cce) \
  if(CCE_cct(cce) < 2) \
  { \
    if(unlikely(CCE_act(cce) > 4)) \
      ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION); \
  } \
  else if(!CCE_d(cce)) \
  { \
    if(unlikely(CCE_cct(cce) == 7)) \
      ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION); \
  } \
  else if(unlikely(CCE_cct(cce) > 5)) \
    ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* compress                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs)
{
  struct cc cc;                        /* Compression context                 */
  BYTE ch;                             /* Character read                      */
  GREG dictor;                         /* Dictionary origin                   */
  int eos;                             /* indication end of source            */
  GREG exit_value;                     /* return cc=3 on this value           */
  int i;
  U16 index;
  U16 is;                              /* Last matched index symbol           */
  GREG vaddr;

  /* Initialize values */
  dictor = GR1_dictor(regs);
  eos = 0;

  /* Initialize dictionary MADDR addresses */
  vaddr = dictor;
  for(i = 0; i < (0x01 << GR0_cdss(regs)); i++)
  {
    cc.dict[i] = MADDR(vaddr, r2, regs, ACCTYPE_READ, regs->psw.pkey);
    vaddr += 0x800;
  }

  /* Initialize format-1 sibling descriptor indicator */
  cc.f1 = GR0_f1(regs);

  /* Initialize expansion dictionary MADDR addresses */
  if(cc.f1)
  {
    vaddr = dictor + GR0_dctsz(regs);
    for(i = 0; i < (0x01 << GR0_cdss(regs)); i++)
    {
      cc.edict[i] = MADDR(vaddr, r2, regs, ACCTYPE_READ, regs->psw.pkey);
      vaddr += 0x800;
    }
  }

  /* Try to process the CPU-determined amount of data */
  if(likely(GR_A(r2 + 1, regs) <= PROCESS_MAX))
    exit_value = 0;
  else
    exit_value = GR_A(r2 + 1, regs) - PROCESS_MAX;
  while(exit_value <= GR_A(r2 + 1, regs))
  {

    /* Get the next character, return on end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(r2, regs, iregs, &ch, 0)))
      return;

    /* Get the alphabet entry */
    index = ch * 8;
    cc.cce = &cc.dict[index / 0x800][index % 0x800];
    ITIMER_SYNC(dictor + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch cce: index %04X\n", ch);
    print_cce(cc.cce);
#endif

    CCE_check(cc.cce);

    /* We always match the alpabet entry, so set last match */
    ADJUSTREGS(r2, regs, iregs, 1);
    is = ch;

    /* Try to find a child in compression character entry */
    while(ARCH_DEP(search_cce)(r2, regs, iregs, &cc, &ch, &is));

    /* Write the last match, this can be the alphabet entry */
    if(ARCH_DEP(store_is)(r1, r2, regs, iregs, is))
      return;

    /* Commit registers, we have completed a full compression */
    COMMITREGS(regs, iregs, r1, r2);
  }

  /* When reached end of source, return to caller */
  if(unlikely(!GR_A(r2 + 1, regs)))
    return;

  /* Reached model dependent CPU processing amount */
  regs->psw.cc = 3;

#ifdef OPTION_CMPSC_DEBUG
  logmsg("compress : reached CPU determined amount of data\n");
#endif

}

#ifndef NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* print_cce (compression character entry).                                   */
/*----------------------------------------------------------------------------*/
static void print_cce(BYTE *cce)
{
  int j;
  int prt_detail;

  logmsg("  cce    : ");
  prt_detail = 0;
  for(j = 0; j < 8; j++)
  {
    if(!prt_detail && cce[j])
      prt_detail = 1;
    logmsg("%02X", cce[j]);
  }
  logmsg("\n");
  if(prt_detail)
  {
    logmsg("  cct    : %d\n", CCE_cct(cce));
    switch(CCE_cct(cce))
    { 
      case 0:
      {
        logmsg("  act    : %d\n", (int) CCE_act(cce));
        if(CCE_act(cce))
        {
          logmsg("  ec(s)  :");
          for(j = 0; j < CCE_ecs(cce); j++)
            logmsg(" %02X", CCE_ec(cce, j));
          logmsg("\n");
        }
        break;
      }
      case 1:
      {
        logmsg("  x1     : %c\n", (int) (CCE_x(cce, j) ? '1' : '0'));
        logmsg("  act    : %d\n", (int) CCE_act(cce));
        logmsg("  cptr   : %04X\n", CCE_cptr(cce));
        if(CCE_act(cce))
        {
          logmsg("  ec(s)  :");
          for(j = 0; j < CCE_ecs(cce); j++)
            logmsg(" %02X", CCE_ec(cce, j));
          logmsg("\n");
        }
        logmsg("  cc     : %02X\n", CCE_cc(cce, 0));
        break;
      }
      default:
      {
        logmsg("  x1..x5 : ");
        for(j = 0; j < 5; j++)
          logmsg("%c", (int) (CCE_x(cce, j) ? '1' : '0'));
        logmsg("\n  y1..y2 : ");
        for(j = 0; j < 2; j++)
          logmsg("%c", (int) (CCE_y(cce, j) ? '1' : '0'));
        logmsg("\n  d      : %s\n", TRUEFALSE(CCE_d(cce)));
        logmsg("  cptr   : %04X\n", CCE_cptr(cce));
        if(CCE_d(cce))
          logmsg("  ec     : %02X\n", CCE_ec(cce, 0));
        logmsg("  ccs    :");
        for(j = 0; j < CCE_ccs(cce); j++)
          logmsg(" %02X", CCE_cc(cce, j));
        logmsg("\n");
        break;
      }
    }
  }
}
#endif
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* fetch_ch (character)                                                       */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int ofst)
{
  /* Check for end of source condition */
  if(unlikely(GR_A(r2 + 1, iregs) <= (U32) ofst))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch_ch : reached end of source\n");
#endif

    regs->psw.cc = 0;
    return(1);
  }
  *ch = ARCH_DEP(vfetchb)((GR_A(r2, iregs) + ofst) & ADDRESS_MAXWRAP(regs), r2, regs);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_ch : %02X at " F_VADR "\n", *ch, (GR_A(r2, iregs) + ofst));
#endif

  return(0);
}

#ifndef NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* fetch_sd (sibling descriptor).                                             */
/*----------------------------------------------------------------------------*/
static void print_sd(int f1, BYTE *sd1, BYTE *sd2)
{
  int j;
  int prt_detail;

  if(f1)
  {
    logmsg("  sd1    : ");
    prt_detail = 0;
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd1[j])
        prt_detail = 1;
      logmsg("%02X", sd1[j]);
    }
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd2[j])
        prt_detail = 1;
      logmsg("%02X", sd2[j]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD1_sct(sd1));
      logmsg("  y1..y12: ");
      for(j = 0; j < 12; j++)
        logmsg("%c", (SD1_y(sd1, j) ? '1' : '0'));
      logmsg("\n  sc(s)  :");
      for(j = 0; j < SD1_scs(sd1); j++)
        logmsg(" %02X", SD1_sc(sd1, sd2, j));
      logmsg("\n");
    }
  }
  else
  {
    logmsg("  sd0    : ");
    prt_detail = 0;
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd1[j])
        prt_detail = 1;
      logmsg("%02X", sd1[j]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD0_sct(sd1));
      logmsg("  y1..y5 : ");
      for(j = 0; j < 5; j++)
        logmsg("%c", (SD0_y(sd1, j) ? '1' : '0'));
      logmsg("\n  sc(s)  :");
      for(j = 0; j < SD0_scs(sd1); j++)
        logmsg(" %02X", SD0_sc(sd1, j));
      logmsg("\n");
    }
  }
}
#endif
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* search_cce (compression character entry)                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, struct cc *cc, BYTE *ch, U16 *is)
{
  BYTE *ccce;                          /* child compression character entry   */

#ifdef FEATURE_INTERVAL_TIMER
  GREG dictor;                         /* Dictionary origin                   */
#endif

  int i;                               /* child character index               */
  U16 index;
  int ind_search_siblings;             /* Indicator for searching siblings    */

  /* Initialize values */
  ind_search_siblings = 1;

#ifdef FEATURE_INTERVAL_TIMER
  dictor = GR1_dictor(regs);
#endif

  /* Get the next character when there are children */
  if(unlikely((CCE_ccs(cc->cce) && ARCH_DEP(fetch_ch)(r2, regs, iregs, ch, 0))))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("search_cce: end of source\n");
#endif

    return(0);
  }

  /* Now check all children in parent */
  for(i = 0; i < CCE_ccs(cc->cce); i++)
  {

    /* Stop searching when child tested and no consecutive child character */
    if(unlikely(!ind_search_siblings && !CCE_ccc(cc->cce, i)))
      return(0);

    /* Compare character with child */
    if(unlikely(*ch == CCE_cc(cc->cce, i)))
    {

      /* Child is tested, so stop searching for siblings*/
      ind_search_siblings = 0;

      /* Check if child should not be examined */
      if(unlikely(!CCE_x(cc->cce, i)))
      {

        /* No need to examine child, found the last match */
        ADJUSTREGS(r2, regs, iregs, 1);
        *is = CCE_cptr(cc->cce) + i;

        /* Index symbol found */
        return(0);
      }

      /* Found a child get the character entry */
      index = (CCE_cptr(cc->cce) + i) * 8;
      ccce = &cc->dict[index / 0x800][index % 0x800];
      ITIMER_SYNC(dictor + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
      logmsg("fetch cce: index %04X\n", CCE_cptr(cc->cce) + i);
      print_cce(ccce);
#endif

      CCE_check(ccce);

      /* Check if additional extension characters match */
      if(likely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
      {

        /* Set last match */
        ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
        *is = CCE_cptr(cc->cce) + i;

#ifdef OPTION_CMPSC_DEBUG
        logmsg("search_cce index %04X parent\n", *is);
#endif 

        /* Found a matching child, make it parent */
        cc->cce = ccce;

        /* We found a parent */
        return(1);
      }
    }
  }

  /* Are there siblings? */
  if(likely(CCE_mcc(cc->cce)))
    return(ARCH_DEP(search_sd)(r2, regs, iregs, cc, ch, is));

  /* No siblings, write found index symbol */
  return(0);
}

/*----------------------------------------------------------------------------*/
/* search_sd (sibling descriptor)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, struct cc *cc, BYTE *ch, U16 *is)
{
  BYTE *ccce;                          /* Child compression character entry   */

#ifdef FEATURE_INTERVAL_TIMER
  GREG dictor;                         /* Dictionary origin                   */
#endif

  int i;                               /* Sibling character index             */
  U16 index;
  int ind_search_siblings;             /* Indicator for keep searching        */
  BYTE *sd1;                           /* Sibling descriptor fmt-0|1 part 1   */
  BYTE *sd2;                           /* Sibling descriptor fmt-1 part 2     */
  int sd_ptr;                          /* Pointer to sibling descriptor       */
  int searched;                        /* Number of children searched         */
  int y_in_parent;                     /* Indicator if y bits are in parent   */

  /* Initialize values */
#ifdef FEATURE_INTERVAL_TIMER
  dictor = GR1_dictor(regs);
#endif
  ind_search_siblings = 1;

  /* For the first sibling descriptor y bits are in the cce parent */
  y_in_parent = 1;

  /* Sibling follows last possible child */
  sd_ptr = CCE_ccs(cc->cce);

  /* set searched childs */
  searched = sd_ptr;

  /* As long there are sibling characters */
  do
  {
    /* Get the sibling descriptor */
    index = (CCE_cptr(cc->cce) + sd_ptr) * 8;
    sd1 = &cc->dict[index / 0x800][index % 0x800];
    ITIMER_SYNC(GR0_dictor(regs) + index, 8 - 1, regs);

    /* If format-1, get second half from the expansion dictionary */ 
    if(cc->f1)
    {
      sd2 = &cc->edict[index / 0x800][index % 0x800];
      ITIMER_SYNC(GR0_dictor(regs) + GR0_dctsz(regs) + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
      /* Print before possible exception */
      logmsg("fetch sd1: index %04X\n", CCE_cptr(cc->cce) + sd_ptr);
      print_sd(1, sd1, sd2);
#endif

      /* Check for data exception */
      if(!SD1_sct(sd1))
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);
    }
    else
    {

#ifdef OPTION_CMPSC_DEBUG
      /* Also print for format-0 sibling descriptors */
      logmsg("fetchr sd0: index %04X\n", CCE_cptr(cc->cce) + sd_ptr);
      print_sd(0, sd1, sd2);
#endif
 
    }

    /* Check all children in sibling descriptor */
    for(i = 0; i < SD_scs(cc->f1, sd1); i++)
    {

      /* Stop searching when child tested and no consecutive child character */
      if(unlikely(!ind_search_siblings && !SD_ccc(cc->f1, sd1, sd2, i)))
        return(0);

      if(unlikely(*ch == SD_sc(cc->f1, sd1, sd2, i)))
      {

        /* Child is tested, so stop searching for siblings*/
        ind_search_siblings = 0;

        /* Check if child should not be examined */
        if(unlikely(!SD_ecb(cc->f1, sd1, i, cc->cce, y_in_parent)))
        {

          /* No need to examine child, found the last match */
          ADJUSTREGS(r2, regs, iregs, 1);
          *is = CCE_cptr(cc->cce) + sd_ptr + i + 1;

          /* Index symbol found */
          return(0);
        }

        /* Found a child get the character entry */
        index = (CCE_cptr(cc->cce) + sd_ptr + i + 1) * 8;
        ccce = &cc->dict[index / 0x800][index % 0x800];
        ITIMER_SYNC(dictor + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
        logmsg("fetch cce: index %04X\n", CCE_cptr(cc->cce) + sd_ptr + i + 1);
        print_cce(ccce);
#endif

        CCE_check(ccce);

        /* Check if additional extension characters match */
        if(unlikely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
        {

          /* Set last match */
          ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
          *is = CCE_cptr(cc->cce) + sd_ptr + i + 1;

#ifdef OPTION_CMPSC_DEBUG
          logmsg("search_sd: index %04X parent\n", *is);
#endif 

          /* Found a matching child, make it parent */
          cc->cce = ccce;

          /* We found a parent */
          return(1);
        }
      }
    }

    /* Next sibling follows last possible child */
    sd_ptr += SD_scs(cc->f1, sd1) + 1;

    /* test for searching child 261 */
    searched += SD_scs(cc->f1, sd1);
    if(unlikely(searched > 260))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION); 

    /* We get the next sibling descriptor, no y bits in parent for him */
    y_in_parent = 0;

  }
  while(ind_search_siblings && SD_msc(cc->f1, sd1));
  return(0);
}

/*----------------------------------------------------------------------------*/
/* store_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is)
{
  U32 set_mask;                        /* mask to set the bits                */
  BYTE work[3];                        /* work bytes                          */

  /* Can we write an index or interchange symbol */
  if(unlikely(GR_A(r1 + 1, iregs) < 2))
  {
    if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r1 + 1, iregs)))
    {
      regs->psw.cc = 1;

#ifdef OPTION_CMPSC_DEBUG
      logmsg("store_is : end of output buffer\n");
#endif 

      return(-1);
    }
  }

  /* Check if symbol translation is requested */
  if(unlikely(GR0_st(regs)))
  {

    /* Get the interchange symbol */
    ARCH_DEP(vfetchc)(work, 1, (GR1_dictor(regs) + GR1_sttoff(regs) + is * 2) & ADDRESS_MAXWRAP(regs), r2, regs);

#ifdef OPTION_CMPSC_DEBUG
    logmsg("store_is : %04X -> %02X%02X\n", is, work[0], work[1]);
#endif

    /* set index_symbol to interchange symbol */
    is = (work[0] << 8) + work[1];
  }

  /* Allign set mask */
  set_mask = ((U32) is) << (24 - GR0_smbsz(regs) - GR1_cbn(iregs));

  /* Calculate first byte */
  if(likely(GR1_cbn(iregs)))
  {
    work[0] = ARCH_DEP(vfetchb)(GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);
    work[0] |= (set_mask >> 16) & 0xff;
  }
  else
    work[0] = (set_mask >> 16) & 0xff;

  /* Calculate second byte */
  work[1] = (set_mask >> 8) & 0xff;

  /* Calculate possible third byte and store */
  if(unlikely((GR0_smbsz(regs) + GR1_cbn(iregs)) > 16))
  {
    work[2] = set_mask & 0xff;
    ARCH_DEP(vstorec)(work, 2, GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);
  }
  else
    ARCH_DEP(vstorec)(work, 1, GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);

  /* Adjust destination registers */
  ADJUSTREGS(r1, regs, iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) / 8);

  /* Calculate and set the new Compressed-data Bit Number */
  GR1_setcbn(iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) % 8);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("store_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", is, GR1_cbn(iregs), r1, iregs->GR(r1), r1 + 1, iregs->GR(r1 + 1));
#endif 

  return(0);
}

/*----------------------------------------------------------------------------*/
/* test_ec (extension characters)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(test_ec)(int r2, REGS *regs, REGS *iregs, BYTE *cce)
{
  BYTE ch;
  int i;

  for(i = 0; i < CCE_ecs(cce); i++)
  {

    /* Get a character return on nomatch or end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(r2, regs, iregs, &ch, i + 1) || ch != CCE_ec(cce, i)))
      return(0);
  }

  /* a perfect match */
  return(1);
}

/*============================================================================*/
/* Expand                                                                     */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* bit34 : indication of bits 3 and 4 (what else ;-)                          */
/* csl   : complete symbol length                                             */
/* ofst  : offset from current position in output area                        */
/* pptr  : predecessor pointer                                                */
/* psl   : partial symbol length                                              */
/*----------------------------------------------------------------------------*/
#define ECE_bit34(ece)       ((ece)[0] & 0x18)
#define ECE_csl(ece)         ((ece)[0] & 0x07)
#define ECE_ofst(ece)        ((ece)[7])
#define ECE_pptr(ece)        ((((ece)[0] & 0x1f) << 8) | (ece)[1])
#define ECE_psl(ece)         ((ece)[0] >> 5)

/*----------------------------------------------------------------------------*/
/* expand                                                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs)
{
  unsigned cw;                         /* Characters written                  */
  int dcten;                           /* Number of different symbols         */
  struct ec ec;                        /* Expand cache                        */
  int i;
  U16 is;                              /* Index symbol                        */
  U16 iss[8];                          /* Index symbols                       */
  unsigned smbsz;                      /* Symbol size                         */
  GREG vaddr;

  /* Initialize values */
  cw = 0;
  dcten = GR0_dcten(regs);
  smbsz = GR0_smbsz(regs);

  /* Initialize destination dictionary address */
  ec.dest = NULL;

  /* Initialize dictionary maddr addresses */
  vaddr = GR1_dictor(regs);
  for(i = 0; i < (0x01 << GR0_cdss(regs)); i++)
  {
    ec.dict[i] = MADDR(vaddr, r2, regs, ACCTYPE_READ, regs->psw.pkey);
    vaddr += 0x800;
  }

  /* Initialize expanded index symbol cache and prefill with alphabet entries */
  for(i = 0; i < 256; i++)             /* Alphabet entries                    */
  {
    ec.ec[i] = i;
    ec.eci[i] = i;
    ec.ecl[i] = 1;
  }
  for(i = 256; i < dcten; i++)         /* Clear all other index symbols       */
    ec.ecl[i] = 0;
  ec.ecwm = 256;                       /* Set watermark after alphabet part   */

  /* Initialize source maddr page address */
  ec.src = NULL;

  /* Process individual index symbols until cbn becomes zero */
  if(unlikely(GR1_cbn(regs)))
  {
    while(likely(GR1_cbn(regs)))
    {
      if(unlikely(ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
        return;
      if(likely(!ec.ecl[is]))
      {
        ec.ocl = 0;                    /* Initialize output cache             */
        ARCH_DEP(expand_is)(regs, &ec, is);
        if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec, ec.oc, ec.ocl)))
          return;
        cw += ec.ocl;
      }
      else
      {
        if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec, &ec.ec[ec.eci[is]], ec.ecl[is])))
          return;
        cw += ec.ecl[is];
      }
    }

    /* Commit, including GR1 */
    COMMITREGS(regs, iregs, r1, r2);
  }

  /* Block processing, cbn stays zero */
  while(likely(GR_A(r2 + 1, iregs) >= smbsz && cw < PROCESS_MAX))
  {
    ARCH_DEP(fetch_iss)(r2, regs, iregs, &ec, iss);
    ec.ocl = 0;                        /* Initialize output cache             */
    for(i = 0; i < 8; i++)
    {

#ifdef OPTION_CMPSC_DEBUG
      logmsg("expand   : is %04X (%d)\n", iss[i], i);
#endif

      if(unlikely(!ec.ecl[iss[i]]))
        ARCH_DEP(expand_is)(regs, &ec, iss[i]);
      else
      {
        memcpy(&ec.oc[ec.ocl], &ec.ec[ec.eci[iss[i]]], ec.ecl[iss[i]]);
        ec.ocl += ec.ecl[iss[i]];
      }
    }

    /* Write and commit, cbn unchanged, so no commit for GR1 needed */
    if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec, ec.oc, ec.ocl)))
      return;
    COMMITREGS2(regs, iregs, r1, r2);
    cw += ec.ocl;
  }

  if(unlikely(cw >= PROCESS_MAX))
  {
    regs->psw.cc = 3;

#ifdef OPTION_CMPSC_DEBUG
    logmsg("expand   : reached CPU determined amount of data\n");
#endif

    return;
  }

  /* Process last index symbols, never mind about childs written */
  while(likely(!ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
  {
    if(unlikely(!ec.ecl[is]))
    {
      ec.ocl = 0;                      /* Initialize output cache             */
      ARCH_DEP(expand_is)(regs, &ec, is);
      if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec, ec.oc, ec.ocl)))
        return;
    }
    else
    {
      if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec, &ec.ec[ec.eci[is]], ec.ecl[is])))
        return;
    }
  }

  /* Commit, including GR1 */
  COMMITREGS(regs, iregs, r1, r2);
}

/*----------------------------------------------------------------------------*/
/* expand_is (index symbol).                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand_is)(REGS *regs, struct ec *ec, U16 is)
{
  int csl;                             /* Complete symbol length              */
  unsigned cw;                         /* Characters written                  */
  U16 index;

#ifdef FEATURE_INTERVAL_TIMER
  GREG dictor;                         /* Dictionary origin                   */
#endif

  BYTE *ece;                           /* Expansion Character Entry           */
  int psl;                             /* Partial symbol length               */

  /* Initialize values */
  cw = 0;

#ifdef FEATURE_INTERVAL_TIMER
  dictor = GR1_dictor(regs);
#endif

  /* Get expansion character entry */
  index = is * 8;
  ece = &ec->dict[index / 0x800][index % 0x800];
  ITIMER_SYNC(dictor + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch ece: index %04X\n", is);
  print_ece(ece);
#endif

  /* Process preceded entries */
  psl = ECE_psl(ece);
  while(likely(psl))
  {
    /* Check data exception */
    if(unlikely(psl > 5))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

    /* Count and check for writing child 261 */
    cw += psl;
    if(unlikely(cw > 260))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

    /* Process extension characters in preceded entry */
    memcpy(&ec->oc[ec->ocl + ECE_ofst(ece)], &ece[2], psl);

    /* Get preceding entry */
    index = ECE_pptr(ece) * 8;
    ece = &ec->dict[index / 0x800][index % 0x800];
    ITIMER_SYNC(dictor + index, 8 - 1, regs);

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch ece: index %04X\n", ECE_pptr(ece));
    print_ece(ece);
#endif

    /* Calculate partial symbol length */
    psl = ECE_psl(ece);
  }

  /* Check data exception */
  csl = ECE_csl(ece);
  if(unlikely(!csl || ECE_bit34(ece)))
    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

  /* Count and check for writing child 261 */
  cw += csl;
  if(unlikely(cw > 260))
    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

  /* Process extension characters in unpreceded entry */
  memcpy(&ec->oc[ec->ocl], &ece[1], csl);

  /* Place within cache */
  if(likely(ec->ecwm + cw <= ECACHE_SIZE))
  {
    memcpy(&ec->ec[ec->ecwm], &ec->oc[ec->ocl], cw);
    ec->eci[is] = ec->ecwm;
    ec->ecl[is] = cw;
    ec->ecwm += cw;
  }

  /* Commit in output buffer */
  ec->ocl += cw;
}

/*----------------------------------------------------------------------------*/
/* fetch_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *is)
{
  U32 mask;
  BYTE work[3];

  /* Check if we can read an index symbol */
  if(unlikely(GR_A(r2 + 1, iregs) < 2))
  {
    if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r2 + 1, iregs)))
    {

#ifdef OPTION_CMPSC_DEBUG
      logmsg("fetch_is : reached end of source\n");
#endif

      regs->psw.cc = 0;
      return(-1);
    }
  }

  /* Clear possible fetched 3rd byte */
  work[2] = 0;
  ARCH_DEP(vfetchc)(&work, (GR0_smbsz(regs) + GR1_cbn(iregs) - 1) / 8, GR_A(r2, iregs) & ADDRESS_MAXWRAP(regs), r2, regs);

  /* Get the bits */
  mask = work[0] << 16 | work[1] << 8 | work[2];
  mask >>= (24 - GR0_smbsz(regs) - GR1_cbn(iregs));
  mask &= 0xFFFF >> (16 - GR0_smbsz(regs));
  *is = mask;

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) / 8);

  /* Calculate and set the new compressed-data bit number */
  GR1_setcbn(iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) % 8);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", *is, GR1_cbn(iregs), r2, iregs->GR(r2), r2 + 1, iregs->GR(r2 + 1));
#endif

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_iss                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, struct ec *ec, U16 is[8])
{
  BYTE buf[13];                        /* Buffer for 8 index symbols          */
  unsigned len1;                       /* Lenght in first page                */
  BYTE *mem;                           /* Pointer to maddr or buf             */
  unsigned ofst;                       /* Offset in first page                */
  int smbsz;                           /* Symbol size                         */

  /* Initialize values */
  smbsz = GR0_smbsz(regs);

  /* Fingers crossed that we stay within one page */
  ofst = GR_A(r2, iregs) & 0x7ff;
  if(unlikely(!ec->src))
    ec->src = MADDR((GR_A(r2, iregs) & ~0x7ff) & ADDRESS_MAXWRAP(regs), r2, regs, ACCTYPE_READ, regs->psw.pkey);
  if(likely(ofst + smbsz < 0x800))
  { 
    ITIMER_SYNC(GR_A(r2, iregs), smbsz, regs);
    mem = &ec->src[ofst]; 
  } 
  else
  {
    /* We need data spread over 2 pages */
    len1 = 0x800 - ofst;
    memcpy(buf, &ec->src[ofst], len1);
    ec->src = MADDR((GR_A(r2, iregs) + len1) & ADDRESS_MAXWRAP(regs), r2, regs, ACCTYPE_READ, regs->psw.pkey);
    memcpy(&buf[len1], ec->src, smbsz - len1 + 1);
    mem = buf;
  }

  /* Calculate the 8 index symbols */
  switch(smbsz)
  {
    case 9: /*9-bits */
    {
      /* 0       1        2        3        4        5        6        7        8        */
      /* 012345670 123456701 234567012 345670123 456701234 567012345 670123456 701234567 */
      /* 012345678 012345678 012345678 012345678 012345678 012345678 012345678 012345678 */
      /* 0         1         2         3         4         5         6         7         */
      is[0] = ((mem[0] << 1) | (mem[1] >> 7));
      is[1] = ((mem[1] << 2) | (mem[2] >> 6)) & 0x01ff;
      is[2] = ((mem[2] << 3) | (mem[3] >> 5)) & 0x01ff;
      is[3] = ((mem[3] << 4) | (mem[4] >> 4)) & 0x01ff;
      is[4] = ((mem[4] << 5) | (mem[5] >> 3)) & 0x01ff;
      is[5] = ((mem[5] << 6) | (mem[6] >> 2)) & 0x01ff;
      is[6] = ((mem[6] << 7) | (mem[7] >> 1)) & 0x01ff;
      is[7] = ((mem[7] << 8) | (mem[8]     )) & 0x01ff;
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0       1        2        3        4        5       6        7        8        9        */
      /* 0123456701 2345670123 4567012345 6701234567 0123456701 2345670123 4567012345 6701234567 */
      /* 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 */
      /* 0          1          2          3          4          5          6          7          */
      is[0] = ((mem[0] << 2) | (mem[1] >> 6));
      is[1] = ((mem[1] << 4) | (mem[2] >> 4)) & 0x03ff;
      is[2] = ((mem[2] << 6) | (mem[3] >> 2)) & 0x03ff;
      is[3] = ((mem[3] << 8) | (mem[4]     )) & 0x03ff;
      is[4] = ((mem[5] << 2) | (mem[6] >> 6));
      is[5] = ((mem[6] << 4) | (mem[7] >> 4)) & 0x03ff;
      is[6] = ((mem[7] << 6) | (mem[8] >> 2)) & 0x03ff;
      is[7] = ((mem[8] << 8) | (mem[9]     )) & 0x03ff;
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0       1        2        3       4        5        6        7       8        9        a        */
      /* 01234567012 34567012345 67012345670 12345670123 45670123456 70123456701 23456701234 56701234567 */
      /* 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a */
      /* 0           1           2           3           4           5           6           7           */
      is[0] = ((mem[0] <<  3) | (mem[ 1] >> 5)                );
      is[1] = ((mem[1] <<  6) | (mem[ 2] >> 2)                ) & 0x07ff;
      is[2] = ((mem[2] <<  9) | (mem[ 3] << 1) | (mem[4] >> 7)) & 0x07ff;
      is[3] = ((mem[4] <<  4) | (mem[ 5] >> 4)                ) & 0x07ff;
      is[4] = ((mem[5] <<  7) | (mem[ 6] >> 1)                ) & 0x07ff;
      is[5] = ((mem[6] << 10) | (mem[ 7] << 2) | (mem[8] >> 6)) & 0x07ff;
      is[6] = ((mem[8] <<  5) | (mem[ 9] >> 3)                ) & 0x07ff;
      is[7] = ((mem[9] <<  8) | (mem[10]     )                ) & 0x07ff;
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0       1        2        3       4        5        6       7        8        9       a        b        */
      /* 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 */
      /* 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab */
      /* 0            1            2            3            4            5            6            7            */
      is[0] = ((mem[ 0] << 4) | (mem[ 1] >> 4));
      is[1] = ((mem[ 1] << 8) | (mem[ 2]     )) & 0x0fff;
      is[2] = ((mem[ 3] << 4) | (mem[ 4] >> 4));
      is[3] = ((mem[ 4] << 8) | (mem[ 5]     )) & 0x0fff;
      is[4] = ((mem[ 6] << 4) | (mem[ 7] >> 4));
      is[5] = ((mem[ 7] << 8) | (mem[ 8]     )) & 0x0fff;
      is[6] = ((mem[ 9] << 4) | (mem[10] >> 4));
      is[7] = ((mem[10] << 8) | (mem[11]     )) & 0x0fff;
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0       1        2       3        4        5       6        7       8        9        a       b        c        */
      /* 0123456701234 5670123456701 2345670123456 7012345670123 4567012345670 1234567012345 6701234567012 3456701234567 */
      /* 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc */
      /* 0             1             2             3             4             5             6             7             */
      is[0] = ((mem[ 0] <<  5) | (mem[ 1] >> 3)                 );
      is[1] = ((mem[ 1] << 10) | (mem[ 2] << 2) | (mem[ 3] >> 6)) & 0x1fff;
      is[2] = ((mem[ 3] <<  7) | (mem[ 4] >> 1)                 ) & 0x1fff;
      is[3] = ((mem[ 4] << 12) | (mem[ 5] << 4) | (mem[ 6] >> 4)) & 0x1fff;
      is[4] = ((mem[ 6] <<  9) | (mem[ 7] << 1) | (mem[ 8] >> 7)) & 0x1fff;
      is[5] = ((mem[ 8] <<  6) | (mem[ 9] >> 2)                 ) & 0x1fff;
      is[6] = ((mem[ 9] << 11) | (mem[10] << 3) | (mem[11] >> 5)) & 0x1fff;
      is[7] = ((mem[11] <<  8) | (mem[12]     )                 ) & 0x1fff;
      break;
    }
  }

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, smbsz);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_iss: GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", r2, iregs->GR(r2), r2 + 1, iregs->GR(r2 + 1));
#endif
}

#ifndef NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* print_ece (expansion character entry).                                     */
/*----------------------------------------------------------------------------*/
static void print_ece(BYTE *ece)
{
  int i;
  int prt_detail;

  logmsg("  ece    : ");
  prt_detail = 0;
  for(i = 0; i < 8; i++)
  {
    if(!prt_detail && ece[i])
      prt_detail = 1;
    logmsg("%02X", ece[i]);
  }
  logmsg("\n");
  if(prt_detail)
  {
    if(ECE_psl(ece))
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  pptr   : %04X\n", ECE_pptr(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_psl(ece); i++)
        logmsg(" %02X", ece[i + 2]);
      logmsg("\n");
      logmsg("  ofst   : %02X\n", ECE_ofst(ece));
    }
    else
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  bit34  : %s\n", TRUEFALSE(ECE_bit34(ece)));
      logmsg("  csl    : %d\n", ECE_csl(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_csl(ece); i++)
        logmsg(" %02X", ece[i + 1]);
      logmsg("\n");
    }
  }
}
#endif
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* vstore                                                                     */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(vstore)(int r1, REGS *regs, REGS *iregs, struct ec *ec, BYTE *buf, unsigned len)
{
  unsigned len1;
  unsigned len2;
  BYTE *main1;
  unsigned ofst;
  BYTE *sk;

  /* Check destination size */
  if(unlikely(GR_A(r1 + 1, iregs) < len))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("vstore   : Reached end of destination\n");
#endif

    /* Indicate end of destination */
    regs->psw.cc = 1;
    return(-1);
  }

#ifdef OPTION_CMPSC_DEBUG
  unsigned i;
  unsigned j;
  static BYTE pbuf[2060];
  static unsigned plen = 2061;         /* Impossible value                    */

  if(plen == len && !memcmp(pbuf, buf, plen))
    logmsg(F_GREG " - " F_GREG " Same buffer as previously shown\n", iregs->GR(r1), iregs->GR(r1) + len - 1);
  else
  {
    for(i = 0; i < len; i += 32)
    {
      logmsg(F_GREG, iregs->GR(r1) + i);
      if(i && i + 32 < len && !memcmp(&buf[i], &buf[i - 32], 32))
      {
        for(j = i + 32; j + 32 < len && !memcmp(&buf[j], &buf[j - 32], 32); j += 32);
        if(j > 32)
        {
          logmsg(" - " F_GREG " Same line as above\n" F_GREG, iregs->GR(r1) + j - 32, iregs->GR(r1) + j);
          i = j;
        }
      }
      logmsg(": ");
      for(j = 0; j < 32; j++)
      {
        if(!(j % 8))
          logmsg(" ");
        if(i + j >= len)
          logmsg("  ");
        else
          logmsg("%02X", buf[i + j]);
      }
      logmsg(" | ");
      for(j = 0; j < 32; j++)
      {
        if(i + j >= len)
          logmsg(" ");
        else
        {
          if(isprint(guest_to_host(buf[i + j])))
            logmsg("%c", guest_to_host(buf[i + j]));
          else
            logmsg(".");
        }
      } 
      logmsg(" |\n");
    }
    memcpy(pbuf, buf, len);
    plen = len;
  }
#endif

  /* Fingers crossed that we stay within one page */
  ofst = GR_A(r1, iregs) & 0x7ff;
  if(likely(ofst + len < 0x800))
  {
    if(unlikely(!ec->dest))
      ec->dest = MADDR((GR_A(r1, iregs) & ~0x7ff) & ADDRESS_MAXWRAP(regs), r1, regs, ACCTYPE_WRITE, regs->psw.pkey);
    memcpy(&ec->dest[ofst], buf, len);
    ITIMER_UPDATE(GR_A(r1, iregs), len - 1, regs);
  }
  else
  {
    /* We need multiple pages */
    if(unlikely(!ec->dest))
      main1 = MADDR((GR_A(r1, iregs) & ~0x7ff) & ADDRESS_MAXWRAP(regs), r1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    else
      main1 = ec->dest;
    sk = regs->dat.storkey;
    len1 = 0x800 - ofst;
    ec->dest = MADDR((GR_A(r1, iregs) + len1) & ADDRESS_MAXWRAP(regs), r1, regs, ACCTYPE_WRITE, regs->psw.pkey);
    memcpy(&main1[ofst], buf, len1);
    len2 = len - len1 + 1;
    while(len2)
    {
      memcpy(ec->dest, &buf[len1], (len2 > 0x800 ? 0x800 : len2));
      *sk |= (STORKEY_REF | STORKEY_CHANGE);
      if(unlikely(len2 > 0x800))
      {
        len1 += 0x800;
        len2 -= 0x800;
        ec->dest = MADDR((GR_A(r1, iregs) + len1) & ADDRESS_MAXWRAP(regs), r1, regs, ACCTYPE_WRITE, regs->psw.pkey);
        sk = regs->dat.storkey;
      }
      else
        len2 = 0;
    }
  }
  
  ADJUSTREGS(r1, regs, iregs, len);
  return(0); 
}

#define NO_2ND_COMPILE
#endif /* FEATURE_COMPRESSION */

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "cmpsc.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "cmpsc.c"
#endif

#endif /*!defined(_GEN_ARCH) */
