/* SIE.C        (c) Copyright Jan Jaeger, 1999-2001                  */
/*              Interpretive Execution                               */

/*      This module contains the SIE instruction as                  */
/*      described in IBM S/370 Extended Architecture                 */
/*      Interpretive Execution, SA22-7095-01                         */
/*      and                                                          */
/*      Enterprise Systems Architecture / Extended Configuration     */
/*      Principles of Operation, SC24-5594-02 and SC24-5965-00       */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

// #define SIE_DEBUG

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if !defined(_SIE_C)

#define _SIE_C

int s370_run_sie (REGS *regs);
int s390_run_sie (REGS *regs);
int z900_run_sie (REGS *regs);
static int (* run_sie[GEN_MAXARCH]) (REGS *regs) =
                { s370_run_sie, s390_run_sie, z900_run_sie };

#define GUESTREGS (regs->guestregs)
#define STATEBK   ((SIEBK*)(GUESTREGS->siebk))

#define SIE_I_WAIT(_guestregs) \
        ((_guestregs)->psw.wait)

#define SIE_I_STOP(_guestregs) \
        ((_guestregs)->siebk->v & SIE_V_STOP)

#define SIE_I_EXT(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_EXT) \
          && ((_guestregs)->psw.sysmask & PSW_EXTMASK))

#define SIE_I_HOST(_hostregs) IC_INTERRUPT_CPU(_hostregs)

#endif /*!defined(_SIE_C)*/

#undef SIE_I_IO
#if !defined(FEATURE_ESAME)
#define SIE_I_IO(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_IO) \
           && ((_guestregs)->psw.sysmask \
                 & ((_guestregs)->psw.ecmode ? PSW_IOMASK : 0xFE) ))
#else /*defined(FEATURE_ESAME)*/
#define SIE_I_IO(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_IO) \
           && ((_guestregs)->psw.sysmask & PSW_IOMASK ))
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_INTERPRETIVE_EXECUTION)

/*-------------------------------------------------------------------*/
/* B214 SIE   - Start Interpretive Execution                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(start_interpretive_execution)
{
int     b2;                             /* Values of R fields        */
RADR    effective_addr2;                /* address of state desc.    */
int     gpv;                            /* guest psw validity        */
int     n;                              /* Loop counter              */
U16     lhcpu;                          /* Last Host CPU address     */
int     icode;                          /* Interception code         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    if(!regs->psw.amode || !PRIMARY_SPACE_MODE(&(regs->psw)))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    if((effective_addr2 & (sizeof(SIEBK)-1)) != 0
#if defined(FEATURE_ESAME)
      || (effective_addr2 & 0xFFFFFFFFFFFFF000ULL) == 0
      || (effective_addr2 & 0xFFFFFFFFFFFFF000ULL) == regs->PX)
#else /*!defined(FEATURE_ESAME)*/
      || (effective_addr2 & 0x7FFFF000) == 0
      || (effective_addr2 & 0x7FFFF000) == regs->PX)
#endif /*!defined(FEATURE_ESAME)*/
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

#if defined(SIE_DEBUG)
    logmsg("SIE: state descriptor " F_RADR "\n",effective_addr2);
    ARCH_DEP(display_inst) (regs, regs->ip);
#endif /*defined(SIE_DEBUG)*/

    if(effective_addr2 > regs->mainsize - (sizeof(SIEBK)-1))
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Direct pointer to state descriptor block */
    STATEBK = (void*)(sysblk.mainstor + effective_addr2);

#if defined(FEATURE_ESAME)
    if(STATEBK->mx & SIE_MX_ESAME)
    {
        GUESTREGS->arch_mode = ARCH_900;
        GUESTREGS->sie_guestpi = (SIEFN)&z900_program_interrupt;
        gpv = z900_load_psw(GUESTREGS, STATEBK->psw);
    }
#else /*!defined(FEATURE_ESAME)*/
    if(STATEBK->m & SIE_M_370)
    {
        GUESTREGS->arch_mode = ARCH_370;
        GUESTREGS->sie_guestpi = (SIEFN)&s370_program_interrupt;
        gpv = s370_load_psw(GUESTREGS, STATEBK->psw);
    }
#endif /*!defined(FEATURE_ESAME)*/
    else
    if(STATEBK->m & SIE_M_XA)
    {
        GUESTREGS->arch_mode = ARCH_390;
        GUESTREGS->sie_guestpi = (SIEFN)&s390_program_interrupt;
        gpv = s390_load_psw(GUESTREGS, STATEBK->psw);
    }
    else
    {
        /* Validity intercept for invalid mode */
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_MODE, GUESTREGS);
        return;
    }

#if defined(OPTION_REDUCED_INVAL)
    INVALIDATE_AIA(GUESTREGS);

    INVALIDATE_AEA_ALL(GUESTREGS);
#endif

    /* Set host program interrupt routine */
    GUESTREGS->sie_hostpi = (SIEFN)&ARCH_DEP(program_interrupt);

    /* Prefered guest indication */
    GUESTREGS->sie_pref = (STATEBK->m & SIE_M_VR) ? 1 : 0;

#if !defined(FEATURE_ESAME)
    /* Reference and Change Preservation Origin */
    FETCH_FW(GUESTREGS->sie_rcpo, STATEBK->rcpo);
    if(!GUESTREGS->sie_rcpo)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_RCZER, GUESTREGS);
        return;
    }
#endif /*!defined(FEATURE_ESAME)*/

    /* System Control Area Origin */
    FETCH_FW(GUESTREGS->sie_scao, STATEBK->scao);
    if(GUESTREGS->sie_scao >= sysblk.mainsize)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_SCADR, GUESTREGS);
        return;
    }

    /* Load prefix from state descriptor */
    FETCH_FW(GUESTREGS->PX, STATEBK->prefix);
    GUESTREGS->PX &=
#if !defined(FEATURE_ESAME)
                     PX_MASK;
#else /*defined(FEATURE_ESAME)*/
                     (GUESTREGS->arch_mode == ARCH_900) ? PX_MASK : 0x7FFFF000;
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
    /* Load main storage origin */
    FETCH_DW(GUESTREGS->sie_mso,STATEBK->mso);
    GUESTREGS->sie_mso &= SIE2_MS_MASK;

    /* Load main storage extend */
    FETCH_DW(GUESTREGS->mainsize,STATEBK->mse);
    GUESTREGS->mainsize += 0x100000;
    GUESTREGS->mainsize &= SIE2_MS_MASK;

    if(GUESTREGS->sie_mso > GUESTREGS->mainsize)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_MSDEF, GUESTREGS);
        return;
    }

    /* Calculate main storage size */
    GUESTREGS->mainsize -= GUESTREGS->sie_mso;

#else /*!defined(FEATURE_ESAME)*/
    /* Load main storage origin */
    FETCH_HW(GUESTREGS->sie_mso,STATEBK->mso);
    GUESTREGS->sie_mso <<= 16;

    /* Load main storage extend */
    FETCH_HW(GUESTREGS->mainsize,STATEBK->mse);
    GUESTREGS->mainsize = (GUESTREGS->mainsize + 1) << 16;
#endif /*!defined(FEATURE_ESAME)*/

    /* Validate Guest prefix */
    if(GUESTREGS->PX >= GUESTREGS->mainsize)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_PFOUT, GUESTREGS);
        return;
    }

    if(GUESTREGS->sie_mso)
    {
        /* Preferred guest must have zero MSO */
        if(GUESTREGS->sie_pref)
        {
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSONZ, GUESTREGS);
            return;
        }

        /* MCDS guest must have zero MSO */
        if(STATEBK->mx & SIE_MX_XC)
        {
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSODS, GUESTREGS);
            return;
        }

    }

    /* Load expanded storage origin */
    GUESTREGS->sie_xso = STATEBK->xso[0] << 16
                       | STATEBK->xso[1] << 8
                       | STATEBK->xso[2];
    GUESTREGS->sie_xso *= (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);

    /* Load expanded storage limit */
    GUESTREGS->sie_xsl = STATEBK->xsl[0] << 16
                       | STATEBK->xsl[1] << 8
                       | STATEBK->xsl[2];
    GUESTREGS->sie_xsl *= (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);

    /* Load the CPU timer */
    FETCH_DW(GUESTREGS->ptimer, STATEBK->cputimer);

    /* Reset the CPU timer pending flag according to its value */
    if( (S64)GUESTREGS->ptimer < 0 )
        ON_IC_PTIMER(GUESTREGS);
    else
        OFF_IC_PTIMER(GUESTREGS);

    /* Load the TOD clock offset for this guest */
    FETCH_DW(GUESTREGS->sie_epoch, STATEBK->epoch);
    GUESTREGS->todoffset = regs->todoffset + (GUESTREGS->sie_epoch >> 8);

    /* Load the clock comparator */
    FETCH_DW(GUESTREGS->clkc, STATEBK->clockcomp);
    GUESTREGS->clkc >>= 8; /* Internal Hercules format */

    /* Reset the clock comparator pending flag according to
       the setting of the TOD clock */
    if( (sysblk.todclk + GUESTREGS->todoffset) > GUESTREGS->clkc )
        ON_IC_CLKC(GUESTREGS);
    else
        OFF_IC_CLKC(GUESTREGS);

    /* Load TOD Programmable Field */
    FETCH_HW(GUESTREGS->todpr, STATEBK->todpf);

    /* Load the guest registers */
    memcpy(GUESTREGS->gr, regs->gr, 14 * sizeof(U64));
    memcpy(GUESTREGS->ar, regs->ar, 16 * sizeof(U32));
    memcpy(GUESTREGS->fpr, regs->fpr, 32 * sizeof(U32));
#if defined(FEATURE_BINARY_FLOATING_POINT)
    GUESTREGS->fpc =  regs->fpc;
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/

    /* Load GR14 */
    FETCH_W(GUESTREGS->GR(14), STATEBK->gr14);

    /* Load GR15 */
    FETCH_W(GUESTREGS->GR(15), STATEBK->gr15);

    /* Load control registers */
    for(n = 0;n < 16; n++)
        FETCH_W(GUESTREGS->CR(n), STATEBK->cr[n]);

    FETCH_HW(lhcpu, STATEBK->lhcpu);

    /* If this is not the last host cpu that dispatched this state
       descriptor then clear the guest TLB entries */
    if((regs->cpuad != lhcpu)
      || (GUESTREGS->sie_state != effective_addr2))
    {
        /* Absolute address of state descriptor block */
        GUESTREGS->sie_state = effective_addr2;

        /* Update Last Host CPU address */
        STORE_HW(STATEBK->lhcpu, regs->cpuad);

        /* Purge guest TLB entries */
        ARCH_DEP(purge_tlb) (GUESTREGS);
        ARCH_DEP(purge_alb) (GUESTREGS);
    }

    /* Initialise the last fetched instruction pointer */
    GUESTREGS->ip = GUESTREGS->inst;

    /* Set SIE active */
    GUESTREGS->instvalid = 0;
    regs->sie_active = 1;

    /* Get PSA pointer and ensure PSA is paged in */
    if(GUESTREGS->sie_pref)
        GUESTREGS->sie_psa = (PSA_3XX*)(sysblk.mainstor + GUESTREGS->PX);
    else
        GUESTREGS->sie_psa = (PSA_3XX*)(sysblk.mainstor
                           + ARCH_DEP(logical_to_abs) (GUESTREGS->sie_mso
                           + GUESTREGS->PX, USE_PRIMARY_SPACE, regs,
                             ACCTYPE_SIE, 0) );

#if !defined(FEATURE_ESAME)
    /* If this is a S/370 guest, and the interval timer is enabled
       then initialize the timer */
    if( (STATEBK->m & SIE_M_370)
     && !(STATEBK->m & SIE_M_ITMOF))
    {
    S32 itimer,
        olditimer;
    U32 residue;

        obtain_lock(&sysblk.todlock);

        /* Fetch the residu from the state descriptor */
        FETCH_FW(residue,STATEBK->residue);

        /* Fetch the timer value from location 80 */
        FETCH_FW(olditimer,GUESTREGS->sie_psa->inttimer);

        /* Bit position 23 of the interval timer is deremented 
           once for each multiple of 3,333 usecs containded in 
           bit position 0-19 of the residue counter */
        itimer = olditimer - ((residue / 3333) >> 4);

        /* Store the timer back */
        STORE_FW(GUESTREGS->sie_psa->inttimer, itimer);

        release_lock(&sysblk.todlock);

        /* Set interrupt flag and interval timer interrupt pending
           if the interval timer went from positive to negative */
        if (itimer < 0 && olditimer >= 0)
            ON_IC_ITIMER(GUESTREGS);
    }
#endif /*!defined(FEATURE_ESAME)*/

    /* Early exceptions associated with the guest PSW */
    if(gpv)
        ARCH_DEP(program_interrupt) (GUESTREGS, gpv);

    /* Run SIE in guests architecture mode */
    icode = run_sie[GUESTREGS->arch_mode] (regs);

    ARCH_DEP(sie_exit) (regs, icode);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);
} 


/* Exit SIE state, restore registers and update the state descriptor */
void ARCH_DEP(sie_exit) (REGS *regs, int code)
{
int     n;

#if defined(SIE_DEBUG)
    logmsg("SIE: interception code %d\n",code);
    ARCH_DEP(display_inst) (GUESTREGS, GUESTREGS->instvalid ?
                                        GUESTREGS->inst : NULL);
#endif /*defined(SIE_DEBUG)*/

    /* Indicate we have left SIE mode */
    regs->sie_active = 0;

    /* zeroize interception status */
    STATEBK->f = 0;

    switch(code)
    {
        case SIE_HOST_INTERRUPT:
           /* If a host interrupt is pending
              then backup the psw and exit */
            regs->psw.IA -= regs->psw.ilc;
            regs->psw.IA &= ADDRESS_MAXWRAP(regs);
            break;
        case SIE_HOST_PGMINT:
            break;
        case SIE_INTERCEPT_INST:
            STATEBK->c = SIE_C_INST;
            break;
        case SIE_INTERCEPT_PER:
            STATEBK->f |= SIE_F_IF;
            /*fallthru*/
        case SIE_INTERCEPT_INSTCOMP:
            STATEBK->c = SIE_C_PGMINST;
            break;
        case SIE_INTERCEPT_WAIT:
            STATEBK->c = SIE_C_WAIT;
            break;
        case SIE_INTERCEPT_STOPREQ:
            STATEBK->c = SIE_C_STOPREQ;
            break;
        case SIE_INTERCEPT_IOREQ:
            STATEBK->c = SIE_C_IOREQ;
            break;
        case SIE_INTERCEPT_EXTREQ:
            STATEBK->c = SIE_C_EXTREQ;
            break;
        case SIE_INTERCEPT_EXT:
            STATEBK->c = SIE_C_EXTINT;
            break;
        case SIE_INTERCEPT_VALIDITY:
            STATEBK->c = SIE_C_VALIDITY;
            break;
        case PGM_OPERATION_EXCEPTION:
            STATEBK->c = SIE_C_OPEREXC;
            break;
        default:
            STATEBK->c = SIE_C_PGMINT;
            break;
    }

    /* Save CPU timer  */
    STORE_DW(STATEBK->cputimer, GUESTREGS->ptimer);

    /* Save clock comparator */
    GUESTREGS->clkc <<= 8; /* Internal Hercules format */
    STORE_DW(STATEBK->clockcomp, GUESTREGS->clkc);

    /* Save TOD Programmable Field */
    STORE_HW(STATEBK->todpf, GUESTREGS->todpr);

    /* Save GR14 */
    STORE_W(STATEBK->gr14, GUESTREGS->GR(14));

    /* Save GR15 */
    STORE_W(STATEBK->gr15, GUESTREGS->GR(15));

    /* Store the PSW */
    if(GUESTREGS->arch_mode == ARCH_390)
        s390_store_psw (GUESTREGS, STATEBK->psw);
    else
#if defined(FEATURE_ESAME)
        z900_store_psw (GUESTREGS, STATEBK->psw);
#else /*!defined(FEATURE_ESAME)*/
        s370_store_psw (GUESTREGS, STATEBK->psw);
#endif /*!defined(FEATURE_ESAME)*/

    /* save control registers */
    for(n = 0;n < 16; n++)
        STORE_W(STATEBK->cr[n], GUESTREGS->CR(n));

    /* Update the approprate host registers */
    memcpy(regs->gr, GUESTREGS->gr, 14 * sizeof(U64));
    memcpy(regs->ar, GUESTREGS->ar, 16 * sizeof(U32));
    memcpy(regs->fpr, GUESTREGS->fpr, 32 * sizeof(U32));
#if defined(FEATURE_BINARY_FLOATING_POINT)
    regs->fpc =  GUESTREGS->fpc;
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/

    /* Zeroize the interruption parameters */
    memset(STATEBK->ipa, 0, 10);

    if( STATEBK->c == SIE_C_INST
     || STATEBK->c == SIE_C_PGMINST
     || STATEBK->c == SIE_C_OPEREXC )
    {
        /* Indicate interception format 2 */
        STATEBK->f |= SIE_F_IN;

#if defined(_FEATURE_PER)
        /* Handle PER or concurrent PER event */
        if( OPEN_IC_PERINT(GUESTREGS)
          && (GUESTREGS->psw.sysmask & PSW_PERMODE) )
        {
        PSA *psa;   
#if defined(_FEATURE_PER2)
            GUESTREGS->perc |= OPEN_IC_PERINT(GUESTREGS) >> ((32 - IC_CR9_SHIFT) - 16);
            /* Positions 14 and 15 contain zeros if a storage alteration
               event was not indicated */
            if( !(OPEN_IC_PERINT(GUESTREGS) & IC_PER_SA)
              || (OPEN_IC_PERINT(GUESTREGS) & IC_PER_STURA) )
                GUESTREGS->perc &= 0xFFFC;

#endif /*defined(_FEATURE_PER2)*/
            /* Point to PSA fields in state descriptor */
            psa = (void*)(sysblk.mainstor + GUESTREGS->sie_state + SIE_IP_PSA_OFFSET);
            STORE_HW(psa->perint, GUESTREGS->perc);
            STORE_W(psa->peradr, GUESTREGS->peradr);

            STATEBK->f |= SIE_F_IF;
        }
#endif /*defined(_FEATURE_PER)*/

        /* Update interception parameters in the state descriptor */
        if(GUESTREGS->inst[0] != 0x44)
        {
            if(GUESTREGS->instvalid)
                memcpy(STATEBK->ipa, GUESTREGS->inst, GUESTREGS->psw.ilc);
        }
        else
        {
        int exilc;
            STATEBK->f |= SIE_F_EX;
            exilc = (GUESTREGS->exinst[0] < 0x40) ? 2 :
                    (GUESTREGS->exinst[0] < 0xC0) ? 4 : 6;
            memcpy(STATEBK->ipa, GUESTREGS->exinst, exilc);
        }
    }

}
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/


#if defined(_FEATURE_SIE)
/* Execute guest instructions */
int ARCH_DEP(run_sie) (REGS *regs)
{
    int icode;

    SET_IC_EXTERNAL_MASK(GUESTREGS);
    SET_IC_MCK_MASK(GUESTREGS);
    SET_IC_IO_MASK(GUESTREGS);
    SET_IC_PER_MASK(GUESTREGS);
#if defined(_FEATURE_PER)
    /* Reset any PER pending indication */
    OFF_IC_PER(GUESTREGS);
#endif /*defined(_FEATURE_PER)*/

    do {
        if(!(icode = setjmp(GUESTREGS->progjmp)))
            while(! SIE_I_WAIT(GUESTREGS)
               && ! SIE_I_STOP(GUESTREGS)
               && ! SIE_I_EXT(GUESTREGS)
               && ! SIE_I_IO(GUESTREGS)
              /* also exit if pending interrupts for the host cpu */
               && ! SIE_I_HOST(regs) )
            {

                if( SIE_IC_INTERRUPT_CPU(GUESTREGS) )
                {
                    /* Process PER program interrupts */
                    if( OPEN_IC_PERINT(GUESTREGS) )
                        ARCH_DEP(program_interrupt) (GUESTREGS, PGM_PER_EVENT);

                    obtain_lock(&sysblk.intlock);

#if MAX_CPU_ENGINES > 1
                    /* Perform broadcasted purge of ALB and TLB if requested
                       synchronize_broadcast() must be called until there are
                       no more broadcast pending because synchronize_broadcast()
                       releases and reacquires the mainlock. */

                    while ((IS_IC_BROADCAST(regs)))
                        ARCH_DEP(synchronize_broadcast)(regs, 0, 0);
#endif /*MAX_CPU_ENGINES > 1*/

                    if( OPEN_IC_EXTPENDING(GUESTREGS) )
                        ARCH_DEP(perform_external_interrupt) (GUESTREGS);

                    release_lock(&sysblk.intlock);
                }

                GUESTREGS->instvalid = 0;

                INSTRUCTION_FETCH(GUESTREGS->inst, GUESTREGS->psw.IA, GUESTREGS);

                GUESTREGS->instvalid = 1;

#if defined(SIE_DEBUG)
                /* Display the instruction */
                ARCH_DEP(display_inst) (GUESTREGS, GUESTREGS->inst);
#endif /*defined(SIE_DEBUG)*/

#if defined(OPTION_CPU_UNROLL)
                regs->instcount += 8;
#else
                regs->instcount++;
#endif
                EXECUTE_INSTRUCTION(GUESTREGS->inst, 0, GUESTREGS);

#if defined(OPTION_CPU_UNROLL)
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
                UNROLLED_EXECUTE(GUESTREGS);
#endif
            }

        if(icode == 0 || icode == SIE_NO_INTERCEPT)
        {
            if( SIE_I_EXT(GUESTREGS) )
                icode = SIE_INTERCEPT_EXTREQ;
            else
                if( SIE_I_IO(GUESTREGS) )
                    icode = SIE_INTERCEPT_IOREQ;
                else
                    if( SIE_I_STOP(GUESTREGS) )
                        icode = SIE_INTERCEPT_STOPREQ;
                    else
                        if( SIE_I_WAIT(GUESTREGS) )
                            icode = SIE_INTERCEPT_WAIT;
                        else
                            if( SIE_I_HOST(regs) )
                                icode = SIE_HOST_INTERRUPT;
        }

    } while(icode == 0 || icode == SIE_NO_INTERCEPT);

    return icode;
}
#endif


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "sie.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "sie.c"

#endif /*!defined(_GEN_ARCH)*/
