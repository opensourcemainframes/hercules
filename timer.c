/* TIMER.C   */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

#include "hercules.h"

#include "opcode.h"

#include "feat390.h"
#include "feat370.h"

int ecpsvm_testvtimer(REGS *,int);

void check_timer_event(void);

/*-------------------------------------------------------------------*/
/* Update TOD clock                                                  */
/*                                                                   */
/* This function updates the TOD clock.                              */
/*                                                                   */
/* This function is called by timer_update_thread and by cpu_thread  */
/* instructions that manipulate any of the timer related entities    */
/* (clock comparator, cpu timer and interval timer).                 */
/*                                                                   */
/* Internal function `check_timer_event' is called which will signal */
/* any timer related interrupts to the appropriate cpu_thread.       */
/*                                                                   */
/* Callers *must* own the todlock and *must not* own the intlock.    */
/*                                                                   */
/*-------------------------------------------------------------------*/
void update_TOD_clock(void)
{
struct timeval  tv;         /* Current time              */
U64     dreg;           /* Double register work area */

    /* Get current time */
    gettimeofday (&tv, NULL);
    ADJUST_TOD (tv, sysblk.lasttod);

    /* Load number of seconds since 00:00:00 01 Jan 1970 */
    dreg = (U64)tv.tv_sec;

    /* Convert to microseconds */
    dreg = dreg * 1000000 + tv.tv_usec;

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
    if (sysblk.toddrag > 1)
        dreg = sysblk.todclock_init +
        (dreg - sysblk.todclock_init) / sysblk.toddrag;
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

    /* Shift left 4 bits so that bits 0-7=TOD Clock Epoch,
       bits 8-59=TOD Clock bits 0-51, bits 60-63=zero */
    dreg <<= 4;

    /* Ensure that the clock does not go backwards and always
       returns a unique value in the microsecond range */
    if( dreg > sysblk.todclk)
        sysblk.todclk = dreg;
    else sysblk.todclk += 16;

    /* Get the difference between the last TOD saved and this one */
    sysblk.todclock_diff = (sysblk.todclock_prev == 0 ? 0 :
                            sysblk.todclk - sysblk.todclock_prev);

    /* Save the old TOD clock value */
    sysblk.todclock_prev = sysblk.todclk;

    /* Check if timer event has occurred */
    check_timer_event();
}

/*-------------------------------------------------------------------*/
/* Check for timer event                                             */
/*                                                                   */
/* Checks for the following interrupts:                              */
/* [1] Clock comparator                                              */
/* [2] CPU timer                                                     */
/* [3] Interval timer                                                */
/* CPUs with an outstanding interrupt are signalled                  */
/*                                                                   */
/*-------------------------------------------------------------------*/
void check_timer_event(void)
{
int             cpu;                    /* CPU counter               */
REGS           *regs;                   /* -> CPU register context   */
PSA_3XX        *psa;                    /* -> Prefixed storage area  */
S32             itimer;                 /* Interval timer value      */
S32             olditimer;              /* Previous interval timer   */
#if defined(OPTION_MIPS_COUNTING) && ( defined(_FEATURE_SIE) || defined(_FEATURE_ECPSVM) )
S32             itimer_diff;            /* TOD difference in TU      */
#endif
U32             intmask = 0;            /* Interrupt CPU mask        */

    /* Access the diffent register contexts with the intlock held */
    obtain_lock (&sysblk.intlock);

    /* Check for [1] clock comparator, [2] cpu timer, and
     * [3] interval timer interrupts for each CPU.
     */
    for (cpu = 0; cpu < HI_CPU; cpu++)
    {
        /* Ignore this CPU if it is not started */
        if (!IS_CPU_ONLINE(cpu)
         || CPUSTATE_STOPPED == sysblk.regs[cpu]->cpustate)
            continue;

        /* Point to the CPU register context */
        regs = sysblk.regs[cpu];

        /*-------------------------------------------*
         * [1] Check for clock comparator interrupt  *
         *-------------------------------------------*/
        if ((sysblk.todclk + regs->todoffset) > regs->clkc)
        {
            if (!IS_IC_CLKC(regs))
            {
                ON_IC_CLKC(regs);
                intmask |= BIT(regs->cpuad);
            }
        }
        else if (IS_IC_CLKC(regs))
            OFF_IC_CLKC(regs);

#if defined(_FEATURE_SIE)
        /* If running under SIE also check the SIE copy */
        if(regs->sie_active)
        {
        /* Signal clock comparator interrupt if needed */
            if((sysblk.todclk + regs->guestregs->todoffset) > regs->guestregs->clkc)
            {
                ON_IC_CLKC(regs->guestregs);
                intmask |= BIT(regs->cpuad);
            }
            else
                OFF_IC_CLKC(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/

        /*-------------------------------------------*
         * [2] Decrement the CPU timer for each CPU  *
         *-------------------------------------------*/

        (S64)regs->ptimer -= (S64)sysblk.todclock_diff << 8;

        /* Set interrupt flag if the CPU timer is negative */
        if ((S64)regs->ptimer < 0)
        {
            if (!IS_IC_PTIMER(regs))
            {
                ON_IC_PTIMER(regs);
                intmask |= BIT(regs->cpuad);
            }
        }
        else if(IS_IC_PTIMER(regs))
            OFF_IC_PTIMER(regs);

#if defined(_FEATURE_SIE)
        /* When running under SIE also update the SIE copy */
        if(regs->sie_active)
        {
            /* Decrement the guest CPU timer */
            (S64)regs->guestregs->ptimer -= (S64)sysblk.todclock_diff << 8;

            /* Set interrupt flag if the CPU timer is negative */
            if ((S64)regs->guestregs->ptimer < 0)
            {
                ON_IC_PTIMER(regs->guestregs);
                intmask |= BIT(regs->cpuad);
            }
            else
                OFF_IC_PTIMER(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/

        /*-------------------------------------------*
         * [3] Check for interval timer interrupt    *
         *-------------------------------------------*/

#if defined(OPTION_MIPS_COUNTING) && ( defined(_FEATURE_SIE) || defined(_FEATURE_ECPSVM) )
        /* Calculate diff in interval timer units plus rounding to improve accuracy */
        itimer_diff = (S32) ((((6*sysblk.todclock_diff)/625)+1) >> 1);
        if (itimer_diff <= 0)           /* Handle gettimeofday low */
            itimer_diff = 1;            /* resolution problems     */
#endif

        if(regs->arch_mode == ARCH_370)
        {
            /* Point to PSA in main storage */
            psa = (PSA_3XX*)(regs->mainstor + regs->PX);

                    /* Decrement the location 80 timer */
            FETCH_FW(itimer,psa->inttimer);
                    olditimer = itimer;

                    /* The interval timer is decremented as though bit 23 is
                       decremented by one every 1/300 of a second. This comes
                       out to subtracting 768 (X'300') every 1/100 of a second.
                       76800/CLK_TCK comes out to 768 on Intel versions of
                       Linux, where the clock ticks every 1/100 second; it
                       comes out to 75 on the Alpha, with its 1024/second
                       tick interval. See 370 POO page 4-29. (ESA doesn't
                       even have an interval timer.) */
#if defined(OPTION_MIPS_COUNTING) && ( defined(_FEATURE_SIE) || defined(_FEATURE_ECPSVM) )
            itimer -= itimer_diff;
#else
            itimer -= 76800 / CLK_TCK;
#endif
            STORE_FW(psa->inttimer,itimer);

            /* Set interrupt flag and interval timer interrupt pending
               if the interval timer went from positive to negative */
            if (itimer < 0 && olditimer >= 0)
            {
#if defined(_FEATURE_ECPSVM)
                regs->rtimerint=1;      /* To resolve concurrent V/R Int Timer Ints */
#endif
                ON_IC_ITIMER(regs);
                intmask |= BIT(regs->cpuad);
            }
#if defined(_FEATURE_ECPSVM)
#if defined(OPTION_MIPS_COUNTING)
            if(ecpsvm_testvtimer(regs,itimer_diff)==0)
#else /* OPTION_MIPS_COUNTING */
            if(ecpsvm_testvtimer(regs,76800 / CLK_TCK)==0)
#endif /* OPTION_MIPS_COUNTING */
            {
                ON_IC_ITIMER(regs);
                intmask |= BIT(regs->cpuad);
            }
#endif /* _FEATURE_ECPSVM */

        } /*if(regs->arch_mode == ARCH_370)*/

#if defined(_FEATURE_SIE)
        /* When running under SIE also update the SIE copy */
        if(regs->sie_active)
        {
            if(SIE_STATB(regs->guestregs, M, 370)
              && SIE_STATNB(regs->guestregs, M, ITMOF))
            {
                /* Decrement the location 80 timer */
                FETCH_FW(itimer,regs->guestregs->sie_psa->inttimer);
                olditimer = itimer;

#if defined(OPTION_MIPS_COUNTING)
                itimer -= itimer_diff;
#else
                itimer -= 76800 / CLK_TCK;
#endif
                STORE_FW(regs->guestregs->sie_psa->inttimer,itimer);

                /* Set interrupt flag and interval timer interrupt pending
                   if the interval timer went from positive to negative */
                if (itimer < 0 && olditimer >= 0)
                {
                    ON_IC_ITIMER(regs->guestregs);
                    intmask |= BIT(regs->cpuad);
                }
            }
        }
#endif /*defined(_FEATURE_SIE)*/


    } /* end for(cpu) */

    /* If a timer interrupt condition was detected for any CPU
       then wake up those CPUs if they are waiting */
    WAKEUP_CPUS_MASK (intmask);

    release_lock(&sysblk.intlock);

} /* end function check_timer_event */

/*-------------------------------------------------------------------*/
/* TOD clock and timer thread                                        */
/*                                                                   */
/* This function runs as a separate thread.  It wakes up every       */
/* x milliseconds, updates the TOD clock, and decrements the         */
/* CPU timer for each CPU.  If any CPU timer goes negative, or       */
/* if the TOD clock exceeds the clock comparator for any CPU,        */
/* it signals any waiting CPUs to wake up and process interrupts.    */
/*-------------------------------------------------------------------*/
void *timer_update_thread (void *argp)
{
#ifdef OPTION_MIPS_COUNTING
int     msecctr = 0;                    /* Millisecond counter       */
int     cpu;                            /* CPU counter               */
REGS   *regs;                           /* -> CPU register context   */
U64     prev = 0;                       /* Previous TOD clock value  */
U64     diff;                           /* Difference between new and
                                           previous TOD clock values */
U64     waittime;                       /* CPU wait time in interval */
U64     now = 0;                        /* Current time of day (us)  */
U64     then;                           /* Previous time of day (us) */
int     interval;                       /* Interval (us)             */
#endif /*OPTION_MIPS_COUNTING*/
struct  timeval tv;                     /* Structure for gettimeofday
                                           and select function calls */
  UNREFERENCED(argp);

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set timer thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.todprio))
        logmsg (_("HHCTT001W Timer thread set priority %d failed: %s\n"),
                sysblk.todprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    logmsg (_("HHCTT002I Timer thread started: tid="TIDPAT", pid=%d, "
            "priority=%d\n"),
            thread_id(), getpid(), getpriority(PRIO_PROCESS,0));

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
    /* Get current time */
    gettimeofday (&tv, NULL);

    /* Load number of seconds since 00:00:00 01 Jan 1970 */
    sysblk.todclock_init = (U64)tv.tv_sec;

    /* Convert to microseconds */
    sysblk.todclock_init = sysblk.todclock_init * 1000000 + tv.tv_usec;
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

    while (sysblk.cpus)
    {
        /* Obtain the TOD lock */
        obtain_lock (&sysblk.todlock);

        /* Update TOD clock */
        update_TOD_clock();

#ifdef OPTION_MIPS_COUNTING
        /* Calculate MIPS rate and percentage CPU busy */

        /* Get the difference between the last TOD saved and this one */
        diff = (prev == 0 ? 0 : sysblk.todclk - prev);

        /* Save the old TOD clock value */
        prev = sysblk.todclk;

        /* Shift the epoch out of the difference for the CPU timer */
        diff <<= 8;

        msecctr += (int)(diff/4096000);
        if (msecctr > 999)
        {
            U32  mipsrate = 0;   /* (total for ALL CPUs together) */
            U32  siosrate = 0;   /* (total for ALL CPUs together) */
            /* Get current time */
            then = now;
            gettimeofday (&tv, NULL);
            now = (U64)tv.tv_sec;
            now = now * 1000000 + tv.tv_usec;
            interval = (int)(now - then);

#if defined(OPTION_SHARED_DEVICES)
            sysblk.shrdrate = sysblk.shrdcount;
            sysblk.shrdcount = 0;
            siosrate = sysblk.shrdrate;
#endif

            for (cpu = 0; cpu < HI_CPU; cpu++)
            {
                if (!IS_CPU_ONLINE(cpu))
                    continue;

                obtain_lock (&sysblk.cpulock[cpu]);

                if (!IS_CPU_ONLINE(cpu))
                {
                    release_lock(&sysblk.cpulock[cpu]);
                    continue;
                }

                regs = sysblk.regs[cpu];

                /* 0% if first time thru */
                if (then == 0 || regs->waittod == 0)
                {
                    regs->mipsrate = regs->siosrate = 0;
                    regs->cpupct = 0.0;
                    release_lock(&sysblk.cpulock[cpu]);
                    continue;
                }

                /* Calculate instructions/millisecond for this CPU */
                regs->mipsrate =
                    ((regs->instcount - regs->prevcount)*1000) / interval;
                regs->siosrate = regs->siocount;

                /* Total for ALL CPUs together */
                mipsrate += regs->mipsrate;
                siosrate += regs->siosrate;

                /* Save the instruction counter */
                regs->prevcount = regs->instcount;
                regs->siocount = 0;

                /* Calculate CPU busy percentage */
                waittime = regs->waittime;
                if ( test_bit (4, regs->cpuad, &sysblk.waiting_mask) )
                    waittime += now - regs->waittod;
                regs->cpupct = ((interval - waittime)*1.0) / (interval*1.0);

                /* Reset the wait values */
                regs->waittime = 0;
                regs->waittod = now;

                release_lock(&sysblk.cpulock[cpu]);

            } /* end for(cpu) */

            /* Total for ALL CPUs together */
            sysblk.mipsrate = mipsrate;
            sysblk.siosrate = siosrate;

            /* Reset the millisecond counter */
            msecctr = 0;

        } /* end if(msecctr) */
#endif /*OPTION_MIPS_COUNTING*/

        /* Release the TOD lock */
        release_lock (&sysblk.todlock);

        /* Sleep for one system clock tick by specifying a one-microsecond
           delay, which will get stretched out to the next clock tick */
        tv.tv_sec = 0;
        tv.tv_usec = 1;
        select (0, NULL, NULL, NULL, &tv);

    } /* end while */

    logmsg (_("HHCTT003I Timer thread ended\n"));

    sysblk.todtid = 0;

    return NULL;

} /* end function timer_update_thread */
