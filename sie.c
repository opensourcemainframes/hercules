/* SIE.C        (c) Copyright Jan Jaeger, 1999-2003                  */
/*              Interpretive Execution                               */

/*      This module contains the SIE instruction as                  */
/*      described in IBM S/370 Extended Architecture                 */
/*      Interpretive Execution, SA22-7095-01                         */
/*      and                                                          */
/*      Enterprise Systems Architecture / Extended Configuration     */
/*      Principles of Operation, SC24-5594-02 and SC24-5965-00       */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */

// #define SIE_DEBUG

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(_FEATURE_SIE)

#if !defined(_SIE_C)

#define _SIE_C

int s370_run_sie (REGS *regs);
int s390_run_sie (REGS *regs);
#if defined(_900)
int z900_run_sie (REGS *regs);
#endif /*defined(_900)*/
static int (* run_sie[GEN_MAXARCH]) (REGS *regs) =
    {
#if defined(_370)
        s370_run_sie,
#endif
#if defined(_390)
        s390_run_sie,
#endif
#if defined(_900)
        z900_run_sie
#endif
    };

#define GUESTREGS (regs->guestregs)
#define STATEBK   ((SIEBK*)(GUESTREGS->siebk))

#define SIE_I_STOP(_guestregs) \
        ((_guestregs)->siebk->v & SIE_V_STOP)

#define SIE_I_EXT(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_EXT) \
          && ((_guestregs)->psw.sysmask & PSW_EXTMASK))

#define SIE_I_HOST(_hostregs) IC_INTERRUPT_CPU(_hostregs)

#endif /*!defined(_SIE_C)*/

#undef SIE_I_WAIT
#if defined(_FEATURE_WAITSTATE_ASSIST)
#define SIE_I_WAIT(_guestregs) \
        ((_guestregs)->psw.wait && !((_guestregs)->siebk->ec[0] & SIE_EC0_WAIA))
#else
#define SIE_I_WAIT(_guestregs) \
        ((_guestregs)->psw.wait)
#endif

#undef SIE_I_IO
#if defined(FEATURE_BCMODE)
#define SIE_I_IO(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_IO) \
           && ((_guestregs)->psw.sysmask \
                 & ((_guestregs)->psw.ecmode ? PSW_IOMASK : 0xFE) ))
#else /*!defined(FEATURE_BCMODE)*/
#define SIE_I_IO(_guestregs) \
        (((_guestregs)->siebk->v & SIE_V_IO) \
           && ((_guestregs)->psw.sysmask & PSW_IOMASK ))
#endif /*!defined(FEATURE_BCMODE)*/

#endif /*defined(_FEATURE_SIE)*/
#if defined(FEATURE_INTERPRETIVE_EXECUTION)

/*-------------------------------------------------------------------*/
/* B214 SIE   - Start Interpretive Execution                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(start_interpretive_execution)
{
int     b2;                             /* Values of R fields        */
RADR    effective_addr2;                /* address of state desc.    */
int     n;                              /* Loop counter              */
U16     lhcpu;                          /* Last Host CPU address     */
int     icode = 0;                      /* Interception code         */

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
    logmsg(_("SIE: state descriptor " F_RADR "\n"),effective_addr2);
    ARCH_DEP(display_inst) (regs, regs->ip);
#endif /*defined(SIE_DEBUG)*/

    if(effective_addr2 > regs->mainlim - (sizeof(SIEBK)-1))
    ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Direct pointer to state descriptor block */
    STATEBK = (void*)(regs->mainstor + effective_addr2);

#if defined(FEATURE_ESAME)
    if(STATEBK->mx & SIE_MX_ESAME)
    {
        GUESTREGS->arch_mode = ARCH_900;
        GUESTREGS->sie_guestpi = (SIEFN)&z900_program_interrupt;
        icode = z900_load_psw(GUESTREGS, STATEBK->psw);
    }
#else /*!defined(FEATURE_ESAME)*/
    if(STATEBK->m & SIE_M_370)
    {
#if defined(_370)
        GUESTREGS->arch_mode = ARCH_370;
        GUESTREGS->sie_guestpi = (SIEFN)&s370_program_interrupt;
        icode = s370_load_psw(GUESTREGS, STATEBK->psw);
#else
        /* Validity intercept when 370 mode not installed */
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_370NI, GUESTREGS);
        STATEBK->c = SIE_C_VALIDITY;
        return;
#endif
    }
#endif /*!defined(FEATURE_ESAME)*/
    else
#if !defined(FEATURE_ESAME)
    if(STATEBK->m & SIE_M_XA)
#endif /*!defined(FEATURE_ESAME)*/
    {
        GUESTREGS->arch_mode = ARCH_390;
        GUESTREGS->sie_guestpi = (SIEFN)&s390_program_interrupt;
        icode = s390_load_psw(GUESTREGS, STATEBK->psw);
    }
#if !defined(FEATURE_ESAME)
    else
    {
        /* Validity intercept for invalid mode */
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_MODE, GUESTREGS);
        STATEBK->c = SIE_C_VALIDITY;
        return;
    }
#endif /*!defined(FEATURE_ESAME)*/

#if defined(OPTION_REDUCED_INVAL)
    INVALIDATE_AIA(GUESTREGS);

    INVALIDATE_AEA_ALL(GUESTREGS);
#endif

    /* Set host program interrupt routine */
    GUESTREGS->sie_hostpi = (SIEFN)&ARCH_DEP(program_interrupt);

    /* Prefered guest indication */
    GUESTREGS->sie_pref = (STATEBK->m & SIE_M_VR) ? 1 : 0;

    /* Load prefix from state descriptor */
    FETCH_FW(GUESTREGS->PX, STATEBK->prefix);
    GUESTREGS->PX &=
#if !defined(FEATURE_ESAME)
                     PX_MASK;
#else /*defined(FEATURE_ESAME)*/
                     (GUESTREGS->arch_mode == ARCH_900) ? PX_MASK : 0x7FFFF000;
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_REGION_RELOCATE)
    if(STATEBK->mx & SIE_MX_RRF)
    {
    RADR mso, msl, eso = 0, esl = 0;
    
        if(STATEBK->zone >= FEATURE_SIE_MAXZONES)
        {
            /* Validity intercept for invalid zone */
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_AZNNI, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY;
            return;
        }
       
        mso = sysblk.zpb[STATEBK->zone].mso << 20;
        msl = (sysblk.zpb[STATEBK->zone].msl << 20) | 0xFFFFF;
        eso = sysblk.zpb[STATEBK->zone].eso << 20;
        esl = (sysblk.zpb[STATEBK->zone].esl << 20) | 0xFFFFF;

        if(mso > msl)
        {
            /* Validity intercept for invalid zone */
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSDEF, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY;
            return;
        }
       
        /* Ensure addressing exceptions on incorrect zone defs */
        if(mso > regs->mainlim || msl > regs->mainlim)
            mso = msl = 0;


#if defined(SIE_DEBUG)
        logmsg(_("SIE: zone %d: mso=" F_RADR " msl=" F_RADR "\n"),
            STATEBK->zone, mso, msl);
#endif /*defined(SIE_DEBUG)*/

        GUESTREGS->sie_pref = 1;
        GUESTREGS->sie_mso = 0;
        GUESTREGS->mainstor = &(sysblk.mainstor[mso]);
        GUESTREGS->mainlim = msl - mso;
        GUESTREGS->storkeys = &(STORAGE_KEY(mso, &sysblk));
        GUESTREGS->sie_xso = eso;
        GUESTREGS->sie_xsl = esl;
        GUESTREGS->sie_xso *= (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);
        GUESTREGS->sie_xsl *= (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);
    }
    else
#endif /*defined(FEATURE_REGION_RELOCATE)*/
    {
        GUESTREGS->mainstor = sysblk.mainstor;
        GUESTREGS->storkeys = sysblk.storkeys;

        if(STATEBK->zone)
        {
            /* Validity intercept for invalid zone */
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_AZNNZ, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY;
            return;
        }

#if defined(FEATURE_ESAME)
        /* Load main storage origin */
        FETCH_DW(GUESTREGS->sie_mso,STATEBK->mso);
        GUESTREGS->sie_mso &= SIE2_MS_MASK;

        /* Load main storage extend */
        FETCH_DW(GUESTREGS->mainlim,STATEBK->mse);
        GUESTREGS->mainlim |= ~SIE2_MS_MASK;

        if(GUESTREGS->sie_mso > GUESTREGS->mainlim)
        {
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSDEF, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY;
            return;
        }

        /* Calculate main storage size */
        GUESTREGS->mainlim -= GUESTREGS->sie_mso;

#else /*!defined(FEATURE_ESAME)*/
        /* Load main storage origin */
        FETCH_HW(GUESTREGS->sie_mso,STATEBK->mso);
        GUESTREGS->sie_mso <<= 16;
    
        /* Load main storage extend */
        FETCH_HW(GUESTREGS->mainlim,STATEBK->mse);
        GUESTREGS->mainlim = ((GUESTREGS->mainlim + 1) << 16) - 1;
#endif /*!defined(FEATURE_ESAME)*/

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
    }

    /* Validate Guest prefix */
    if(GUESTREGS->PX > GUESTREGS->mainlim)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_PFOUT, GUESTREGS);
        STATEBK->c = SIE_C_VALIDITY;
        return;
    }

    /* System Control Area Origin */
    FETCH_FW(GUESTREGS->sie_scao, STATEBK->scao);
    if(GUESTREGS->sie_scao > regs->mainlim)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_SCADR, GUESTREGS);
        STATEBK->c = SIE_C_VALIDITY;
        return;
    }

    /* Validate MSO */
    if(GUESTREGS->sie_mso)
    {
        /* Preferred guest must have zero MSO */
        if(GUESTREGS->sie_pref)
        {
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSONZ, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY; 
            return;
        }

        /* MCDS guest must have zero MSO */
        if(STATEBK->mx & SIE_MX_XC)
        {
            SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
              SIE_VI_WHY_MSODS, GUESTREGS);
            STATEBK->c = SIE_C_VALIDITY;
            return;
        }
    }

#if !defined(FEATURE_ESAME)
    /* Reference and Change Preservation Origin */
    FETCH_FW(GUESTREGS->sie_rcpo, STATEBK->rcpo);
    if(!GUESTREGS->sie_rcpo && !GUESTREGS->sie_pref)
    {
        SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
          SIE_VI_WHY_RCZER, GUESTREGS);
        STATEBK->c = SIE_C_VALIDITY;
        return;
    }
#endif /*!defined(FEATURE_ESAME)*/

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
    GUESTREGS->instvalid = 0;

    if(!setjmp(GUESTREGS->progjmp))
    {
        /* Set SIE active */
        regs->sie_active = 1;

        /* Get PSA pointer and ensure PSA is paged in */
        if(GUESTREGS->sie_pref)
            GUESTREGS->sie_psa = (PSA_3XX*)(GUESTREGS->mainstor + GUESTREGS->PX);
        else
        {
        U16 sie_xcode;
        int sie_private,
            sie_protect = 0,
            sie_stid;
        RADR sie_px;
    
            if (ARCH_DEP(translate_addr) (GUESTREGS->sie_mso + GUESTREGS->PX,
                    USE_PRIMARY_SPACE, regs, ACCTYPE_SIE, &sie_px,
                    &sie_xcode, &sie_private, &sie_protect, &sie_stid))
            {
                SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
                  SIE_VI_WHY_PFACC, GUESTREGS);
                STATEBK->c = SIE_C_VALIDITY;
                regs->sie_active = 0;
                return;
            }
    
            /* Convert host real address to host absolute address */
            sie_px = APPLY_PREFIXING (sie_px, regs->PX);
            
            if (sie_protect || sie_px > regs->mainlim)
            {
                SIE_SET_VI(SIE_VI_WHO_CPU, SIE_VI_WHEN_SIENT,
                  SIE_VI_WHY_PFACC, GUESTREGS);
                STATEBK->c = SIE_C_VALIDITY;
                regs->sie_active = 0;
                return;
            }
    
            GUESTREGS->sie_psa = (PSA_3XX*)(GUESTREGS->mainstor + sie_px);
    
        }
    
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
    
            /* Set the interval timer pending according to the T bit
               in the state control */
            if(STATEBK->s & SIE_S_T)
                ON_IC_ITIMER(GUESTREGS);
            else
                OFF_IC_ITIMER(GUESTREGS);
    
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
        if(icode)
            (GUESTREGS->sie_guestpi) (GUESTREGS, icode);
    
        /* Run SIE in guests architecture mode */
        icode = run_sie[GUESTREGS->arch_mode] (regs);
    }

    ARCH_DEP(sie_exit) (regs, icode);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    longjmp(regs->progjmp, SIE_NO_INTERCEPT);
} 


/* Exit SIE state, restore registers and update the state descriptor */
void ARCH_DEP(sie_exit) (REGS *regs, int code)
{
int     n;

#if defined(SIE_DEBUG)
    logmsg(_("SIE: interception code %d\n"),code);
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
        case SIE_INTERCEPT_IOINT:
        case SIE_INTERCEPT_IOINTP:
            STATEBK->c = SIE_C_IOINT;
            break;
        case SIE_INTERCEPT_IOINST:
            STATEBK->c = SIE_C_IOINST;
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

#if !defined(FEATURE_ESAME)
    /* If this is a S/370 guest, and the interval timer is enabled
       then save the timer state control bit */
    if( (STATEBK->m & SIE_M_370)
     && !(STATEBK->m & SIE_M_ITMOF))
    {
        if(IS_IC_ITIMER(GUESTREGS))
            STATEBK->s |= SIE_S_T;
        else
            STATEBK->s &= ~SIE_S_T;
    }
#endif /*!defined(FEATURE_ESAME)*/

    /* Save TOD Programmable Field */
    STORE_HW(STATEBK->todpf, GUESTREGS->todpr);

    /* Save GR14 */
    STORE_W(STATEBK->gr14, GUESTREGS->GR(14));

    /* Save GR15 */
    STORE_W(STATEBK->gr15, GUESTREGS->GR(15));

    /* Store the PSW */
    if(GUESTREGS->arch_mode == ARCH_390)
        s390_store_psw (GUESTREGS, STATEBK->psw);
#if defined(_370) || defined(_900)
    else
#endif
#if defined(FEATURE_ESAME)
        z900_store_psw (GUESTREGS, STATEBK->psw);
#else /*!defined(FEATURE_ESAME)*/
#if defined(_370)
        s370_store_psw (GUESTREGS, STATEBK->psw);
#endif
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
     || STATEBK->c == SIE_C_OPEREXC
     || STATEBK->c == SIE_C_IOINST )
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
            psa = (void*)(regs->mainstor + GUESTREGS->sie_state + SIE_IP_PSA_OFFSET);
            STORE_HW(psa->perint, GUESTREGS->perc);
            STORE_W(psa->peradr, GUESTREGS->peradr);
        }

        if (IS_IC_PER_IF(GUESTREGS))
            STATEBK->f |= SIE_F_IF;

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
            do
            {
                if(
                    SIE_I_STOP(GUESTREGS)
                 || SIE_I_EXT(GUESTREGS)
                 || SIE_I_IO(GUESTREGS)
                                          )
                    break;

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

                    if( (STATEBK->ec[0] & SIE_EC0_IOA) && OPEN_IC_IOPENDING(GUESTREGS) )
                    {
                        PERFORM_SERIALIZATION (GUESTREGS);
                        PERFORM_CHKPT_SYNC (GUESTREGS);
                        ARCH_DEP (perform_io_interrupt) (GUESTREGS);
                    }

#if defined(_FEATURE_WAITSTATE_ASSIST)
                    /* Wait state assist */
                    if (GUESTREGS->psw.wait && (STATEBK->ec[0] & SIE_EC0_WAIA))
                    {

                        /* Test for disabled wait PSW and issue message */
                        if( IS_IC_DISABLED_WAIT_PSW(GUESTREGS) )
                        {
                            release_lock (&sysblk.intlock);
                            longjmp(GUESTREGS->progjmp, SIE_INTERCEPT_WAIT);
                        }

                        if(
                            SIE_I_STOP(GUESTREGS)
                         || SIE_I_EXT(GUESTREGS)
                         || SIE_I_IO(GUESTREGS)
                         || SIE_I_HOST(regs)
                                          )
                        {
                            release_lock (&sysblk.intlock);
                            break;
                        }

                        INVALIDATE_AIA(GUESTREGS);

                        INVALIDATE_AEA_ALL(GUESTREGS);

                        {
                        struct timespec waittime;
                        struct timeval  now;

                            gettimeofday(&now, NULL);
                            waittime.tv_sec = now.tv_sec;
                            waittime.tv_nsec = ((now.tv_usec + 3333) * 1000);

                            sysblk.waitmask |= regs->cpumask;

                            timed_wait_condition
                                 (&INTCOND, &sysblk.intlock, &waittime);

                            sysblk.waitmask &= ~regs->cpumask;
                        }

                        release_lock (&sysblk.intlock);

                        break;

                    } /* end if(wait) */
#endif

                    release_lock(&sysblk.intlock);
                }

                if(
                    SIE_I_WAIT(GUESTREGS)
                                          )
                    break;


                GUESTREGS->instvalid = 0;

                INSTRUCTION_FETCH(GUESTREGS->inst, GUESTREGS->psw.IA, GUESTREGS);

                GUESTREGS->instvalid = 1;

#if defined(SIE_DEBUG)
                /* Display the instruction */
                ARCH_DEP(display_inst) (GUESTREGS, GUESTREGS->inst);
#endif /*defined(SIE_DEBUG)*/

                regs->instcount++;
                EXECUTE_INSTRUCTION(GUESTREGS->inst, 0, GUESTREGS);

#if defined(OPTION_CPU_UNROLL)
#ifdef FEATURE_PER
                if (!PER_MODE(GUESTREGS))
#endif
                {
                    regs->instcount += 7;
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                    UNROLLED_EXECUTE(GUESTREGS);
                }
#endif
            } while( !SIE_I_HOST(regs) );

        if(icode == 0 || icode == SIE_NO_INTERCEPT)
        {
            /* Check PER first, higher priority */
            if( OPEN_IC_PERINT(GUESTREGS) )
                ARCH_DEP(program_interrupt) (GUESTREGS, PGM_PER_EVENT);

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


#if defined(FEATURE_INTERPRETIVE_EXECUTION)
#if defined(FEATURE_REGION_RELOCATE)
/*-------------------------------------------------------------------*/
/* B23D STZP  - Store Zone Parameter                             [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_zone_parameter)
{
int     b2;                             /* Values of R fields        */
RADR    effective_addr2;                /* address of state desc.    */
ZPB     zpb;                            /* Zone Parameter Block      */
int     zone;                           /* Zone number               */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(regs->GR(2), regs);

    zone = regs->GR_LHLCL(1);

    if(zone >= FEATURE_SIE_MAXZONES)
    {
        regs->psw.cc = 3;
        return;
    }

    STORE_W(zpb.mso,sysblk.zpb[zone].mso);
    STORE_W(zpb.msl,sysblk.zpb[zone].msl);
    STORE_W(zpb.eso,sysblk.zpb[zone].eso);
    STORE_W(zpb.esl,sysblk.zpb[zone].esl);

    ARCH_DEP(vstorec(&zpb, sizeof(ZPB)-1,regs->GR(2), 2, regs));

    regs->psw.cc = 0;
}


/*-------------------------------------------------------------------*/
/* B23E SZP   - Set Zone Parameter                               [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_zone_parameter)
{
int     b2;                             /* Values of R fields        */
RADR    effective_addr2;                /* address of state desc.    */
ZPB     zpb;                            /* Zone Parameter Block      */
int     zone;                           /* Zone number               */
RADR    mso,                            /* Main Storage Origin       */
        msl,                            /* Main Storage Length       */
        eso,                            /* Expanded Storage Origin   */
        esl;                            /* Expanded Storage Length   */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(regs->GR(2), regs);

    zone = regs->GR_LHLCL(1);

    if(zone == 0 || zone >= FEATURE_SIE_MAXZONES)
    {
        regs->psw.cc = 3;
        return;
    }

    ARCH_DEP(vfetchc(&zpb, sizeof(ZPB)-1,regs->GR(2), 2, regs));

    FETCH_W(mso,zpb.mso);
    FETCH_W(msl,zpb.msl);
    FETCH_W(eso,zpb.eso);
    FETCH_W(esl,zpb.esl);

#if defined(FEATURE_ESAME)
    if(  (mso & ~ZPB2_MS_VALID)
      || (msl & ~ZPB2_MS_VALID)
      || (eso & ~ZPB2_ES_VALID)
      || (esl & ~ZPB2_ES_VALID) )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
#endif /*defined(FEATURE_ESAME)*/

    sysblk.zpb[zone].mso = mso;
    sysblk.zpb[zone].msl = msl;
    sysblk.zpb[zone].eso = eso;
    sysblk.zpb[zone].esl = esl;

    regs->psw.cc = 0;
}
#endif /*defined(FEATURE_REGION_RELOCATE)*/


#if defined(FEATURE_IO_ASSIST)
/*-------------------------------------------------------------------*/
/* B23F TPZI  - Test Pending Zone Interrupt                      [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_pending_zone_interrupt)
{
int     b2;                             /* Values of R fields        */
RADR    effective_addr2;                /* address of state desc.    */
U32     ioid;                           /* I/O interruption address  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     iointid;                        /* I/O interruption ident    */
FWORD   tpziid[3];
int     zone;                           /* Zone number               */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(regs->GR(2), regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    zone = regs->GR_LHLCL(1);

    if(zone >= FEATURE_SIE_MAXZONES)
    {
        regs->psw.cc = 0;
        return;
    }

    if( IS_IC_IOPENDING )
    {
        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* Test and clear pending interrupt, set condition code */
        if( ARCH_DEP(present_zone_io_interrupt) (&ioid, &ioparm,
                                                       &iointid, zone) )

        /* Store the SSID word and I/O parameter if an interrupt was pending */
        {
            /* Store interruption parms */
            STORE_FW(tpziid[0],ioid);
            STORE_FW(tpziid[1],ioparm);
            STORE_FW(tpziid[2],iointid);

            /* Release the interrupt lock */
            release_lock (&sysblk.intlock);

            ARCH_DEP(vstorec(&tpziid, sizeof(tpziid)-1,regs->GR(2), 2, regs));

            regs->psw.cc = 1;
        }
        else
        {
            /* Release the interrupt lock */
            release_lock (&sysblk.intlock);
            regs->psw.cc = 0;
        }

    }
    else
        regs->psw.cc = 0;
}


/*-------------------------------------------------------------------*/
/* DIAG 002   - Update Interrupt Interlock Control Bit in PMCW       */
/*-------------------------------------------------------------------*/
void ARCH_DEP(diagnose_002) (REGS *regs, int r1, int r3)
{
DEVBLK *dev;
U32    newgr1;

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Set newgr1 to the current value of pending and interlock control */
    newgr1 = ((dev->scsw.flag3 & SCSW3_SC_PEND)
              || (dev->pciscsw.flag3 & SCSW3_SC_PEND)) ? 0x02 : 0;
    if(dev->pmcw.flag27 & PMCW27_I)
        newgr1 |= 0x01;

    /* Do a compare-and-swap operation on the interrupt interlock
       control bit where both interlock and pending bits are
       compared, but only the interlock bit is swapped */
    if((regs->GR_L(r1) & 0x03) == newgr1)
    {
        dev->pmcw.flag27 &= ~PMCW27_I;
        dev->pmcw.flag27 |= (regs->GR_L(r3) & 0x01) ? PMCW27_I : 0;
        regs->psw.cc = 0;
    }
    else
    {
        regs->GR_L(r1) &= ~0x03;
        regs->GR_L(r1) |= newgr1;
        regs->psw.cc = 1;
    }

    /* Release the device lock */
    release_lock (&dev->lock);
}
#endif /*defined(FEATURE_IO_ASSIST)*/
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/


#endif


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "sie.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "sie.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
