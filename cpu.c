/* CPU.C        (c) Copyright Roger Bowler, 1994-2004                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2004      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

/*-------------------------------------------------------------------*/
/* This module implements the CPU instruction execution function of  */
/* the S/370 and ESA/390 architectures, as described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/* SA22-7832-00 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Nullification corrections by Jan Jaeger                      */
/*      Set priority by Reed H. Petty from an idea by Steve Gay      */
/*      Corrections to program check by Jan Jaeger                   */
/*      Light optimization on critical path by Valery Pogonchenko    */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      CPU timer and clock comparator interrupt improvements by     */
/*          Jan Jaeger, after a suggestion by Willem Konynenberg     */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Basic FP extensions support - Peter Kuschnerus           v209*/
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

/*-------------------------------------------------------------------*/
/* Store current PSW at a specified address in main storage          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(store_psw) (REGS *regs, BYTE *addr)
{

#if defined(FEATURE_BCMODE)
    if ( ECMODE(&regs->psw) ) {
#endif /*defined(FEATURE_BCMODE)*/
#if !defined(FEATURE_ESAME)
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | ( ( (regs->psw.asc)
                       | (regs->psw.cc << 4)
                       | (regs->psw.progmask)
                       ) << 8
                     )
                   | regs->psw.zerobyte
                   )
                 );
        STORE_FW ( addr + 4,
                   ( regs->psw.IA | (regs->psw.amode ? 0x80000000 : 0) )
                 );
#endif /*!defined(FEATURE_ESAME)*/
#if defined(FEATURE_BCMODE)
    } else {
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | (regs->psw.intcode)
                   )
                 );
        STORE_FW ( addr + 4,
                   ( ( (regs->psw.ilc << 5)
                     | (regs->psw.cc << 4)
                     | regs->psw.progmask
                     ) << 24
                   ) | regs->psw.IA
                 );
    }
#elif defined(FEATURE_ESAME)
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | ( ( (regs->psw.asc) 
                       | (regs->psw.cc << 4)
                       | (regs->psw.progmask)
                       ) << 8
                     )
                   | (regs->psw.amode64 ? 0x01 : 0)
                   | regs->psw.zerobyte
                   )
                 );
        STORE_FW ( addr + 4, regs->psw.amode ? 0x80000000 : 0 );
        STORE_DW ( addr + 8, regs->psw.IA );
#endif /*defined(FEATURE_ESAME)*/
} /* end function ARCH_DEP(store_psw) */

/*-------------------------------------------------------------------*/
/* Load current PSW from a specified address in main storage         */
/* Returns 0 if valid, 0x0006 if specification exception             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_psw) (REGS *regs, BYTE *addr)
{
int     permode = PER_MODE(regs);
int     realmode = REAL_MODE(&regs->psw);
int     space = SPACE_BIT(&regs->psw);
int     homemode = HOME_SPACE_MODE(&regs->psw);
BYTE    pkey = regs->psw.pkey;

    regs->psw.sysmask = addr[0];
    regs->psw.pkey    = (addr[1] & 0xF0);
    regs->psw.states  = (addr[1] & 0x0F);

#if defined(FEATURE_BCMODE)
    if ( ECMODE(&regs->psw) ) {
#endif /*defined(FEATURE_BCMODE)*/

        SET_IC_ECMODE_MASK(regs);

        /* Processing for EC mode PSW */
        regs->psw.intcode  = regs->psw.ilc = 0;
        regs->psw.asc      = (addr[2] & 0xC0);
        regs->psw.cc       = (addr[2] & 0x30) >> 4;
        regs->psw.progmask = (addr[2] & 0x0F);
        regs->psw.amode    = (addr[4] & 0x80) ? 1 : 0;

#if defined(FEATURE_ESAME)
        regs->psw.zerobyte = addr[3] & 0xFE;
        regs->psw.amode64  = addr[3] & 0x01;
        regs->psw.IA       = fetch_dw (addr + 8);
        regs->psw.AMASK    = regs->psw.amode64 ? AMASK64
                           : regs->psw.amode   ? AMASK31 : AMASK24;
#else /*!defined(FEATURE_ESAME)*/
        regs->psw.zerobyte = addr[3];
        regs->psw.amode64  = 0;
        regs->psw.IA       = fetch_fw(addr + 4) & 0x7FFFFFFF;
        regs->psw.AMASK    = regs->psw.amode ? AMASK31 : AMASK24;
#endif /*!defined(FEATURE_ESAME)*/

        /* Bits 0 and 2-4 of system mask must be zero */
        if ((addr[0] & 0xB8) != 0)
            return PGM_SPECIFICATION_EXCEPTION;

#if defined(FEATURE_ESAME)
        /* For ESAME, bit 12 must be zero */
        if (NOTESAME(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 24-30 must be zero */
        if (regs->psw.zerobyte)
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 33-63 must be zero */
        if ( fetch_fw(addr+4) & 0x7FFFFFFF )
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
        /* Bits 24-31 must be zero */
        if ( regs->psw.zerobyte )
            return PGM_SPECIFICATION_EXCEPTION;

        /* For ESA/390, bit 12 must be one */
        if (!ECMODE(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!defined(FEATURE_ESAME)*/

#ifndef FEATURE_DUAL_ADDRESS_SPACE
        /* If DAS feature not installed then bit 16 must be zero */
        if (SPACE_BIT(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_DUAL_ADDRESS_SPACE*/

#ifndef FEATURE_ACCESS_REGISTERS
        /* If not ESA/370 or ESA/390 then bit 17 must be zero */
        if (AR_BIT(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_ACCESS_REGISTERS*/

        /* Check validity of amode and instruction address */
#if defined(FEATURE_ESAME)
        /* For ESAME, bit 32 cannot be zero if bit 31 is one */
        if (regs->psw.amode64 && !regs->psw.amode)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 32 is zero then IA cannot exceed 24 bits */
        if (!regs->psw.amode && regs->psw.IA > 0x00FFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 31 is zero then IA cannot exceed 31 bits */
        if (!regs->psw.amode64 && regs->psw.IA > 0x7FFFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
  #ifdef FEATURE_BIMODAL_ADDRESSING
        /* For 370-XA, ESA/370, and ESA/390,
           if amode=24, bits 33-39 must be zero */
        if (!regs->psw.amode && regs->psw.IA > 0x00FFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;
  #else /*!FEATURE_BIMODAL_ADDRESSING*/
        /* For S/370, bits 32-39 must be zero */
        if (addr[4] != 0x00)
            return PGM_SPECIFICATION_EXCEPTION;
  #endif /*!FEATURE_BIMODAL_ADDRESSING*/
#endif /*!defined(FEATURE_ESAME)*/

#if defined(FEATURE_BCMODE)
    } else {

        SET_IC_BCMODE_MASK(regs);

        /* Processing for S/370 BC mode PSW */
        regs->psw.intcode = fetch_hw (addr + 2);
        regs->psw.ilc = (addr[4] >> 6) * 2;
        regs->psw.cc = (addr[4] & 0x30) >> 4;
        regs->psw.progmask = (addr[4] & 0x0F);

        FETCH_FW(regs->psw.IA, addr + 4);
        regs->psw.IA &= 0x00FFFFFF;
        regs->psw.AMASK = AMASK24;

        regs->psw.zerobyte = 0;
        regs->psw.asc = 0;
        regs->psw.amode64 = regs->psw.amode = 0;
    }
#endif /*defined(FEATURE_BCMODE)*/

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Bits 5 and 16 must be zero in XC mode */
    if( SIE_STATB(regs, MX, XC)
      && ( (regs->psw.sysmask & PSW_DATMODE) || SPACE_BIT(&regs->psw)) )
        return PGM_SPECIFICATION_EXCEPTION;
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* Check for wait state PSW */
    if (WAITSTATE(&regs->psw) && (sysblk.insttrace || sysblk.inststep))
    {
        logmsg (_("HHCCP043I Wait state PSW loaded: "));
        display_psw (regs);
    }

    /* Perform AIA/AEA invalidations */
    if (realmode != REAL_MODE(&regs->psw)
     || permode  != PER_MODE(regs)
     || homemode != HOME_SPACE_MODE(&regs->psw)
     ||(pkey     != regs->psw.pkey && regs->psw.pkey != 0))
        INVALIDATE_AIA(regs);

    if ( (realmode  != REAL_MODE(&regs->psw)) ||
         (space     != SPACE_BIT(&regs->psw))
#if defined (FEATURE_PER)
      || PER_MODE(regs)
#endif /* defined (FEATURE_PER) */
       )
        INVALIDATE_AEA_ALL(regs);

    return 0;
} /* end function ARCH_DEP(load_psw) */

/*-------------------------------------------------------------------*/
/* Load program interrupt new PSW                                    */
/*-------------------------------------------------------------------*/
void ARCH_DEP(program_interrupt) (REGS *regs, int pcode)
{
PSA    *psa;                            /* -> Prefixed storage area  */
REGS   *realregs;                       /* True regs structure       */
RADR    px;                             /* host real address of pfx  */
int     code;                           /* pcode without PER ind.    */
#if defined(_FEATURE_SIE)
int     nointercept;                    /* True for virtual pgmint   */
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
U32     n;
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/

static char *pgmintname[] = {
        /* 01 */        "Operation exception",
        /* 02 */        "Privileged-operation exception",
        /* 03 */        "Execute exception",
        /* 04 */        "Protection exception",
        /* 05 */        "Addressing exception",
        /* 06 */        "Specification exception",
        /* 07 */        "Data exception",
        /* 08 */        "Fixed-point-overflow exception",
        /* 09 */        "Fixed-point-divide exception",
        /* 0A */        "Decimal-overflow exception",
        /* 0B */        "Decimal-divide exception",
        /* 0C */        "HFP-exponent-overflow exception",
        /* 0D */        "HFP-exponent-underflow exception",
        /* 0E */        "HFP-significance exception",
        /* 0F */        "HFP-floating-point-divide exception",
        /* 10 */        "Segment-translation exception",
        /* 11 */        "Page-translation exception",
        /* 12 */        "Translation-specification exception",
        /* 13 */        "Special-operation exception",
        /* 14 */        "Pseudo-page-fault exception",
        /* 15 */        "Operand exception",
        /* 16 */        "Trace-table exception",
        /* 17 */        "ASN-translation exception",
        /* 18 */        "Page access exception",
        /* 19 */        "Vector/Crypto operation exception",
        /* 1A */        "Page state exception",
        /* 1B */        "Page transition exception",
        /* 1C */        "Space-switch event",
        /* 1D */        "Square-root exception",
        /* 1E */        "Unnormalized-operand exception",
        /* 1F */        "PC-translation specification exception",
        /* 20 */        "AFX-translation exception",
        /* 21 */        "ASX-translation exception",
        /* 22 */        "LX-translation exception",
        /* 23 */        "EX-translation exception",
        /* 24 */        "Primary-authority exception",
        /* 25 */        "Secondary-authority exception",
        /* 26 */        "Page-fault-assist exception",
        /* 27 */        "Control-switch exception",
        /* 28 */        "ALET-specification exception",
        /* 29 */        "ALEN-translation exception",
        /* 2A */        "ALE-sequence exception",
        /* 2B */        "ASTE-validity exception",
        /* 2C */        "ASTE-sequence exception",
        /* 2D */        "Extended-authority exception",
        /* 2E */        "Unassigned exception",
        /* 2F */        "Unassigned exception",
        /* 30 */        "Stack-full exception",
        /* 31 */        "Stack-empty exception",
        /* 32 */        "Stack-specification exception",
        /* 33 */        "Stack-type exception",
        /* 34 */        "Stack-operation exception",
        /* 35 */        "Unassigned exception",
        /* 36 */        "Unassigned exception",
        /* 37 */        "Unassigned exception",
        /* 38 */        "ASCE-type exception",
        /* 39 */        "Region-first-translation exception",
        /* 3A */        "Region-second-translation exception",
        /* 3B */        "Region-third-translation exception",
        /* 3C */        "Unassigned exception",
        /* 3D */        "Unassigned exception",
        /* 3E */        "Unassigned exception",
        /* 3F */        "Unassigned exception",
        /* 40 */        "Monitor event" };

    /* If called with ghost registers (ie from hercules command
       then ignore all interrupt handling and report the error
       to the caller */
    if(regs->ghostregs)
        longjmp(regs->progjmp, pcode);
   
    /* program_interrupt() may be called with a shadow copy of the
       regs structure, realregs is the pointer to the real structure
       which must be used when loading/storing the psw, or backing up
       the instruction address in case of nullification */
#if defined(_FEATURE_SIE)
        realregs = SIE_MODE(regs)
                 ? sysblk.regs[regs->cpuad]->guestregs
                 : sysblk.regs[regs->cpuad];
#else /*!defined(_FEATURE_SIE)*/
    realregs = sysblk.regs[regs->cpuad];
#endif /*!defined(_FEATURE_SIE)*/

    /* Set `execflag' to 0 in case EXecuted instruction program-checked */
    realregs->execflag = 0;
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active)
        realregs->guestregs->execflag = 0;
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Unlock the main storage lock if held */
    if (realregs->mainlock)
        RELEASE_MAINLOCK(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active && realregs->guestregs->mainlock)
        RELEASE_MAINLOCK(realregs->guestregs);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Unlock the TOD lock if held */
    if (realregs->todlock)
    {
        realregs->todlock = 0;
        release_lock(&sysblk.todlock);
    }
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active && realregs->guestregs->todlock)
    {
        realregs->guestregs->todlock = 0;
        release_lock(&sysblk.todlock);
    }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Remove PER indication from program interrupt code
       such that interrupt code specific tests may be done.
       The PER indication will be stored in the PER handling
       code */
    code = pcode & ~PGM_PER_EVENT;

    /* If this is a concurrent PER event then we must add the PER
       bit to the interrupts code */
    if( OPEN_IC_PERINT(realregs) )
        pcode |= PGM_PER_EVENT;

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (realregs);
    PERFORM_CHKPT_SYNC (realregs);

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* Host protection and addressing exceptions must be
       reflected to the guest */
    if(realregs->sie_active &&
        (code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      || code == PGM_ALET_SPECIFICATION_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        ) )
    {
#if defined(SIE_DEBUG)
        logmsg(_("program_int() passing to guest code=%4.4X\n"),pcode);
#endif /*defined(SIE_DEBUG)*/
        realregs->guestregs->TEA = realregs->TEA;
        realregs->guestregs->excarid = realregs->excarid;
        realregs->guestregs->opndrid = realregs->opndrid;
#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
        realregs->guestregs->hostint = 1;
#endif /*defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
        (realregs->guestregs->sie_guestpi) (realregs->guestregs, pcode);
    }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Back up the PSW for exceptions which cause nullification,
       unless the exception occurred during instruction fetch */
    if ((code == PGM_PAGE_TRANSLATION_EXCEPTION
      || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
      || code == PGM_ASCE_TYPE_EXCEPTION
      || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
      || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
      || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
      || code == PGM_TRACE_TABLE_EXCEPTION
      || code == PGM_AFX_TRANSLATION_EXCEPTION
      || code == PGM_ASX_TRANSLATION_EXCEPTION
      || code == PGM_LX_TRANSLATION_EXCEPTION
      || code == PGM_EX_TRANSLATION_EXCEPTION
      || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
      || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
      || code == PGM_ALE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_VALIDITY_EXCEPTION
      || code == PGM_ASTE_SEQUENCE_EXCEPTION
      || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
      || code == PGM_STACK_FULL_EXCEPTION
      || code == PGM_STACK_EMPTY_EXCEPTION
      || code == PGM_STACK_SPECIFICATION_EXCEPTION
      || code == PGM_STACK_TYPE_EXCEPTION
      || code == PGM_STACK_OPERATION_EXCEPTION
      || code == PGM_VECTOR_OPERATION_EXCEPTION)
      && realregs->instvalid)
    {
        realregs->psw.IA -= realregs->psw.ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
        /* When in SIE mode the guest instruction causing this
           host exception must also be nullified */
        if(realregs->sie_active && realregs->guestregs->instvalid)
        {
            realregs->guestregs->psw.IA -= realregs->guestregs->psw.ilc;
            realregs->guestregs->psw.IA &= ADDRESS_MAXWRAP(realregs->guestregs);
        }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
    }

    /* The OLD PSW must be incremented on the following 
       exceptions during instfetch */
    if(!realregs->instvalid &&
      (  code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
      || code == PGM_SPECIFICATION_EXCEPTION
      || code == PGM_TRANSLATION_SPECIFICATION_EXCEPTION ))
    {
        realregs->psw.ilc = ILC(realregs->ip[0]);
        realregs->psw.IA += realregs->psw.ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
    }
        
    /* Store the interrupt code in the PSW */
    realregs->psw.intcode = pcode;

    /* Call debugger if active */
    HDC(debug_program_interrupt, regs, pcode);

    /* Trace program checks other then PER event */
    if(code && (sysblk.insttrace || sysblk.inststep
        || sysblk.pgminttr & ((U64)1 << ((code - 1) & 0x3F))))
    {
#if defined(OPTION_FOOTPRINT_BUFFER)
        if(!(sysblk.insttrace || sysblk.inststep))
            for(n = sysblk.footprptr[realregs->cpuad] + 1 ;
                n != sysblk.footprptr[realregs->cpuad];
                n++, n &= OPTION_FOOTPRINT_BUFFER - 1)
                ARCH_DEP(display_inst)
                        (&sysblk.footprregs[realregs->cpuad][n],
                        sysblk.footprregs[realregs->cpuad][n].inst);
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
        logmsg(_("HHCCP014I "));
#if defined(_FEATURE_SIE)
        if(SIE_MODE(realregs))
            logmsg(_("SIE: "));
#endif /*defined(_FEATURE_SIE)*/
#if defined(SIE_DEBUG)
        logmsg (MSTRING(_GEN_ARCH) " ");
#endif /*defined(SIE_DEBUG)*/
        logmsg (_("CPU%4.4X: %s CODE=%4.4X ILC=%d\n"), realregs->cpuad,
                pgmintname[ (code - 1) & 0x3F], pcode, realregs->psw.ilc);
        ARCH_DEP(display_inst) (realregs, realregs->instvalid ?
                                                realregs->ip : NULL);
    }

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* If this is a host exception in SIE state then leave SIE */
    if(realregs->sie_active)
        ARCH_DEP(sie_exit) (realregs, SIE_HOST_PGMINT);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Absolute address of prefix page */
    px = realregs->PX;

    /* If under SIE translate to host absolute */
    SIE_TRANSLATE(&px, ACCTYPE_WRITE, realregs);

#if defined(_FEATURE_SIE)
    if(!SIE_MODE(regs) ||
      /* Interception is mandatory for the following exceptions */
      (
#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
         !(code == PGM_PROTECTION_EXCEPTION
           && (!SIE_FEATB(regs, EC2, PROTEX)
             || realregs->hostint))
#else /*!defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
         code != PGM_PROTECTION_EXCEPTION
#endif /*!defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
      && code != PGM_ADDRESSING_EXCEPTION
      && code != PGM_SPECIFICATION_EXCEPTION
      && code != PGM_SPECIAL_OPERATION_EXCEPTION
#ifdef FEATURE_VECTOR_FACILITY
      && code != PGM_VECTOR_OPERATION_EXCEPTION
#endif /*FEATURE_VECTOR_FACILITY*/
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      && !(code == PGM_ALEN_TRANSLATION_EXCEPTION
        && SIE_FEATB(regs, MX, XC))
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
      /* And conditional for the following exceptions */
      && !(code == PGM_OPERATION_EXCEPTION
        && SIE_FEATB(regs, IC0, OPEREX))
      && !(code == PGM_PRIVILEGED_OPERATION_EXCEPTION
        && SIE_FEATB(regs, IC0, PRIVOP))
#ifdef FEATURE_BASIC_FP_EXTENSIONS
      && !(code == PGM_DATA_EXCEPTION
        && (regs->dxc == 1 || regs->dxc == 2)
        && (regs->CR(0) & CR0_AFP)
        && !(regs->hostregs->CR(0) & CR0_AFP))
#endif /*FEATURE_BASIC_FP_EXTENSIONS*/
      /* Or all exceptions if requested as such */
      && !SIE_FEATB(regs, IC0, PGMALL) )
    )
    {
#endif /*defined(_FEATURE_SIE)*/
        /* Set the main storage reference and change bits */
        STORAGE_KEY(px, regs) |= (STORKEY_REF | STORKEY_CHANGE);

        /* Point to PSA in main storage */
        psa = (void*)(regs->mainstor + px);
#if defined(_FEATURE_SIE)
        nointercept = 1;
    }
    else
    {
        /* This is a guest interruption interception so point to
           the interruption parm area in the state descriptor
           rather then the PSA, except for the operation exception */
        if(code != PGM_OPERATION_EXCEPTION)
        {
            psa = (void*)(regs->hostregs->mainstor + SIE_STATE(regs) + SIE_IP_PSA_OFFSET);
            /* Set the main storage reference and change bits */
            STORAGE_KEY(SIE_STATE(regs), regs->hostregs) |= (STORKEY_REF | STORKEY_CHANGE);
        }
        else
        {
            /* Point to PSA in main storage */
            psa = (void*)(regs->mainstor + px);

            /* Set the main storage reference and change bits */
            STORAGE_KEY(px, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        }

        nointercept = 0;
    }
#endif /*defined(_FEATURE_SIE)*/


#if defined(_FEATURE_PER)
    /* Handle PER or concurrent PER event */

    /* Throw out Stor Alter PER if merged with nullified/suppressed rupt */
    if ( IS_IC_PER_SA(realregs) && !IS_IC_PER_STURA(realregs) &&
                                   (realregs->ip[0] != 0x0E) &&
         !(code == 0x00 || code == 0x06 || code == 0x08 || code == 0x0A ||
           code == 0x0C || code == 0x0D || code == 0x0E || code == 0x1C ||
           code == 0x40) )
              OFF_IC_PER_SA(realregs);

    if( OPEN_IC_PERINT(realregs) )
    {
        if( realregs->tracing )
            logmsg(_("HHCCP015I CPU%4.4X PER event: code=%4.4X perc=%2.2X "
                     "addr=" F_VADR "\n"),
              regs->cpuad, pcode, IS_IC_PER(realregs) >> 16,
              (realregs->psw.IA - realregs->psw.ilc) & ADDRESS_MAXWRAP(realregs) );

        realregs->perc |= OPEN_IC_PERINT(realregs) >> ((32 - IC_CR9_SHIFT) - 16);
        /* Positions 14 and 15 contain zeros if a storage alteration
           event was not indicated */
        if( !(OPEN_IC_PERINT(realregs) & IC_PER_SA)
          || (OPEN_IC_PERINT(realregs) & IC_PER_STURA) )
            realregs->perc &= 0xFFFC;

        STORE_HW(psa->perint, realregs->perc);

        STORE_W(psa->peradr, realregs->peradr);

        if( IS_IC_PER_SA(realregs) && ACCESS_REGISTER_MODE(&realregs->psw) )
            psa->perarid = realregs->peraid;

        /* Reset PER pending indication */
        OFF_IC_PER(realregs);

    }
    else
    {
        pcode &= 0xFF7F;
    }
#endif /*defined(_FEATURE_PER)*/


#if defined(FEATURE_BCMODE)
    /* For ECMODE, store extended interrupt information in PSA */
    if ( ECMODE(&realregs->psw) )
#endif /*defined(FEATURE_BCMODE)*/
    {
        /* Store the program interrupt code at PSA+X'8C' */
        psa->pgmint[0] = 0;
        psa->pgmint[1] = realregs->psw.ilc;
        STORE_HW(psa->pgmint + 2, pcode);

        /* Store the access register number at PSA+160 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
          || code == PGM_ALEN_TRANSLATION_EXCEPTION
          || code == PGM_ALE_SEQUENCE_EXCEPTION
          || code == PGM_ASTE_VALIDITY_EXCEPTION
          || code == PGM_ASTE_SEQUENCE_EXCEPTION
          || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            psa->excarid = regs->excarid;
            if(regs->TEA | TEA_MVPG)    
                psa->opndrid = regs->opndrid;
            realregs->opndrid = 0;
        }

#if defined(FEATURE_ESAME)
        /* Store the translation exception address at PSA+168 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_DW(psa->TEA_G, regs->TEA);
        }

        /* Store the translation exception address at PSA+172 */
        if ( code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_EX_TRANSLATION_EXCEPTION)
        {
            STORE_FW(psa->TEA_L, regs->TEA);
        }
#else /*!defined(FEATURE_ESAME)*/
        /* Store the translation exception address at PSA+144 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_EX_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_FW(psa->tea, regs->TEA);
        }
#endif /*!defined(FEATURE_ESAME)*/
        realregs->TEA = 0;

        /* Store Data exception code in PSA */
        if (code == PGM_DATA_EXCEPTION)
        {
            STORE_FW(psa->DXC, regs->dxc);
#ifdef FEATURE_BASIC_FP_EXTENSIONS
            /* Load data exception code into FPC register byte 2 */
            if(regs->CR(0) & CR0_AFP)
            {
                regs->fpc &= ~(FPC_DXC);
                regs->fpc |= ((regs->dxc << 8)) & FPC_DXC;
            }
#endif /*FEATURE_BASIC_FP_EXTENSIONS*/
        }

        /* Store the monitor class and event code */
        if (code == PGM_MONITOR_EVENT)
        {
            STORE_HW(psa->monclass, regs->monclass);

            /* Store the monitor code at PSA+156 */
            STORE_W(psa->moncode, regs->MONCODE);
        }
    }

#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
    realregs->hostint = 0;
#endif /*defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/

#if defined(_FEATURE_SIE)
    if(nointercept)
    {
#endif /*defined(_FEATURE_SIE)*/

        obtain_lock(&sysblk.intlock);

        /* Store current PSW at PSA+X'28' or PSA+X'150' for ESAME */
        ARCH_DEP(store_psw) (realregs, psa->pgmold);

        /* Load new PSW from PSA+X'68' or PSA+X'1D0' for ESAME */
        if ( (code = ARCH_DEP(load_psw) (realregs, psa->pgmnew)) )
        {
#if defined(_FEATURE_SIE)
            if(SIE_MODE(realregs))
            {
                release_lock(&sysblk.intlock); 
                longjmp(realregs->progjmp, pcode);
            }
            else
#endif /*defined(_FEATURE_SIE)*/
            {
                logmsg (_("HHCCP016I CPU%4.4X: Program interrupt loop: "),
                          realregs->cpuad);
                display_psw (realregs);
                realregs->cpustate = CPUSTATE_STOPPING;
                ON_IC_INTERRUPT(realregs);
            }
        }

        release_lock(&sysblk.intlock);

        longjmp(realregs->progjmp, SIE_NO_INTERCEPT);

#if defined(_FEATURE_SIE)
    }

    longjmp (realregs->progjmp, pcode);
#endif /*defined(_FEATURE_SIE)*/

} /* end function ARCH_DEP(program_interrupt) */

/*-------------------------------------------------------------------*/
/* Load restart new PSW                                              */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(restart_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(regs->mainstor + regs->PX);

    /* Store current PSW at PSA+X'8' or PSA+X'120' for ESAME  */
    ARCH_DEP(store_psw) (regs, psa->RSTOLD);

    /* Load new PSW from PSA+X'0' or PSA+X'1A0' for ESAME */
    rc = ARCH_DEP(load_psw) (regs, psa->RSTNEW);

    if ( rc == 0)
    {
        regs->cpustate = CPUSTATE_STARTED;
    }

    release_lock(&sysblk.intlock);

    if ( rc )
        ARCH_DEP(program_interrupt)(regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_RESTART);
} /* end function restart_interrupt */


/*-------------------------------------------------------------------*/
/* Perform I/O interrupt if pending                                  */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
void ARCH_DEP(perform_io_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
int     icode;                          /* Intercept code            */
PSA    *psa;                            /* -> Prefixed storage area  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     ioid;                           /* I/O interruption address  */
U32     iointid;                        /* I/O interruption ident    */
RADR    pfx;                            /* Prefix                    */
DWORD   csw;                            /* CSW for S/370 channels    */

    /* Test and clear pending I/O interrupt */
    icode = ARCH_DEP(present_io_interrupt) (regs, &ioid, &ioparm, &iointid, csw);

    /* Exit if no interrupt was presented */
    if (icode == 0) return;

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs) && icode != SIE_NO_INTERCEPT)
    {
        /* Point to SIE copy of PSA in state descriptor */
        psa = (void*)(regs->hostregs->mainstor + SIE_STATE(regs) + SIE_II_PSA_OFFSET);
        STORAGE_KEY(SIE_STATE(regs), regs->hostregs) |= (STORKEY_REF | STORKEY_CHANGE);
    }
    else
#endif
    {
        /* Point to PSA in main storage */
        pfx = regs->PX;
        SIE_TRANSLATE(&pfx, ACCTYPE_SIE, regs);
        psa = (void*)(regs->mainstor + pfx);
        STORAGE_KEY(pfx, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

#ifdef FEATURE_S370_CHANNEL
    /* Store the channel status word at PSA+X'40' */
    memcpy (psa->csw, csw, 8);

    /* Set the interrupt code to the I/O device address */
    regs->psw.intcode = ioid;

    /* For ECMODE, store the I/O device address at PSA+X'B8' */
    if (ECMODE(&regs->psw))
        STORE_FW(psa->ioid, ioid);

    /* Trace the I/O interrupt */
    if (sysblk.insttrace || sysblk.inststep)
        logmsg (_("HHCCP044I I/O interrupt code=%4.4X "
                "CSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                regs->psw.intcode,
                csw[0], csw[1], csw[2], csw[3],
                csw[4], csw[5], csw[6], csw[7]);
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Store X'0001' + subchannel number at PSA+X'B8' */
    STORE_FW(psa->ioid, ioid);

    /* Store the I/O interruption parameter at PSA+X'BC' */
    STORE_FW(psa->ioparm, ioparm);

#if defined(FEATURE_ESAME) || defined(_FEATURE_IO_ASSIST)
    /* Store the I/O interruption identification word at PSA+X'C0' */
    STORE_FW(psa->iointid, iointid);
#endif /*defined(FEATURE_ESAME)*/

    /* Trace the I/O interrupt */
    if (sysblk.insttrace || sysblk.inststep)
#if !defined(FEATURE_ESAME) && !defined(_FEATURE_IO_ASSIST)
        logmsg (_("HHCCP045I I/O interrupt code=%8.8X parm=%8.8X\n"),
                  ioid, ioparm);
#else /*defined(FEATURE_ESAME)*/
        logmsg (_("HHCCP046I I/O interrupt code=%8.8X parm=%8.8X id=%8.8X\n"),
          ioid, ioparm, iointid);
#endif /*defined(FEATURE_ESAME)*/
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

#if defined(_FEATURE_IO_ASSIST)
    if(icode == SIE_NO_INTERCEPT)
#endif
    {
        /* Store current PSW at PSA+X'38' or PSA+X'170' for ESAME */
        ARCH_DEP(store_psw) ( regs, psa->iopold );

        /* Load new PSW from PSA+X'78' or PSA+X'1F0' for ESAME */
        rc = ARCH_DEP(load_psw) ( regs, psa->iopnew );

        if ( rc )
        {
            release_lock(&sysblk.intlock);
            ARCH_DEP(program_interrupt) (regs, rc);
        }
    }
    
    release_lock(&sysblk.intlock);

    longjmp(regs->progjmp, icode);

} /* end function perform_io_interrupt */

/*-------------------------------------------------------------------*/
/* Perform Machine Check interrupt if pending                        */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(perform_mck_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
U64     mcic;                           /* Mach.check interrupt code */
U32     xdmg;                           /* External damage code      */
RADR    fsta;                           /* Failing storage address   */

    /* Test and clear pending machine check interrupt */
    rc = ARCH_DEP(present_mck_interrupt) (regs, &mcic, &xdmg, &fsta);

    /* Exit if no machine check was presented */
    if (rc == 0) return;

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to the PSA in main storage */
    psa = (void*)(regs->mainstor + regs->PX);

    /* Store registers in machine check save area */
    ARCH_DEP(store_status) (regs, regs->PX);

#if !defined(FEATURE_ESAME)
// ZZ
    /* Set the extended logout area to zeros */
    memset(psa->storepsw, 0, 16);
#endif

    /* Store the machine check interrupt code at PSA+232 */
    STORE_DW(psa->mckint, mcic);

    /* Trace the machine check interrupt */
    if (sysblk.insttrace || sysblk.inststep)
        logmsg (_("HHCCP022I Machine Check code=%16.16llu\n"),
                  (long long)mcic);

    /* Store the external damage code at PSA+244 */
    STORE_FW(psa->xdmgcode, xdmg);

#if defined(FEATURE_ESAME)
    /* Store the failing storage address at PSA+248 */
    STORE_DW(psa->mcstorad, fsta);
#else /*!defined(FEATURE_ESAME)*/
    /* Store the failing storage address at PSA+248 */
    STORE_FW(psa->mcstorad, fsta);
#endif /*!defined(FEATURE_ESAME)*/

    /* Store current PSW at PSA+X'30' */
    ARCH_DEP(store_psw) ( regs, psa->mckold );

    /* Load new PSW from PSA+X'70' */
    rc = ARCH_DEP(load_psw) ( regs, psa->mcknew );

    release_lock(&sysblk.intlock);

    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_MCK);
} /* end function perform_mck_interrupt */


#if !defined(_GEN_ARCH)


REGS *s370_run_cpu (int cpu, REGS *oldregs);
REGS *s390_run_cpu (int cpu, REGS *oldregs);
REGS *z900_run_cpu (int cpu, REGS *oldregs);
static REGS *(* run_cpu[GEN_MAXARCH]) (int cpu, REGS *oldregs) =
                {
#if defined(_370)
                    s370_run_cpu,
#endif
#if defined(_390)
                    s390_run_cpu,
#endif
#if defined(_900)
                    z900_run_cpu
#endif
                };

/*-------------------------------------------------------------------*/
/* CPU instruction execution thread                                  */
/*-------------------------------------------------------------------*/
void *cpu_thread (int *ptr)
{
REGS *regs = NULL;
int   cpu  = *ptr;

    /* Set root mode in order to set priority */
    SETMODE(ROOT);
    
    /* Set CPU thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.cpuprio))
        logmsg (_("HHCCP001W CPU%4.4X thread set priority %d failed: %s\n"),
                cpu, sysblk.cpuprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);
    
    /* Display thread started message on control panel */
    logmsg (_("HHCCP002I CPU%4.4X thread started: tid="TIDPAT", pid=%d, "
            "priority=%d\n"),
            cpu, thread_id(), getpid(),
            getpriority(PRIO_PROCESS,0));

    obtain_lock(&sysblk.intlock);

    /* Signal cpu has started */
    signal_condition (&sysblk.cpucond);

    /* Increment number of CPUs online */
    sysblk.cpus++;

    /* Set hi CPU */
    if (cpu >= sysblk.hicpu)
        sysblk.hicpu = cpu + 1;

    /* Start the TOD clock and CPU timer thread */
    if (!sysblk.todtid)
    {
        if ( create_thread (&sysblk.todtid, &sysblk.detattr,
             timer_update_thread, NULL) )
        {
            logmsg (_("HHCCP006S Cannot create timer thread: %s\n"),
                           strerror(errno));
            release_lock(&sysblk.intlock);
            return NULL;
        }
    }

    /* Execute the program in specified mode */
    do {
        regs = run_cpu[sysblk.arch_mode] (cpu, regs);
    } while (regs);

    /* Decrement number of CPUs online */
    sysblk.cpus--;

    /* Reset hi cpu */
    if (cpu + 1 >= sysblk.hicpu)
    {
        int i;
        for (i = MAX_CPU_ENGINES - 1; i >= 0; i--)
            if (IS_CPU_ONLINE(i))
                break;
        sysblk.hicpu = i + 1;
    }

    /* Signal cpu has terminated */
    signal_condition (&sysblk.cpucond);

    /* Display thread ended message on control panel */
    logmsg (_("HHCCP008I CPU%4.4X thread ended: tid="TIDPAT", pid=%d\n"),
            cpu, thread_id(), getpid());

    release_lock(&sysblk.intlock);

    return NULL;
}


/*-------------------------------------------------------------------*/
/* Initialize a CPU                                                  */
/*-------------------------------------------------------------------*/
int cpu_init (int cpu, REGS *regs)
{
    if (!regs->hostregs)
        obtain_lock (&sysblk.cpulock[cpu]);

    regs->cpuad = cpu;
    regs->arch_mode = sysblk.arch_mode;
    regs->chanset = cpu;
    regs->mainstor = sysblk.mainstor;
    regs->storkeys = sysblk.storkeys;
    regs->mainlim = sysblk.mainsize - 1;
    regs->todoffset = sysblk.todoffset;

    initialize_condition (&regs->intcond);

#if defined(_FEATURE_VECTOR_FACILITY)
    regs->vf = &sysblk.vf[cpu];
    regs->vf->online = (cpu < sysblk.numvec);
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

    regs->tlb = calloc (sizeof(TLBE), TLBN);
    if (!regs->tlb)
    {
        logmsg (_("HHCCP078E CPU%4.4X: calloc failed for%s tlb: %s\n"),
                    cpu, regs->hostregs ? " sie" : "", strerror(errno));
            return -1;
    }

    initial_cpu_reset(regs);

    regs->configured = 1;

    if (!regs->hostregs)
    {
#if defined(_FEATURE_SIE)
        regs->guestregs = calloc(sizeof(REGS), 1);
        if (!regs->guestregs)
        {
            logmsg (_("HHCCP079E CPU%4.4X: calloc failed for sie regs: %s\n"),
                    cpu, strerror(errno));
            return -1;
        }
        regs->guestregs->hostregs = regs;
        regs->guestregs->sie_mode = 1;
        if (cpu_init (cpu, regs->guestregs))
            return -1;
#endif /*defined(_FEATURE_SIE)*/
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_INTERRUPT(regs);
        sysblk.regs[cpu] = regs;
        set_bit(4, cpu, &sysblk.config_mask);
        set_bit(4, cpu, &sysblk.started_mask);
        release_lock (&sysblk.cpulock[cpu]);
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* Uninitialize a CPU                                                */
/*-------------------------------------------------------------------*/
void *cpu_uninit (int cpu, REGS *regs)
{
    if (!regs->hostregs)
        obtain_lock (&sysblk.cpulock[cpu]);

    if (regs->guestregs)
    {
        cpu_uninit (cpu, regs->guestregs);
        free (regs->guestregs);
    }

    if (regs->tlb)
        free (regs->tlb);

    destroy_condition(&regs->intcond);

    if (!regs->hostregs)
    {
        /* Remove CPU from all CPU bit masks */
        clear_bit (4, cpu, &sysblk.config_mask);
        clear_bit (4, cpu, &sysblk.started_mask);
        clear_bit (4, cpu, &sysblk.waiting_mask);

        sysblk.regs[cpu] = NULL;
        release_lock (&sysblk.cpulock[cpu]);
    }

    return NULL;
}


#endif /*!defined(_GEN_ARCH)*/


/*-------------------------------------------------------------------*/
/* Process interrupt                                                 */
/*-------------------------------------------------------------------*/
void ARCH_DEP(process_interrupt)(REGS *regs)
{
    /* Process PER program interrupts */
    if( OPEN_IC_PERINT(regs) )
        ARCH_DEP(program_interrupt) (regs, PGM_PER_EVENT);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);
    OFF_IC_INTERRUPT(regs);

    /* Perform broadcasted purge of ALB and TLB */
    while (IS_IC_BROADCAST(regs))
        ARCH_DEP(synchronize_broadcast)(regs, 0, 0);

    /* Refresh opcode table */
    if (regs->reset_opctab)
    {
        regs->reset_opctab = 0;
        memcpy(regs->opctab, ARCH_DEP(opcode_table), 256*sizeof(zz_func));
    }

    /* Set tracing bit */
    regs->tracing = (sysblk.instbreak || sysblk.inststep || sysblk.insttrace);

    /* Perform invalidations */
    if (regs->invalidate)
    {
        if (regs->INVABS == 1)
        {
            INVALIDATE_AIA(regs);
            INVALIDATE_AEA_ALL(regs);
        }
        else
        {
            INVALIDATE_AIA_ABS(regs->INVABS, regs);
            INVALIDATE_AEA_ABS(regs->INVABS, regs);
        }
        regs->invalidate = 0;
    }

    /* Take interrupts if CPU is not stopped */
    if (regs->cpustate == CPUSTATE_STARTED)
    {
        /* Process machine check interrupt */
        if ( OPEN_IC_MCKPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_mck_interrupt) (regs);
        }

        /* Process external interrupt */
        if ( OPEN_IC_EXTPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_external_interrupt) (regs);
        }

        /* Process I/O interrupt */
        if (IS_IC_IOPENDING)
        {
            if ( OPEN_IC_IOPENDING(regs) )
            {
                PERFORM_SERIALIZATION (regs);
                PERFORM_CHKPT_SYNC (regs);
                ARCH_DEP (perform_io_interrupt) (regs);
            }
            else
                WAKEUP_CPU_MASK(sysblk.waiting_mask);
        }
    } /*CPU_STARTED*/

    /* If CPU is stopping, change status to stopped */
    if (regs->cpustate == CPUSTATE_STOPPING)
    {
        /* Change CPU status to stopped */
        regs->cpustate = CPUSTATE_STOPPED;

        if (!regs->configured)
        {
#ifdef FEATURE_VECTOR_FACILITY
            /* Mark Vector Facility offline */
            regs->vf->online = 0;
#endif /*FEATURE_VECTOR_FACILITY*/

            /* Thread exit (note - intlock still held) */
            return;
        }

        /* If initial CPU reset pending then perform reset */
        if (regs->sigpireset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (initial_cpu_reset) (regs);
            release_lock(&sysblk.intlock);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* If a CPU reset is pending then perform the reset */
        if (regs->sigpreset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP(cpu_reset) (regs);
            release_lock(&sysblk.intlock);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Store status at absolute location 0 if requested */
        if (IS_IC_STORSTAT(regs))
        {
            OFF_IC_STORSTAT(regs);
            ARCH_DEP(store_status) (regs, 0);
            logmsg (_("HHCCP010I CPU%4.4X store status completed.\n"),
                    regs->cpuad);
            release_lock(&sysblk.intlock);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }
    } /*CPUSTATE_STOPPING*/

    /* Perform restart interrupt if pending */
    if ( IS_IC_RESTART(regs) )
    {
        PERFORM_SERIALIZATION (regs);
        PERFORM_CHKPT_SYNC (regs);
        OFF_IC_RESTART(regs);
        ARCH_DEP(restart_interrupt) (regs);
    } /* end if(restart) */

    /* This is where a stopped CPU will wait */
    if (regs->cpustate == CPUSTATE_STOPPED)
    {
#ifdef OPTION_MIPS_COUNTING
        struct timeval tv;
        gettimeofday (&tv, NULL);
        ADJUST_TOD (tv, regs->lasttod);
        regs->waittod = (U64)tv.tv_sec;
        regs->waittod = regs->waittod * 1000000 + tv.tv_usec;
#endif

        /* Wait until there is work to do */
        HDC(debug_cpu_state, regs);

        regs->ints_state = IC_INITIAL_STATE;
        clear_bit (4, regs->cpuad, &sysblk.started_mask);
        while (regs->cpustate == CPUSTATE_STOPPED)
        {
            wait_condition (&regs->intcond, &sysblk.intlock);
        }
        set_bit (4, regs->cpuad, &sysblk.started_mask);
        regs->ints_state |= sysblk.ints_state;

        HDC(debug_cpu_state, regs);

        /* Purge the lookaside buffers */
        INVALIDATE_AIA(regs);
        INVALIDATE_AEA_ALL(regs);
        ARCH_DEP(purge_tlb) (regs);
#if defined(FEATURE_ACCESS_REGISTERS)
        ARCH_DEP(purge_alb) (regs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

        release_lock (&sysblk.intlock);
 
        /* If the architecture mode has changed we must adapt */
        if(sysblk.arch_mode != regs->arch_mode)
            longjmp(regs->archjmp,SIE_NO_INTERCEPT);
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /*CPUSTATE_STOPPED*/

    /* Test for wait state */
    if (WAITSTATE(&regs->psw))
    {
        INVALIDATE_AIA(regs);
        INVALIDATE_AEA_ALL(regs);

        /* Test for disabled wait PSW and issue message */
        if( IS_IC_DISABLED_WAIT_PSW(regs) )
        {
            logmsg (_("HHCCP011I CPU%4.4X: Disabled wait state\n"
                      "          "),
                    regs->cpuad);
            display_psw (regs);
            regs->cpustate = CPUSTATE_STOPPING;
            release_lock (&sysblk.intlock);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Wait for I/O, external or restart interrupt */
        set_bit (4, regs->cpuad, &sysblk.waiting_mask);
        wait_condition (&regs->intcond, &sysblk.intlock);
        clear_bit (4, regs->cpuad, &sysblk.waiting_mask);

        release_lock (&sysblk.intlock);

        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /* end if(wait) */

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* Do progjmp if tracing so we get into the right loop */
    if (regs->tracing)
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);

} /* process_interrupt */

/*-------------------------------------------------------------------*/
/* Process Trace                                                     */
/*-------------------------------------------------------------------*/
void ARCH_DEP(process_trace)(REGS *regs)
{
int     shouldbreak;                    /* 1=Stop at breakpoint      */

     /* Test for breakpoint */
    shouldbreak = sysblk.instbreak
                ? sysblk.breakaddr[0] <= sysblk.breakaddr[1]
                  ?      sysblk.breakaddr[0] <= regs->psw.IA
                      && regs->psw.IA <= sysblk.breakaddr[1]
                    ? 1
                    : 0
                  :      sysblk.breakaddr[1] <= regs->psw.IA
                      && regs->psw.IA <= sysblk.breakaddr[0]
                    ? 1
                    : 0
                : 0;

    /* Display the instruction */
    if (sysblk.insttrace || sysblk.inststep || shouldbreak)
    {
        ARCH_DEP(display_inst) (regs, regs->ip);
        if (sysblk.inststep || shouldbreak)
        {
            /* Put CPU into stopped state */
            regs->cpustate = CPUSTATE_STOPPED;

            /* Wait for start command from panel */
            obtain_lock (&sysblk.intlock);

            HDC(debug_cpu_state, regs);

            set_bit (4, regs->cpuad, &sysblk.waiting_mask);
            while (regs->cpustate == CPUSTATE_STOPPED)
            {
                wait_condition (&regs->intcond, &sysblk.intlock);
            }
            clear_bit (4, regs->cpuad, &sysblk.waiting_mask);

            HDC(debug_cpu_state, regs);

            release_lock (&sysblk.intlock);
        }
    }
} /* process_trace */


/*-------------------------------------------------------------------*/
/* Run CPU                                                           */
/*-------------------------------------------------------------------*/
REGS *ARCH_DEP(run_cpu) (int cpu, REGS *oldregs)
{
REGS    regs;
zz_func opcode_table[256];

    if (oldregs)
    {
        memcpy (&regs, oldregs, sizeof(REGS));
        free (oldregs);
        sysblk.regs[cpu] = &regs;
        release_lock(&sysblk.cpulock[cpu]);
        logmsg (_("HHCCP007I CPU%4.4X architecture mode set to %s\n"),
                cpu, get_arch_mode_string(&regs));
    }
    else
    {
        memset (&regs, 0, sizeof(REGS));

        if (cpu_init (cpu, &regs))
            return NULL;

        logmsg (_("HHCCP003I CPU%4.4X architecture mode %s\n"),
                cpu, get_arch_mode_string(&regs));

#ifdef FEATURE_VECTOR_FACILITY
        if (regs->vf->online)
            logmsg (_("HHCCP004I CPU%4.4X Vector Facility online\n"),
                    cpu);
#endif /*FEATURE_VECTOR_FACILITY*/
    }

    regs.tracing = (sysblk.instbreak || sysblk.inststep || sysblk.insttrace);
    regs.ints_state |= sysblk.ints_state;

    /* Copy the opcode table */
    memcpy(opcode_table, ARCH_DEP(opcode_table), sizeof(opcode_table));
    regs.opctab = opcode_table;

    release_lock (&sysblk.intlock);

    /* Establish longjmp destination for architecture switch */
    setjmp(regs.archjmp);

    /* Switch architecture mode if appropriate */
    if(sysblk.arch_mode != regs.arch_mode)
    {
        obtain_lock(&sysblk.intlock);
        regs.arch_mode = sysblk.arch_mode;
        oldregs = malloc (sizeof(REGS));
        if (oldregs)
        {
            memcpy(oldregs, &regs, sizeof(REGS));
            obtain_lock(&sysblk.cpulock[cpu]);
        }
        else
        {
            logmsg (_("HHCCP080E CPU%4.4X malloc failed for archjmp regs: %s\n"),
                    cpu, strerror(errno));
            cpu_uninit (cpu, &regs);
        }
        return oldregs;
    }

    /* Invalidate AIA and AEA */
    INVALIDATE_AIA(&regs);
    INVALIDATE_AEA_ALL(&regs);

    /* Establish longjmp destination for program check */
    setjmp(regs.progjmp);

    /* Set `execflag' is 0 in case EXecuted instruction did a longjmp() */
    regs.execflag = 0;

    if (regs.tracing || PER_MODE(&regs))
        goto slowloop;

    while (1)
    {
        /* Test for interrupts if it appears that one may be pending */
        if ( unlikely(IC_INTERRUPT_CPU_NO_PER(&regs)) )
        {
            ARCH_DEP(process_interrupt)(&regs);
            if (!regs.configured)
                return cpu_uninit(cpu, &regs);
        }

        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);

        regs.instcount += 8;

        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);
        UNROLLED_EXECUTE(&regs, opcode_table);
    }

slowloop:
    while (1)
    {
        /* Test for interrupts if it appears that one may be pending */
        if ( IC_INTERRUPT_CPU(&regs) )
        {
            ARCH_DEP(process_interrupt)(&regs);
            if (!regs.configured)
                return cpu_uninit(cpu, &regs);
        }

        /* Fetch the next sequential instruction */
        regs.ip = INSTRUCTION_FETCH(regs.inst, regs.psw.IA, &regs, 0);

        if( regs.tracing )
            ARCH_DEP(process_trace)(&regs);

        /* Execute the instruction */
        regs.instcount++;
        EXECUTE_INSTRUCTION(regs.ip, &regs, opcode_table);
    }

} /* end function cpu_thread */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "cpu.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "cpu.c"
#endif

/*-------------------------------------------------------------------*/
/* Store program status word                                         */
/*-------------------------------------------------------------------*/
void store_psw (REGS *regs, BYTE *addr)
{
    switch(regs->arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_store_psw(regs, addr);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_store_psw(regs, addr);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_store_psw(regs, addr);
            break;
#endif
    }
} /* end function store_psw */

/*-------------------------------------------------------------------*/
/* Display program status word                                       */
/*-------------------------------------------------------------------*/
void display_psw (REGS *regs)
{
QWORD   qword;                            /* quadword work area      */

    memset(qword, 0, sizeof(qword));

    if( regs->arch_mode != ARCH_900 )
    {
        store_psw (regs, qword);
        logmsg (_("PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
    }
    else
    {
        store_psw (regs, qword);
        logmsg (_("PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n"),
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
    }

} /* end function display_psw */

const char* arch_name[GEN_MAXARCH] =
    {
#if defined(_370)
        _ARCH_370_NAME,
#endif
#if defined(_390)
        _ARCH_390_NAME,
#endif
#if defined(_900)
        _ARCH_900_NAME
#endif
    };

const char* get_arch_mode_string(REGS* regs)
{
    if (!regs) return arch_name[sysblk.arch_mode];
    else return arch_name[regs->arch_mode];
        }

#if defined(OPTION_CPU_UTILIZATION)

int timeval_subtract (struct timeval *beg_timeval, struct timeval *end_timeval, struct timeval *dif_timeval)
{
    struct timeval begtime;
    struct timeval endtime;
    ASSERT ( beg_timeval -> tv_sec >= 0  &&  beg_timeval -> tv_usec >= 0 );
    ASSERT ( end_timeval -> tv_sec >= 0  &&  end_timeval -> tv_usec >= 0 );

    memcpy(&begtime,beg_timeval,sizeof(struct timeval));
    memcpy(&endtime,end_timeval,sizeof(struct timeval));

    dif_timeval->tv_sec = endtime.tv_sec - begtime.tv_sec;

    if (endtime.tv_usec >= begtime.tv_usec)
    {
        dif_timeval->tv_usec = endtime.tv_usec - begtime.tv_usec;
    }
    else
    {
        dif_timeval->tv_sec--;
        dif_timeval->tv_usec = (endtime.tv_usec + 1000000) - begtime.tv_usec;
    }

    return ((dif_timeval->tv_sec < 0 || dif_timeval->tv_usec < 0) ? -1 : 0);
}

int timeval_add (struct timeval *dif_timeval, struct timeval *accum_timeval)
        {
    ASSERT ( dif_timeval   -> tv_sec >= 0  &&  dif_timeval   -> tv_usec >= 0 );
    ASSERT ( accum_timeval -> tv_sec >= 0  &&  accum_timeval -> tv_usec >= 0 );

    accum_timeval->tv_sec  += dif_timeval->tv_sec;
    accum_timeval->tv_usec += dif_timeval->tv_usec;

    if (accum_timeval->tv_usec > 1000000)
    {
        int nsec = accum_timeval->tv_usec / 1000000;
        accum_timeval->tv_sec  += nsec;
        accum_timeval->tv_usec -= nsec * 1000000;
    }

    return ((accum_timeval->tv_sec < 0 || accum_timeval->tv_usec < 0) ? -1 : 0);
}

#endif /*defined(OPTION_CPU_UTILIZATION)*/

#endif /*!defined(_GEN_ARCH)*/


