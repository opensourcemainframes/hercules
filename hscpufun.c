/* HSCPUFUN.C   (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              CPU functions                                        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _HSCPUFUN_C_

#include "hercules.h"

/* Issue generic Missing device number message */
static inline void missing_devnum()
{
    WRMSG(HHC02201,"E");
}

/*-------------------------------------------------------------------*/
/* cpu command - define target cpu for panel display and commands    */
/*-------------------------------------------------------------------*/
int cpu_cmd(int argc, char *argv[], char *cmdline)
{
    BYTE    c;
    int     rc      =  0;
    int     cpu     = -1;
    int     currcpu = sysblk.pcpu;
    char    cmd[32768];

    bzero(cmd,sizeof(cmd));

    if (argc < 2)
    {
        WRMSG(HHC02202, "E", argv[0] );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1
     || cpu < 0 || cpu >= sysblk.maxcpu)
    {
        WRMSG(HHC02205, "E", argv[1], ": target processor is invalid" );
        return -1;
    }

    sysblk.dummyregs.cpuad = cpu;
    sysblk.pcpu = cpu;

    strlcpy(cmd,cmdline,sizeof(cmd));

    if ( argc > 2 )
    {
         u_int i = 0;
         u_int j = 0;
         u_int n = (u_int)strlen(cmd);

         /* Skip leading blanks, if any */
         for ( ; i < n && cmd[i] == ' '; i++ );

         /* Skip two words */
         for ( ; j < 2; j++ )
         {
           for ( ; i < n && cmd[i] != ' '; i++ );
           for ( ; i < n && cmd[i] == ' '; i++ );
         }

         /* Issue command to temporary target cpu */
         if (i < n)
         {
             rc = ProcessCmdLine(cmd+i);
             sysblk.pcpu = currcpu;
             sysblk.dummyregs.cpuad = currcpu;
         }
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* startall command - start all CPU's                                */
/*-------------------------------------------------------------------*/
int startall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int rc = 0;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
        OBTAIN_INTLOCK(NULL);
        mask = (~sysblk.started_mask) & sysblk.config_mask;
        for (i = 0; mask; i++)
        {
            if (mask & 1)
            {
                REGS *regs = sysblk.regs[i];
                regs->opinterv = 0;
                regs->cpustate = CPUSTATE_STARTED;
                signal_condition(&regs->intcond);
            }
            mask >>= 1;
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    return rc;
}


/*-------------------------------------------------------------------*/
/* stopall command - stop all CPU's                                  */
/*-------------------------------------------------------------------*/
DLL_EXPORT int stopall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int rc = 0;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
        OBTAIN_INTLOCK(NULL);

        mask = sysblk.started_mask;
        for (i = 0; mask; i++)
        {
            if (mask & 1)
            {
                REGS *regs = sysblk.regs[i];
                regs->opinterv = 1;
                regs->cpustate = CPUSTATE_STOPPING;
                ON_IC_INTERRUPT(regs);
                signal_condition(&regs->intcond);
            }
            mask >>= 1;
        }

        RELEASE_INTLOCK(NULL);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}

#ifdef _FEATURE_CPU_RECONFIG

/*-------------------------------------------------------------------*/
/* cf command - configure/deconfigure a CPU                          */
/*-------------------------------------------------------------------*/
int cf_cmd(int argc, char *argv[], char *cmdline)
{
    int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if ( CMD(argv[1],on,2) )
            on = 1;
        else if ( CMD(argv[1],off,3) )
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        if (on < 0)
            WRMSG(HHC00819, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu );
        else if (on == 0)
            deconfigure_cpu(sysblk.pcpu);
    }
    else
    {
        if (on < 0)
            WRMSG(HHC00820, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu );
        else if (on > 0)
            configure_cpu(sysblk.pcpu);
    }

    RELEASE_INTLOCK(NULL);

    if (on >= 0)
    {
        cf_cmd (1, argv, argv[0]);
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* cfall command - configure/deconfigure all CPU's                   */
/*-------------------------------------------------------------------*/
int cfall_cmd(int argc, char *argv[], char *cmdline)
{
static char *qproc[] = { "qproc", NULL };
int rc = 0;
int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if ( CMD(argv[1],on,2) )
            on = 1;
        else if ( CMD(argv[1],off,3) )
            on = 0;
        else
        {
            WRMSG( HHC17000, "E" );
            rc = -1;
        }
        if ( rc == 0 )
        {
            rc = configure_numcpu(on ? sysblk.maxcpu : 0);
        }
    }
    else if ( argc == 1 )
    {
        rc = qproc_cmd(1, qproc, *qproc);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    return rc;
}

#else
int cf_cmd(int argc, char *argv[], char *cmdline)
{    
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    WRMSG( HHC02310, "S", "cf", "FEATURE_CPU_RECONFIG" );
    return -1;
}
int cfall_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    WRMSG( HHC02310, "S", "cfall", "FEATURE_CPU_RECONFIG" );
    return -1;
}
#endif /*_FEATURE_CPU_RECONFIG*/


/*-------------------------------------------------------------------*/
/* system reset/system reset clear function                          */
/*-------------------------------------------------------------------*/
static int reset_cmd(int ac,char *av[],char *cmdline,int clear)
{
    int i;
    int rc;

    UNREFERENCED(ac);
    UNREFERENCED(av);
    UNREFERENCED(cmdline);
    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < sysblk.maxcpu; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02235, "E");
            return -1;
        }

    rc = system_reset (sysblk.pcpu, clear);

    RELEASE_INTLOCK(NULL);

    return rc;

}


/*-------------------------------------------------------------------*/
/* system reset command                                              */
/*-------------------------------------------------------------------*/
int sysreset_cmd(int ac,char *av[],char *cmdline)
{
    int rc = reset_cmd(ac,av,cmdline,0);
    if ( rc >= 0 )
    {
        WRMSG( HHC02311, "I", av[0] );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* system reset clear command                                        */
/*-------------------------------------------------------------------*/
int sysclear_cmd(int ac,char *av[],char *cmdline)
{
    int rc = reset_cmd(ac,av,cmdline,1);

    if ( rc >= 0 )
    {
        WRMSG( HHC02311, "I", av[0] );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* ipl function                                                      */
/*-------------------------------------------------------------------*/
int ipl_cmd2(int argc, char *argv[], char *cmdline, int clear)
{
BYTE c;                                 /* Character work area       */
int  rc;                                /* Return code               */
int  i;
#if defined(OPTION_IPLPARM)
int j;
size_t  maxb;
#endif
U16  lcss;
U16  devnum;
char *cdev, *clcss;
char *strtok_str = NULL;

#if defined(OPTION_IPLPARM)
char save_loadparm[16];
int  rest_loadparm = FALSE;

    save_loadparm[0] = '\0';
#endif

    /* Check that target processor type allows IPL */
    if (sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_IFA
     || sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_SUP)
    {
        WRMSG(HHC00818, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return -1;
    }

    /* Check the parameters of the IPL command */
    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }
#if defined(OPTION_IPLPARM)
#define MAXPARMSTRING   sizeof(sysblk.iplparmstring)
    sysblk.haveiplparm=0;
    maxb=0;
    if ( argc > 2 )
    {
        if ( CMD( argv[2], loadparm, 4) )
        {
            strlcpy( save_loadparm, str_loadparm(), sizeof(save_loadparm) );
            rest_loadparm = TRUE;
            if ( argc == 4 )
                set_loadparm(argv[3]);
        }
        else if ( CMD(argv[2], parm, 4) )
        {
            memset(sysblk.iplparmstring,0,MAXPARMSTRING);
            sysblk.haveiplparm=1;
            for ( i = 3; i < argc && maxb < MAXPARMSTRING; i++ )
            {
                if ( i !=3 )
                {
                    sysblk.iplparmstring[maxb++]=0x40;
                }
                for ( j = 0; j < (int)strlen(argv[i]) && maxb < MAXPARMSTRING; j++ )
                {
                    if ( islower(argv[i][j]) )
                    {
                        argv[i][j]=toupper(argv[i][j]);
                    }
                    sysblk.iplparmstring[maxb]=host_to_guest(argv[i][j]);
                    maxb++;
                }
            }
        }
    }
#endif

    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < sysblk.maxcpu; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate == CPUSTATE_STARTED)
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02236, "E");
            if ( rest_loadparm ) set_loadparm( save_loadparm );
            return -1;
        }

    /* The ipl device number might be in format lcss:devnum */
    if ( (cdev = strchr(argv[1],':')) )
    {
        clcss = argv[1];
        cdev++;
    }
    else
    {
        clcss = NULL;
        cdev = argv[1];
    }

    /* If the ipl device is not a valid hex number we assume */
    /* This is a load from the service processor             */
    if (sscanf(cdev, "%hx%c", &devnum, &c) != 1)
        rc = load_hmc(strtok_r(cmdline+3+clear," \t", &strtok_str ), sysblk.pcpu, clear);
    else
    {
        *--cdev = '\0';

        if (clcss)
        {
            if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
            {
                WRMSG(HHC02205, "E", clcss, ": LCSS id is invalid" );
                if ( rest_loadparm ) set_loadparm( save_loadparm );
                return -1;
            }
        }
        else
            lcss = 0;

        if((rc = load_ipl (lcss, devnum, sysblk.pcpu, clear)))
        {
            switch (rc)
            {
            case HERRCPUOFF:
                WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
                rc = 5;
                break;
            }
        }
    }

    RELEASE_INTLOCK(NULL);

    return rc;
}


/*-------------------------------------------------------------------*/
/* ipl command                                                       */
/*-------------------------------------------------------------------*/
int ipl_cmd(int argc, char *argv[], char *cmdline)
{
    return ipl_cmd2(argc,argv,cmdline,0);
}


/*-------------------------------------------------------------------*/
/* ipl clear command                                                 */
/*-------------------------------------------------------------------*/
int iplc_cmd(int argc, char *argv[], char *cmdline)
{
    return ipl_cmd2(argc,argv,cmdline,1);
}



#ifdef OPTION_CAPPING
/*-------------------------------------------------------------------*/
/* capping - cap mip rate                                            */
/*-------------------------------------------------------------------*/
int capping_cmd(int argc, char *argv[], char *cmdline)
{
U32     cap;
BYTE    c;
int     rc = 0;

    UNREFERENCED(cmdline);

    /* Update capping value */
    if ( argc == 2 )
    {
        if( CMD( argv[1], off, 3))
            rc = configure_capping(0);
        else
        if (strlen(argv[1]) >= 1
          && sscanf(argv[1], "%u%c", &cap, &c) == 1)
        {
            if((rc = configure_capping(cap)))
            {
                WRMSG(HHC00102, "E", strerror(rc));
                rc = -1;
            }
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            rc = -1;
        }
    }
    else
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    else
    {
        if ( sysblk.capvalue == 0 )
        {
            WRMSG( HHC00838, "I" );
        }
        else
            WRMSG( HHC00832, "I", sysblk.capvalue );
    }

    return rc;
}
#endif // OPTION_CAPPING


/*-------------------------------------------------------------------*/
/* restart command - generate restart interrupt                      */
/*-------------------------------------------------------------------*/
int restart_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    /* Check that target processor type allows IPL */
    if (sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_IFA
     || sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_SUP)
    {
        WRMSG(HHC00818, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return -1;
    }

    WRMSG(HHC02228, "I", "restart");

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }

    /* Indicate that a restart interrupt is pending */
    ON_IC_RESTART(sysblk.regs[sysblk.pcpu]);

    /* Ensure that a stopped CPU will recognize the restart */
    if (sysblk.regs[sysblk.pcpu]->cpustate == CPUSTATE_STOPPED)
        sysblk.regs[sysblk.pcpu]->cpustate = CPUSTATE_STOPPING;

    sysblk.regs[sysblk.pcpu]->checkstop = 0;

    /* Signal CPU that an interrupt is pending */
    WAKEUP_CPU (sysblk.regs[sysblk.pcpu]);

    /* Release the interrupt lock */
    RELEASE_INTLOCK(NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* ext command - generate external interrupt                         */
/*-------------------------------------------------------------------*/
int ext_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);

    ON_IC_INTKEY;

    WRMSG(HHC02228, "I", "interrupt");

    /* Signal waiting CPUs that an interrupt is pending */
    WAKEUP_CPUS_MASK (sysblk.waiting_mask);

    RELEASE_INTLOCK(NULL);

    return 0;
}

/*-------------------------------------------------------------------*/
/* timerint - display or set the timer interval                      */
/*-------------------------------------------------------------------*/
int timerint_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;
    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        if ( CMD( argv[1], default, 7 ) || CMD( argv[1], reset, 5 ) )
        {
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], argv[1] );
        }
        else
        {
            int timerint = 0; BYTE c;

            if (1
                && sscanf(argv[1], "%d%c", &timerint, &c) == 1
                && timerint >= 1
                && timerint <= 1000000
            )
            {
                sysblk.timerint = timerint;
                if ( MLVL(VERBOSE) )
                {
                    char buf[25];
                    MSGBUF( buf, "%d", sysblk.timerint);
                    WRMSG(HHC02204, "I", argv[0], buf );
                }
            }
            else
            {
                WRMSG( HHC02205, "E", argv[1], ": must be 'default' or n where 1<=n<=1000000" );
                rc = -1;
            }
        }
    }
    else if ( argc == 1 )
    {
        char buf[25];
        MSGBUF( buf, "%d", sysblk.timerint);
        WRMSG(HHC02203, "I", argv[0], buf );
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/* format_tod - generate displayable date from TOD value */
/* always uses epoch of 1900 */
char * format_tod(char *buf, U64 tod, int flagdate)
{
    int leapyear, years, days, hours, minutes, seconds, microseconds;

    if ( tod >= TOD_YEAR )
    {
        tod -= TOD_YEAR;
        years = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if ( ( leapyear = tod / TOD_YEAR ) == 4 )
        {
            tod %= TOD_YEAR;
            years--;
            tod += TOD_YEAR;
        }
        else
            tod %= TOD_YEAR;

        years += leapyear;
    }
    else
        years = 0;

    days = tod / TOD_DAY;
    tod %= TOD_DAY;
    hours = tod / TOD_HOUR;
    tod %= TOD_HOUR;
    minutes = tod / TOD_MIN;
    tod %= TOD_MIN;
    seconds = tod / TOD_SEC;
    microseconds = (tod % TOD_SEC) / TOD_USEC;

    if (flagdate)
    {
        years += 1900;
        days += 1;
    }

    sprintf(buf,"%4d.%03d %02d:%02d:%02d.%06d",
        years,days,hours,minutes,seconds,microseconds);

    return buf;
}


/*-------------------------------------------------------------------*/
/* clocks command - display tod clkc and cpu timer                   */
/*-------------------------------------------------------------------*/
int clocks_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char clock_buf[30];
U64 tod_now;
U64 hw_now;
S64 epoch_now;
U64 epoch_now_abs;
char epoch_sign;
U64 clkc_now;
S64 cpt_now;
#if defined(_FEATURE_SIE)
U64 vtod_now = 0;
S64 vepoch_now = 0;
U64 vepoch_now_abs = 0;
char vepoch_sign = ' ';
U64 vclkc_now = 0;
S64 vcpt_now = 0;
char sie_flag = 0;
#endif
U32 itimer = 0;
char itimer_formatted[32];
char arch370_flag = 0;
char buf[64];
int rc = 0;

    UNREFERENCED(cmdline);

    if ( argc == 1 ) for (;;)        
    {
        obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

        if (!IS_CPU_ONLINE(sysblk.pcpu))
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
            break;
        }
        regs = sysblk.regs[sysblk.pcpu];

    /* Get the clock values all at once for consistency and so we can
        release the CPU lock more quickly. */
        tod_now = (tod_clock(regs) << 8) >> 8;
        hw_now = hw_tod;
        epoch_now = regs->tod_epoch;
        clkc_now = regs->clkc;
        cpt_now = CPU_TIMER(regs);
#if defined(_FEATURE_SIE)
        if ( regs->sie_active )
        {
            vtod_now = (TOD_CLOCK(regs->guestregs) << 8) >> 8;
            vepoch_now = regs->guestregs->tod_epoch;
            vclkc_now = regs->guestregs->clkc;
            vcpt_now = CPU_TIMER(regs->guestregs);
            sie_flag = 1;
        }
#endif
        if (regs->arch_mode == ARCH_370)
        {
            itimer = INT_TIMER(regs);
        /* The interval timer counts 76800 per second, or one every
           13.0208 microseconds. */
            MSGBUF(itimer_formatted,"%02u:%02u:%02u.%06u",
                (itimer/(76800*60*60)),((itimer%(76800*60*60))/(76800*60)),
                ((itimer%(76800*60))/76800),((itimer%76800)*13));
            arch370_flag = 1;
        }

        release_lock(&sysblk.cpulock[sysblk.pcpu]);

        MSGBUF( buf, "tod = %16.16" I64_FMT "X    %s",
               (tod_now << 8),format_tod(clock_buf,tod_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        MSGBUF( buf, "h/w = %16.16" I64_FMT "X    %s",
               (hw_now << 8),format_tod(clock_buf,hw_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        if (epoch_now < 0) 
        {
            epoch_now_abs = -(epoch_now);
            epoch_sign = '-';
        }
        else
        {
            epoch_now_abs = epoch_now;
            epoch_sign = ' ';
        }
        MSGBUF( buf, "off = %16.16" I64_FMT "X   %c%s",
               (epoch_now << 8),epoch_sign,
               format_tod(clock_buf,epoch_now_abs,FALSE));
        WRMSG(HHC02274, "I", buf);

        MSGBUF( buf, "ckc = %16.16" I64_FMT "X    %s",
               (clkc_now << 8),format_tod(clock_buf,clkc_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        if (regs->cpustate != CPUSTATE_STOPPED)
            MSGBUF( buf, "cpt = %16.16" I64_FMT "X", cpt_now << 8);
        else
            MSGBUF( buf, "cpt = not decrementing");
        WRMSG(HHC02274, "I", buf);

#if defined(_FEATURE_SIE)
        if (sie_flag)
        {

            MSGBUF( buf, "vtod = %16.16" I64_FMT "X    %s",
                   (vtod_now << 8), format_tod(clock_buf,vtod_now,TRUE) );
            WRMSG(HHC02274, "I", buf);

            if (vepoch_now < 0) 
            {
                vepoch_now_abs = -(vepoch_now);
                vepoch_sign = '-';
            }
            else
            {
                vepoch_now_abs = vepoch_now;
                vepoch_sign = ' ';
            }
            MSGBUF( buf, "voff = %16.16" I64_FMT "X   %c%s",
                   (vepoch_now << 8),vepoch_sign,
                   format_tod(clock_buf,vepoch_now_abs,FALSE));
            WRMSG(HHC02274, "I", buf);

            MSGBUF( buf, "vckc = %16.16" I64_FMT "X    %s",
                   (vclkc_now << 8),format_tod(clock_buf,vclkc_now,TRUE));
            WRMSG(HHC02274, "I", buf);

            MSGBUF( buf, "vcpt = %16.16" I64_FMT "X",vcpt_now << 8);
            WRMSG(HHC02274, "I", buf);
        }
#endif

        if (arch370_flag)
        {
            MSGBUF( buf, "itm = %8.8" I32_FMT "X                     %s",
                   itimer, itimer_formatted );
            WRMSG(HHC02274, "I", buf);
        }
        break;
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* store command - store CPU status at absolute zero                 */
/*-------------------------------------------------------------------*/
int store_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (regs->cpustate != CPUSTATE_STOPPED)
    {
        WRMSG(HHC02224, "E");
        return -1;
    }

    /* Store status in 512 byte block at absolute location 0 */
    store_status (regs, 0);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC00817, "I", PTYPSTR(regs->cpuad), regs->cpuad);

    return 0;
}

/*-------------------------------------------------------------------*/
/* start command - start current CPU                                 */
/*-------------------------------------------------------------------*/
int start_cmd_cpu(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    OBTAIN_INTLOCK(NULL);
    
    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        REGS *regs = sysblk.regs[sysblk.pcpu];
        if ( regs->cpustate == CPUSTATE_STARTED )
        {
            WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "stopped");
            rc = 1;
        }
        else
        {
            regs->opinterv = 0;
            regs->cpustate = CPUSTATE_STARTED;
            regs->checkstop = 0;
            WAKEUP_CPU(regs);
            WRMSG( HHC00834, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "running state selected" );
        }
    }
    else
    {
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        rc = 1;
    }

    RELEASE_INTLOCK(NULL);

    return rc;
}

/*-------------------------------------------------------------------*/
/* stop command - stop current CPU                                   */
/*-------------------------------------------------------------------*/
int stop_cmd_cpu(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    OBTAIN_INTLOCK(NULL);

    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        REGS *regs = sysblk.regs[sysblk.pcpu];
        if ( regs->cpustate != CPUSTATE_STARTED )
        {
            WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "started");
            rc = 1;
        }
        else
        {
            regs->opinterv = 1;
            regs->cpustate = CPUSTATE_STOPPING;
            ON_IC_INTERRUPT(regs);
            WAKEUP_CPU (regs);
            WRMSG( HHC00834, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "manual state selected" );
        }
    }
    else
    {
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        rc = 1;
    }
            
    RELEASE_INTLOCK(NULL);

    return rc;
}

#if defined(_FEATURE_ASN_AND_LX_REUSE)
/*-------------------------------------------------------------------*/
/* alrf command - display or set asn_and_lx_reuse                    */
/*-------------------------------------------------------------------*/
int alrf_cmd(int argc, char *argv[], char *cmdline)
{
static char *ArchlvlCmd[3] = { "archlvl", "Query", "asn_lx_reuse" };
int     rc = 0;

    UNREFERENCED(cmdline);

    WRMSG( HHC02256, "W", "ALRF", "archlvl enable|disable|query asn_lx_reuse" );

    if ( argc == 2 )
    {
        ArchlvlCmd[1] = argv[1];
        ProcessCommand(3,ArchlvlCmd,NULL);
    }
    else if ( argc == 1 )
    {
        ProcessCommand(3,ArchlvlCmd,NULL);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}
#endif /*defined(_FEATURE_ASN_AND_LX_REUSE)*/

