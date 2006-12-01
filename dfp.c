/* DFP.C        (c) Copyright Roger Bowler, 2006                     */
/*              Decimal Floating Point instructions                  */

/*-------------------------------------------------------------------*/
/* This module implements the Decimal Floating Point instructions    */
/* and the Floating Point Support Enhancement Facility instructions  */
/* described in the z/Architecture Principles of Operation manual.   */
/*-------------------------------------------------------------------*/

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


/* Additional Decimal Floating Point instructions to be inserted here */

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
