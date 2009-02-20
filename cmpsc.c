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
//
// $Log$
// Revision 1.66  2008/11/01 09:57:49  bernard
// Added likely, unlikely
//
// Revision 1.65  2008/10/26 13:26:39  bernard
// Store_is will zero the remaining bits in the working byte, just like
// other famous manufacturers will do. All former generated compressed
// code will expand correctly, don't worry!! But their can be a difference
// in the compressed output. (This is the case when bits are on, but those
// are the unused bits after the last index symbol).
//
// Revision 1.64  2008/10/24 15:03:21  bernard
// update copyright notice to current year 2008
//
// Revision 1.63  2008/09/24 06:57:47  bernard
// No need to set data exception code to iregs.
//
// Revision 1.62  2008/02/14 07:21:32  bernard
// added last likely / unlikely
//
// Revision 1.61  2008/02/13 08:50:11  bernard
// The cpu determined amount of data is now 16384 and it is determined
// on 16k uncompressed data processing in expand and compress.
//
// Revision 1.60  2008/02/11 12:15:36  bernard
// Errors in intermediate registers. They did not harm yet!
//
// Revision 1.59  2008/01/19 08:24:47  bernard
// Grouped data exceptions to the fetching functions. No functional change.
//
// Revision 1.58  2008/01/16 22:37:01  fish
// Data exception test for nonzero bit34 ece entries should ONLY be for UNPRECEDED ece entries, NOT ALL ece entries.
//
// Revision 1.57  2008/01/01 16:57:42  bernard
// Performance patch
//
// Revision 1.56  2008/01/01 12:10:18  bernard
// Added bit34 ece data exception check
//
// Revision 1.55  2007/12/31 23:48:21  fish
// To prevent PC loop, ensure proper Data Exception code is always used:
//   regs->dxc = DXC_DECIMAL;
//   iregs.dxc = DXC_DECIMAL;
//
// Revision 1.54  2007/12/31 17:45:15  bernard
// Data exception on fetching cce, ece and sd
//
// Revision 1.53  2007/12/31 13:14:43  bernard
// Added data exeption during expansion when psl and cls are zero
//
// Revision 1.52  2007/12/24 14:06:00  bernard
// compress check output, input swapped to input (cc0), output (cc1)
//
// Revision 1.51  2007/06/23 00:04:04  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.50  2007/02/10 09:14:32  bernard
// Decompression performance patch
//
// Revision 1.49  2007/01/13 07:11:45  bernard
// backout ccmask
//
// Revision 1.48  2007/01/12 15:21:12  bernard
// ccmask phase 1
//
// Revision 1.47  2006/12/08 09:43:18  jj
// Add CVS message log
//

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

/* Work in progress */
//#define WIP

/*----------------------------------------------------------------------------*/
/* Debugging options:                                                         */
/*   0x00: Debug CMPSC calling                                                */
/*   0x01: Debug compression                                                  */
/*   0x02: Debug expansion                                                    */
/*----------------------------------------------------------------------------*/
//#define OPTION_CMPSC_DEBUGLVL 3      /* Debug all                           */
#if 0
#define OPTION_CMPSC_EXPAND_DEBUG
#define OPTION_CMPSC_COMPRESS_DEBUG
#endif

/******************************************************************************/
/******************************************************************************/
/* The first part are macro's that just implements the definitions described  */
/* in the manuals.                                                            */
/******************************************************************************/
/******************************************************************************/

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
#define CCE_act(cce)         (SBITS((cce), 8, 10))
#define CCE_cct(cce)         (SBITS((cce), 0, 2))
#define CCE_cptr(cce)        ((SBITS((cce), 11, 15) << 8) | (cce)[2])
#define CCE_d(cce)           (STBIT((cce), 10))
#define CCE_x(cce, i)        (STBIT((cce), (i) + 3))
#define CCE_y(cce, i)        (STBIT((cce), (i) + 8))

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* bit34 : Value of bits 3 and 4 (what else ;-)                               */
/* csl   : complete symbol length                                             */
/* ofst  : offset from current position in output area                        */
/* pptr  : predecessor pointer                                                */
/* psl   : partial symbol length                                              */
/*----------------------------------------------------------------------------*/
#define ECE_bit34(ece)       (SBITS((ece), 3, 4))
#define ECE_csl(ece)         (SBITS((ece), 5, 7))
#define ECE_ofst(ece)        ((ece)[7])
#define ECE_pptr(ece)        ((SBITS((ece), 3, 7) << 8) | ((ece)[1]))
#define ECE_psl(ece)         (SBITS((ece), 0, 2))

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
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for siblings 1 to 5                             */
/*----------------------------------------------------------------------------*/
#define SD0_sct(sd0)         (SBITS((sd0), 0, 2))
#define SD0_y(sd0, i)        (STBIT((sd0), (i) + 3))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for sibling 1 to 12                             */
/*----------------------------------------------------------------------------*/
#define SD1_sct(sd1)         (SBITS((sd1), 0, 3))
#define SD1_y(sd1, i)        (STBIT((sd1), (i) + 4))

/******************************************************************************/
/******************************************************************************/
/* The next part are definitions that are abstracted from the definitions     */
/* described in the book.                                                     */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE)                                  */
/*----------------------------------------------------------------------------*/
/* cc(i)  : child character                                                   */
/* ccc(i) : indication consecutive child character                            */
/* ccs    : number of child characters                                        */
/* ec(i)  : additional extension character                                    */
/* ecs    : number of additional extension characters                         */
/* mcc    : indication if siblings follow child characters                    */
/*----------------------------------------------------------------------------*/
#define CCE_cc(cce, i)       ((&(&(cce)[3])[CCE_ecs((cce))])[(i)])
#define CCE_ccc(cce, i)      (CCE_cc((cce), (i)) == CCE_cc((cce), 0))
#define CCE_ccs(cce)         (CCE_cct((cce)) - (CCE_mcc((cce)) ? 1 : 0))
#define CCE_ec(cce, i)       ((&(cce)[3])[(i)])
#define CCE_ecs(cce)         ((CCE_cct((cce)) <= 1) ? CCE_act((cce)) : (CCE_d((cce)) ? 1 : 0))
#define CCE_mcc(cce)         ((CCE_cct((cce)) + (CCE_d((cce)) ? 1 : 0) == 6))

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* ec    : address of first extension character                               */
/* upr   : indication wheter entry is unpreceded                              */
/*----------------------------------------------------------------------------*/
#define ECE_ec(ece)          (ECE_upr((ece)) ? &(ece)[1] : &(ece)[2])
#define ECE_upr(ece)         (!ECE_psl((ece)))

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* dctsz      : dictionary size                                               */
/* smbsz      : symbol size                                                   */
/*----------------------------------------------------------------------------*/
#define GR0_dctsz(regs)      (0x800 << GR0_cdss((regs)))
#define GR0_smbsz(regs)      (GR0_cdss((regs)) + 8)

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
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
#define SD0_sc(sd0, i)       ((&(sd0)[1])[(i)])
#define SD0_scs(sd0)         (SD0_msc((sd0)) ? 7 : SD0_sct((sd0)))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 13th/14th fetched from parent        */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD1_ccc(sd1, i)      (SD1_sc((sd1), (i)) == SD1_sc((sd1), 0))
#define SD1_ecb(sd1, i, cce, y) (((i) < 12) ? SD1_y((sd1), (i)) : (y) ? CCE_y((cce), ((i) - 12)) : 1)
#define SD1_msc(sd1)         ((SD1_sct((sd1)) == 15))
#define SD1_sc(sd1,i)        ((&(sd1)[2])[(i)])
#define SD1_scs(sd1)         (SD1_msc((sd1)) ? 14 : SD1_sct((sd1)))

/*----------------------------------------------------------------------------*/
/* Format independent sibling descriptor macro's                              */
/*----------------------------------------------------------------------------*/
#define SD_ccc(regs, sd, i)  (GR0_f1((regs)) ? SD1_ccc((sd), (i)) : SD0_ccc((sd), (i)))
#define SD_ecb(regs, sd, i, cce, y) (GR0_f1((regs)) ? SD1_ecb((sd), (i), (cce), (y)) : SD0_ecb((sd), (i), (cce), (y)))
#define SD_msc(regs, sd)     (GR0_f1((regs)) ? SD1_msc((sd)) : SD0_msc((sd)))
#define SD_sc(regs, sd, i)   (GR0_f1((regs)) ? SD1_sc((sd), (i)) : SD0_sc((sd), (i)))
#define SD_scs(regs, sd)     (GR0_f1((regs)) ? SD1_scs((sd)) : SD0_scs((sd)))

/******************************************************************************/
/******************************************************************************/
/* This part are macro's that handle the bit operations.                      */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Bit operation macro's                                                      */
/*----------------------------------------------------------------------------*/
/* TBIT   : return bit in byte                                                */
/* BITS   : return bits in byte                                               */
/* STBIT  : return bit in bytes                                               */
/* SBITS  : return bits in bytes (bits must be in one byte!)                  */
/*----------------------------------------------------------------------------*/
#define TBIT(byte, bit)      ((byte) & (0x80 >> (bit)))
#define BITS(byte, start, end) (((BYTE)((byte) << (start))) >> (7 - (end) + (start)))
#define STBIT(bytes, bit)    (TBIT((bytes)[(bit) / 8], (bit) % 8))
#define SBITS(bytes, strt, end) (BITS((bytes)[(strt) / 8], (strt) % 8, (end) % 8))

/******************************************************************************/
/******************************************************************************/
/* These last macro's do make the the code more readable. I hope.             */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* The next macro sets the compressed bit number in GR1                       */
/*----------------------------------------------------------------------------*/
#define GR1_setcbn(regs, cbn) ((regs)->GR_L(1) = ((regs)->GR_L(1) & 0xFFFFFFF8) | ((cbn) & 0x00000007))

/*----------------------------------------------------------------------------*/
/* After a succesful compression of characters to an index symbol or a        */
/* succsful translation of an index symbol to characters, the registers must  */
/* be updated.                                                                */
/*----------------------------------------------------------------------------*/
#define ADJUSTREGS(r, regs, iregs, len) \
{\
  SET_GR_A((r), (iregs), (GR_A((r), (iregs)) + (len)) & ADDRESS_MAXWRAP((regs)));\
  SET_GR_A((r) + 1, (iregs), GR_A((r)+1, (iregs)) - (len));\
}

/*----------------------------------------------------------------------------*/
/* One of my first implementations was faulty on a page fault. Greg Smith     */
/* implemented the technique of synchronizing the registers. To keep the      */
/* instruction atomic, we copy intermediate registers to the real ones when a */
/* full compression or expansion occurred. Thanks Greg!                       */
/*----------------------------------------------------------------------------*/
#define COMMITREGS(regs, iregs, r1, r2) \
{\
  SET_GR_A(1, (regs), GR_A(1, (iregs)));\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

/* Same as COMMITREGS, except for GR1 */
#define COMMITREGS2(regs, iregs, r1, r2) \
{\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

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
/* During compression and expansion a data exception is recognized on writing */
/* or searching child number 261. The next macro's does the counting and      */
/* checking.                                                                  */
/*----------------------------------------------------------------------------*/
#if !defined(TESTCH261)
  #if defined(OPTION_CMPSC_DEBUGLVL) && (OPTION_CMPSC_DEBUGLVL & 3)
    #define TESTCH261(regs, processed, length) \
      if(unlikely(((processed) += (length)) > 260)) \
      { \
        logmsg("writing or searching child 261 -> data exception\n"); \
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION); \
      }
  #else /* !defined(OPTION_CMPSC_DEBUGLVL) || !(OPTION_CMPSC_DEBUGLVL & 3) */
    #define TESTCH261(regs, processed, length) \
      if(unlikely(((processed) += (length)) > 260)) \
      { \
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION); \
      }
  #endif /* defined(OPTION_CMPSC_DEBUGLVL) && (OPTION_CMPSC_DEBUGLVL & 3) */
#endif /* !defined(TESTCH261) */

/******************************************************************************/
/******************************************************************************/
/* Here is the last part of definitions. The constants,typedefs and function  */
/* proto types.                                                               */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX          65536     /* CPU-determined amount of data       */ 
#define TRUEFALSE(boolean)   ((boolean) ? "True" : "False")

/*----------------------------------------------------------------------------*/
/* Compression status enumeration for communicating between compress and      */
/* search_cce and search_sd.                                                  */
/*----------------------------------------------------------------------------*/
#if !defined(CMPSC_STATUS_DEFINED)
#define CMPSC_STATUS_DEFINED
enum cmpsc_status
{
  end_of_source,
  parent_found,
  search_siblings,
  write_index_symbol
};
#endif /* !defined(CMPSC_STATUS_DEFINED) */

/*----------------------------------------------------------------------------*/
/* Function proto types                                                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs);
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs);
static int ARCH_DEP(expand_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is);
static void ARCH_DEP(fetch_cce)(int r2, REGS *regs, BYTE *cce, int index);
static int ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int offset);
static void ARCH_DEP(fetch_ece)(int r2, REGS *regs, BYTE *ece, int index);
static int ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *index_symbol);
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, U16 is[8]);
static void ARCH_DEP(fetch_sd)(int r2, REGS *regs, BYTE *sd, int index);
static enum cmpsc_status ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *next_ch, U16 *last_match);
static enum cmpsc_status ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *next_ch, U16 *last_match);
static int ARCH_DEP(store_ch)(int r1, REGS *regs, REGS *iregs, BYTE *data, int length, int offset);
static void ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 index_symbol);
#ifdef WIP
static void ARCH_DEP(store_iss)(int r1, int r2, REGS *regs, REGS *iregs, U16 is[8]);
#endif
static int ARCH_DEP(test_ec)(int r2, REGS *regs, REGS *iregs, BYTE *cce);

/******************************************************************************/
/******************************************************************************/
/* Ok, here is the start of the code!                                         */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* compress                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs)
{
  BYTE cce[8];                         /* compression character entry         */
  int eos;                             /* indication end of source            */
  GREG exit_value;                     /* return cc=3 on this value           */
  U16 last_match;                      /* Last matched index symbol           */
  BYTE next_ch=0;                      /* next character read                 */

  /* Initialize end of source */
  eos = 0;

  /* Try to process the CPU-determined amount of data */
  if(likely(GR_A(r2 + 1, regs) <= PROCESS_MAX))
    exit_value = 0;
  else
    exit_value = GR_A(r2 + 1, regs) - PROCESS_MAX;
  while(exit_value <= GR_A(r2 + 1, regs))
  {

    /* Get the next character, return on end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(r2, regs, iregs, &next_ch, 0)))
      return;

    /* Can we write an index or interchange symbol */
    if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r1 + 1, iregs)))
    {
      regs->psw.cc = 1;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
      logmsg("compress : end of output buffer\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

      return;
    }

    /* Get the alphabet entry */
    ARCH_DEP(fetch_cce)(r2, regs, cce, next_ch);

    /* We always match the alpabet entry, so set last match */
    ADJUSTREGS(r2, regs, iregs, 1);
    last_match = next_ch;

    /* As long there is a parent */
    while(1)
    {

      /* Try to find a child in compression character entry */
      switch(ARCH_DEP(search_cce)(r2, regs, iregs, cce, &next_ch, &last_match))
      {
        case parent_found:
          continue;

        case search_siblings:

          /* Try to find a child in the sibling descriptors */
          if(likely(ARCH_DEP(search_sd)(r2, regs, iregs, cce, &next_ch, &last_match) == parent_found))
            continue;
          break;

        case end_of_source:
          eos = 1;
          break;

        default:
          break;
      }

      /* No parent found, write index symbol */
      break;
    }

    /* Write the last match, this can be the alphabet entry */
    ARCH_DEP(store_is)(r1, r2, regs, iregs, last_match);

    /* Commit registers, we have completed a full compression */
    COMMITREGS(regs, iregs, r1, r2);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
    logmsg("compress : registers committed\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

    /* When reached end of source, return to caller */
    if(unlikely(eos))
      return;
  }

  /* Reached model dependent CPU processing amount */
  regs->psw.cc = 3;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg("compress : reached CPU determined amount of data\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

}

/*----------------------------------------------------------------------------*/
/* expand                                                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs)
{
  int cw;                              /* Characters written                  */
  int i;
  U16 is;                              /* Index symbol                        */
  U16 iss[8];                          /* Index symbols                       */
  int rc;                              /* Return code                         */
  unsigned smbsz;                      /* Symbol size                         */

  /* Initialize values */
  cw = 0;
  smbsz = GR0_smbsz(regs);

  /* Process individual index symbols until cbn becomes zero */
  if(unlikely(GR1_cbn(regs)))
  {
    while(likely(GR1_cbn(regs)))
    {
      if(unlikely(ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
        return;
      rc = ARCH_DEP(expand_is)(r1, r2, regs, iregs, is);
      if(unlikely(rc == -1))
        return;
      cw += rc;
    }

    /* Commit, including GR1 */
    COMMITREGS(regs, iregs, r1, r2);

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("Registers committed\n");
#endif
  }

  /* Block processing, cbn stays zero */
  while(likely(GR_A(r2 + 1, iregs) >= smbsz && cw < PROCESS_MAX))
  {
    ARCH_DEP(fetch_iss)(r2, regs, iregs, iss);
    for(i = 0; i < 8; i++)
    {
#ifdef OPTION_CMPSC_EXPAND_DEBUG
      logmsg("expand   : is %04X (%d)\n", iss[i], i);
#endif
      rc = ARCH_DEP(expand_is)(r1, r2, regs, iregs, iss[i]);
      if(unlikely(rc == -1))
        return;
      cw += rc;
    }

    /* Commit, cbn unchanged so except for GR1 */
    COMMITREGS2(regs, iregs, r1, r2);

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("Registers committed\n");
#endif 
  }

  if(unlikely(cw >= PROCESS_MAX))
  {
    regs->psw.cc = 3;

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("expand   : reached CPU determined amount of data\n");
#endif

    return;
  }

  /* Process last index symbols, never mind about childs written */
  while(unlikely(!ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
  {
    if(unlikely(ARCH_DEP(expand_is)(r1, r2, regs, iregs, is) == -1))
      return;
  }

  /* Commit, including GR1 */
  COMMITREGS(regs, iregs, r1, r2);

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("Registers committed\n");
#endif 
 
}

/*----------------------------------------------------------------------------*/
/* expand_is                                                                  */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(expand_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is)
{
  BYTE b;                              /* a byte                              */
  int cw;                              /* Characters written                  */
  BYTE ece[8];                         /* Expansion Character Entry           */
  int eces;                            /* Entries processed                   */
  U16 pptr;                            /* predecessor pointer                 */

  /* Check if this is an alphabet entry */
  if(unlikely(is <= 0xff))
  {

    /* Write the alphabet entry, return on trouble */
    b = is;
    if(unlikely(ARCH_DEP(store_ch)(r1, regs, iregs, &b, 1, 0)))
      return(-1);
    cw = 1;
  }
  else
  {

    /* Get the Expansion character entry */
    ARCH_DEP(fetch_ece)(r2, regs, ece, is);
    eces = 1;
    cw = 0;

    /* Process the whole tree to the top */
    while(likely(!ECE_upr(ece)))
    {
      /* Count and check for writing child 261 */
      cw += ECE_psl(ece);
      if(unlikely(cw > 260))
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

      /* Output extension characters in preceded entry, return on trouble */
      if(unlikely(ARCH_DEP(store_ch)(r1, regs, iregs, ECE_ec(ece), ECE_psl(ece), ECE_ofst(ece))))
        return(-1);

      /* Get the preceding entry */
      pptr = ECE_pptr(ece);
      ARCH_DEP(fetch_ece)(r2, regs, ece, pptr);
      eces += 1;

      /* Check for processing entry 128 */
      if(unlikely(eces > 127))
        ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
    }

    /* Count and check for writing child 261 */
    cw += ECE_csl(ece);
    if(unlikely(cw > 260))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

    /* Output extension characters in last or only unpreceded entry, return on trouble */
    if(unlikely(unlikely(ARCH_DEP(store_ch)(r1, regs, iregs, ECE_ec(ece), ECE_csl(ece), 0))))
      return(-1);
  }

  /* Adjust registers */
  ADJUSTREGS(r1, regs, iregs, cw);
  return(cw);
}

/*----------------------------------------------------------------------------*/
/* fetch_cce (compression character entry).                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_cce)(int r2, REGS *regs, BYTE *cce, int index)
{
  ARCH_DEP(vfetchc)((cce), 7, (GR1_dictor((regs)) + (index) * 8) & ADDRESS_MAXWRAP((regs)), (r2), (regs));

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  int i;
  int prt_detail;

  logmsg("fetch_cce: index %04X\n", index);
  logmsg("  cce    : ");
  prt_detail = 0;
  for(i = 0; i < 8; i++)
  {
    if(!prt_detail && cce[i])
      prt_detail = 1;
    logmsg("%02X", cce[i]);
  }
  logmsg("\n");
  if(prt_detail)
  {
    logmsg("  cct    : %d\n", CCE_cct(cce));
    logmsg("  x1..x5 : ");
    for(i = 0; i < 5; i++)
      logmsg("%c", (int) (CCE_x(cce, i) ? '1' : '0'));
    logmsg("\n  act    : %d\n", (int) CCE_ecs(cce));
    logmsg("  y1..y2 : ");
    for(i = 0; i < 2; i++)
      logmsg("%c", (int) (CCE_y(cce, i) ? '1' : '0'));
    logmsg("\n  d      : %s\n", TRUEFALSE(CCE_d(cce)));
    logmsg("  cptr   : %04X\n", CCE_cptr(cce));
    logmsg("  mcc    > %s\n", TRUEFALSE(CCE_mcc(cce)));
    logmsg("  ecs    >");
    for(i = 0; i < CCE_ecs(cce); i++)
      logmsg(" %02X", CCE_ec(cce, i));
    logmsg("\n  ccs    >");
    for(i = 0; i < CCE_ccs(cce); i++)
      logmsg(" %02X", CCE_cc(cce, i));
    logmsg("\n");
  }
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  /* Check for data exceptions */
  if(CCE_cct(cce) < 2)
  {
    if(unlikely(CCE_act(cce) > 4))
      ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
  }
  else
  {
    if(!CCE_d(cce))
    {
      if(unlikely(CCE_cct(cce) == 7))
        ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
    }
    else
    {
      if(unlikely(CCE_cct(cce) > 5))
        ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
    }
  }
}

/*----------------------------------------------------------------------------*/
/* fetch_ch (character)                                                       */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int offset)
{
  /* Check for end of source condition */
  if(unlikely(GR_A(r2 + 1, iregs) <= (U32) offset))
  {

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
    logmsg("fetch_ch : reached end of source\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

    regs->psw.cc = 0;
    return(1);
  }
  *ch = ARCH_DEP(vfetchb)((GR_A(r2, iregs) + offset) & ADDRESS_MAXWRAP(regs), r2, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg("fetch_ch : %02X at " F_VADR "\n", *ch, (GR_A(r2, iregs) + offset));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_ece (expansion character entry).                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_ece)(int r2, REGS *regs, BYTE *ece, int index)
{
  ARCH_DEP(vfetchc)((ece), 7, (GR1_dictor((regs)) + (index) * 8) & ADDRESS_MAXWRAP((regs)), (r2), (regs));

#ifdef OPTION_CMPSC_EXPAND_DEBUG
  int i;
  int prt_detail;

  logmsg("fetch_ece: index %04X\n", index);
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
    if(!ECE_psl(ece))
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  bit34  : %s\n", TRUEFALSE(ECE_bit34(ece)));
      logmsg("  csl    : %d\n", ECE_csl(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_csl(ece); i++)
        logmsg(" %02X", ECE_ec(ece)[i]);
      logmsg("\n");
    }
    else
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  pptr   : %04X\n", ECE_pptr(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_psl(ece); i++)
        logmsg(" %02X", ECE_ec(ece)[i]);
      logmsg("\n");
      logmsg("  ofst   : %02X\n", ECE_ofst(ece));
    }
  }
#endif 

  /* Check for data exceptions */
  if(ECE_psl(ece))
  {
    if(unlikely(ECE_psl(ece) > 5))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);
  }
  else
  {
    if(unlikely(!ECE_csl(ece) || ECE_bit34(ece)))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);
  }
}

/*----------------------------------------------------------------------------*/
/* fetch_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *index_symbol)
{
  U32 mask;
  BYTE work[3];

  /* Check if we can read an index symbol */
  if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r2 + 1, iregs)))
  {

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("fetch_is : reached end of source\n");
#endif

    regs->psw.cc = 0;
    return(-1);
  }

  /* Clear possible fetched 3rd byte */
  work[2] = 0;
  ARCH_DEP(vfetchc)(&work, (GR0_smbsz(regs) + GR1_cbn(iregs) - 1) / 8, GR_A(r2, iregs) & ADDRESS_MAXWRAP(regs), r2, regs);

  /* Get the bits */
  mask = work[0] << 16 | work[1] << 8 | work[2];
  mask >>= (24 - GR0_smbsz(regs) - GR1_cbn(iregs));
  mask &= 0xFFFF >> (16 - GR0_smbsz(regs));
  *index_symbol = mask;

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) / 8);

  /* Calculate and set the new compressed-data bit number */
  GR1_setcbn(iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) % 8);

#ifdef OPTION_CMPSC_EXPAND_DEBUG
  logmsg("fetch_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", *index_symbol, GR1_cbn(iregs), r2, iregs->GR(r2), r2 + 1, iregs->GR(r2 + 1));
#endif

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_iss                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, U16 is[8])
{
  BYTE buf[13];
  int smbsz;

  /* Initialize values */
  smbsz = GR0_smbsz(regs);

  /* Fetch symbol size bytes for 8 index symbols */
  ARCH_DEP(vfetchc)(&buf, smbsz - 1, GR_A(r2, iregs) & ADDRESS_MAXWRAP(regs), r2, regs);

  /* Calculate the 8 index symbols */
  switch(smbsz)
  {
    case 9: /*9-bits */
    {
      /* 0       1        2        3        4        5        6        7        8        */
      /* 012345670 123456701 234567012 345670123 456701234 567012345 670123456 701234567 */
      /* 012345678 012345678 012345678 012345678 012345678 012345678 012345678 012345678 */
      /* 0         1         2         3         4         5         6         7         */
      is[0] = ((buf[0]       ) << 1) | (buf[1] >> 7);
      is[1] = ((buf[1] & 0x7f) << 2) | (buf[2] >> 6);
      is[2] = ((buf[2] & 0x3f) << 3) | (buf[3] >> 5);
      is[3] = ((buf[3] & 0x1f) << 4) | (buf[4] >> 4);
      is[4] = ((buf[4] & 0x0f) << 5) | (buf[5] >> 3);
      is[5] = ((buf[5] & 0x07) << 6) | (buf[6] >> 2);
      is[6] = ((buf[6] & 0x03) << 7) | (buf[7] >> 1);
      is[7] = ((buf[7] & 0x01) << 8) | (buf[8]     );
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0       1        2        3        4        5       6        7        8        9        */
      /* 0123456701 2345670123 4567012345 6701234567 0123456701 2345670123 4567012345 6701234567 */
      /* 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 */
      /* 0          1          2          3          4          5          6          7          */
      is[0] = ((buf[0]       ) << 2) | (buf[1] >> 6);
      is[1] = ((buf[1] & 0x3f) << 4) | (buf[2] >> 4);
      is[2] = ((buf[2] & 0x0f) << 6) | (buf[3] >> 2);
      is[3] = ((buf[3] & 0x03) << 8) | (buf[4]     );
      is[4] = ((buf[5]       ) << 2) | (buf[6] >> 6);
      is[5] = ((buf[6] & 0x3f) << 4) | (buf[7] >> 4);
      is[6] = ((buf[7] & 0x0f) << 6) | (buf[8] >> 2);
      is[7] = ((buf[8] & 0x03) << 8) | (buf[9]     );
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0       1        2        3       4        5        6        7       8        9        a        */
      /* 01234567012 34567012345 67012345670 12345670123 45670123456 70123456701 23456701234 56701234567 */
      /* 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a */
      /* 0           1           2           3           4           5           6           7           */
      is[0] = ((buf[0]       ) <<  3) | (buf[ 1] >> 5);
      is[1] = ((buf[1] & 0x1f) <<  6) | (buf[ 2] >> 2);
      is[2] = ((buf[2] & 0x03) <<  9) | (buf[ 3] << 1) | (buf[4] >> 7);
      is[3] = ((buf[4] & 0x7f) <<  4) | (buf[ 5] >> 4);
      is[4] = ((buf[5] & 0x0f) <<  7) | (buf[ 6] >> 1);
      is[5] = ((buf[6] & 0x01) << 10) | (buf[ 7] << 2) | (buf[8] >> 6);
      is[6] = ((buf[8] & 0x3f) <<  5) | (buf[ 9] >> 3);
      is[7] = ((buf[9] & 0x07) <<  8) | (buf[10]     );
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0       1        2        3       4        5        6       7        8        9       a        b        */
      /* 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 */
      /* 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab */
      /* 0            1            2            3            4            5            6            7            */
      is[0] = ((buf[ 0]       ) << 4) | (buf[ 1] >> 4);
      is[1] = ((buf[ 1] & 0x0f) << 8) | (buf[ 2]     );
      is[2] = ((buf[ 3]       ) << 4) | (buf[ 4] >> 4);
      is[3] = ((buf[ 4] & 0x0f) << 8) | (buf[ 5]     );
      is[4] = ((buf[ 6]       ) << 4) | (buf[ 7] >> 4);
      is[5] = ((buf[ 7] & 0x0f) << 8) | (buf[ 8]     );
      is[6] = ((buf[ 9]       ) << 4) | (buf[10] >> 4);
      is[7] = ((buf[10] & 0x0f) << 8) | (buf[11]     );
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0       1        2       3        4        5       6        7       8        9        a       b        c        */
      /* 0123456701234 5670123456701 2345670123456 7012345670123 4567012345670 1234567012345 6701234567012 3456701234567 */
      /* 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc */
      /* 0             1             2             3             4             5             6             7             */
      is[0] = ((buf[ 0]       ) <<  5) | (buf[ 1] >> 3);
      is[1] = ((buf[ 1] & 0x07) << 10) | (buf[ 2] << 2) | (buf[ 3] >> 6);
      is[2] = ((buf[ 3] & 0x3f) <<  7) | (buf[ 4] >> 1);
      is[3] = ((buf[ 4] & 0x01) << 12) | (buf[ 5] << 4) | (buf[ 6] >> 4);
      is[4] = ((buf[ 6] & 0x0f) <<  9) | (buf[ 7] << 1) | (buf[ 8] >> 7);
      is[5] = ((buf[ 8] & 0x7f) <<  6) | (buf[ 9] >> 2);
      is[6] = ((buf[ 9] & 0x03) << 11) | (buf[10] << 3) | (buf[11] >> 5);
      is[7] = ((buf[11] & 0x1f) <<  8) | (buf[12]     );
      break;
    }
  }

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, smbsz);
}

/*----------------------------------------------------------------------------*/
/* fetch_sd (sibling descriptor).                                             */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_sd)(int r2, REGS *regs, BYTE *sd, int index)
{
  ARCH_DEP(vfetchc)((sd), 7, (GR1_dictor((regs)) + (index) * 8) & ADDRESS_MAXWRAP((regs)), (r2), (regs));
  if(GR0_f1(regs))
    ARCH_DEP(vfetchc)(&(sd)[8], 7, (GR1_dictor((regs)) + GR0_dctsz((regs)) + (index) * 8) & ADDRESS_MAXWRAP((regs)), r2, (regs));

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  int i;
  int prt_detail;

  if(GR0_f1(regs))
  {
    logmsg("fetch_sd1: index %04X\n", index);
    logmsg("  sd1    : ");
    prt_detail = 0;
    for(i = 0; i < 16; i++)
    {
      if(!prt_detail && sd[i])
        prt_detail = 1;
      logmsg("%02X", sd[i]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD1_sct(sd));
      logmsg("  y1..y12: ");
      for(i = 0; i < 12; i++)
        logmsg("%c", (SD1_y(sd, i) ? '1' : '0'));
      logmsg("\n  msc    > %s\n", TRUEFALSE(SD1_msc(sd)));
      logmsg("  sc's   >");
      for(i = 0; i < SD1_scs(sd); i++)
        logmsg(" %02X", SD1_sc(sd, i));
      logmsg("\n");
    }
  }
  else
  {
    logmsg("fetch_sd0: index %04X\n", index);
    logmsg("  sd0    : ");
    prt_detail = 0;
    for(i = 0; i < 8; i++)
    {
      if(!prt_detail && sd[i])
        prt_detail = 1;
      logmsg("%02X", sd[i]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD0_sct(sd));
      logmsg("  y1..y5 : ");
      for(i = 0; i < 5; i++)
        logmsg("%c", (SD0_y(sd, i) ? '1' : '0'));
      logmsg("\n  msc    > %s\n", TRUEFALSE(SD0_msc(sd)));
      logmsg("  sc's   >");
      for(i = 0; i < SD0_scs(sd); i++)
        logmsg(" %02X", SD0_sc(sd, i));
      logmsg("\n");
    }
  }
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  /* Check for data exceptions */
  if(unlikely((GR0_f1(regs) && !SD1_sct(sd))))
  {

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
    logmsg("fetch_sd : format-1 sd and sct=0 -> data exception\n");
#endif

    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);
  }
}

/*----------------------------------------------------------------------------*/
/* search_cce (compression character entry)                                   */
/*----------------------------------------------------------------------------*/
static enum cmpsc_status ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *next_ch, U16 *last_match)
{
  BYTE ccce[8];                        /* child compression character entry   */
  int i;                               /* child character index               */
  int ind_search_siblings;             /* Indicator for searching siblings    */

  ind_search_siblings = 1;

  /* Get the next character when there are children */
  if(unlikely((CCE_ccs(cce) && ARCH_DEP(fetch_ch)(r2, regs, iregs, next_ch, 0))))
  {

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
    logmsg("search_cce: end of source\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2 */

    return(end_of_source);
  }

  /* Now check all children in parent */
  for(i = 0; i < CCE_ccs(cce); i++)
  {

    /* Stop searching when child tested and no consecutive child character */
    if(unlikely(!ind_search_siblings && !CCE_ccc(cce, i)))
      return(write_index_symbol);

    /* Compare character with child */
    if(unlikely(*next_ch == CCE_cc(cce, i)))
    {

      /* Child is tested, so stop searching for siblings*/
      ind_search_siblings = 0;

      /* Check if child should not be examined */
      if(unlikely(!CCE_x(cce, i)))
      {

        /* No need to examine child, found the last match */
        ADJUSTREGS(r2, regs, iregs, 1);
        *last_match = CCE_cptr(cce) + i;

        /* Index symbol found */
        return(write_index_symbol);
      }

      /* Found a child get the character entry */
      ARCH_DEP(fetch_cce)(r2, regs, ccce, CCE_cptr(cce) + i);

      /* Check if additional extension characters match */
      if(likely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
      {

        /* Set last match */
        ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
        *last_match = CCE_cptr(cce) + i;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
        logmsg("search_cce index %04X parent\n", *last_match);
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

        /* Found a matching child, make it parent */
        memcpy(cce, ccce, 8);

        /* We found a parent */
        return(parent_found);
      }
    }
  }

  /* Are there siblings? */
  if(likely(CCE_mcc(cce)))
    return(search_siblings);

  /* No siblings, write found index symbol */
  return(write_index_symbol);
}

/*----------------------------------------------------------------------------*/
/* search_sd (sibling descriptor)                                             */
/*----------------------------------------------------------------------------*/
static enum cmpsc_status ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *next_ch, U16 *last_match)
{
  BYTE ccce[8];                        /* child compression character entry   */
  int i;                               /* sibling character index             */
  int ind_search_siblings;             /* indicator for keep searching        */
  BYTE sd[16];                         /* sibling descriptor fmt-0 and fmt-1  */
  int sd_ptr;                          /* pointer to sibling descriptor       */
  int searched;                        /* number of children searched         */
  int y_in_parent;                     /* indicator if y bits are in parent   */

  /* For now keep searching for siblings */
  ind_search_siblings = 1;

  /* For the first sibling descriptor y bits are in the cce parent */
  y_in_parent = 1;

  /* Sibling follows last possible child */
  sd_ptr = CCE_ccs(cce);

  /* set searched childs */
  searched = sd_ptr;

  /* As long there are sibling characters */
  do
  {

    /* Get the sibling descriptor */
    ARCH_DEP(fetch_sd)(r2, regs, sd, CCE_cptr(cce) + sd_ptr);

    /* Check all children in sibling descriptor */
    for(i = 0; i < SD_scs(regs, sd); i++)
    {

      /* Stop searching when child tested and no consecutive child character */
      if(unlikely(!ind_search_siblings && !SD_ccc(regs, sd, i)))
        return(write_index_symbol);

      if(unlikely(*next_ch == SD_sc(regs, sd, i)))
      {

        /* Child is tested, so stop searching for siblings*/
        ind_search_siblings = 0;

        /* Check if child should not be examined */
        if(unlikely(!SD_ecb(regs, sd, i, cce, y_in_parent)))
        {

          /* No need to examine child, found the last match */
          ADJUSTREGS(r2, regs, iregs, 1);
          *last_match = CCE_cptr(cce) + sd_ptr + i + 1;

          /* Index symbol found */
          return(write_index_symbol);
        }

        /* Found a child get the character entry */
        ARCH_DEP(fetch_cce)(r2, regs, ccce, CCE_cptr(cce) + sd_ptr + i + 1);

        /* Check if additional extension characters match */
        if(unlikely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
        {

          /* Set last match */
          ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
          *last_match = CCE_cptr(cce) + sd_ptr + i + 1;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
          logmsg("search_sd: index %04X parent\n", *last_match);
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

          /* Found a matching child, make it parent */
          memcpy(cce, ccce, 8);

          /* We found a parent */
          return(parent_found);
        }
      }
    }

    /* Next sibling follows last possible child */
    sd_ptr += SD_scs(regs, sd) + 1;

    /* test for searching child 261 */
    TESTCH261(regs, searched, SD_scs(regs, sd));

    /* We get the next sibling descriptor, no y bits in parent for him */
    y_in_parent = 0;

  }
  while(ind_search_siblings && SD_msc(regs, sd));
  return(write_index_symbol);
}

/*----------------------------------------------------------------------------*/
/* store_ch (character)                                                       */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(store_ch)(int r1, REGS *regs, REGS *iregs, BYTE *data, int length, int offset)
{

  /* Check destination size */
  if(unlikely(GR_A(r1 + 1, iregs) < length + (U32) offset))
  {

#ifdef OPTION_CMPSC_EXPAND_DEBUG
    logmsg("store_ch : Reached end of destination\n");
#endif 

    /* Indicate end of destination */
    regs->psw.cc = 1;
    return(-1);
  }

  /* Store the data */
  ARCH_DEP(vstorec)(data, length - 1, (GR_A(r1, iregs) + offset) & ADDRESS_MAXWRAP(regs), r1, regs);

#ifdef OPTION_CMPSC_EXPAND_DEBUG
  logmsg("store_ch : at " F_VADR ", len %04d: ", (iregs->GR(r1) + offset), length);
  {
    int i;

    for(i = 0; i < length; i++)
      logmsg("%02X", data[i]);
    logmsg("\n");
  }
#endif 

  return(0);
}

/*----------------------------------------------------------------------------*/
/* store_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 index_symbol)
{
  U32 set_mask;                        /* mask to set the bits                */
  BYTE work[3];                        /* work bytes                          */

  /* Check if symbol translation is requested */
  if(unlikely(GR0_st(regs)))
  {

    /* Get the interchange symbol */
    ARCH_DEP(vfetchc)(work, 1, (GR1_dictor(regs) + GR1_sttoff(regs) + index_symbol * 2) & ADDRESS_MAXWRAP(regs), r2, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
    logmsg("store_is : %04X -> %02X%02X\n", index_symbol, work[0], work[1]);
#endif

    /* set index_symbol to interchange symbol */
    index_symbol = (work[0] << 8) + work[1];
  }

  /* Allign set mask */
  set_mask = ((U32) index_symbol) << (24 - GR0_smbsz(regs) - GR1_cbn(iregs));

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

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg("store_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", index_symbol, GR1_cbn(iregs), r1, iregs->GR(r1), r1 + 1, iregs->GR(r1 + 1));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */
}

#ifdef WIP
/*----------------------------------------------------------------------------*/
/* store_iss (index symbols)                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(store_iss)(int r1, int r2, REGS *regs, REGS *iregs, U16 iss[8])
{
  BYTE buf[13];
  int i;
  int smbsz;

  /* Check symbol translation */
  if(unlikely(GR0_st(regs)))
  {
    /* Get and set interchange symbols */
    for(i = 0; i < 8; i++)
    {
      ARCH_DEP(vfetchc)(buf, 1, (GR1_dictor(regs) + GR1_sttoff(regs) + iss[i] * 2) & ADDRESS_MAXWRAP(regs), r2, regs);
      iss[i] = (buf[0] << 8) | buf[1];

#ifdef OPTION_CMPSC_COMPRESS_DEBUG
      logmsg("store_iss: %04X -> %02X%02X\n", iss, buf[0], buf[1]);
#endif

    }
  } 

  /* Initialize values */
  smbsz = GR0_smbsz(regs);

  /* Calculate the 8 index symbols */
  switch(smbsz)
  {
    case 9: /* 9-bits */
    {
      /* 0        1        2        3        4        5        6        7        8        */
      /* 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 */
      /* 01234567 80123456 78012345 67801234 56780123 45678012 34567801 23456780 12345678 */
      /* 0         1         2         3         4         5         6         7          */
      buf[0] = ((iss[0]       ) >> 1);
      buf[1] = ((iss[0] & 0x01) << 7) | (iss[1] >> 2);
      buf[2] = ((iss[1] & 0x03) << 6) | (iss[2] >> 3);
      buf[3] = ((iss[2] & 0x07) << 5) | (iss[3] >> 4);
      buf[4] = ((iss[3] & 0x0f) << 4) | (iss[4] >> 5);
      buf[5] = ((iss[4] & 0x1f) << 3) | (iss[5] >> 6);
      buf[6] = ((iss[5] & 0x3f) << 2) | (iss[6] >> 7);
      buf[7] = ((iss[6] & 0x7f) << 1) | (iss[7] >> 8);
      buf[8] = ((iss[8] & 0xff)     );
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0        1        2        3        4        5        6        7        8        9        */
      /* 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 */
      /* 01234567 89012345 67890123 45678901 23456789 01234567 89012345 67890123 45678901 23456789 */
      /* 0          1          2          3           4          5          6          7           */
      buf[0] = ((iss[0]       ) >> 2);
      buf[1] = ((iss[0] & 0x03) << 6) | (iss[1] >> 4);
      buf[2] = ((iss[1] & 0x0f) << 4) | (iss[2] >> 6);
      buf[3] = ((iss[2] & 0x3f) << 2) | (iss[3] >> 8);
      buf[4] = ((iss[3] & 0xff)     );
      buf[5] = ((iss[4]       ) >> 2);
      buf[6] = ((iss[4] & 0x03) << 6) | (iss[5] >> 4);
      buf[7] = ((iss[5] & 0x0f) << 4) | (iss[6] >> 6);
      buf[8] = ((iss[6] & 0x3f) << 2) | (iss[7] >> 8);
      buf[9] = ((iss[7] & 0xff)     ); 
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0        1        2        3        4        5        6        7        8        9        a        */
      /* 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 */
      /* 01234567 89a01234 56789a01 23456789 a0123456 789a0123 456789a0 12345678 9a012345 6789a012 3456789a */
      /* 0           1           2            3           4           5            6           7            */
      buf[ 0] = ((iss[0]       ) >> 3);
      buf[ 1] = ((iss[0] & 0x07) << 5) | (iss[1] >> 6);
      buf[ 2] = ((iss[1] & 0x3f) << 2) | (iss[2] >> 9);
      buf[ 3] = ((iss[2] >> 1) & 0xff);
      buf[ 4] = ((iss[2] & 0x01) << 7) | (iss[3] >> 4);
      buf[ 5] = ((iss[3] & 0x0f) << 4) | (iss[4] >> 7);
      buf[ 6] = ((iss[4] & 0x7f) << 1) | (iss[5] >> 10);
      buf[ 7] = ((iss[5] >> 2) & 0xff);
      buf[ 8] = ((iss[5] & 0x03) << 6) | (iss[6] >> 5);
      buf[ 9] = ((iss[6] & 0x1f) << 3) | (iss[7] >> 8);
      buf[10] = ((iss[7] & 0xff)     );
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0        1        2        3        4        5        6        7        8        9        a        b        */
      /* 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 */
      /* 01234567 89ab0123 456789ab 01234567 89ab0123 456789ab 01234567 89ab0123 456789ab 01234567 89ab0123 456789ab */
      /* 0            1             2            3             4            5             6            7             */
      buf[ 0] = ((iss[0]       ) >> 4);
      buf[ 1] = ((iss[0] & 0x0f) << 4) | (iss[1] >> 8);
      buf[ 2] = ((iss[1] & 0xff)     );
      buf[ 3] = ((iss[2]       ) >> 4);
      buf[ 4] = ((iss[2] & 0x0f) << 4) | (iss[3] >> 8);
      buf[ 5] = ((iss[3] & 0xff)     );
      buf[ 6] = ((iss[4]       ) >> 4);
      buf[ 7] = ((iss[4] & 0x0f) << 4) | (iss[5] >> 8);
      buf[ 8] = ((iss[5] & 0xff)     );
      buf[ 9] = ((iss[6]       ) >> 4);
      buf[10] = ((iss[6] & 0x0f) << 4) | (iss[7] >> 8);
      buf[11] = ((iss[7] & 0xff)     );
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0        1        2        3        4        5        6        7        8        9        a        b        c        */
      /* 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567 */
      /* 01234567 89abc012 3456789a bc012345 6789abc0 12345678 9abc0123 456789ab c0123456 789abc01 23456789 abc01234 56789abc */
      /* 0             1              2             3              4              5             6              7              */
      buf[ 0] = ((iss[0]       ) >> 5);
      buf[ 1] = ((iss[0] & 0x1f) << 3) | (iss[ 1] >> 10);
      buf[ 2] = ((iss[1] >> 2) & 0xff);
      buf[ 3] = ((iss[1] & 0x03) << 6) | (iss[ 2] >> 7);
      buf[ 4] = ((iss[2] & 0x7f) << 1) | (iss[ 3] >> 12);
      buf[ 5] = ((iss[3] >> 4) & 0xff);
      buf[ 6] = ((iss[3] & 0x0f) << 4) | (iss[ 4] >> 9);
      buf[ 7] = ((iss[4] >> 1) & 0xff);
      buf[ 8] = ((iss[4] & 0x01) << 7) | (iss[ 5] >> 6);
      buf[ 9] = ((iss[5] & 0x3f) << 2) | (iss[ 6] >> 11);
      buf[10] = ((iss[6] >> 3) & 0xff);
      buf[11] = ((iss[6] & 0x07) << 5) | (iss[ 7] >> 8);
      buf[12] = ((iss[7] & 0xff)     );
      break;
    }
  } 

  /* Store index symbols and adjust registers */
  ARCH_DEP(vstorec)(buf, smbsz - 1, GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);
  ADJUSTREGS(r1, regs, iregs, smbsz);
}
#endif

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

/*----------------------------------------------------------------------------*/
/* B263 CMPSC - Compression Call                                        [RRE] */
/*----------------------------------------------------------------------------*/
/* compression_call. Compression and expanding looks simple in this function. */
/*----------------------------------------------------------------------------*/
DEF_INST(compression_call)
{
  REGS iregs;                          /* Intermediate registers              */
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#if defined(OPTION_CMPSC_DEBUGLVL) || defined(OPTION_CMPSC_EXPAND_DEBUG) 
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
  logmsg("   smbsz > %d\n", GR0_smbsz(regs));
  logmsg("   dctsz > %08X\n", GR0_dctsz(regs));
  logmsg(" GR01    : " F_GREG "\n", regs->GR(1));
  logmsg("   dictor: " F_GREG "\n", GR1_dictor(regs));
  logmsg("   sttoff: %08X\n", GR1_sttoff(regs));
  logmsg("   cbn   : %d\n", GR1_cbn(regs));
#endif /* OPTION_CMPSC_DEBUGLVL */

  /* Check the registers on even-odd pairs and valid compression-data symbol size */
  if(unlikely(r1 & 0x01 || r2 & 0x01 || !GR0_cdss(regs) || GR0_cdss(regs) > 5))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

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
