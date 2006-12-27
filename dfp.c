/* DFP.C        (c) Copyright Roger Bowler, 2006                     */
/*              Decimal Floating Point instructions                  */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the Decimal Floating Point instructions    */
/* and the Floating Point Support Enhancement Facility instructions  */
/* described in the z/Architecture Principles of Operation manual.   */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.21  2006/12/26 15:49:14  rbowler
// Decimal Floating Point: TDGXT instruction
//
// Revision 1.20  2006/12/25 23:10:12  rbowler
// Decimal Floating Point: TDCXT instruction
//
// Revision 1.19  2006/12/24 23:02:40  rbowler
// Decimal Floating Point: CXTR,KXTR,LTXTR,SXTR instructions
//
// Revision 1.18  2006/12/23 17:47:03  rbowler
// Decimal Floating Point: Decimal rounding mode
//
// Revision 1.17  2006/12/19 22:08:40  rbowler
// Decimal Floating Point: CSXTR correction for NaN and Inf
//
// Revision 1.16  2006/12/18 16:48:06  rbowler
// Decimal Floating Point: DXTR,MXTR instructions
//
// Revision 1.15  2006/12/17 16:31:20  rbowler
// Decimal Floating Point: CSXTR correction
//
// Revision 1.14  2006/12/15 22:49:02  rbowler
// Decimal Floating Point: CSXTR instruction
//
// Revision 1.13  2006/12/15 21:50:23  rbowler
// Decimal Floating Point: CXSTR instruction
//
// Revision 1.12  2006/12/15 13:26:47  rbowler
// Decimal Floating Point: Exception masks
//
// Revision 1.11  2006/12/14 17:25:35  rbowler
// Decimal Floating Point: Exception conditions
//
// Revision 1.10  2006/12/08 09:43:20  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_DFP_C_)
#define _DFP_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#if defined(FEATURE_DECIMAL_FLOATING_POINT)
#include "decimal128.h"
#include "decimal64.h"
#include "decimal32.h"
#include "decPacked.h"
#endif /*defined(FEATURE_DECIMAL_FLOATING_POINT)*/

#if defined(FEATURE_FPS_ENHANCEMENT)
/*===================================================================*/
/* FLOATING POINT SUPPORT INSTRUCTIONS                               */
/*===================================================================*/
/* Note: the Floating Point Support instructions use the HFPREG_CHECK
   and HFPREG2_CHECK macros to enforce an AFP-register data exception
   if an FPS instruction attempts to use one of the 12 additional FPR
   registers when the AFP-register-control bit in CR0 is zero. */

/*-------------------------------------------------------------------*/
/* B370 LPDFR - Load Positive FPR Long Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, clear the sign bit */
    regs->fpr[i1] = regs->fpr[i2] & 0x7FFFFFFF;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_positive_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B371 LNDFR - Load Negative FPR Long Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, set the sign bit */
    regs->fpr[i1] = regs->fpr[i2] | 0x80000000;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_negative_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B372 CPSDR - Copy Sign FPR Long Register                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(copy_sign_fpr_long_reg)
{
int     r1, r2, r3;                     /* Values of R fields        */
int     i1, i2, i3;                     /* FP register subscripts    */

    RRF_M(inst, regs, r1, r2, r3);
    HFPREG2_CHECK(r1, r2, regs);
    HFPREG_CHECK(r3, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);
    i3 = FPR2I(r3);

    /* Copy register contents */
    regs->fpr[i1] = regs->fpr[i2];
    regs->fpr[i1+1] = regs->fpr[i2+1];

    /* Copy the sign bit from r3 register */
    regs->fpr[i1] &= 0x7FFFFFFF;
    regs->fpr[i1] |= regs->fpr[i3] & 0x80000000;

} /* end DEF_INST(copy_sign_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B373 LCDFR - Load Complement FPR Long Register              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, invert sign bit */
    regs->fpr[i1] = regs->fpr[i2] ^ 0x80000000;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_complement_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B3C1 LDGR  - Load FPR from GR Long Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fpr_from_gr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1;                             /* FP register subscript     */

    RRE(inst, regs, r1, r2);
    HFPREG_CHECK(r1, regs);
    i1 = FPR2I(r1);

    /* Load FP register contents from general register */
    regs->fpr[i1] = regs->GR_H(r2);
    regs->fpr[i1+1] = regs->GR_L(r2);

} /* end DEF_INST(load_fpr_from_gr_long_reg) */


/*-------------------------------------------------------------------*/
/* B3CD LGDR  - Load GR from FPR Long Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_gr_from_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i2;                             /* FP register subscript     */

    RRE(inst, regs, r1, r2);
    HFPREG_CHECK(r2, regs);
    i2 = FPR2I(r2);

    /* Load general register contents from FP register */
    regs->GR_H(r1) = regs->fpr[i2];
    regs->GR_L(r1) = regs->fpr[i2+1];

} /* end DEF_INST(load_gr_from_fpr_long_reg) */


#endif /*defined(FEATURE_FPS_ENHANCEMENT)*/


#if defined(FEATURE_DECIMAL_FLOATING_POINT)
/*===================================================================*/
/* DECIMAL FLOATING POINT INSTRUCTIONS                               */
/*===================================================================*/
/* Note: the DFP instructions use the DFPINST_CHECK macro to check the
   setting of the AFP-register-control bit in CR0. If this bit is zero
   then the macro generates a DFP-instruction data exception. */

#if !defined(_DFP_ARCH_INDEPENDENT_)
/*-------------------------------------------------------------------*/
/* Check if IEEE-interruption-simulation event is to be recognized   */
/*                                                                   */
/* This subroutine is called by the LFAS and SFASR instructions to   */
/* determine whether the instruction should raise a data exception   */
/* at the end of the instruction and, if so, the DXC code to be set. */
/*                                                                   */
/* Input:                                                            */
/*      cur_fpc         Current value of the FPC register            */
/*      src_fpc         Value of instruction source operand          */
/* Output:                                                           */
/*      The return value is the data exception code (DXC), or        */
/*      zero if no IEEE-interruption-simulation event is recognized  */
/*-------------------------------------------------------------------*/
static U32
fpc_signal_check(U32 cur_fpc, U32 src_fpc)
{
U32     cf, sm, enabled_flags;          /* Mask and flag work areas  */
U32     dxc;                            /* Data exception code or 0  */

    /* AND the current FPC flags with the source FPC mask */
    cf = (cur_fpc & FPC_FLAG) >> FPC_FLAG_SHIFT;
    sm = (src_fpc & FPC_MASK) >> FPC_MASK_SHIFT;
    enabled_flags = (cf & sm) << FPC_FLAG_SHIFT;

    /* An IEEE-interruption-simulation event is recognized 
       if any current flag corresponds to the source mask */
    if (enabled_flags & FPC_FLAG_SFI)
    {
        dxc = DXC_IEEE_INV_OP_IISE;
    }
    else if (enabled_flags & FPC_FLAG_SFZ)
    {
        dxc = DXC_IEEE_DIV_ZERO_IISE;
    }
    else if (enabled_flags & FPC_FLAG_SFO)
    {
        dxc = (cur_fpc & FPC_FLAG_SFX) ?
                DXC_IEEE_OF_INEX_IISE :
                DXC_IEEE_OF_EXACT_IISE;
    }
    else if (enabled_flags & FPC_FLAG_SFU)
    {
        dxc = (cur_fpc & FPC_FLAG_SFX) ?
                DXC_IEEE_UF_INEX_IISE :
                DXC_IEEE_UF_EXACT_IISE;
    }
    else if (enabled_flags & FPC_FLAG_SFX)
    {
        dxc = DXC_IEEE_INEXACT_IISE;
    }
    else
    {
        dxc = 0;
    }

    /* Return data exception code or zero */
    return dxc;
} /* end function fpc_signal_check */

/* Bit numbers for Test Data Class instructions */
#define DFP_TDC_ZERO            52
#define DFP_TDC_SUBNORMAL       54  
#define DFP_TDC_NORMAL          56  
#define DFP_TDC_INFINITY        58  
#define DFP_TDC_QUIET_NAN       60  
#define DFP_TDC_SIGNALING_NAN   62  

/*-------------------------------------------------------------------*/
/* Test data class and return condition code                         */
/*                                                                   */
/* This subroutine is called by the TDCET, TDCDT, and TDCXT          */
/* instructions. It tests the data class and sign of a decimal       */
/* number. Each combination of data class and sign corresponds       */
/* to one of 12 possible bits in a bitmask. The value (0 or 1)       */
/* of the corresponding bit is returned.                             */
/*                                                                   */
/* Input:                                                            */
/*      pset    Pointer to decimal number context structure          */
/*      dn      Pointer to decimal number structure to be tested     */
/*      bits    Bitmask in rightmost 12 bits                         */
/* Output:                                                           */
/*      The return value is 0 or 1.                                  */
/*-------------------------------------------------------------------*/
static inline int
dfp_test_data_class(decContext *pset, decNumber *dn, U32 bits)
{
int             bitn;                   /* Bit number                */
decNumber       dm;                     /* Normalized value of dn    */

    if (decNumberIsZero(dn))
        bitn = DFP_TDC_ZERO;
    else if (decNumberIsInfinite(dn))
        bitn = DFP_TDC_INFINITY;
    else if (decNumberIsQNaN(dn))
        bitn = DFP_TDC_QUIET_NAN;
    else if (decNumberIsSNaN(dn))
        bitn = DFP_TDC_SIGNALING_NAN;
    else {
        decNumberNormalize(&dm, dn, pset);
        bitn = (dm.exponent < pset->emin) ?
                DFP_TDC_SUBNORMAL :
                DFP_TDC_NORMAL ;
    }

    if (decNumberIsNegative(dn)) bitn++;

    return (bits >> (63 - bitn)) & 0x01;

} /* end function dfp_test_data_class */

/* Bit numbers for Test Data Group instructions */
#define DFP_TDG_SAFE_ZERO       52  
#define DFP_TDG_EXTREME_ZERO    54  
#define DFP_TDG_EXTREME_NONZERO 56  
#define DFP_TDG_SAFE_NZ_LMD_Z   58  
#define DFP_TDG_SAFE_NZ_LMD_NZ  60  
#define DFP_TDG_SPECIAL         62  
 
/*-------------------------------------------------------------------*/
/* Test data group and return condition code                         */
/*                                                                   */
/* This subroutine is called by the TDGET, TDGDT, and TDGXT          */
/* instructions. It tests the exponent and leftmost coefficient      */
/* digit of a decimal number to determine which of 12 possible       */
/* groups the number corresponds to. Each group corresponds to       */
/* one of 12 possible bits in a bitmask. The value (0 or 1) of       */
/* the corresponding bit is returned.                                */
/*                                                                   */
/* Input:                                                            */
/*      pset    Pointer to decimal number context structure          */
/*      dn      Pointer to decimal number structure to be tested     */
/*      cf      Combination field of decimal FP number               */
/*      bits    Bitmask in rightmost 12 bits                         */
/* Output:                                                           */
/*      The return value is 0 or 1.                                  */
/*-------------------------------------------------------------------*/
static inline int
dfp_test_data_group(decContext *pset, decNumber *dn, U32 cf, U32 bits)
{
int             bitn;                   /* Bit number                */
int             extreme;                /* 1=exponent is min or max  */

    extreme = (dn->exponent == pset->emin)
               || (dn->exponent == pset->emax);

    if (decNumberIsZero(dn))
        bitn = extreme ?
                DFP_TDG_EXTREME_ZERO :
                DFP_TDG_SAFE_ZERO ;
    else if (decNumberIsInfinite(dn) || decNumberIsNaN(dn))
        bitn = DFP_TDG_SPECIAL;
    else if (extreme)
        bitn = DFP_TDG_EXTREME_NONZERO;
    else {
        /* Values 00,08,10 in the combination field indicate
           that the leftmost digit of the number is zero */
        bitn = (cf == 0x00 || cf == 0x08 || cf == 0x10) ?
                DFP_TDG_SAFE_NZ_LMD_Z :
                DFP_TDG_SAFE_NZ_LMD_NZ ;
    }

    if (decNumberIsNegative(dn)) bitn++;

    return (bits >> (63 - bitn)) & 0x01;

} /* end function dfp_test_data_group */

#define _DFP_ARCH_INDEPENDENT_
#endif /*!defined(_DFP_ARCH_INDEPENDENT_)*/

/*-------------------------------------------------------------------*/
/* Set rounding mode in decimal context structure                    */
/*                                                                   */
/* Input:                                                            */
/*      pset    Pointer to decimal number context structure          */
/*      mask    4-bit mask value                                     */
/*      regs    CPU register context                                 */
/* Output:                                                           */
/*      If mask bit X'08' is one then the rounding mode in the       */
/*      context structure is set according to the value (0 to 7)     */
/*      indicated by the low-order three bits of the mask.           */
/*      If mask bit X'08' is zero then the rounding mode in the      */
/*      context structure is set according to the value (0 to 7)     */
/*      of the DRM field in the FPC register.                        */
/*-------------------------------------------------------------------*/
static inline void
ARCH_DEP(dfp_rounding_mode) (decContext *pset, int mask, REGS *regs)
{
BYTE    drm;                            /* Decimal rounding mode     */

    /* Load DRM from mask or from FPC register */
    if (mask & 0x08)
        drm = mask & 0x07;
    else
        drm = (regs->fpc & FPC_DRM) >> FPC_DRM_SHIFT;

    /* Set rounding mode according to DRM value */
    switch (drm) {
    case DRM_RNE:  pset->round = DEC_ROUND_HALF_EVEN; break;
    case DRM_RTZ:  pset->round = DEC_ROUND_DOWN; break;
    case DRM_RTPI: pset->round = DEC_ROUND_CEILING; break;
    case DRM_RTMI: pset->round = DEC_ROUND_FLOOR; break;
    case DRM_RNAZ: pset->round = DEC_ROUND_HALF_UP; break;
    case DRM_RNTZ: pset->round = DEC_ROUND_HALF_DOWN; break;
    case DRM_RAFZ: pset->round = DEC_ROUND_UP; break;
    case DRM_RFSP:
    /* Rounding mode DRM_RFSP is not supported by
       the decNumber library, so we arbitrarily 
       convert it to another mode instead... */
        pset->round = DEC_ROUND_DOWN; break;
    } /* end switch(drm) */

} /* end function dfp_rounding_mode */

/*-------------------------------------------------------------------*/
/* Copy a DFP extended register into a decimal128 structure          */
/*                                                                   */
/* Input:                                                            */
/*      rn      FP register number (left register of pair)           */
/*      xp      Pointer to decimal128 structure                      */
/*      regs    CPU register context                                 */
/*-------------------------------------------------------------------*/
static inline void
ARCH_DEP(dfp_reg_to_decimal128) (int rn, decimal128 *xp, REGS *regs)
{
int     i, j;                           /* FP register subscripts    */
QW      *qp;                            /* Quadword pointer          */

    i = FPR2I(rn);                      /* Left register index       */
    j = i + FPREX;                      /* Right register index      */
    qp = (QW*)xp;                       /* Convert to QW pointer     */
    qp->F.HH.F = regs->fpr[i];          /* Copy FPR bits 0-31        */
    qp->F.HL.F = regs->fpr[i+1];        /* Copy FPR bits 32-63       */
    qp->F.LH.F = regs->fpr[j];          /* Copy FPR bits 64-95       */
    qp->F.LL.F = regs->fpr[j+1];        /* Copy FPR bits 96-127      */

} /* end function dfp_reg_to_decimal128 */

/*-------------------------------------------------------------------*/
/* Load a DFP extended register from a decimal128 structure          */
/*                                                                   */
/* Input:                                                            */
/*      rn      FP register number (left register of pair)           */
/*      xp      Pointer to decimal128 structure                      */
/*      regs    CPU register context                                 */
/*-------------------------------------------------------------------*/
static inline void
ARCH_DEP(dfp_reg_from_decimal128) (int rn, decimal128 *xp, REGS *regs)
{
int     i, j;                           /* FP register subscripts    */
QW      *qp;                            /* Quadword pointer          */

    i = FPR2I(rn);                      /* Left register index       */
    j = i + FPREX;                      /* Right register index      */
    qp = (QW*)xp;                       /* Convert to QW pointer     */
    regs->fpr[i]   = qp->F.HH.F;        /* Load FPR bits 0-31        */
    regs->fpr[i+1] = qp->F.HL.F;        /* Load FPR bits 32-63       */
    regs->fpr[j]   = qp->F.LH.F;        /* Load FPR bits 64-95       */
    regs->fpr[j+1] = qp->F.LL.F;        /* Load FPR bits 96-127      */

} /* end function dfp_reg_from_decimal128 */

/*-------------------------------------------------------------------*/
/* Check for DFP exception conditions                                */
/*                                                                   */
/* This subroutine is called by the DFP instruction processing       */
/* routines after the calculation has been performed but before      */
/* the result is loaded into the result register (or before any      */
/* storage location is updated, as the case may be).                 */
/*                                                                   */
/* The purpose of this subroutine is to check whether any DFP        */
/* exception conditions are indicated by the decimal context         */
/* structure, and to initiate the appropriate action if so.          */
/*                                                                   */
/* Input:                                                            */
/*      set     Decimal number context                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/* Output:                                                           */
/*      Return value is DXC (data exception code) or zero.           */
/*                                                                   */
/* When no exception conditions are indicated, the return value      */
/* is zero indicating that the instruction may proceed normally.     */
/*                                                                   */
/* When an exception condition exists and the corresponding mask     */
/* bit in the FPC register is zero, then the corresponding flag      */
/* bit is set in the FPC register. The return value is zero to       */
/* indicate that the instruction may proceed normally.               */
/*                                                                   */
/* When an exception condition exists and the corresponding mask     */
/* bit in the FPC register is one, then the DXC is set according     */
/* to the type of exception, and one of two actions is taken:        */
/* - if the exception is of a type which causes the instruction      */
/*   to be suppressed, then this subroutine raises a program         */
/*   exception and does not return to the calling instruction        */
/* - if the exception is of a type which causes the instruction      */
/*   to be completed, then this subroutine returns with the          */
/*   DXC code as its return value. The calling instruction will      */
/*   then raise a program exception after storing its results.       */
/*-------------------------------------------------------------------*/
static BYTE
ARCH_DEP(dfp_status_check) (decContext *pset, REGS *regs)
{
BYTE    dxc = 0;                        /* Data exception code       */
int     suppress = 0;                   /* 1=suppress, 0=complete    */

    if (pset->status & DEC_IEEE_854_Invalid_operation)
    {
        /* An IEEE-invalid-operation condition was recognized */
        if ((regs->fpc & FPC_MASK_IMI) == 0)
        {
            regs->fpc |= FPC_FLAG_SFI;
        }
        else
        {
            dxc = DXC_IEEE_INVALID_OP;    
            suppress = 1;
        }
    }
    else if (pset->status & DEC_IEEE_854_Division_by_zero)
    {
        /* An IEEE-division-by-zero condition was recognized */
        if ((regs->fpc & FPC_MASK_IMZ) == 0)
        {
            /* Division-by-zero mask is zero */
            regs->fpc |= FPC_FLAG_SFZ;
        }
        else
        {
            /* Division-by-zero mask is one */
            dxc = DXC_IEEE_DIV_ZERO;
            suppress = 1;
        }
    }
    else if (pset->status & DEC_IEEE_854_Overflow)
    {
        /* An IEEE-overflow condition was recognized */
        if ((regs->fpc & FPC_MASK_IMO) == 0)
        {
            /* Overflow mask is zero */
            regs->fpc |= FPC_FLAG_SFO;
        }
        else
        {
            /* Overflow mask is one */
            dxc = (pset->status & DEC_IEEE_854_Inexact) ?
                    ((pset->status & DEC_Rounded) ?
                        DXC_IEEE_OF_INEX_INCR :
                        DXC_IEEE_OF_INEX_TRUNC ) :
                        DXC_IEEE_OF_EXACT ;
        }
    }
    else if (pset->status & DEC_IEEE_854_Underflow)
    {
        /* An IEEE-underflow condition was recognized */
        if ((regs->fpc & FPC_MASK_IMU) == 0)
        {
            /* Underflow mask is zero */
            if (pset->status & DEC_IEEE_854_Inexact)
            {
                if ((regs->fpc & FPC_MASK_IMX) == 0)
                {
                    /* Inexact result with inexact mask zero */
                    regs->fpc |= (FPC_FLAG_SFU | FPC_FLAG_SFX);
                }
                else
                {
                    /* Inexact result with inexact mask one */
                    regs->fpc |= FPC_FLAG_SFU;
                    dxc = (pset->status & DEC_Rounded) ?
                            DXC_IEEE_INEXACT_INCR :
                            DXC_IEEE_INEXACT_TRUNC ;
                }
            }
        }
        else
        {
            /* Underflow mask is one */
            if (pset->status & DEC_IEEE_854_Inexact)
            {
                /* Underflow with inexact result */
                dxc = (pset->status & DEC_Rounded) ?
                        DXC_IEEE_UF_INEX_INCR :
                        DXC_IEEE_UF_INEX_TRUNC ;
            }
            else
            {
                /* Underflow with exact result */
                dxc = DXC_IEEE_UF_EXACT;
            }
        }
    }
    else if (pset->status & DEC_IEEE_854_Inexact)
    {
        /* An IEEE-inexact condition was recognized */
        if ((regs->fpc & FPC_MASK_IMX) == 0)
        {
            /* Inexact mask is zero */
            regs->fpc |= FPC_FLAG_SFX;
        }
        else
        {
            /* Inexact mask is one */
            dxc = (pset->status & DEC_Rounded) ?
                    DXC_IEEE_INEXACT_INCR :
                    DXC_IEEE_INEXACT_TRUNC ;
        }
    }

    /* If suppression is indicated, raise a data exception */
    if (suppress)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

    /* Otherwise return to complete the instruction */
    return dxc;

} /* end function dfp_status_check */


#define UNDEF_INST(_x) \
        DEF_INST(_x) { ARCH_DEP(operation_exception) \
        (inst,regs); }

/*-------------------------------------------------------------------*/
/* B3DA AXTR  - Add DFP Extended Register                      [RRR] */
/*-------------------------------------------------------------------*/
DEF_INST(add_dfp_ext_reg) 
{
int             r1, r2, r3;             /* Values of R fields        */
decimal128      x1, x2, x3;             /* Extended DFP values       */
decNumber       d1, d2, d3;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRR(inst, regs, r1, r2, r3);
    DFPINST_CHECK(regs);
    DFPREGPAIR3_CHECK(r1, r2, r3, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);
    ARCH_DEP(dfp_rounding_mode)(&set, 0, regs);

    /* Add FP register r3 to FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r3, &x3, regs);
    decimal128ToNumber(&x2, &d2);
    decimal128ToNumber(&x3, &d3);
    decNumberAdd(&d1, &d2, &d3, &set);
    decimal128FromNumber(&x1, &d1, &set);

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Load result into FP register r1 */
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

    /* Set condition code */
    regs->psw.cc = decNumberIsNaN(&d1) ? 3 :
                   decNumberIsZero(&d1) ? 0 :
                   decNumberIsNegative(&d1) ? 1 : 2;

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(add_dfp_ext_reg) */ 


/* Additional Decimal Floating Point instructions to be inserted here */
UNDEF_INST(add_dfp_long_reg)


/*-------------------------------------------------------------------*/
/* B3EC CXTR  - Compare DFP Extended Register                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_dfp_ext_reg)
{
int             r1, r2;                 /* Values of R fields        */
decimal128      x1, x2;                 /* Extended DFP values       */
decNumber       d1, d2, dr;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRE(inst, regs, r1, r2);
    DFPINST_CHECK(regs);
    DFPREGPAIR2_CHECK(r1, r2, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Compare FP register r1 with FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r1, &x1, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    decimal128ToNumber(&x1, &d1);
    decimal128ToNumber(&x2, &d2);
    decNumberCompare(&dr, &d1, &d2, &set);

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Set condition code */
    regs->psw.cc = decNumberIsNaN(&dr) ? 3 :
                   decNumberIsZero(&dr) ? 0 :
                   decNumberIsNegative(&dr) ? 1 : 2;

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(compare_dfp_ext_reg) */


UNDEF_INST(compare_dfp_long_reg)


/*-------------------------------------------------------------------*/
/* B3E8 KXTR  - Compare and Signal DFP Extended Register       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_dfp_ext_reg)
{
int             r1, r2;                 /* Values of R fields        */
decimal128      x1, x2;                 /* Extended DFP values       */
decNumber       d1, d2, dr;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRE(inst, regs, r1, r2);
    DFPINST_CHECK(regs);
    DFPREGPAIR2_CHECK(r1, r2, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Compare FP register r1 with FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r1, &x1, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    decimal128ToNumber(&x1, &d1);
    decimal128ToNumber(&x2, &d2);
    decNumberCompare(&dr, &d1, &d2, &set);

    /* Force signaling condition if result is a NaN */
    if (decNumberIsNaN(&dr))
        set.status |= DEC_IEEE_854_Invalid_operation;

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Set condition code */
    regs->psw.cc = decNumberIsNaN(&dr) ? 3 :
                   decNumberIsZero(&dr) ? 0 :
                   decNumberIsNegative(&dr) ? 1 : 2;

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(compare_and_signal_dfp_ext_reg) */


UNDEF_INST(compare_and_signal_dfp_long_reg)
UNDEF_INST(compare_exponent_dfp_ext_reg)
UNDEF_INST(compare_exponent_dfp_long_reg)
UNDEF_INST(convert_fix64_to_dfp_ext_reg)
UNDEF_INST(convert_fix64_to_dfp_long_reg)

/*-------------------------------------------------------------------*/
/* B3FB CXSTR - Convert from signed BCD (128-bit to DFP ext)   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_sbcd128_to_dfp_ext_reg)
{
int             r1, r2;                 /* Values of R fields        */
decimal128      x1;                     /* Extended DFP values       */
decNumber       dwork, *dp;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            qwork[16];              /* 31-digit packed work area */
int32_t         scale = 0;              /* Scaling factor            */

    RRE(inst, regs, r1, r2);
    DFPINST_CHECK(regs);
    DFPREGPAIR_CHECK(r1, regs);
    ODD_CHECK(r2, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Store general register pair in work area */
    STORE_DW(qwork, regs->GR_G(r2));
    STORE_DW(qwork+8, regs->GR_G(r2+1));

    /* Convert signed BCD to internal number format */
    dp = decPackedToNumber(qwork, sizeof(qwork), &scale, &dwork);

    /* Data exception if digits or sign was invalid */
    if (dp == NULL)
    {
        regs->dxc = DXC_DECIMAL;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

    /* Convert internal number to DFP extended format */
    decimal128FromNumber(&x1, &dwork, &set);

    /* Load result into FP register r1 */
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

} /* end DEF_INST(convert_sbcd128_to_dfp_ext_reg) */


UNDEF_INST(convert_sbcd64_to_dfp_long_reg)
UNDEF_INST(convert_ubcd128_to_dfp_ext_reg)
UNDEF_INST(convert_ubcd64_to_dfp_long_reg)
UNDEF_INST(convert_dfp_ext_to_fix64_reg)
UNDEF_INST(convert_dfp_long_to_fix64_reg)


/*-------------------------------------------------------------------*/
/* B3EB CSXTR - Convert to signed BCD (DFP ext to 128-bit)     [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_dfp_ext_to_sbcd128_reg)
{
int             r1, r2;                 /* Values of R fields        */
int             m4;                     /* Values of M fields        */
decimal128      x2;                     /* Extended DFP values       */
QW              *qp;                    /* Quadword pointer          */
decNumber       dwork;                  /* Working decimal number    */
decContext      set;                    /* Working context           */
int32_t         scale;                  /* Scaling factor            */
BYTE            qwork[17];              /* 33-digit packed work area */

    RRF_M4(inst, regs, r1, r2, m4);
    DFPINST_CHECK(regs);
    DFPREGPAIR_CHECK(r2, regs);
    ODD_CHECK(r1, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Load DFP extended number from FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);

    /* If NaN or Inf then use coefficient only */
    qp = (QW*)&x2;
    if ((qp->F.HH.F & 0x78000000) == 0X78000000)
        qp->F.HH.F &= 0x80003FFF;

    /* Convert to internal decimal number format */
    decimal128ToNumber(&x2, &dwork);

    /* Convert number to signed BCD in work area */
    decPackedFromNumber(qwork, sizeof(qwork), &scale, &dwork);

    /* Make the plus-sign X'F' if m4 bit 3 is one */
    if ((m4 & 0x01) && !decNumberIsNegative(&dwork))
        qwork[16] |= 0x0F;

    /* Load general register pair r1 and r1+1 from
       rightmost 31 packed decimal digits of work area */
    FETCH_DW(regs->GR_G(r1), qwork+1);
    FETCH_DW(regs->GR_G(r1+1), qwork+9);

} /* end DEF_INST(convert_dfp_ext_to_sbcd128_reg) */


UNDEF_INST(convert_dfp_long_to_sbcd64_reg)
UNDEF_INST(convert_dfp_ext_to_ubcd128_reg)
UNDEF_INST(convert_dfp_long_to_ubcd64_reg)
/*-------------------------------------------------------------------*/
/* B3D9 DXTR  - Divide DFP Extended Register                   [RRR] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_dfp_ext_reg)
{
int             r1, r2, r3;             /* Values of R fields        */
decimal128      x1, x2, x3;             /* Extended DFP values       */
decNumber       d1, d2, d3;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRR(inst, regs, r1, r2, r3);
    DFPINST_CHECK(regs);
    DFPREGPAIR3_CHECK(r1, r2, r3, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);
    ARCH_DEP(dfp_rounding_mode)(&set, 0, regs);

    /* Divide FP register r2 by FP register r3 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r3, &x3, regs);
    decimal128ToNumber(&x2, &d2);
    decimal128ToNumber(&x3, &d3);
    decNumberDivide(&d1, &d2, &d3, &set);
    decimal128FromNumber(&x1, &d1, &set);

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Load result into FP register r1 */
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(divide_dfp_ext_reg) */


UNDEF_INST(divide_dfp_long_reg)
UNDEF_INST(extract_biased_exponent_dfp_ext_to_fix64_reg)
UNDEF_INST(extract_biased_exponent_dfp_long_to_fix64_reg)


/*-------------------------------------------------------------------*/
/* B3EF ESXTR - Extract Significance DFP Extended Register     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_significance_dfp_ext_reg)
{
int             r1, r2;                 /* Values of R fields        */
decimal128      x2;                     /* Extended DFP value        */
decNumber       d2;                     /* Working decimal number    */
decContext      set;                    /* Working context           */
S64             digits;                 /* Number of decimal digits  */

    RRE(inst, regs, r1, r2);
    DFPINST_CHECK(regs);
    DFPREGPAIR_CHECK(r2, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Load DFP extended number from FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);

    /* Convert to internal decimal number format */
    decimal128ToNumber(&x2, &d2);

    /* Calculate number of significant digits */
    if (decNumberIsZero(&d2))
        digits = 0;
    else if (decNumberIsInfinite(&d2))
        digits = -1;
    else if (decNumberIsQNaN(&d2))
        digits = -2;
    else if (decNumberIsSNaN(&d2))
        digits = -3;
    else
        digits = d2.digits;

    /* Load result into general register r1 */
    regs->GR(r1) = digits;

} /* end DEF_INST(extract_significance_dfp_ext_reg) */


UNDEF_INST(extract_significance_dfp_long_reg)
UNDEF_INST(insert_biased_exponent_fix64_to_dfp_ext_reg)
UNDEF_INST(insert_biased_exponent_fix64_to_dfp_long_reg)
/*-------------------------------------------------------------------*/
/* B3DE LTXTR - Load and Test DFP Extended Register            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_dfp_ext_reg)
{
int             r1, r2;                 /* Values of R fields        */
decimal128      x1, x2;                 /* Extended DFP values       */
decNumber       d;                      /* Working decimal number    */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRE(inst, regs, r1, r2);
    DFPINST_CHECK(regs);
    DFPREGPAIR2_CHECK(r1, r2, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Load value from FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    decimal128ToNumber(&x2, &d);

    /* For SNaN, force signaling condition and convert to QNaN */
    if (decNumberIsSNaN(&d))
    {
        set.status |= DEC_IEEE_854_Invalid_operation;
        d.bits &= ~DECSNAN;
        d.bits |= DECNAN;
    } 

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Reencode value and load into FP register r1 */
    decimal128FromNumber(&x1, &d, &set);
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

    /* Set condition code */
    regs->psw.cc = decNumberIsNaN(&d) ? 3 :
                   decNumberIsZero(&d) ? 0 :
                   decNumberIsNegative(&d) ? 1 : 2;

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(load_and_test_dfp_ext_reg) */


UNDEF_INST(load_and_test_dfp_long_reg)
UNDEF_INST(load_fp_int_dfp_ext_reg)
UNDEF_INST(load_fp_int_dfp_long_reg)

 
/*-------------------------------------------------------------------*/
/* B2BD LFAS  - Load FPC and Signal                              [S] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fpc_and_signal)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     src_fpc, new_fpc;               /* New value for FPC         */
U32     dxc;

    S(inst, regs, b2, effective_addr2);

    DFPINST_CHECK(regs);

    /* Load new FPC register contents from operand location */
    src_fpc = ARCH_DEP(vfetch4) (effective_addr2, b2, regs);

    /* Program check if reserved bits are non-zero */
    FPC_CHECK(src_fpc, regs);

    /* OR the flags from the current FPC register */
    new_fpc = src_fpc | (regs->fpc & FPC_FLAG);

    /* Determine whether an event is to be signaled */
    dxc = fpc_signal_check(regs->fpc, src_fpc);

    /* Update the FPC register */
    regs->fpc = new_fpc;

    /* Signal an IEEE-interruption-simulation event if needed */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(load_fpc_and_signal) */


UNDEF_INST(load_lengthened_dfp_long_to_ext_reg)
UNDEF_INST(load_lengthened_dfp_short_to_long_reg)
UNDEF_INST(load_rounded_dfp_ext_to_long_reg)
UNDEF_INST(load_rounded_dfp_long_to_short_reg)
/*-------------------------------------------------------------------*/
/* B3D8 MXTR  - Multiply DFP Extended Register                 [RRR] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_dfp_ext_reg)
{
int             r1, r2, r3;             /* Values of R fields        */
decimal128      x1, x2, x3;             /* Extended DFP values       */
decNumber       d1, d2, d3;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRR(inst, regs, r1, r2, r3);
    DFPINST_CHECK(regs);
    DFPREGPAIR3_CHECK(r1, r2, r3, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);
    ARCH_DEP(dfp_rounding_mode)(&set, 0, regs);

    /* Multiply FP register r2 by FP register r3 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r3, &x3, regs);
    decimal128ToNumber(&x2, &d2);
    decimal128ToNumber(&x3, &d3);
    decNumberMultiply(&d1, &d2, &d3, &set);
    decimal128FromNumber(&x1, &d1, &set);

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Load result into FP register r1 */
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(multiply_dfp_ext_reg) */


UNDEF_INST(multiply_dfp_long_reg)
UNDEF_INST(quantize_dfp_ext_reg)
UNDEF_INST(quantize_dfp_long_reg)
UNDEF_INST(reround_dfp_ext_reg)
UNDEF_INST(reround_dfp_long_reg)

 
/*-------------------------------------------------------------------*/
/* B2B9 SRNMT - Set DFP Rounding Mode                            [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_dfp_rounding_mode)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, regs, b2, effective_addr2);

    DFPINST_CHECK(regs);

    /* Set DFP rounding mode in FPC register from address bits 61-63 */
    regs->fpc &= ~(FPC_DRM);
    regs->fpc |= ((effective_addr2 << FPC_DRM_SHIFT) & FPC_DRM);

} /* end DEF_INST(set_dfp_rounding_mode) */


/*-------------------------------------------------------------------*/
/* B385 SFASR - Set FPC and Signal                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_fpc_and_signal)
{
int     r1, unused;                     /* Values of R fields        */
U32     src_fpc, new_fpc;               /* New value for FPC         */
U32     dxc;

    RRE(inst, regs, r1, unused);

    DFPINST_CHECK(regs);

    /* Load new FPC register contents from R1 register bits 32-63 */
    src_fpc = regs->GR_L(r1);

    /* Program check if reserved bits are non-zero */
    FPC_CHECK(src_fpc, regs);
     
    /* OR the flags from the current FPC register */
    new_fpc = src_fpc | (regs->fpc & FPC_FLAG);

    /* Determine whether an event is to be signaled */
    dxc = fpc_signal_check(regs->fpc, src_fpc);

    /* Update the FPC register */
    regs->fpc = new_fpc;

    /* Signal an IEEE-interruption-simulation event if needed */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(set_fpc_and_signal) */


UNDEF_INST(shift_coefficient_left_dfp_ext)
UNDEF_INST(shift_coefficient_left_dfp_long)
UNDEF_INST(shift_coefficient_right_dfp_ext)
UNDEF_INST(shift_coefficient_right_dfp_long)
/*-------------------------------------------------------------------*/
/* B3DB SXTR  - Subtract DFP Extended Register                 [RRR] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_dfp_ext_reg)
{
int             r1, r2, r3;             /* Values of R fields        */
decimal128      x1, x2, x3;             /* Extended DFP values       */
decNumber       d1, d2, d3;             /* Working decimal numbers   */
decContext      set;                    /* Working context           */
BYTE            dxc;                    /* Data exception code       */

    RRR(inst, regs, r1, r2, r3);
    DFPINST_CHECK(regs);
    DFPREGPAIR3_CHECK(r1, r2, r3, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);
    ARCH_DEP(dfp_rounding_mode)(&set, 0, regs);

    /* Subtract FP register r3 from FP register r2 */
    ARCH_DEP(dfp_reg_to_decimal128)(r2, &x2, regs);
    ARCH_DEP(dfp_reg_to_decimal128)(r3, &x3, regs);
    decimal128ToNumber(&x2, &d2);
    decimal128ToNumber(&x3, &d3);
    decNumberSubtract(&d1, &d2, &d3, &set);
    decimal128FromNumber(&x1, &d1, &set);

    /* Check for exception condition */
    dxc = ARCH_DEP(dfp_status_check)(&set, regs);

    /* Load result into FP register r1 */
    ARCH_DEP(dfp_reg_from_decimal128)(r1, &x1, regs);

    /* Set condition code */
    regs->psw.cc = decNumberIsNaN(&d1) ? 3 :
                   decNumberIsZero(&d1) ? 0 :
                   decNumberIsNegative(&d1) ? 1 : 2;

    /* Raise data exception if error occurred */
    if (dxc != 0)
    {
        regs->dxc = dxc;
        ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
    }

} /* end DEF_INST(subtract_dfp_ext_reg) */


UNDEF_INST(subtract_dfp_long_reg)


/*-------------------------------------------------------------------*/
/* ED58 TDCXT - Test Data Class DFP Extended                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_dfp_ext)
{
int             r1;                     /* Value of R field          */
int             b2;                     /* Base of effective addr    */
VADR            effective_addr2;        /* Effective address         */
decimal128      x1;                     /* Extended DFP value        */
decNumber       d1;                     /* Working decimal number    */
decContext      set;                    /* Working context           */
U32             bits;                   /* Low 12 bits of address    */

    RXE(inst, regs, r1, b2, effective_addr2);
    DFPINST_CHECK(regs);
    DFPREGPAIR_CHECK(r1, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Convert FP register r1 to decimal number format */
    ARCH_DEP(dfp_reg_to_decimal128)(r1, &x1, regs);
    decimal128ToNumber(&x1, &d1);

    /* Isolate rightmost 12 bits of second operand address */
    bits = effective_addr2 & 0xFFF;

    /* Test data class and set condition code */
    regs->psw.cc = dfp_test_data_class(&set, &d1, bits);

} /* end DEF_INST(test_data_class_dfp_ext) */


UNDEF_INST(test_data_class_dfp_long)
UNDEF_INST(test_data_class_dfp_short)


/*-------------------------------------------------------------------*/
/* ED59 TDGXT - Test Data Group DFP Extended                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_group_dfp_ext)
{
int             r1;                     /* Value of R field          */
int             b2;                     /* Base of effective addr    */
VADR            effective_addr2;        /* Effective address         */
decimal128      x1;                     /* Extended DFP value        */
QW              *qp;                    /* Quadword pointer          */
decNumber       d1;                     /* Working decimal number    */
decContext      set;                    /* Working context           */
U32             bits;                   /* Low 12 bits of address    */
U32             cf;                     /* Combination field         */

    RXE(inst, regs, r1, b2, effective_addr2);
    DFPINST_CHECK(regs);
    DFPREGPAIR_CHECK(r1, regs);

    /* Initialise the context for extended DFP */
    decContextDefault(&set, DEC_INIT_DECIMAL128);

    /* Load DFP extended number from FP register r1 */
    ARCH_DEP(dfp_reg_to_decimal128)(r1, &x1, regs);

    /* Extract the combination field from FP register r1 */
    qp = (QW*)&x1;
    cf = (qp->F.HH.F & 0x7C000000) >> 26;

    /* Convert to internal decimal number format */
    decimal128ToNumber(&x1, &d1);

    /* Isolate rightmost 12 bits of second operand address */
    bits = effective_addr2 & 0xFFF;

    /* Test data group and set condition code */
    regs->psw.cc = dfp_test_data_group(&set, &d1, cf, bits);

} /* end DEF_INST(test_data_group_dfp_ext) */


UNDEF_INST(test_data_group_dfp_long)
UNDEF_INST(test_data_group_dfp_short)

#endif /*defined(FEATURE_DECIMAL_FLOATING_POINT)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "dfp.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "dfp.c"
#endif

#endif /*!defined(_GEN_ARCH)*/


/* end of dfp.c */
