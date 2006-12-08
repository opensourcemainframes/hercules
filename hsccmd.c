/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2006                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2006     */
/*              Execute Hercules System Commands                     */
/*                                                                   */
/*   Released under the Q Public License (http://www.conmicro.cx/    */
/*     hercules/herclic.html) as modifications to Hercules.          */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* (i.e. hardware console) commands that the emulator supports.      */
/* To define a new commmand, add an entry to the "Commands" CMDTAB   */
/* table pointing to the command processing function, and optionally */
/* add additional help text to the HelpTab HELPTAB. Both tables are  */
/* near the end of this module.                                      */
/*-------------------------------------------------------------------*/

// $Log$

#include "hstdinc.h"

#define _HSCCMD_C_
#define _HENGINE_DLL_

#include "hercules.h"

#include "devtype.h"

#include "opcode.h"

#include "history.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif /* defined(OPTION_FISHIO) */

#include "tapedev.h"
#include "dasdtab.h"

///////////////////////////////////////////////////////////////////////
// (forward references, etc)

#define MAX_DEVLIST_DEVICES  1024

#if defined(FEATURE_ECPSVM)
extern void ecpsvm_command(int argc,char **argv);
#endif
int process_script_file(char *,int);


///////////////////////////////////////////////////////////////////////
/* $test_cmd - do something or other */

#ifdef _MSVC_ // (damn optimizer is getting in the way so disable it)
#pragma optimize( "", off )
#endif

int $test_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    cause_crash();
    return 0;
}

#ifdef _MSVC_ // (damn optimizer is getting in the way so disable it)
#pragma optimize( "", on )
#endif

/* Issue generic Device not found error message */
static inline int devnotfound_msg(U16 lcss,U16 devnum)
{
    logmsg(_("HHCPN181E Device number %d:%4.4X not found\n"),lcss,devnum);
    return -1;
}
/* Issue generic Missing device number message */
static inline void missing_devnum()
{
    logmsg( _("HHCPN031E Missing device number\n") );
}

///////////////////////////////////////////////////////////////////////
/* maxrates command - report maximum seen mips/sios rates */

#ifdef OPTION_MIPS_COUNTING

int maxrates_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int bError = FALSE;
        if (argc > 2)
        {
            logmsg( _("Improper command format") );
            bError = TRUE;
        }
        else
        {
            int   interval = 0;
            BYTE  c;
            if ( sscanf( argv[1], "%d%c", &interval, &c ) != 1 || interval < 1 )
            {
                logmsg( _("\"%s\": invalid maxrates interval"), argv[1] );
                bError = TRUE;
            }
            else
            {
                maxrates_rpt_intvl = interval;
                logmsg( _("Maxrates interval set to %d minutes.\n"), maxrates_rpt_intvl );
            }
        }
        if (bError)
            logmsg( _("; enter \"help maxrates\" for help.\n") );
    }
    else
    {
        char*   pszPrevIntervalStartDateTime;
        char*   pszCurrIntervalStartDateTime;
        char*   pszCurrentDateTime;
        time_t  current_time;

        current_time = time( NULL );

        pszPrevIntervalStartDateTime = strdup( ctime( &prev_int_start_time ) );
        pszCurrIntervalStartDateTime = strdup( ctime( &curr_int_start_time ) );
        pszCurrentDateTime           = strdup( ctime(    &current_time     ) );

        logmsg
        (
            "Highest observed MIPS/SIOS rates:\n\n"

            "  From: %s"
            "  To:   %s\n"

            ,pszPrevIntervalStartDateTime
            ,pszCurrIntervalStartDateTime
        );

        logmsg
        (
            "        MIPS: %2.1d.%2.2d\n"
            "        SIOS: %d\n\n"

            ,prev_high_mips_rate / 1000000
            ,prev_high_mips_rate % 1000000
            ,prev_high_sios_rate
        );

        logmsg
        (
            "  From: %s"
            "  To:   %s\n"

            ,pszCurrIntervalStartDateTime
            ,pszCurrentDateTime
        );

        logmsg
        (
            "        MIPS: %2.1d.%2.2d\n"
            "        SIOS: %d\n\n"

            ,curr_high_mips_rate / 1000000
            ,curr_high_mips_rate % 1000000
            ,curr_high_sios_rate
        );

        logmsg
        (
            "Current interval = %d minutes.\n"

            ,maxrates_rpt_intvl
        );

        free( pszPrevIntervalStartDateTime );
        free( pszCurrIntervalStartDateTime );
        free( pszCurrentDateTime           );
    }

    return 0;   // (make compiler happy)
}

#endif // OPTION_MIPS_COUNTING

///////////////////////////////////////////////////////////////////////
/* comment command - do absolutely nothing */

int comment_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    // Do nothing; command has already been echo'ed to console...
    return 0;   // (make compiler happy)
}

///////////////////////////////////////////////////////////////////////
/* quit or exit command - terminate the emulator */

int quit_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    do_shutdown();
    return 0;   /* (make compiler happy) */
}

///////////////////////////////////////////////////////////////////////
/* history command  */

int History(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    /* last stored command is for sure command 'hst' so remove it
       this is the only place where history_remove is called */
    history_remove();
    history_requested = 1;
    /* only 'hst' called */
    if (argc == 1) {
      if (history_relative_line(-1) == -1)
        history_requested = 0;
      return 0;
    }
    /* hst with argument called */
    if (argc == 2) {
      int x;
      switch (argv[1][0]) {
      case 'l':
        history_show();
        history_requested = 0;
        break;
      default:
        x = atoi(argv[1]);
        if (x>0) {
          if (history_absolute_line(x) == -1)
            history_requested = 0;
        }
        else {
          if (x<0) {
            if (history_relative_line(x) == -1)
              history_requested = 0;
          }
          else {
            /* x == 0 */
            history_show();
            history_requested = 0;
          }
        }
      }
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////
/* log command - direct log output */

int log_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc > 1)
    {
        if(strcasecmp("off",argv[1]))
            log_sethrdcpy(argv[1]);
        else
            log_sethrdcpy(NULL);
    }
    else
        logmsg(_("HHCPN160E no argument\n"));

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* version command - display version information */

int version_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_version (stdout, "Hercules ", TRUE);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* start command (or just Enter) - start CPU (or printer device if argument given) */

int start_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        OBTAIN_INTLOCK(NULL);
        if (IS_CPU_ONLINE(sysblk.pcpu))
        {
            REGS *regs = sysblk.regs[sysblk.pcpu];
            regs->opinterv = 0;
            regs->cpustate = CPUSTATE_STARTED;
            regs->checkstop = 0;
            WAKEUP_CPU(regs);
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        /* start specified printer device */

        U16      devnum;
        U16      lcss;
        int      stopprt;
        DEVBLK*  dev;
        char*    devclass;
        char     devnam[256];
        int      rc;

        rc=parse_single_devnum(argv[1],&lcss,&devnum);
        if (rc<0)
        {
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss,devnum)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN017E Device %d:%4.4X is not a printer device\n"),
                      lcss, devnum );
            return -1;
        }

        /* un-stop the printer and raise attention interrupt */

        stopprt = dev->stopprt; dev->stopprt = 0;

        rc = device_attention (dev, CSW_ATTN);

        if (rc) dev->stopprt = stopprt;

        switch (rc) {
            case 0: logmsg(_("HHCPN018I Printer %d:%4.4X started\n"), lcss,devnum);
                    break;
            case 1: logmsg(_("HHCPN019E Printer %d:%4.4X not started: "
                             "busy or interrupt pending\n"), lcss, devnum);
                    break;
            case 2: logmsg(_("HHCPN020E Printer %d:%4.4X not started: "
                             "attention request rejected\n"), lcss, devnum);
                    break;
            case 3: logmsg(_("HHCPN021E Printer %d:%4.4X not started: "
                             "subchannel not enabled\n"), lcss, devnum);
                    break;
        }

    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* g command - turn off single stepping and start CPU */

int g_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    sysblk.inststep = 0;
    SET_IC_TRACE;

    return  start_cmd(0,NULL,NULL);
}

///////////////////////////////////////////////////////////////////////
/* stop command - stop CPU (or printer device if argument given) */

int stop_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        OBTAIN_INTLOCK(NULL);
        if (IS_CPU_ONLINE(sysblk.pcpu))
        {
            REGS *regs = sysblk.regs[sysblk.pcpu];
            regs->opinterv = 1;
            regs->cpustate = CPUSTATE_STOPPING;
            ON_IC_INTERRUPT(regs);
            WAKEUP_CPU (regs);
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        /* stop specified printer device */

        U16      devnum;
        U16      lcss;
        DEVBLK*  dev;
        char*    devclass;
        char     devnam[256];
        int     rc;

        rc=parse_single_devnum(argv[1],&lcss,&devnum);
        if (rc<0)
        {
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN024E Device %d:%4.4X is not a printer device\n"),
                      lcss, devnum );
            return -1;
        }

        dev->stopprt = 1;

        logmsg( _("HHCPN025I Printer %d:%4.4X stopped\n"), lcss, devnum );
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* startall command - start all CPU's */

int startall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    U32 mask;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

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

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* stopall command - stop all CPU's */

DLL_EXPORT int stopall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    U32 mask;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

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

    return 0;
}

#ifdef _FEATURE_CPU_RECONFIG

///////////////////////////////////////////////////////////////////////
/* cf command - configure/deconfigure a CPU */

int cf_cmd(int argc, char *argv[], char *cmdline)
{
    int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if (!strcasecmp(argv[1],"on"))
            on = 1;
        else if (!strcasecmp(argv[1], "off"))
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        if (on < 0)
            logmsg(_("HHCPN152I CPU%4.4X online\n"), sysblk.pcpu);
        else if (on == 0)
            deconfigure_cpu(sysblk.pcpu);
    }
    else
    {
        if (on < 0)
            logmsg(_("HHCPN153I CPU%4.4X offline\n"), sysblk.pcpu);
        else if (on > 0)
            configure_cpu(sysblk.pcpu);
    }

    RELEASE_INTLOCK(NULL);

    if (on >= 0) cf_cmd (0, NULL, NULL);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* cfall command - configure/deconfigure all CPU's */

int cfall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if (!strcasecmp(argv[1],"on"))
            on = 1;
        else if (!strcasecmp(argv[1], "off"))
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i))
        {
            if (on < 0)
                logmsg(_("HHCPN154I CPU%4.4X online\n"), i);
            else if (on == 0)
                deconfigure_cpu(i);
        }
        else
        {
            if (on < 0)
                logmsg(_("HHCPN155I CPU%4.4X offline\n"), i);
            else if (on > 0 && i < MAX_CPU)
                configure_cpu(i);
        }

    RELEASE_INTLOCK(NULL);

    if (on >= 0) cfall_cmd (0, NULL, NULL);

    return 0;
}

#endif /*_FEATURE_CPU_RECONFIG*/

///////////////////////////////////////////////////////////////////////
/* quiet command - quiet PANEL */

int quiet_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
#ifdef EXTERNALGUI
    if (extgui)
    {
        logmsg( _("HHCPN026W Ignored. (external GUI active)\n") );
        return 0;
    }
#endif /*EXTERNALGUI*/
    sysblk.npquiet = !sysblk.npquiet;
    logmsg( _("HHCPN027I Automatic refresh %s.\n"),
              sysblk.npquiet ? _("disabled") : _("enabled") );
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* format_tod - generate displayable date from TOD value */
/* always uses epoch of 1900 */
char * format_tod(char *buf, U64 tod, int flagdate)
{
    int leapyear, years, days, hours, minutes, seconds, microseconds;

    if(tod >= TOD_YEAR)
    {
        tod -= TOD_YEAR;
        years = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if((leapyear = tod / TOD_YEAR) == 4)
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

///////////////////////////////////////////////////////////////////////
/* timerint - display or set the timer interval */

int timerint_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"default"))
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
        else if (!strcasecmp(argv[1],"reset"))
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
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
            }
        }
    }

    logmsg( _("HHCPN037I Timer update interval = %d microsecond(s)\n"),
              sysblk.timerint );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* clocks command - display tod clkc and cpu timer */

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
char itimer_formatted[20];
char arch370_flag = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
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
    if(regs->sie_active)
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
        sprintf(itimer_formatted,"%02u:%02u:%02u.%06u",
                (itimer/(76800*60*60)),((itimer%(76800*60*60))/(76800*60)),
                ((itimer%(76800*60))/76800),((itimer%76800)*13));
        arch370_flag = 1;
    }

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN028I tod = %16.16" I64_FMT "X    %s\n"),
               (tod_now << 8),format_tod(clock_buf,tod_now,TRUE));

    logmsg( _("          h/w = %16.16" I64_FMT "X    %s\n"),
               (hw_now << 8),format_tod(clock_buf,hw_now,TRUE));

    if (epoch_now < 0) {
        epoch_now_abs = -(epoch_now);
        epoch_sign = '-';
    }
    else
    {
        epoch_now_abs = epoch_now;
        epoch_sign = ' ';
    }
    logmsg( _("          off = %16.16" I64_FMT "X   %c%s\n"),
               (epoch_now << 8),epoch_sign,
               format_tod(clock_buf,epoch_now_abs,FALSE));

    logmsg( _("          ckc = %16.16" I64_FMT "X    %s\n"),
               (clkc_now << 8),format_tod(clock_buf,clkc_now,TRUE));

    if (regs->cpustate != CPUSTATE_STOPPED)
        logmsg( _("          cpt = %16.16" I64_FMT "X\n"), cpt_now << 8);
    else
        logmsg( _("          cpt = not decrementing\n"));

#if defined(_FEATURE_SIE)
    if(sie_flag)
    {

        logmsg( _("         vtod = %16.16" I64_FMT "X    %s\n"),
                   (vtod_now << 8),format_tod(clock_buf,vtod_now,TRUE));

        if (epoch_now < 0) {
            epoch_now_abs = -(epoch_now);
            epoch_sign = '-';
        }
        else
        {
            epoch_now_abs = epoch_now;
            epoch_sign = ' ';
        }
        logmsg( _("         voff = %16.16" I64_FMT "X   %c%s\n"),
                   (vepoch_now << 8),vepoch_sign,
                   format_tod(clock_buf,vepoch_now_abs,FALSE));

        logmsg( _("         vckc = %16.16" I64_FMT "X    %s\n"),
                   (vclkc_now << 8),format_tod(clock_buf,vclkc_now,TRUE));

        logmsg( _("         vcpt = %16.16" I64_FMT "X\n"),vcpt_now << 8);
    }
#endif

    if (arch370_flag)
    {
        logmsg( _("          itm = %8.8" I32_FMT "X                     %s\n"),
                   itimer, itimer_formatted );
    }

    return 0;
}

#ifdef OPTION_IODELAY_KLUDGE

///////////////////////////////////////////////////////////////////////
/* iodelay command - display or set I/O delay value */

int iodelay_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int iodelay = 0;
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%d%c", &iodelay, &c) != 1)
            logmsg( _("HHCPN029E Invalid I/O delay value: %s\n"), argv[1] );
        else
            sysblk.iodelay = iodelay;
    }

    logmsg( _("HHCPN030I I/O delay = %d\n"), sysblk.iodelay );

    return 0;
}

#endif /*OPTION_IODELAY_KLUDGE*/

#if defined( OPTION_SCSI_TAPE )

///////////////////////////////////////////////////////////////////////
// (helper function for 'scsimount' and 'devlist' commands)

static void try_scsi_refresh( DEVBLK* dev )
{
    // PROGRAMMING NOTE: we can only ever cause the auto-scsi-mount
    // thread to startup or shutdown [according to the current user
    // setting] if the current drive status is "not mounted".

    // What we unfortunately CANNOT do (indeed MUST NOT do!) however
    // is actually "force" a refresh of a current [presumably bogus]
    // "mounted" status (to presumably detect that a tape that was
    // once mounted has now been manually unmounted for example).

    // The reasons for why this is not possible is clearly explained
    // in the 'force_status_update' function in 'scsitape.c'. All we
    // can ever hope to do here is either cause an already-running
    // auto-mount thread to exit (if the user has just now disabled
    // auto-mounts) or else cause one to automatically start (if they
    // just enabled auto-mounts and there's no tape already mounted).

    // If the user manually unloaded a mounted tape (such that there
    // is now no longer a tape mounted even though the drive status
    // says there is), then they unfortunately have no choice but to
    // manually issue the 'devinit' command themselves, because, as
    // explained, we unfortunately cannot refresh a mounted status
    // for them (due to the inherent danger of doing so as explained
    // by the comments in 'force_status_update' in member scsitape.c).

    broadcast_condition( &dev->stape_exit_cond );   // (force exit if needed)
    dev->tmh->passedeot( dev );                     // (maybe update status)
    usleep(10*1000);                                // (let thread start/end)
}

///////////////////////////////////////////////////////////////////////
/* scsimount command - display or adjust the SCSI auto-mount option */

int scsimount_cmd(int argc, char *argv[], char *cmdline)
{
    char*  eyecatcher =
"*************************************************************************************************";
    DEVBLK*  dev;
    int      tapeloaded;
    char*    tapemsg="";
    char     volname[7];
    BYTE     mountreq, unmountreq;
    char*    label_type;
    // Unused..
    // int      old_auto_scsi_mount_secs = sysblk.auto_scsi_mount_secs;
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if ( strcasecmp( argv[1], "no" ) == 0 )
        {
            sysblk.auto_scsi_mount_secs = 0;
        }
        else if ( strcasecmp( argv[1], "yes" ) == 0 )
        {
            sysblk.auto_scsi_mount_secs = DEFAULT_AUTO_SCSI_MOUNT_SECS;
        }
        else
        {
            int auto_scsi_mount_secs; BYTE c;
            if ( sscanf( argv[1], "%d%c", &auto_scsi_mount_secs, &c ) != 1
                || auto_scsi_mount_secs < 0 || auto_scsi_mount_secs > 99 )
            {
                logmsg
                (
                    _( "HHCCF068E Invalid value: %s; Enter \"help scsimount\" for help.\n" )

                    ,argv[1]
                );
                return 0;
            }
            sysblk.auto_scsi_mount_secs = auto_scsi_mount_secs;
        }
    }

    if ( sysblk.auto_scsi_mount_secs )
        logmsg( _("SCSI auto-mount queries = every %d seconds (when needed)\n"),
            sysblk.auto_scsi_mount_secs );
    else
        logmsg( _("SCSI auto-mount queries are disabled.\n") );

    // Scan the device list looking for all SCSI tape devices
    // with either an active scsi mount thread and/or an out-
    // standing tape mount request...

    for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
    {
        if ( !dev->allocated || TAPEDEVT_SCSITAPE != dev->tapedevt )
            continue;  // (not an active SCSI tape device; skip)

        try_scsi_refresh( dev );    // (see comments in function)

        logmsg
        (
            _("SCSI auto-mount thread %s active for drive %u:%4.4X = %s.\n")

            ,dev->stape_mountmon_tid ? "IS" : "is NOT"
            ,SSID_TO_LCSS(dev->ssid)
            ,dev->devnum
            ,dev->filename
        );

        if (!dev->tdparms.displayfeat)
            continue;

        mountreq   = FALSE;     // (default)
        unmountreq = FALSE;     // (default)

        if (0
            || TAPEDISPTYP_MOUNT       == dev->tapedisptype
            || TAPEDISPTYP_UNMOUNT     == dev->tapedisptype
            || TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype
        )
        {
            tapeloaded = dev->tmh->tapeloaded( dev, NULL, 0 );

            if ( TAPEDISPTYP_MOUNT == dev->tapedisptype && !tapeloaded )
            {
                mountreq   = TRUE;
                unmountreq = FALSE;
                tapemsg = dev->tapemsg1;
            }
            else if ( TAPEDISPTYP_UNMOUNT == dev->tapedisptype && tapeloaded )
            {
                unmountreq = TRUE;
                mountreq   = FALSE;
                tapemsg = dev->tapemsg1;
            }
            else // ( TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype )
            {
                if (tapeloaded)
                {
                    if ( !(dev->tapedispflags & TAPEDISPFLG_MESSAGE2) )
                    {
                        unmountreq = TRUE;
                        mountreq   = FALSE;
                        tapemsg = dev->tapemsg1;
                    }
                }
                else // (!tapeloaded)
                {
                    mountreq   = TRUE;
                    unmountreq = FALSE;
                    tapemsg = dev->tapemsg2;
                }
            }
        }

        if ((mountreq || unmountreq) && ' ' != *tapemsg)
        {
            switch (*(tapemsg+7))
            {
                case 'A': label_type = "ascii-standard"; break;
                case 'S': label_type = "standard"; break;
                case 'N': label_type = "non"; break;
                case 'U': label_type = "un"; break;
                default : label_type = "??"; break;
            }

            volname[0]=0;

            if (*(tapemsg+1))
            {
                strncpy( volname, tapemsg+1, 6 );
                volname[6]=0;
            }

            logmsg
            (
                _("\n%s\nHHCCF069I %s of %s-labeled volume \"%s\" pending for drive %u:%4.4X = %s\n%s\n\n")

                ,eyecatcher
                ,mountreq ? "Mount" : "Dismount"
                ,label_type
                ,volname
                ,SSID_TO_LCSS(dev->ssid)
                ,dev->devnum
                ,dev->filename
                ,eyecatcher
            );
        }
        else
        {
            logmsg( _("No mount/dismount requests pending for drive %u:%4.4X = %s.\n"),
                SSID_TO_LCSS(dev->ssid),dev->devnum, dev->filename );
        }
    }

    return 0;
}
#endif /* defined( OPTION_SCSI_TAPE ) */

///////////////////////////////////////////////////////////////////////
/* cckd command */

int cckd_cmd(int argc, char *argv[], char *cmdline)
{
    char* p = strtok(cmdline+4," \t");

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    return cckd_command(p,1);
}

#if defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////
/* tt32stats command - display CTCI-W32 statistics */

int tt32stats_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    logmsg( _("HHCPNxxxE cmd deprecated; try 'tt32' instead\n") );
    return -1;
}

/* tt32stats command - display CTCI-W32 statistics */

int tt32_cmd( int argc, char *argv[], char *cmdline )
{
    int      rc = 0;
    U16      devnum;
    U16      lcss;
    DEVBLK*  dev;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPNxxxE Missing arguments; enter 'help tt32' for help.\n") );
        rc = -1;
    }
    else if (strcasecmp(argv[1],"stats") == 0)
    {
        if (argc < 3)
        {
            missing_devnum();
            return -1;
        }

        if ((rc = parse_single_devnum(argv[2], &lcss, &devnum)) < 0)
            return -1;

        if (!(dev = find_device_by_devnum (lcss, devnum)) &&
            !(dev = find_device_by_devnum (lcss, devnum ^ 0x01)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        if (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype)
        {
            logmsg( _("HHCPN034E Device %d:%4.4X is not a CTCI or LCS device\n"),
                      lcss, devnum );
            return -1;
        }

        if (debug_tt32_stats)
            rc = debug_tt32_stats (dev->fd);
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else if (strcasecmp(argv[1],"debug") == 0)
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(1); // 1=ON
            rc = 0;
            logmsg( _("HHCPNxxxI TT32 debug tracing messages enabled\n") );
        }
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else if (strcasecmp(argv[1],"nodebug") == 0)
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(0); // 0=OFF
            rc = 0;
            logmsg( _("HHCPNxxxI TT32 debug tracing messages disabled\n") );
        }
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else
    {
        logmsg( _("HHCPNxxxE Invalid argument\n") );
        rc = -1;
    }

    return rc;
}

#endif /* defined(OPTION_W32_CTCI) */

///////////////////////////////////////////////////////////////////////
/* store command - store CPU status at absolute zero */

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
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (regs->cpustate != CPUSTATE_STOPPED)
    {
        logmsg( _("HHCPN035E store status rejected: CPU not stopped\n") );
        return -1;
    }

    /* Store status in 512 byte block at absolute location 0 */
    store_status (regs, 0);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg (_("HHCCP010I CPU%4.4X store status completed.\n"),
            regs->cpuad);

    return 0;
}


///////////////////////////////////////////////////////////////////////
/* toddrag command - display or set TOD clock drag factor */

int toddrag_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        double toddrag = -1.0;

        sscanf(argv[1], "%lf", &toddrag);

        if (toddrag >= 0.0001 && toddrag <= 10000.0)
        {
            /* Set clock steering based on drag factor */
            set_tod_steering(-(1.0-(1.0/toddrag)));
        }
    }

    logmsg( _("HHCPN036I TOD clock drag factor = %lf\n"), (1.0/(1.0+get_tod_steering())));

    return 0;
}


#ifdef PANEL_REFRESH_RATE

///////////////////////////////////////////////////////////////////////
/* panrate command - display or set rate at which console refreshes */

int panrate_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"fast"))
            sysblk.panrate = PANEL_REFRESH_RATE_FAST;
        else if (!strcasecmp(argv[1],"slow"))
            sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
        else
        {
            int trate = 0;

            sscanf(argv[1],"%d", &trate);

            if (trate >= (1000 / CLK_TCK) && trate < 5001)
                sysblk.panrate = trate;
        }
    }

    logmsg( _("HHCPN037I Panel refresh rate = %d millisecond(s)\n"),
              sysblk.panrate );

    return 0;
}

#endif /*PANEL_REFRESH_RATE */

///////////////////////////////////////////////////////////////////////
/* shell command */

int sh_cmd(int argc, char *argv[], char *cmdline)
{
    char* cmd;
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if (sysblk.shcmdopt & SHCMDOPT_DISABLE)
    {
        logmsg( _("HHCPN180E 'sh' commands are disabled\n"));
        return -1;
    }
    cmd = cmdline + 2;
    while (isspace(*cmd)) cmd++;
    if (*cmd)
        return herc_system (cmd);
    panel_command ("help sh");
    return -1;
}

///////////////////////////////////////////////////////////////////////
/* gpr command - display general purpose registers */

int gpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_regs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* fpr command - display floating point registers */

int fpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_fregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* cr command - display control registers */

int cr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_cregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ar command - display access registers */

int ar_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_aregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* pr command - display prefix register */

int pr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if(regs->arch_mode == ARCH_900)
        logmsg( "Prefix=%16.16" I64_FMT "X\n", (long long)regs->PX_G );
    else
        logmsg( "Prefix=%8.8X\n", regs->PX_L );

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* psw command - display program status word */

int psw_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_psw (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* restart command - generate restart interrupt */

int restart_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg( _("HHCPN038I Restart key depressed\n") );

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu );
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

///////////////////////////////////////////////////////////////////////
/* r command - display or alter real storage */

int r_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    alter_display_real (cmdline+1, regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* u command - disassemble */

int u_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    disasm_stor (regs, cmdline+2);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* v command - display or alter virtual storage */

int v_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    alter_display_virt (cmdline+1, regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* b command - set breakpoint */

int bset_cmd(int argc, char *argv[], char *cmdline)
{
int  rc;                                /* Return code               */
BYTE c[2];                              /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc != 2)
    {
        logmsg( _("HHCPN039E Invalid or missing argument\n") );
        return -1;
    }

    rc = sscanf(argv[1], "%"I64_FMT"x%c%"I64_FMT"x%c",
                        &sysblk.breakaddr[0], &c[0],
                        &sysblk.breakaddr[1], &c[1]);

    if (rc == 1 || (rc == 3 && c[0] == '-'))
    {
        if (rc == 1)
            sysblk.breakaddr[1] = sysblk.breakaddr[0];
        logmsg( _("HHCPN040I Setting breakpoint at %16.16"I64_FMT"X-%16.16"I64_FMT"X\n"),
            sysblk.breakaddr[0],sysblk.breakaddr[1]);
        sysblk.instbreak = 1;
        SET_IC_TRACE;
    }
    else
    {
        logmsg( _("HHCPN039E Invalid or missing argument\n") );
        return -1;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* b- command - delete breakpoint */

int bdelete_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    logmsg( _("HHCPN041I Deleting breakpoint\n") );
    sysblk.instbreak = 0;
    SET_IC_TRACE;
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* i command - generate I/O attention interrupt for device */

int i_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    int      rc = 0;
    U16      devnum;
    U16      lcss;
    DEVBLK*  dev;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss, devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    rc = device_attention (dev, CSW_ATTN);

    switch (rc) {
        case 0: logmsg(_("HHCPN045I Device %4.4X attention request raised\n"),
                         devnum);
                break;
        case 1: logmsg(_("HHCPN046E Device %4.4X busy or interrupt pending\n"),
                         devnum);
                break;
        case 2: logmsg(_("HHCPN047E Device %4.4X attention request rejected\n"),
                         devnum);
                break;
        case 3: logmsg(_("HHCPN048E Device %4.4X subchannel not enabled\n"),
                         devnum);
                break;
    }

    regs = sysblk.regs[sysblk.pcpu];
    if (rc == 3 && IS_CPU_ONLINE(sysblk.pcpu) && CPUSTATE_STOPPED == regs->cpustate)
        logmsg( _("HHCPN049W Are you sure you didn't mean 'ipl %4.4X' "
                  "instead?\n"), devnum );

    return rc;
}

///////////////////////////////////////////////////////////////////////
/* ext command - generate external interrupt */

int ext_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);

    ON_IC_INTKEY;

    logmsg( _("HHCPN050I Interrupt key depressed\n") );

    /* Signal waiting CPUs that an interrupt is pending */
    WAKEUP_CPUS_MASK (sysblk.waiting_mask);

    RELEASE_INTLOCK(NULL);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadparm xxxxxxxx command - set IPL parameter */

int loadparm_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update IPL parameter if operand is specified */
    if (argc > 1)
    set_loadparm(argv[1]);

    /* Display IPL parameter */
    logmsg( _("HHCPN051I LOADPARM=%s\n"),str_loadparm());

    return 0;
}
/* system reset/system reset clear handlers */
int reset_cmd(int ac,char *av[],char *cmdline,int clear)
{
    int i;

    UNREFERENCED(ac);
    UNREFERENCED(av);
    UNREFERENCED(cmdline);
    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN053E System reset/clear rejected: All CPU's must be stopped\n") );
            return -1;
        }

    system_reset (sysblk.pcpu, clear);

    RELEASE_INTLOCK(NULL);

    return 0;

}
int sysr_cmd(int ac,char *av[],char *cmdline)
{
    return(reset_cmd(ac,av,cmdline,0));
}
int sysc_cmd(int ac,char *av[],char *cmdline)
{
    return(reset_cmd(ac,av,cmdline,1));
}

///////////////////////////////////////////////////////////////////////
/* ipl xxxx command - IPL from device xxxx */


int ipl_cmd2(int argc, char *argv[], char *cmdline, int clear)
{
BYTE c;                                 /* Character work area       */
int  rc;                                /* Return code               */

unsigned i;
U16  lcss;
U16  devnum;
char *cdev, *clcss;


    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN053E ipl rejected: All CPU's must be stopped\n") );
            return -1;
        }

    /* If the ipl device is not a valid hex number we assume */
    /* This is a load from the service processor             */

    if((cdev = strchr(argv[1],':')))
    {
        clcss = argv[1];
        cdev++;
    }
    else
    {
        clcss = NULL;
        cdev = argv[1];
    }

    if (sscanf(cdev, "%hx%c", &devnum, &c) != 1)
        rc = load_hmc(strtok(cmdline+3," \t"), sysblk.pcpu, clear);
    else
    {
        *--cdev = '\0';

        if(clcss)
        {
            if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
            {
                logmsg( _("HHCPN059E LCSS id %s is invalid\n"), clcss );
                return -1;
            }
        }
        else
            lcss = 0;

        rc = load_ipl (lcss, devnum, sysblk.pcpu, clear);
    }

    RELEASE_INTLOCK(NULL);

    return rc;
}

int ipl_cmd(int argc, char *argv[], char *cmdline)
{
    return(ipl_cmd2(argc,argv,cmdline,0));
}
int iplc_cmd(int argc, char *argv[], char *cmdline)
{
    return(ipl_cmd2(argc,argv,cmdline,1));
}

///////////////////////////////////////////////////////////////////////
/* cpu command - define target cpu for panel display and commands */

int cpu_cmd(int argc, char *argv[], char *cmdline)
{
BYTE c;                                 /* Character work area       */

    int cpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN054E Missing argument\n") );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1
     || cpu < 0 || cpu >= MAX_CPU)
    {
        logmsg( _("HHCPN055E Target CPU %s is invalid\n"), argv[1] );
        return -1;
    }

    sysblk.dummyregs.cpuad = cpu;
    sysblk.pcpu = cpu;

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* FishHangReport - verify/debug proper Hercules LOCK handling...    */

#if defined(FISH_HANG)

int FishHangReport_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    FishHangReport();
#if defined(OPTION_FISHIO)
    PrintAllDEVTHREADPARMSs();
#endif
    return 0;
}

#endif // defined(FISH_HANG)

///////////////////////////////////////////////////////////////////////
/* devlist command - list devices */

int SortDevBlkPtrsAscendingByDevnum(const void* pDevBlkPtr1, const void* pDevBlkPtr2)
{
    return
        ((int)((*(DEVBLK**)pDevBlkPtr1)->devnum) -
         (int)((*(DEVBLK**)pDevBlkPtr2)->devnum));
}

int devlist_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    char*    devclass;
    char     devnam[1024];
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   nDevCount, i;
    int      bTooMany = 0;
    U16      lcss;
    U16      ssid=0;
    U16      devnum;
    int      single_devnum = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (argc >= 2)
    {
        single_devnum = 1;

        if (parse_single_devnum(argv[1], &lcss, &devnum) < 0)   
        {
            // (error message already issued)
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss, devnum);
            return -1;
        }

        ssid = LCSS_TO_SSID(lcss);
    }

    // Since we wish to display the list of devices in ascending device
    // number order, we build our own private a sorted array of DEVBLK
    // pointers and use that instead to make the devlist command wholly
    // immune from the actual order/sequence of the actual DEVBLK chain.

    // Note too that there is no lock to lock access to ALL device blocks
    // (even though there really SHOULD be), only one to lock an individual
    // DEVBLK (which doesn't do us much good here).

    if (!(orig_pDevBlkPtrs = malloc(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES)))
    {
        logmsg( _("HHCPN146E Work buffer malloc failed: %s\n"),
            strerror(errno) );
        return -1;
    }

    nDevCount = 0;
    pDevBlkPtr = orig_pDevBlkPtrs;

    for (dev = sysblk.firstdev; dev && nDevCount <= MAX_DEVLIST_DEVICES; dev = dev->nextdev)
    {
        if (dev->pmcw.flag5 & PMCW5_V)  // (valid device?)
        {
            if (single_devnum && (dev->ssid != ssid || dev->devnum != devnum))
                continue;

            if (nDevCount < MAX_DEVLIST_DEVICES)
            {
                *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                nDevCount++;            // (count array entries)
                pDevBlkPtr++;           // (bump to next entry)

                if (single_devnum)
                    break;
            }
            else
            {
                bTooMany = 1;           // (no more room)
                break;                  // (no more room)
            }
        }
    }

    ASSERT(nDevCount <= MAX_DEVLIST_DEVICES);   // (sanity check)

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort(orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum);

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK
        ASSERT(dev->pmcw.flag5 & PMCW5_V);  // (sanity check)

        /* Call device handler's query definition function */

#if defined(OPTION_SCSI_TAPE)
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            try_scsi_refresh( dev );  // (see comments in function)
#endif
        dev->hnd->query( dev, &devclass, sizeof(devnam), devnam );

        /* Display the device definition and status */
        logmsg( "%d:%4.4X %4.4X %s %s%s%s\n",
                SSID_TO_LCSS(dev->ssid),
                dev->devnum, dev->devtype, devnam,
                (dev->fd > 2 ? _("open ") : ""),
                (dev->busy ? _("busy ") : ""),
                (IOPENDING(dev) ? _("pending ") : "")
            );

        if (dev->bs)
        {
            char *clientip, *clientname;

            get_connected_client(dev,&clientip,&clientname);

            if (clientip)
            {
                logmsg( _("     (client %s (%s) connected)\n"),
                    clientip, clientname
                    );
            }
            else
            {
                logmsg( _("     (no one currently connected)\n") );
            }

            if (clientip)   free(clientip);
            if (clientname) free(clientname);
        }
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        logmsg( _("HHCPN147W Warning: not all devices shown (max %d)\n"),
            MAX_DEVLIST_DEVICES);

        return -1;      // (treat as error)
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* qd command - query dasd */
int qd_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   nDevCount, i, j, num;
    int      bTooMany = 0;
    U16      lcss;
    U16      ssid=0;
    U16      devnum;
    int      single_devnum = 0;
    BYTE     iobuf[256];
    BYTE     cbuf[17];

    UNREFERENCED(cmdline);

    if (argc >= 2)
    {
        single_devnum = 1;

        if (parse_single_devnum(argv[1], &lcss, &devnum) < 0)   
            return -1;
        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss, devnum);
            return -1;
        }
        ssid = LCSS_TO_SSID(lcss);
    }

    if (!(orig_pDevBlkPtrs = malloc(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES)))
    {
        logmsg( _("HHCPN146E Work buffer malloc failed: %s\n"),
            strerror(errno) );
        return -1;
    }

    nDevCount = 0;
    pDevBlkPtr = orig_pDevBlkPtrs;

    for (dev = sysblk.firstdev; dev && nDevCount <= MAX_DEVLIST_DEVICES; dev = dev->nextdev)
    {
        if (dev->pmcw.flag5 & PMCW5_V)  // (valid device?)
        {
            if (single_devnum && (dev->ssid != ssid || dev->devnum != devnum))
                continue;
            if (!dev->ckdcyls)
                continue;

            if (nDevCount < MAX_DEVLIST_DEVICES)
            {
                *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                nDevCount++;            // (count array entries)
                pDevBlkPtr++;           // (bump to next entry)

                if (single_devnum)
                    break;
            }
            else
            {
                bTooMany = 1;           // (no more room)
                break;                  // (no more room)
            }
        }
    }

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort(orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum);

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK

        /* Display sense-id */
        for (j = 0; j < dev->numdevid; j++)
        {
            if (j == 0)
                logmsg("%4.4x SNSID 00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", dev->devid[j]);
        }
        logmsg("\n");

        /* Display device characteristics */
        for (j = 0; j < dev->numdevchar; j++)
        {
            if (j == 0)
                logmsg("%4.4x RDC   00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", dev->devchar[j]);
        }
        logmsg("\n");

        /* Display configuration data */
        dasd_build_ckd_config_data (dev, iobuf, 256);
        for (j = 0; j < 256; j++)
        {
            if (j == 0)
                logmsg("%4.4x RCD   00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg(" |%s|\n           %2.2x ", cbuf, j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", iobuf[j]);
            cbuf[j%16] = isprint(guest_to_host(iobuf[j])) ? guest_to_host(iobuf[j]) : '.';
        }
        logmsg(" |%s|\n", cbuf);

        /* Display subsystem status */
        num = dasd_build_ckd_subsys_status(dev, iobuf, 44);
        for (j = 0; j < num; j++)
        {
            if (j == 0)
                logmsg("%4.4x SNSS  00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", iobuf[j]);
        }
        logmsg("\n");
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        logmsg( _("HHCPN147W Warning: not all devices shown (max %d)\n"),
            MAX_DEVLIST_DEVICES);

        return -1;      // (treat as error)
    }

    return 0;
#undef myssid
#undef CONFIG_DATA_SIZE
}

///////////////////////////////////////////////////////////////////////
/* attach command - configure a device */

int attach_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN057E Missing argument(s)\n") );
        return -1;
    }
    return parse_and_attach_devices(argv[1],argv[2],argc-3,&argv[3]);

#if 0
    if((cdev = strchr(argv[1],':')))
    {
        clcss = argv[1];
        *cdev++ = '\0';
    }
    else
    {
        clcss = NULL;
        cdev = argv[1];
    }

    if (sscanf(cdev, "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN059E Device number %s is invalid\n"), cdev );
        return -1;
    }

    if(clcss)
    {
        if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
        {
            logmsg( _("HHCPN059E LCSS id %s is invalid\n"), clcss );
            return -1;
        }
    }
    else
        lcss = 0;

#if 0 /* JAP - Breaks the whole idea behind devtype.c */
    if (sscanf(argv[2], "%hx%c", &dummy_devtype, &c) != 1)
    {
        logmsg( _("Device type %s is invalid\n"), argv[2] );
        return -1;
    }
#endif

    return  attach_device (lcss, devnum, argv[2], argc-3, &argv[3]);
#endif
}

///////////////////////////////////////////////////////////////////////
/* detach command - remove device */

int detach_cmd(int argc, char *argv[], char *cmdline)
{
U16  devnum;
U16  lcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }

    return  detach_device (lcss, devnum);
}

///////////////////////////////////////////////////////////////////////
/* define command - rename a device */

int define_cmd(int argc, char *argv[], char *cmdline)
{
U16  devnum, newdevn;
U16 lcss,newlcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN062E Missing argument(s)\n") );
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }
    rc=parse_single_devnum(argv[2],&newlcss,&newdevn);
    if (rc<0)
    {
        return -1;
    }
    if(lcss!=newlcss)
    {
        logmsg(_("HHCPN182E Device numbers can only be redefined within the same Logical channel subsystem\n"));
        return -1;
    }

    return  define_device (lcss, devnum, newdevn);
}

///////////////////////////////////////////////////////////////////////
/* pgmtrace command - trace program interrupts */

int pgmtrace_cmd(int argc, char *argv[], char *cmdline)
{
int abs_rupt_num, rupt_num;
BYTE    c;                              /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
#if 0
        logmsg( _("HHCPN065E Missing argument(s)\n") );
        return -1;
#else // fishtest
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL)
            logmsg("pgmtrace == all\n");
        else if (sysblk.pgminttr == 0)
            logmsg("pgmtrace == none\n");
        else
        {
            char flags[64+1]; int i;
            for (i=0; i < 64; i++)
                flags[i] = (sysblk.pgminttr & (1ULL << i)) ? ' ' : '*';
            flags[64] = 0;
            logmsg
            (
                " * = Tracing suppressed; otherwise tracing enabled\n"
                " 0000000000000001111111111111111222222222222222233333333333333334\n"
                " 123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0\n"
                " %s\n"
                ,flags
            );
        }
        return 0;
#endif
    }

    if (sscanf(argv[1], "%x%c", &rupt_num, &c) != 1)
    {
        logmsg( _("HHCPN066E Program interrupt number %s is invalid\n"),
                  argv[1] );
        return -1;
    }

    if ((abs_rupt_num = abs(rupt_num)) < 1 || abs_rupt_num > 0x40)
    {
        logmsg( _("HHCPN067E Program interrupt number out of range (%4.4X)\n"),
                  abs_rupt_num );
        return -1;
    }

    /* Add to, or remove interruption code from mask */

    if (rupt_num < 0)
        sysblk.pgminttr &= ~((U64)1 << (abs_rupt_num - 1));
    else
        sysblk.pgminttr |=  ((U64)1 << (abs_rupt_num - 1));

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ostailor command - trace program interrupts */

int ostailor_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        char* sostailor = "(custom)";
        if (sysblk.pgminttr == OS_OS390 ) sostailor = "OS/390";
        if (sysblk.pgminttr == OS_ZOS   ) sostailor = "z/OS";
        if (sysblk.pgminttr == OS_VSE   ) sostailor = "VSE";
        if (sysblk.pgminttr == OS_VM    ) sostailor = "VM";
        if (sysblk.pgminttr == OS_LINUX ) sostailor = "LINUX";
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL) sostailor = "NULL";
        if (sysblk.pgminttr == 0                    ) sostailor = "QUIET";
        logmsg( _("OSTAILOR %s\n"),sostailor);
        return 0;
    }
    if (strcasecmp (argv[1], "OS/390") == 0)
    {
        sysblk.pgminttr = OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "+OS/390") == 0)
    {
        sysblk.pgminttr &= OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "-OS/390") == 0)
    {
        sysblk.pgminttr |= ~OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "Z/OS") == 0)
    {
        sysblk.pgminttr = OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "+Z/OS") == 0)
    {
        sysblk.pgminttr &= OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "-Z/OS") == 0)
    {
        sysblk.pgminttr |= ~OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "VSE") == 0)
    {
        sysblk.pgminttr = OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "+VSE") == 0)
    {
        sysblk.pgminttr &= OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "-VSE") == 0)
    {
        sysblk.pgminttr |= ~OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "VM") == 0)
    {
        sysblk.pgminttr = OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "+VM") == 0)
    {
        sysblk.pgminttr &= OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "-VM") == 0)
    {
        sysblk.pgminttr |= ~OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "LINUX") == 0)
    {
        sysblk.pgminttr = OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "+LINUX") == 0)
    {
        sysblk.pgminttr &= OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "-LINUX") == 0)
    {
        sysblk.pgminttr |= ~OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "NULL") == 0)
    {
        sysblk.pgminttr = 0xFFFFFFFFFFFFFFFFULL;
        return 0;
    }
    if (strcasecmp (argv[1], "QUIET") == 0)
    {
        sysblk.pgminttr = 0;
        return 0;
    }
    logmsg( _("Unknown OS tailor specification %s\n"),
                argv[1] );
    return -1;
}

///////////////////////////////////////////////////////////////////////
/* k command - print out cckd internal trace */

int k_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    cckd_print_itrace ();

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ds - display subchannel */

int ds_cmd(int argc, char *argv[], char *cmdline)
{
DEVBLK*  dev;
U16      devnum;
U16      lcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);

    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss,devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    display_subchannel (dev);

    return 0;
}


///////////////////////////////////////////////////////////////////////
/* syncio command - list syncio devices statistics */

int syncio_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*   dev;
    U64       syncios = 0, asyncios = 0;
    int       found = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!dev->syncio) continue;

        found = 1;

        logmsg( _("HHCPN072I %4.4X  synchronous: %12" I64_FMT "d "
                  "asynchronous: %12" I64_FMT "d\n"),
                dev->devnum, (long long)dev->syncios,
                (long long)dev->asyncios
            );

        syncios  += dev->syncios;
        asyncios += dev->asyncios;
    }

    if (!found)
        logmsg( _("HHCPN073I No synchronous I/O devices found\n") );
    else
        logmsg( _("HHCPN074I TOTAL synchronous: %12" I64_FMT "d "
                  "asynchronous: %12" I64_FMT "d  %3" I64_FMT "d%%\n"),
               (long long)syncios, (long long)asyncios,
               (long long)((syncios * 100) / (syncios + asyncios + 1))
            );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* devtmax command - display or set max device threads */

#if !defined(OPTION_FISHIO)
void *device_thread(void *arg);
#endif /* !defined(OPTION_FISHIO) */

int devtmax_cmd(int argc, char *argv[], char *cmdline)
{
    int devtmax = -2;

#if defined(OPTION_FISHIO)

    UNREFERENCED(cmdline);

    /* Note: no need to lock scheduler vars since WE are
     * the only one that updates "ios_devtmax" (the scheduler
     * just references it) and we only display (but not update)
     * all the other variables.
     */

    if (argc > 1)
        sscanf(argv[1], "%d", &devtmax);
    else
        devtmax = ios_devtmax;

    if (devtmax >= -1)
        ios_devtmax = devtmax;
    else
    {
        logmsg( _("HHCPN075E Invalid max device threads value "
                  "(must be -1 to n)\n") );
        return -1;
    }

    TrimDeviceThreads();    /* (enforce newly defined threshold) */

    logmsg( _("HHCPN076I Max device threads: %d, current: %d, most: %d, "
            "waiting: %d, max exceeded: %d\n"),
            ios_devtmax, ios_devtnbr, ios_devthwm,
            (int)ios_devtwait, ios_devtunavail
        );

#else /* !defined(OPTION_FISHIO) */

    TID tid;

    UNREFERENCED(cmdline);

    if (argc > 1)
        sscanf(argv[1], "%d", &devtmax);
    else
        devtmax = sysblk.devtmax;

    if (devtmax >= -1)
        sysblk.devtmax = devtmax;
    else
    {
        logmsg( _("HHCPN077E Invalid max device threads value "
                  "(must be -1 to n)\n") );
        return -1;
    }

    /* Create a new device thread if the I/O queue is not NULL
       and more threads can be created */

    if (sysblk.ioq && (!sysblk.devtmax || sysblk.devtnbr < sysblk.devtmax))
        create_thread(&tid, &sysblk.detattr, device_thread, NULL, "idle device thread");

    /* Wakeup threads in case they need to terminate */
    broadcast_condition (&sysblk.ioqcond);

    logmsg( _("HHCPN078E Max device threads %d current %d most %d "
            "waiting %d total I/Os queued %d\n"),
            sysblk.devtmax, sysblk.devtnbr, sysblk.devthwm,
            sysblk.devtwait, sysblk.devtunavail
        );

#endif /* defined(OPTION_FISHIO) */

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* sf commands - shadow file add/remove/set/compress/display */

int ShadowFile_cmd(int argc, char *argv[], char *cmdline)
{
char    action;                         /* Action character `+-cd'   */
char   *devascii;                       /* -> Device name            */
DEVBLK *dev;                            /* -> Device block           */
U16     devnum;                         /* Device number             */
U16     lcss;                           /* Logical CSS               */
int     scan = 0;                       /* 1=Device name is `*'      */
int     n = 0;                          /* Number devices scanned    */
int     flag = 1;                       /* sf- flag (default merge)  */

    UNREFERENCED(cmdline);

    if (strlen(argv[0]) < 3 || strchr ("+-cd", argv[0][2]) == NULL)
    {
        logmsg( _("HHCPN091E Command must be 'sf+', 'sf-', "
                                "'sfc', or 'sfd'\n") );
        return -1;
    }

    action = argv[0][2];
    /*
     * device name either follows the action character or is the
     * next operand
     */
    if (strlen(argv[0]) > 3)
        devascii = argv[0] + 3;
    else
    {
        argv++; argc--;
        if (argc < 0 || (devascii = argv[0]) == NULL)
        {
            missing_devnum();
            return -1;
        }
    }

    /* device name can be `*' meaning all cckd devices */
    if (strcmp (devascii, "*") == 0)
    {
        for (dev=sysblk.firstdev; dev && !dev->cckd_ext; dev=dev->nextdev);
            /* nothing */
        if (!dev)
        {
            logmsg( _("HHCPN081E No cckd devices found\n") );
            return -1;
        }
        scan = 1;
    }
    else
    {
        if (parse_single_devnum(devascii,&lcss,&devnum) < 0)
            return -1;
        if ((dev = find_device_by_devnum (lcss,devnum)) == NULL)
            return devnotfound_msg(lcss,devnum);
        if (dev->cckd_ext == NULL)
        {
            logmsg( _("HHCPN084E Device number %d:%4.4X "
                      "is not a cckd device\n"), lcss, devnum );
            return -1;
        }
    }

    /* For `sf-' the operand can be `nomerge', `merge' or `force' */
    if (action == '-' && argc > 1)
    {
        if (strcmp(argv[1], "nomerge") == 0)
            flag = 0;
        else if (strcmp(argv[1], "merge") == 0)
            flag = 1;
        else if (strcmp(argv[1], "force") == 0)
            flag = 2;
        else
        {
            logmsg( _("HHCPN087E Operand must be "
                      "`merge', `nomerge' or `force'\n") );
            return -1;
        }
        argv++; argc--;
    }

    /* No other operands allowed */
    if (argc > 1)
    {
        logmsg( _("HHCPN089E Unexpected operand: %s\n"), argv[1] );
        return -1;
    }

    /* Perform the action */
    while (dev)
    {
        if (scan) logmsg( _("HHCPN085I Processing device %d:%4.4X\n"),
                            SSID_TO_LCSS(dev->ssid), dev->devnum );

        switch (action) {
        case '+': cckd_sf_add (dev);
                  break;
        case '-': cckd_sf_remove (dev, flag);
                  break;
        case 'c': cckd_sf_comp (dev);
                  break;
        case 'd': cckd_sf_stats (dev);
                  break;
        }
        n++;

        /* Next cckd device if scanning */
        if (scan)
            for (dev=dev->nextdev; dev && !dev->cckd_ext; dev=dev->nextdev);
        else dev = NULL;

    } /* while (dev) */

    if (scan) logmsg( _("HHCPN092I %d devices processed\n"), n );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* devinit command - assign/open a file for a configured device */

int devinit_cmd(int argc, char *argv[], char *cmdline)
{
DEVBLK*  dev;
U16      devnum;
U16      lcss;
int      i, rc;
int      nomountedtapereinit = sysblk.nomountedtapereinit;
int      init_argc;
char   **init_argv;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN093E Missing argument(s)\n") );
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);

    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss, devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Reject if device is busy or interrupt pending */
    if (dev->busy || IOPENDING(dev)
     || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        logmsg( _("HHCPN096E Device %d:%4.4X busy or interrupt pending\n"),
                  lcss, devnum );
        return -1;
    }

    /* Prevent accidental re-init'ing of already loaded tape drives */
    if (nomountedtapereinit)
    {
        if (0
            || TAPEDEVT_SCSITAPE == dev->tapedevt
            || (argc >= 3 && strcmp(argv[2], TAPE_UNLOADED) != 0)
        )
        {
            if (dev->tmh->tapeloaded( dev, NULL, 0 ))
            {
                release_lock (&dev->lock);
                logmsg(_("HHCPN183E Reinit rejected for drive %u:%4.4X; drive not empty\n"),
                    SSID_TO_LCSS(dev->ssid), dev->devnum);
                return -1;
            }
        }
    }

    /* Close the existing file, if any */
    if (dev->fd < 0 || dev->fd > 2)
    {
        (dev->hnd->close)(dev);
    }

    /* Build the device initialization arguments array */
    if (argc > 2)
    {
        /* Use the specified new arguments */
        init_argc = argc-2;
        init_argv = &argv[2];
    }
    else
    {
        /* Use the same arguments as originally used */
        init_argc = dev->argc;
        if (init_argc)
        {
            init_argv = malloc ( init_argc * sizeof(char*) );
            for (i = 0; i < init_argc; i++)
                if (dev->argv[i])
                    init_argv[i] = strdup(dev->argv[i]);
                else
                    init_argv[i] = NULL;
        }
        else
            init_argv = NULL;
    }

    /* Call the device init routine to do the hard work */
    if ((rc = (dev->hnd->init)(dev, init_argc, init_argv)) < 0)
    {
        logmsg( _("HHCPN097E Initialization failed for device %d:%4.4X\n"),
                  lcss, devnum );
    } else {
        logmsg( _("HHCPN098I Device %d:%4.4X initialized\n"), lcss, devnum );
    }

    /* Save arguments for next time */
    if (rc == 0)
    {
        for (i = 0; i < dev->argc; i++)
            if (dev->argv[i])
                free(dev->argv[i]);
        if (dev->argv)
            free(dev->argv);

        dev->argc = init_argc;
        if (init_argc)
        {
            dev->argv = malloc ( init_argc * sizeof(char*) );
            for (i = 0; i < init_argc; i++)
                if (init_argv[i])
                    dev->argv[i] = strdup(init_argv[i]);
                else
                    dev->argv[i] = NULL;
        }
        else
            dev->argv = NULL;
    }

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Raise unsolicited device end interrupt for the device */
    if (rc == 0)
        rc = device_attention (dev, CSW_DE);

    return rc;
}

///////////////////////////////////////////////////////////////////////
/* savecore filename command - save a core image to file */

int savecore_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    char   *fname;                      /* -> File name (ASCIIZ)     */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    U32     aaddr2;                     /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    int     len;                        /* Number of bytes read      */
    BYTE    c;                          /* (dummy sscanf work area)  */
    char    pathname[MAX_PATH];         /* fname in host path format */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN099E savecore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (argc < 3 || '*' == *(loadaddr = argv[2]))
    {
        for (aaddr = 0; aaddr < sysblk.mainsize &&
            !(STORAGE_KEY(aaddr, regs) & STORKEY_CHANGE); aaddr += 4096)
        {
            ;   /* (nop) */
        }

        if (aaddr >= sysblk.mainsize)
            aaddr = 0;
        else
            aaddr &= ~0xFFF;
    }
    else if (sscanf(loadaddr, "%x%c", &aaddr, &c) !=1 ||
                                       aaddr >= sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN100E savecore: invalid starting address: %s \n"),
                  loadaddr );
        return -1;
    }

    if (argc < 4 || '*' == *(loadaddr = argv[3]))
    {
        for (aaddr2 = sysblk.mainsize - 4096; aaddr2 > 0 &&
            !(STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE); aaddr2 -= 4096)
        {
            ;   /* (nop) */
        }

        if ( STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE )
            aaddr2 |= 0xFFF;
        else
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN148E savecore: no modified storage found\n") );
            return -1;
        }
    }
    else if (sscanf(loadaddr, "%x%c", &aaddr2, &c) !=1 ||
                                       aaddr2 >= sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN101E savecore: invalid ending address: %s \n"),
                  loadaddr );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN102E savecore rejected: CPU not stopped\n") );
        return -1;
    }

    if (aaddr > aaddr2)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN103E invalid range: %8.8X-%8.8X\n"), aaddr, aaddr2 );
        return -1;
    }

    /* Save the file from absolute storage */
    logmsg( _("HHCPN104I Saving locations %8.8X-%8.8X to %s\n"),
              aaddr, aaddr2, fname );

    hostpath(pathname, fname, sizeof(pathname));

    if ((fd = open(pathname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
    {
        int saved_errno = errno;
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN105E savecore error creating %s: %s\n"),
                  fname, strerror(saved_errno) );
        return -1;
    }

    if ((len = write(fd, regs->mainstor + aaddr, (aaddr2 - aaddr) + 1)) < 0)
        logmsg( _("HHCPN106E savecore error writing to %s: %s\n"),
                  fname, strerror(errno) );
    else if((U32)len < (aaddr2 - aaddr) + 1)
        logmsg( _("HHCPN107E savecore: unable to save %d bytes\n"),
            ((aaddr2 - aaddr) + 1) - len );

    close(fd);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN170I savecore command complete.\n"));

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadcore filename command - load a core image file */

int loadcore_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    char   *fname;                      /* -> File name (ASCIIZ)     */
    struct stat statbuff;               /* Buffer for file status    */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     len;                        /* Number of bytes read      */
    char    pathname[MAX_PATH];         /* file in host path format  */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN108E loadcore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];
    hostpath(pathname, fname, sizeof(pathname));

    if (stat(pathname, &statbuff) < 0)
    {
        logmsg( _("HHCPN109E Cannot open %s: %s\n"),
            fname, strerror(errno));
        return -1;
    }

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN110E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN111E loadcore rejected: CPU not stopped\n") );
        return -1;
    }

    /* Read the file into absolute storage */
    logmsg( _("HHCPN112I Loading %s to location %x \n"), fname, aaddr );

    len = load_main(fname, aaddr);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN113I %d bytes read from %s\n"), len, fname );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadtext filename command - load a text deck file */

int loadtext_cmd(int argc, char *argv[], char *cmdline)
{
    char   *fname;                      /* -> File name (ASCIIZ)     */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    BYTE    buf[80];                    /* Read buffer               */
    int     len;                        /* Number of bytes read      */
    int     n;
    REGS   *regs;
    char    pathname[MAX_PATH];

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN114E loadtext rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN115E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (aaddr > regs->mainlim)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN116E Address greater than mainstore size\n") );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN117E loadtext rejected: CPU not stopped\n") );
        return -1;
    }

    /* Open the specified file name */
    hostpath(pathname, fname, sizeof(pathname));
    if ((fd = open (pathname, O_RDONLY | O_BINARY)) < 0)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN118E Cannot open %s: %s\n"),
            fname, strerror(errno));
        return -1;
    }

    for ( n = 0; ; )
    {
        /* Read 80 bytes into buffer */
        if ((len = read (fd, buf, 80)) < 0)
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN119E Cannot read %s: %s\n"),
                    fname, strerror(errno));
            close (fd);
            return -1;
        }

        /* if record is "END" then break out of loop */
        if (0xC5 == buf[1] && 0xD5 == buf[2] && 0xC4 == buf[3])
            break;

        /* if record is "TXT" then copy bytes to mainstore */
        if (0xE3 == buf[1] && 0xE7 == buf[2] && 0xE3 == buf[3])
        {
            n   = buf[5]*65536 + buf[6]*256 + buf[7];
            len = buf[11];
            memcpy(regs->mainstor + aaddr + n, &buf[16], len);
            STORAGE_KEY(aaddr + n, regs) |= (STORKEY_REF | STORKEY_CHANGE);
            STORAGE_KEY(aaddr + n + len - 1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        }
    }

    /* Close file and issue status message */
    close (fd);
    logmsg( _("HHCPN120I Finished loading TEXT deck file\n") );
    logmsg( _("          Last 'TXT' record had address: %3.3X\n"), n );
    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ipending command - display pending interrupts */

int ipending_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK *dev;                        /* -> Device block           */
    IOINT  *io;                         /* -> I/O interrupt entry    */
    unsigned i;
    char    sysid[12];
    BYTE    curpsw[16];
    char *states[] = { "?(0)", "STARTED", "STOPPING", "STOPPED" };

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if (!IS_CPU_ONLINE(i))
        {
            logmsg(_("HHCPN123I CPU%4.4X: offline\n"), i);
            continue;
        }

// /*DEBUG*/logmsg( _("CPU%4.4X: Any cpu interrupt %spending\n"),
// /*DEBUG*/    sysblk.regs[i]->cpuad, sysblk.regs[i]->cpuint ? "" : _("not ") );
        logmsg( _("HHCPN123I CPU%4.4X: CPUint=%8.8X "
                  "(State:%8.8X)&(Mask:%8.8X)\n"),
            sysblk.regs[i]->cpuad, IC_INTERRUPT_CPU(sysblk.regs[i]),
            sysblk.regs[i]->ints_state, sysblk.regs[i]->ints_mask
            );
        logmsg( _("          CPU%4.4X: Interrupt %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_INTERRUPT(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: I/O interrupt %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_IOPENDING                 ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Clock comparator %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_CLKC(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: CPU timer %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_PTIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#if defined(_FEATURE_INTERVAL_TIMER)
        logmsg( _("          CPU%4.4X: Interval timer %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_ITIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#if defined(_FEATURE_ECPSVM)
        logmsg( _("          CPU%4.4X: ECPS vtimer %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_ECPSVTIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#endif /*defined(_FEATURE_ECPSVM)*/
#endif /*defined(_FEATURE_INTERVAL_TIMER)*/
        logmsg( _("          CPU%4.4X: External call %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_EXTCALL(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Emergency signal %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_EMERSIG(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Machine check interrupt %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_MCKPENDING(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Service signal %spending\n"),
            sysblk.regs[i]->cpuad,
            IS_IC_SERVSIG                    ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Mainlock held: %s\n"),
            sysblk.regs[i]->cpuad,
            sysblk.regs[i]->cpuad == sysblk.mainowner ? _("yes") : _("no")
            );
        logmsg( _("          CPU%4.4X: Waiting for mainlock: %s\n"),
            sysblk.regs[i]->cpuad,
            sysblk.regs[i]->mainwait ? _("yes") : _("no")
            );
        logmsg( _("          CPU%4.4X: Intlock held: %s\n"),
            sysblk.regs[i]->cpuad,
            sysblk.regs[i]->cpuad == sysblk.intowner ? _("yes") : _("no")
            );
        logmsg( _("          CPU%4.4X: Waiting for intlock: %s\n"),
            sysblk.regs[i]->cpuad,
            sysblk.regs[i]->intwait && !(sysblk.waiting_mask & BIT(i)) ? _("yes") : _("no")
            );
        logmsg( _("          CPU%4.4X: lock %sheld\n"),
            sysblk.regs[i]->cpuad,
            test_lock(&sysblk.cpulock[i]) ? "" : _("not ")
            );
        if (ARCH_370 == sysblk.arch_mode)
        {
            if (0xFFFF == sysblk.regs[i]->chanset)
                logmsg( _("          CPU%4.4X: No channelset connected\n"),
                    sysblk.regs[i]->cpuad
                    );
            else
                logmsg( _("          CPU%4.4X: Connected to channelset "
                          "%4.4X\n"),
                    sysblk.regs[i]->cpuad,sysblk.regs[i]->chanset
                    );
        }
        logmsg( _("          CPU%4.4X: state %s\n"),
               sysblk.regs[i]->cpuad,states[sysblk.regs[i]->cpustate]);
        logmsg( _("          CPU%4.4X: instcount %" I64_FMT "d\n"),
               sysblk.regs[i]->cpuad,(long long)sysblk.regs[i]->instcount);
        logmsg( _("          CPU%4.4X: siocount %" I64_FMT "d\n"),
               sysblk.regs[i]->cpuad,(long long)sysblk.regs[i]->siototal);
        copy_psw(sysblk.regs[i], curpsw);
        logmsg( _("          CPU%4.4X: psw %2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x"),
               sysblk.regs[i]->cpuad,curpsw[0],curpsw[1],curpsw[2],curpsw[3],
               curpsw[4],curpsw[5],curpsw[6],curpsw[7]);
        if (ARCH_900 == sysblk.arch_mode)
        logmsg( _(" %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"),
               curpsw[8],curpsw[9],curpsw[10],curpsw[11],
               curpsw[12],curpsw[13],curpsw[14],curpsw[15]);
        logmsg("\n");

        if (sysblk.regs[i]->sie_active)
        {
            logmsg( _("HHCPN123I SIE%4.4X: CPUint=%8.8X "
                      "(State:%8.8X)&(Mask:%8.8X)\n"),
                sysblk.regs[i]->guestregs->cpuad, IC_INTERRUPT_CPU(sysblk.regs[i]->guestregs),
                sysblk.regs[i]->guestregs->ints_state, sysblk.regs[i]->guestregs->ints_mask
                );
            logmsg( _("          SIE%4.4X: Interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_INTERRUPT(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: I/O interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_IOPENDING                 ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: Clock comparator %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_CLKC(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: CPU timer %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_PTIMER(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: Interval timer %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_ITIMER(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: External call %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_EXTCALL(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: Emergency signal %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_EMERSIG(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: Machine check interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_MCKPENDING(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: Service signal %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_SERVSIG                    ? "" : _("not ")
                );
            logmsg( _("          SIE%4.4X: lock %sheld\n"),
                sysblk.regs[i]->guestregs->cpuad,
                test_lock(&sysblk.cpulock[i]) ? "" : _("not ")
                );
            if (ARCH_370 == sysblk.arch_mode)
            {
                if (0xFFFF == sysblk.regs[i]->guestregs->chanset)
                    logmsg( _("          SIE%4.4X: No channelset connected\n"),
                        sysblk.regs[i]->guestregs->cpuad
                        );
                else
                    logmsg( _("          SIE%4.4X: Connected to channelset "
                              "%4.4X\n"),
                        sysblk.regs[i]->guestregs->cpuad,sysblk.regs[i]->guestregs->chanset
                        );
            }
            logmsg( _("          SIE%4.4X: state %s\n"),
                   sysblk.regs[i]->guestregs->cpuad,states[sysblk.regs[i]->guestregs->cpustate]);
            logmsg( _("          SIE%4.4X: instcount %" I64_FMT "d\n"),
                   sysblk.regs[i]->guestregs->cpuad,(long long)sysblk.regs[i]->guestregs->instcount);
            logmsg( _("          SIE%4.4X: siocount %" I64_FMT "d\n"),
                   sysblk.regs[i]->guestregs->cpuad,(long long)sysblk.regs[i]->guestregs->siototal);
            copy_psw(sysblk.regs[i]->guestregs, curpsw);
            logmsg( _("          SIE%4.4X: psw %2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x"),
                   sysblk.regs[i]->guestregs->cpuad,curpsw[0],curpsw[1],curpsw[2],curpsw[3],
                   curpsw[4],curpsw[5],curpsw[6],curpsw[7]);
            if (ARCH_900 == sysblk.regs[i]->guestregs->arch_mode)
            logmsg( _(" %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"),
               curpsw[8],curpsw[9],curpsw[10],curpsw[11],
               curpsw[12],curpsw[13],curpsw[14],curpsw[15]);
            logmsg("\n");
        }
    }

    logmsg( _("          Config mask %8.8X started mask %8.8X waiting mask %8.8X\n"),
        sysblk.config_mask, sysblk.started_mask, sysblk.waiting_mask
        );
    logmsg( _("          Signaling facility %sbusy\n"),
        test_lock(&sysblk.sigplock) ? "" : _("not ")
        );
    logmsg( _("          TOD lock %sheld\n"),
        test_lock(&sysblk.todlock) ? "" : _("not ")
        );
    logmsg( _("          Mainlock %sheld; owner %4.4x\n"),
        test_lock(&sysblk.mainlock) ? "" : _("not "),
        sysblk.mainowner
        );
    logmsg( _("          Intlock %sheld; owner %4.4x\n"),
        test_lock(&sysblk.intlock) ? "" : _("not "),
        sysblk.intowner
        );
#if !defined(OPTION_FISHIO)
    logmsg( _("          Ioq lock %sheld\n"),
        test_lock(&sysblk.ioqlock) ? "" : _("not ")
        );
#endif

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (dev->ioactive == DEV_SYS_NONE)
            strcpy (sysid, "(none)");
        else if (dev->ioactive == DEV_SYS_LOCAL)
            strcpy (sysid, "local");
        else
            sprintf (sysid, "id=%d", dev->ioactive);
        if (dev->busy && !(dev->suspended && dev->ioactive == DEV_SYS_NONE))
            logmsg( _("          DEV %d:%4.4X: busy %s\n"), SSID_TO_LCSS(dev->ssid), dev->devnum, sysid );
        if (dev->reserved)
            logmsg( _("          DEV %d:%4.4X: reserved %s\n"), SSID_TO_LCSS(dev->ssid), dev->devnum, sysid );
        if (dev->suspended)
            logmsg( _("          DEV %d:%4.4X: suspended\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: I/O pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: PCI pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->attnpending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: Attn pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: CRW pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (test_lock(&dev->lock) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: lock held\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
    }

    logmsg( _("          I/O interrupt queue: ") );

    if (!sysblk.iointq)
        logmsg( _("(NULL)") );
    logmsg("\n");

    for (io = sysblk.iointq; io; io = io->next)
        logmsg
        (
            _("          DEV %d:%4.4X,%s%s%s%s, pri %d\n")

            ,SSID_TO_LCSS(io->dev->ssid)
            ,io->dev->devnum

            ,io->pending      ? " normal"  : ""
            ,io->pcipending   ? " PCI"     : ""
            ,io->attnpending  ? " ATTN"    : ""
            ,!IOPENDING(io)   ? " unknown" : ""

            ,io->priority
        );

    return 0;
}

#if defined(OPTION_INSTRUCTION_COUNTING)

///////////////////////////////////////////////////////////////////////
/* icount command - display instruction counts */

int icount_cmd(int argc, char *argv[], char *cmdline)
{
    int i1, i2;

    UNREFERENCED(cmdline);

    if (argc > 1 && !strcasecmp(argv[1], "clear"))
    {
        memset(IMAP_FIRST,0,IMAP_SIZE);
        logmsg( _("HHCPN124I Instruction counts reset to zero.\n") );
        return 0;
    }

    logmsg(_("HHCPN125I Instruction count display:\n"));
    for (i1 = 0; i1 < 256; i1++)
    {
        switch (i1)
        {
            case 0x01:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imap01[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imap01[i2]);
                break;
            case 0xA4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapa4[i2]);
                break;
            case 0xA5:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa5[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapa5[i2]);
                break;
            case 0xA6:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa6[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapa6[i2]);
                break;
            case 0xA7:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa7[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapa7[i2]);
                break;
            case 0xB2:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb2[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapb2[i2]);
                break;
            case 0xB3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapb3[i2]);
                break;
            case 0xB9:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb9[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapb9[i2]);
                break;
            case 0xC0:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc0[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapc0[i2]);
                break;
            case 0xC2:                                                      /*@Z9*/
                for(i2 = 0; i2 < 16; i2++)                                  /*@Z9*/
                    if(sysblk.imapc2[i2])                                   /*@Z9*/
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" I64_FMT "u\n",  /*@Z9*/
                            i1, i2, sysblk.imapc2[i2]);                     /*@Z9*/
                break;                                                      /*@Z9*/
            case 0xE3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imape3[i2]);
                break;
            case 0xE4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imape4[i2]);
                break;
            case 0xE5:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape5[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imape5[i2]);
                break;
            case 0xEB:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapeb[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapeb[i2]);
                break;
            case 0xEC:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapec[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imapec[i2]);
                break;
            case 0xED:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imaped[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" I64_FMT "u\n",
                            i1, i2, sysblk.imaped[i2]);
                break;
            default:
                if(sysblk.imapxx[i1])
                    logmsg("          INST=%2.2X  \tCOUNT=%" I64_FMT "u\n",
                        i1, sysblk.imapxx[i1]);
                break;
        }
    }
    return 0;
}

#endif /*defined(OPTION_INSTRUCTION_COUNTING)*/


///////////////////////////////////////////////////////////////////////
/* defsym command - define substitution symbol */

#if defined(OPTION_CONFIG_SYMBOLS)

int defsym_cmd(int argc, char *argv[], char *cmdline)
{
    char* sym;
    char* value = "";

    if (argc < 2)
    {
        list_all_symbols();
        return 0;
    }

    /* point to symbol name */
    sym = argv[1];

    if (argc >= 3)
    {
        /* point to first non-blank following symbol name */
        cmdline += strlen("defsym") + 1;
        while (isspace(*cmdline)) cmdline++;
        cmdline += strlen(sym) + 1;
        value = cmdline;
        while (*value && isspace(*value)) value++;
    }

    /* define the symbol */
    set_symbol(sym,value);
    return 0;
}

#endif // defined(OPTION_CONFIG_SYMBOLS)

///////////////////////////////////////////////////////////////////////
/* PATCH ISW20030220 - Script command support */

static int scr_recursion=0;     /* Recursion count (set to 0) */
static int scr_aborted=0;          /* Script abort flag */
static int scr_uaborted=0;          /* Script user abort flag */
TID scr_tid=0;

///////////////////////////////////////////////////////////////////////

int cscript_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if(scr_tid!=0)
    {
        scr_uaborted=1;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////

int script_cmd(int argc, char *argv[], char *cmdline)
{

    int i;

    UNREFERENCED(cmdline);
    if(argc<2)
    {
        logmsg(_("HHCPN996E The script command requires a filename\n"));
        return 1;
    }
    if(scr_tid==0)
    {
        scr_tid=thread_id();
        scr_aborted=0;
        scr_uaborted=0;
    }
    else
    {
        if(scr_tid!=thread_id())
        {
            logmsg(_("HHCPN997E Only 1 script may be invoked from the panel at any time\n"));
            return 1;
        }
    }

    for(i=1;i<argc;i++)
    {
        process_script_file(argv[i],0);
    }
    return(0);
}

///////////////////////////////////////////////////////////////////////

void script_test_userabort()
{
        if(scr_uaborted)
        {
           logmsg(_("HHCPN998E Script aborted : user cancel request\n"));
           scr_aborted=1;
        }
}

///////////////////////////////////////////////////////////////////////

int process_script_file(char *script_name,int isrcfile)
{
FILE   *scrfp;                          /* RC file pointer           */
size_t  scrbufsize = 1024;              /* Size of RC file  buffer   */
char   *scrbuf = NULL;                  /* RC file input buffer      */
int     scrlen;                         /* length of RC file record  */
int     scr_pause_amt = 0;              /* seconds to pause RC file  */
char   *p;                              /* (work)                    */
char    pathname[MAX_PATH];             /* (work)                    */

    /* Check the recursion level - if it exceeds a certain amount
       abort the script stack
    */
    if(scr_recursion>=10)
    {
        logmsg(_("HHCPN998E Script aborted : Script recursion level exceeded\n"));
        scr_aborted=1;
        return 0;
    }

    /* Open RC file */

    hostpath(pathname, script_name, sizeof(pathname));

    if (!(scrfp = fopen(pathname, "r")))
    {
        int save_errno = errno;

        if (!isrcfile)
        {
            if (ENOENT != errno)
                logmsg(_("HHCPN007E Script file \"%s\" open failed: %s\n"),
                    script_name, strerror(errno));
            else
                logmsg(_("HHCPN995E Script file \"%s\" not found\n"),
                    script_name);
        }
        else /* (this IS the .rc file...) */
        {
            if (ENOENT != errno)
                logmsg(_("HHCPN007E Script file \"%s\" open failed: %s\n"),
                    script_name, strerror(errno));
        }

        errno = save_errno;
        return -1;
    }

    scr_recursion++;

    if(isrcfile)
    {
        logmsg(_("HHCPN008I Script file processing started using file \"%s\"\n"),
           script_name);
    }

    /* Obtain storage for the SCRIPT file buffer */

    if (!(scrbuf = malloc (scrbufsize)))
    {
        logmsg(_("HHCPN009E Script file buffer malloc failed: %s\n"),
            strerror(errno));
        fclose(scrfp);
        return 0;
    }

    for (; ;)
    {
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
        /* Read a complete line from the SCRIPT file */

        if (!fgets(scrbuf, scrbufsize, scrfp)) break;

        /* Remove trailing whitespace */

        for (scrlen = strlen(scrbuf); scrlen && isspace(scrbuf[scrlen-1]); scrlen--);
        scrbuf[scrlen] = 0;

        /* '#' == silent comment, '*' == loud comment */

        if ('#' == scrbuf[0] || '*' == scrbuf[0])
        {
            if ('*' == scrbuf[0])
                logmsg ("%s\n",scrbuf);
            continue;
        }

        /* Remove any # comments on the line before processing */

        if ((p = strchr(scrbuf,'#')) && p > scrbuf)
            do *p = 0; while (isspace(*--p) && p >= scrbuf);

        if (strncasecmp(scrbuf,"pause",5) == 0)
        {
            sscanf(scrbuf+5, "%d", &scr_pause_amt);

            if (scr_pause_amt < 0 || scr_pause_amt > 999)
            {
                logmsg(_("HHCPN010W Ignoring invalid SCRIPT file pause "
                         "statement: %s\n"),
                         scrbuf+5);
                continue;
            }

            logmsg (_("HHCPN011I Pausing SCRIPT file processing for %d "
                      "seconds...\n"),
                      scr_pause_amt);
            SLEEP(scr_pause_amt);
            logmsg (_("HHCPN012I Resuming SCRIPT file processing...\n"));

            continue;
        }

        /* Process the command */

        for (p = scrbuf; isspace(*p); p++);

        panel_command(p);
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
    }

    if (feof(scrfp))
        logmsg (_("HHCPN013I EOF reached on SCRIPT file. Processing complete.\n"));
    else
    {
        if(!scr_aborted)
        {
           logmsg (_("HHCPN014E I/O error reading SCRIPT file: %s\n"),
                 strerror(errno));
        }
        else
        {
           logmsg (_("HHCPN999I Script \"%s\" aborted due to previous conditions\n"),
               script_name);
           scr_uaborted=1;
        }
    }

    fclose(scrfp);
    scr_recursion--;    /* Decrement recursion count */
    if(scr_recursion==0)
    {
      scr_aborted=0;    /* reset abort flag */
      scr_tid=0;    /* reset script thread id */
    }

    return 0;
}
/* END PATCH ISW20030220 */

///////////////////////////////////////////////////////////////////////
/* archmode command - set architecture mode */

int archmode_cmd(int argc, char *argv[], char *cmdline)
{
    int i;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN126I Architecture mode = %s\n"),
                  get_arch_mode_string(NULL) );
        return 0;
    }

    OBTAIN_INTLOCK(NULL);

    /* Make sure all CPUs are deconfigured or stopped */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
         && CPUSTATE_STOPPED != sysblk.regs[i]->cpustate)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN127E All CPU's must be stopped to change "
                      "architecture\n") );
            return -1;
        }
#if defined(_370)
    if (!strcasecmp (argv[1], arch_name[ARCH_370]))
    {
        sysblk.arch_mode = ARCH_370;
        sysblk.maxcpu = sysblk.numcpu;
    }
    else
#endif
#if defined(_390)
    if (!strcasecmp (argv[1], arch_name[ARCH_390]))
    {
        sysblk.arch_mode = ARCH_390;
#if defined(_FEATURE_CPU_RECONFIG)
        sysblk.maxcpu = MAX_CPU_ENGINES;
#else
        sysblk.maxcpu = sysblk.numcpu;
#endif
    }
    else
#endif
#if defined(_900)
    if (0
        || !strcasecmp (argv[1], arch_name[ARCH_900])
        || !strcasecmp (argv[1], "ESAME")
    )
    {
        sysblk.arch_mode = ARCH_900;
#if defined(_FEATURE_CPU_RECONFIG)
        sysblk.maxcpu = MAX_CPU_ENGINES;
#else
        sysblk.maxcpu = sysblk.numcpu;
#endif
    }
    else
#endif
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN128E Invalid architecture mode %s\n"), argv[1] );
        return -1;
    }
    if (sysblk.pcpu >= MAX_CPU)
        sysblk.pcpu = 0;

    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif /* defined(OPTION_FISHIO) */

    /* Indicate if z/Architecture is supported */
    sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;

    logmsg( _("HHCPN129I Architecture successfully set to %s mode.\n"),
              get_arch_mode_string(NULL) );

#if defined(_FEATURE_CPU_RECONFIG) && defined(_S370)
    /* Configure CPUs for S/370 mode */
    if (sysblk.archmode == ARCH_S370)
        for (i = MAX_CPU_ENGINES - 1; i >= 0; i--)
            if (i < MAX_CPU && !IS_CPU_ONLINE(i))
                configure_cpu(i);
            else if (i >= MAX_CPU && IS_CPU_ONLINE(i))
                deconfigure_cpu(i);
#endif

    RELEASE_INTLOCK(NULL);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* x+ and x- commands - turn switches on or off */

int OnOffCommand(int argc, char *argv[], char *cmdline)
{
    char   *cmd = cmdline;              /* Copy of panel command     */
    int     oneorzero;                  /* 1=x+ command, 0=x-        */
    char   *onoroff;                    /* "on" or "off"             */
    U32     aaddr;                      /* Absolute storage address  */
    DEVBLK* dev;
    U16     devnum;
    U16     lcss;
REGS *regs;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (cmd[1] == '+') {
        oneorzero = 1;
        onoroff = _("on");
    } else {
        oneorzero = 0;
        onoroff = _("off");
    }

    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs=sysblk.regs[sysblk.pcpu];

    /////////////////////////////////////////////////////
    // f- and f+ commands - mark frames unusable/usable

    if ((cmd[0] == 'f') && sscanf(cmd+2, "%x%c", &aaddr, &c) == 1)
    {
        if (aaddr > regs->mainlim)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN130E Invalid frame address %8.8X\n"), aaddr );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN131I Frame %8.8X marked %s\n"), aaddr,
                oneorzero ? _("usable") : _("unusable")
            );
        return 0;
    }

    /////////////////////////////////////////////////////
    // t+ and t- commands - instruction tracing on/off

    if (cmd[0]=='t' && cmd[2]=='\0')
    {
        sysblk.insttrace = oneorzero;
        SET_IC_TRACE;
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN132I Instruction tracing is now %s\n"), onoroff );
        return 0;
    }

    /////////////////////////////////////////////////////
    // s+ and s- commands - instruction stepping on/off

    if (cmd[0]=='s' && cmd[2]=='\0')
    {
        sysblk.inststep = oneorzero;
        SET_IC_TRACE;
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN133I Instruction stepping is now %s\n"), onoroff );
        return 0;
    }

#ifdef OPTION_CKD_KEY_TRACING
    /////////////////////////////////////////////////////
    // t+ckd and t-ckd commands - turn CKD_KEY tracing on/off

    if ((cmd[0] == 't') && (strcasecmp(cmd+2, "ckd") == 0))
    {
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if (dev->devchar[10] == 0x20)
                dev->ckdkeytrace = oneorzero;
        }
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN134I CKD KEY trace is now %s\n"), onoroff );
        return 0;
    }

#endif
    /////////////////////////////////////////////////////
    // t+devn and t-devn commands - turn CCW tracing on/off
    // s+devn and s-devn commands - turn CCW stepping on/off

    if ((cmd[0] == 't' || cmd[0] == 's')
        && parse_single_devnum_silent(&cmd[2],&lcss,&devnum)==0 )
    {
        dev = find_device_by_devnum (lcss, devnum);
        if (dev == NULL)
        {
            devnotfound_msg(lcss,devnum);
            RELEASE_INTLOCK(NULL);
            return -1;
        }

        if (cmd[0] == 't')
        {
            dev->ccwtrace = oneorzero;
            logmsg( _("HHCPN136I CCW tracing is now %s for device %d:%4.4X\n"),
                onoroff, lcss, devnum
                );
        } else {
            dev->ccwstep = oneorzero;
            logmsg( _("HHCPN137I CCW stepping is now %s for device %d:%4.4X\n"),
                onoroff, lcss, devnum
                );
        }
        RELEASE_INTLOCK(NULL);
        return 0;
    }

    RELEASE_INTLOCK(NULL);
    logmsg( _("HHCPN138E Unrecognized +/- command.\n") );
    return -1;
}

static inline char *aea_mode_str(BYTE mode)
{
static char *name[] = { "DAT-Off", "Primary", "AR", "Secondary", "Home",
0, 0, 0, "PER/DAT-Off", "PER/Primary", "PER/AR", "PER/Secondary", "PER/Home" };

    return name[(mode & 0x0f) | ((mode & 0xf0) ? 8 : 0)];
}

///////////////////////////////////////////////////////////////////////
/* aea - display aea values */

int aea_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    logmsg ("aea mode   %s\n",aea_mode_str(regs->aea_mode));

    logmsg ("aea ar    ");
    for (i = -5; i < 16; i++)
         if(regs->aea_ar[i] > 0)
            logmsg(" %2.2x",regs->aea_ar[i]);
        else
            logmsg(" %2d",regs->aea_ar[i]);
    logmsg ("\n");

    logmsg ("aea common            ");
    for (i = -1; i < 16; i++)
        if(regs->aea_common[i] > 0)
            logmsg(" %2.2x",regs->aea_common[i]);
        else
            logmsg(" %2d",regs->aea_common[i]);
    logmsg ("\n");

    logmsg ("aea cr[1]  %16.16" I64_FMT "x\n    cr[7]  %16.16" I64_FMT "x\n"
            "    cr[13] %16.16" I64_FMT "x\n",
            regs->CR_G(1),regs->CR_G(7),regs->CR_G(13));

    logmsg ("    cr[r]  %16.16" I64_FMT "x\n",
            regs->CR_G(CR_ASD_REAL));

    for(i = 0; i < 16; i++)
        if(regs->aea_ar[i] > 15)
            logmsg ("    alb[%d] %16.16" I64_FMT "x\n",
                    regs->alb[i]);

    if (regs->sie_active)
    {
        regs = regs->guestregs;

        logmsg ("aea SIE\n");
        logmsg ("aea mode   %s\n",aea_mode_str(regs->aea_mode));

        logmsg ("aea ar    ");
        for (i = -5; i < 16; i++)
        if(regs->aea_ar[i] > 0)
            logmsg(" %2.2x",regs->aea_ar[i]);
        else
            logmsg(" %2d",regs->aea_ar[i]);
        logmsg ("\n");

        logmsg ("aea common            ");
        for (i = -1; i < 16; i++)
        if(regs->aea_common[i] > 0)
            logmsg(" %2.2x",regs->aea_common[i]);
        else
            logmsg(" %2d",regs->aea_common[i]);
        logmsg ("\n");

        logmsg ("aea cr[1]  %16.16" I64_FMT "x\n    cr[7]  %16.16" I64_FMT "x\n"
                "    cr[13] %16.16" I64_FMT "x\n",
                regs->CR_G(1),regs->CR_G(7),regs->CR_G(13));

        logmsg ("    cr[r]  %16.16" I64_FMT "x\n",
                regs->CR_G(CR_ASD_REAL));

        for(i = 0; i < 16; i++)
            if(regs->aea_ar[i] > 15)
                logmsg ("    alb[%d] %16.16" I64_FMT "x\n",
                        regs->alb[i]);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* aia - display aia values */

DLL_EXPORT int aia_cmd(int argc, char *argv[], char *cmdline)
{
    /* int     i; */                         /* Index                     */
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    logmsg ("mainstor %p  aim %p  aiv %16.16" I64_FMT "x  aie %16.16" I64_FMT "x\n",
            regs->mainstor,regs->aim,regs->aiv,regs->aie);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        logmsg ("SIE:\n");
        logmsg ("mainstor %p  aim %p  aiv %16.16" I64_FMT "x  aie %16.16" I64_FMT "x\n",
            regs->mainstor,regs->aim,regs->aiv,regs->aie);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* tlb - display tlb table */

int tlb_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    int     shift;                      /* Number of bits to shift   */
    int     bytemask;                   /* Byte mask                 */
    U64     pagemask;                   /* Page mask                 */
    int     matches = 0;                /* Number aeID matches       */
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W CPU%4.4X not configured\n"), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];
    shift = regs->arch_mode == ARCH_370 ? 11 : 12;
    bytemask = regs->arch_mode == ARCH_370 ? 0x1FFFFF : 0x3FFFFF;
    pagemask = regs->arch_mode == ARCH_370 ? 0x00E00000 :
               regs->arch_mode == ARCH_390 ? 0x7FC00000 :
                                     0xFFFFFFFFFFC00000ULL;

    logmsg ("tlbID 0x%6.6x mainstor %p\n",regs->tlbID,regs->mainstor);
    logmsg ("  ix              asd            vaddr              pte   id c p r w ky       main\n");
    for (i = 0; i < TLBN; i++)
    {
        logmsg("%s%3.3x %16.16" I64_FMT "x %16.16" I64_FMT "x %16.16" I64_FMT "x %4.4x %1d %1d %1d %1d %2.2x %8.8x\n",
         ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
         i,regs->tlb.TLB_ASD_G(i),
         ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
         regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
         regs->tlb.common[i],regs->tlb.protect[i],
         (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
         regs->tlb.skey[i],regs->tlb.main[i] - regs->mainstor);
        matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
    }
    logmsg("%d tlbID matches\n", matches);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        shift = regs->guestregs->arch_mode == ARCH_370 ? 11 : 12;
        bytemask = regs->arch_mode == ARCH_370 ? 0x1FFFFF : 0x3FFFFF;
        pagemask = regs->arch_mode == ARCH_370 ? 0x00E00000 :
                   regs->arch_mode == ARCH_390 ? 0x7FC00000 :
                                         0xFFFFFFFFFFC00000ULL;

        logmsg ("\nSIE: tlbID 0x%4.4x mainstor %p\n",regs->tlbID,regs->mainstor);
        logmsg ("  ix              asd            vaddr              pte   id c p r w ky       main\n");
        for (i = matches = 0; i < TLBN; i++)
        {
            logmsg("%s%3.3x %16.16" I64_FMT "x %16.16" I64_FMT "x %16.16" I64_FMT "x %4.4x %1d %1d %1d %1d %2.2x %p\n",
             ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
             i,regs->tlb.TLB_ASD_G(i),
             ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
             regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
             regs->tlb.common[i],regs->tlb.protect[i],
             (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
             regs->tlb.skey[i],regs->tlb.main[i]);
            matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
        }
        logmsg("SIE: %d tlbID matches\n", matches);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}

#if defined(SIE_DEBUG_PERFMON)
///////////////////////////////////////////////////////////////////////
/* spm - SIE performance monitor table */

int spm_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    sie_perfmon_disp();

    return 0;
}
#endif

#if defined(_FEATURE_SYSTEM_CONSOLE)
///////////////////////////////////////////////////////////////////////
/* ssd - signal shutdown command */

int ssd_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    signal_quiesce(0, 0);

    return 0;
}
#endif

#if defined(OPTION_COUNTING)
///////////////////////////////////////////////////////////////////////
/* count - display counts */

int count_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    U64     instcount = 0;              /* Instruction count         */

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (argc > 1 && strcasecmp(argv[1],"clear") == 0)
    {
        for (i = 0; i < MAX_CPU; i++)
            if (IS_CPU_ONLINE(i))
                sysblk.regs[i]->instcount = 0;
        for (i = 0; i < OPTION_COUNTING; i++)
            sysblk.count[i] = 0;
    }
    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i))
            instcount += sysblk.regs[i]->instcount;
    logmsg ("  i: %12" I64_FMT "d\n", instcount);

    for (i = 0; i < OPTION_COUNTING; i++)
        logmsg ("%3d: %12" I64_FMT "d\n", i, sysblk.count[i]);

    return 0;
}
#endif

#if defined(OPTION_DYNAMIC_LOAD)
///////////////////////////////////////////////////////////////////////
/* ldmod - load a module */

int ldmod_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD100I Loading %s ...\n"),argv[i]);
        if(!hdl_load(argv[i], 0))
            logmsg(_("HHCHD101I Module %s loaded\n"),argv[i]);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* rmmod - delete a module */

int rmmod_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD102I Unloading %s ...\n"),argv[i]);
        if(!hdl_dele(argv[i]))
            logmsg(_("HHCHD103I Module %s unloaded\n"),argv[i]);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* lsmod - list dynamic modules */

int lsmod_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_list(HDL_LIST_DEFAULT);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* lsdep - list module dependencies */

int lsdep_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_dlst();

    return 0;
}
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

///////////////////////////////////////////////////////////////////////
/* evm - ECPS:VM command */

#ifdef FEATURE_ECPSVM
int evm_cmd_1(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg(_("HHCPN150W evm command is deprecated. Use \"ecpsvm\" instead\n"));
    ecpsvm_command(argc,argv);
    return 0;
}
int evm_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    ecpsvm_command(argc,argv);
    return 0;
}
#endif

/* Set the hercules logo file */
int herclogo_cmd(int argc,char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if(argc<2)
    {
        sysblk.logofile=NULL;
        clearlogo();
        return 0;
    }
    return readlogo(argv[1]);
}

///////////////////////////////////////////////////////////////////////
/* sizeof - Display sizes of various structures/tables */
int sizeof_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg(_("HHCPN161I (void *) ..........%7d\n"),sizeof(void *));
    logmsg(_("HHCPN161I (unsigned int) ....%7d\n"),sizeof(unsigned int));
    logmsg(_("HHCPN161I (size_t) ..........%7d\n"),sizeof(size_t));
    logmsg(_("HHCPN161I (off_t) ...........%7d\n"),sizeof(off_t));
    logmsg(_("HHCPN161I SYSBLK ............%7d\n"),sizeof(SYSBLK));
    logmsg(_("HHCPN161I REGS ..............%7d\n"),sizeof(REGS));
    logmsg(_("HHCPN161I REGS (copy len) ...%7d\n"),sysblk.regs_copy_len);
    logmsg(_("HHCPN161I DEVBLK ............%7d\n"),sizeof(DEVBLK));
    logmsg(_("HHCPN161I TLB entry .........%7d\n"),sizeof(TLB)/TLBN);
    logmsg(_("HHCPN161I TLB table .........%7d\n"),sizeof(TLB));
    logmsg(_("HHCPN161I FILENAME_MAX ......%7d\n"),FILENAME_MAX);
    logmsg(_("HHCPN161I PATH_MAX ..........%7d\n"),PATH_MAX);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* hao - Hercules Automatic Operator */

#if defined(OPTION_HAO)
int hao_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    hao_command(cmdline);   /* (actual HAO code is in module hao.c) */
    return 0;
}
#endif /* defined(OPTION_HAO) */

///////////////////////////////////////////////////////////////////////
// Handle externally defined commands...

// (for use in CMDTAB COMMAND entry further below)
#define      EXT_CMD(xxx_cmd)  call_ ## xxx_cmd

// (for defining routing function immediately below)
#define CALL_EXT_CMD(xxx_cmd)  \
int call_ ## xxx_cmd ( int argc, char *argv[], char *cmdline )  { \
    return   xxx_cmd (     argc,       argv,         cmdline ); }

// Externally defined commands routing functions...

CALL_EXT_CMD ( ptt_cmd    )
CALL_EXT_CMD ( cache_cmd  )
CALL_EXT_CMD ( shared_cmd )

///////////////////////////////////////////////////////////////////////
// Layout of command routing table...

typedef int CMDFUNC(int argc, char *argv[], char *cmdline);

typedef struct _CMDTAB
{
    const char* pszCommand;     /* command          */
    CMDFUNC*    pfnCommand;     /* handler function */
    const char* pszCmdDesc;     /* description      */
}
CMDTAB;

#define COMMAND(cmd,func,desc)  { cmd, func, desc },

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
// Define all panel command here...

int  ListAllCommands (int argc, char *argv[], char *cmdline);  /*(forward reference)*/
int  HelpCommand     (int argc, char *argv[], char *cmdline);  /*(forward reference)*/

CMDTAB Commands[] =
{
/*        command       function        one-line description...
        (max 9 chars)
*/
COMMAND ( "?",         ListAllCommands, "list all commands" )
COMMAND ( "help",      HelpCommand,   "command specific help\n" )

COMMAND ( "*",         comment_cmd,   "(log comment to syslog)\n" )

COMMAND ( "hst",       History,       "history of commands" )
#if defined(OPTION_HAO)
COMMAND ( "hao",       hao_cmd,       "Hercules Automatic Operator" )
#endif /* defined(OPTION_HAO) */
COMMAND ( "log",       log_cmd,       "direct log output" )
COMMAND ( "version",   version_cmd,   "display version information\n" )

COMMAND ( "quit",      quit_cmd,      "terminate the emulator" )
COMMAND ( "exit",      quit_cmd,      "(synonym for 'quit')\n" )

COMMAND ( "cpu",       cpu_cmd,       "define target cpu for panel display and commands\n" )

COMMAND ( "start",     start_cmd,     "start CPU (or printer device if argument given)" )
COMMAND ( "stop",      stop_cmd,      "stop CPU (or printer device if argument given)\n" )

COMMAND ( "startall",  startall_cmd,  "start all CPU's" )
COMMAND ( "stopall",   stopall_cmd,   "stop all CPU's\n" )

#ifdef _FEATURE_CPU_RECONFIG
COMMAND ( "cf",        cf_cmd,        "configure current CPU online or offline" )
COMMAND ( "cfall",     cfall_cmd,     "configure all CPU's online or offline\n" )
#endif

#ifdef _FEATURE_SYSTEM_CONSOLE
COMMAND ( ".reply",    g_cmd,         "scp command" )
COMMAND ( "!message",    g_cmd,       "scp priority messsage" )
COMMAND ( "ssd",       ssd_cmd,       "Signal Shutdown\n" )
#endif

#ifdef OPTION_PTTRACE
COMMAND ( "ptt",  EXT_CMD(ptt_cmd),   "display pthread trace\n" )
#endif

COMMAND ( "i",         i_cmd,         "generate I/O attention interrupt for device" )
COMMAND ( "ext",       ext_cmd,       "generate external interrupt" )
COMMAND ( "restart",   restart_cmd,   "generate restart interrupt" )
COMMAND ( "archmode",  archmode_cmd,  "set architecture mode" )
COMMAND ( "loadparm",  loadparm_cmd,  "set IPL parameter\n" )

COMMAND ( "ipl",       ipl_cmd,       "IPL Normal from device xxxx" )
COMMAND ( "iplc",      iplc_cmd,      "IPL Clear from device xxxx" )
COMMAND ( "sysreset",  sysr_cmd,      "Issue SYSTEM Reset manual operation" )
COMMAND ( "sysclear",  sysc_cmd,      "Issue SYSTEM Clear Reset manual operation" )
COMMAND ( "store",     store_cmd,     "store CPU status at absolute zero\n" )

COMMAND ( "psw",       psw_cmd,       "display program status word" )
COMMAND ( "gpr",       gpr_cmd,       "display general purpose registers" )
COMMAND ( "fpr",       fpr_cmd,       "display floating point registers" )
COMMAND ( "cr",        cr_cmd,        "display control registers" )
COMMAND ( "ar",        ar_cmd,        "display access registers" )
COMMAND ( "pr",        pr_cmd,        "display prefix register" )
COMMAND ( "timerint",  timerint_cmd,  "display or set timers update interval" )
COMMAND ( "clocks",    clocks_cmd,    "display tod clkc and cpu timer" )
COMMAND ( "ipending",  ipending_cmd,  "display pending interrupts" )
COMMAND ( "ds",        ds_cmd,        "display subchannel" )
COMMAND ( "r",         r_cmd,         "display or alter real storage" )
COMMAND ( "v",         v_cmd,         "display or alter virtual storage" )
COMMAND ( "u",         u_cmd,         "disassemble storage" )
COMMAND ( "devtmax",   devtmax_cmd,   "display or set max device threads" )
COMMAND ( "k",         k_cmd,         "display cckd internal trace\n" )

COMMAND ( "attach",    attach_cmd,    "configure device" )
COMMAND ( "detach",    detach_cmd,    "remove device" )
COMMAND ( "define",    define_cmd,    "rename device" )
COMMAND ( "devinit",   devinit_cmd,   "reinitialize device" )
COMMAND ( "devlist",   devlist_cmd,   "list device or all devices\n" )
COMMAND ( "qd",        qd_cmd,        "query dasd\n" )

#if defined( OPTION_SCSI_TAPE )
COMMAND ( "scsimount", scsimount_cmd, "automatic SCSI tape mounts\n" )
#endif /* defined( OPTION_SCSI_TAPE ) */

COMMAND ( "sh",        sh_cmd,          "shell command" )
COMMAND ( "cache", EXT_CMD(cache_cmd),  "cache command" )
COMMAND ( "cckd",      cckd_cmd,        "cckd command" )
COMMAND ( "shrd",  EXT_CMD(shared_cmd), "shrd command" )
COMMAND ( "quiet",     quiet_cmd,       "toggle automatic refresh of panel display data\n" )

COMMAND ( "b",         bset_cmd,      "set breakpoint" )
COMMAND ( "b-",        bdelete_cmd,   "delete breakpoint" )
COMMAND ( "g",         g_cmd,         "turn off instruction stepping and start CPU\n" )

COMMAND ( "ostailor",  ostailor_cmd,  "trace program interrupts" )
COMMAND ( "pgmtrace",  pgmtrace_cmd,  "trace program interrupts" )
COMMAND ( "savecore",  savecore_cmd,  "save a core image to file" )
COMMAND ( "loadcore",  loadcore_cmd,  "load a core image file" )
COMMAND ( "loadtext",  loadtext_cmd,  "load a text deck file\n" )

#if defined(OPTION_DYNAMIC_LOAD)
COMMAND ( "ldmod",     ldmod_cmd,     "load a module" )
COMMAND ( "rmmod",     rmmod_cmd,     "delete a module" )
COMMAND ( "lsmod",     lsmod_cmd,     "list dynamic modules" )
COMMAND ( "lsdep",     lsdep_cmd,     "list module dependencies\n" )
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#ifdef OPTION_IODELAY_KLUDGE
COMMAND ( "iodelay",   iodelay_cmd,   "display or set I/O delay value" )
#endif
#if defined(OPTION_W32_CTCI)
COMMAND ( "tt32stats", tt32stats_cmd, "(deprecated; use 'tt32' cmd instead)" )
COMMAND ( "tt32",      tt32_cmd,      "control/query CTCI-W32 functionality" )
#endif
COMMAND ( "toddrag",   toddrag_cmd,   "display or set TOD clock drag factor" )
#ifdef PANEL_REFRESH_RATE
COMMAND ( "panrate",   panrate_cmd,   "display or set rate at which console refreshes" )
#endif
COMMAND ( "syncio",    syncio_cmd,    "display syncio devices statistics" )
#if defined(OPTION_INSTRUCTION_COUNTING)
COMMAND ( "icount",    icount_cmd,    "display individual instruction counts" )
#endif
#ifdef OPTION_MIPS_COUNTING
COMMAND ( "maxrates",  maxrates_cmd,  "display maximum observed MIPS/SIOS rate for the\n               defined interval or define a new reporting interval\n" )
#endif // OPTION_MIPS_COUNTING

#if defined(FISH_HANG)
COMMAND ( "FishHangReport", FishHangReport_cmd, "(DEBUG) display thread/lock/event objects\n" )
#endif
#if defined(OPTION_CONFIG_SYMBOLS)
COMMAND ( "defsym",    defsym_cmd,    "Define symbol" )
#endif
COMMAND ( "script",    script_cmd,    "Run a sequence of panel commands contained in a file" )
COMMAND ( "cscript",   cscript_cmd,   "Cancels a running script thread\n" )
#if defined(FEATURE_ECPSVM)
COMMAND ( "evm",       evm_cmd_1,     "ECPS:VM Commands (Deprecated)" )
COMMAND ( "ecpsvm",    evm_cmd,       "ECPS:VM Commands\n" )
#endif

COMMAND ( "aea",       aea_cmd,       "Display AEA tables" )
COMMAND ( "aia",       aia_cmd,       "Display AIA fields" )
COMMAND ( "tlb",       tlb_cmd,       "Display TLB tables\n" )

#if defined(SIE_DEBUG_PERFMON)
COMMAND ( "spm",       spm_cmd,       "SIE performance monitor\n" )
#endif
#if defined(OPTION_COUNTING)
COMMAND ( "count",     count_cmd,     "Display/clear overall instruction count\n" )
#endif
COMMAND ( "sizeof",    sizeof_cmd,    "Display size of structures\n" )

COMMAND ( "suspend",   suspend_cmd,   "Suspend hercules" )
COMMAND ( "resume",    resume_cmd,    "Resume hercules\n" )

COMMAND ( "herclogo",    herclogo_cmd,    "Read a new hercules logo file\n" )

#define   TEST_CMD "$test"          // (hidden internal command)
COMMAND ( TEST_CMD, $test_cmd,        "(hidden internal command)" )

COMMAND ( NULL, NULL, NULL )         /* (end of table) */
};

///////////////////////////////////////////////////////////////////////
// Main panel command processing function...

int    cmd_argc;
char*  cmd_argv[MAX_ARGS];

int ProcessPanelCommand (char* pszCmdLine)
{
    CMDTAB*  pCmdTab         = NULL;
    char*    pszSaveCmdLine  = NULL;
    char*    cl              = NULL;
    int      rc              = -1;

    if (!pszCmdLine || !*pszCmdLine)
    {
        /* [enter key] by itself: start the CPU
           (ignore if not instruction stepping) */
        if (sysblk.inststep)
            rc = start_cmd(0,NULL,NULL);
        goto ProcessPanelCommandExit;
    }

#if defined(OPTION_CONFIG_SYMBOLS)
    /* Perform variable substitution */
    /* First, set some 'dynamic' symbols to their own values */
    set_symbol("CUU","$(CUU)");
    set_symbol("cuu","$(cuu)");
    set_symbol("CCUU","$(CCUU)");
    set_symbol("ccuu","$(ccuu)");
    cl=resolve_symbol_string(pszCmdLine);
#else
    cl=pszCmdLine;
#endif

    /* Save unmodified copy of the command line in case
       its format is unusual and needs customized parsing. */
    pszSaveCmdLine = strdup(cl);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args (cl, MAX_ARGS, cmd_argv, &cmd_argc);

    /* If no command was entered (i.e. they entered just a comment
       (e.g. "# comment")) then ignore their input */
    if ( !cmd_argv[0] )
        goto ProcessPanelCommandExit;

#if defined(OPTION_DYNAMIC_LOAD)
    if(system_command)
        if((rc = system_command(cmd_argc, (char**)cmd_argv, pszSaveCmdLine)))
            goto ProcessPanelCommandExit;
#endif

    /* Route standard formatted commands from our routing table... */
    if (cmd_argc)
        for (pCmdTab = Commands; pCmdTab->pszCommand; pCmdTab++)
        {
            if (!strcasecmp(cmd_argv[0], pCmdTab->pszCommand))
            {
                rc = pCmdTab->pfnCommand(cmd_argc, (char**)cmd_argv, pszSaveCmdLine);
                goto ProcessPanelCommandExit;
            }
        }

    /* Route non-standard formatted commands... */

    /* sf commands - shadow file add/remove/set/compress/display */
    if (0
        || !strncasecmp(pszSaveCmdLine,"sf+",3)
        || !strncasecmp(pszSaveCmdLine,"sf-",3)
        || !strncasecmp(pszSaveCmdLine,"sf=",3)
        || !strncasecmp(pszSaveCmdLine,"sfc",3)
        || !strncasecmp(pszSaveCmdLine,"sfd",3)
    )
    {
        rc = ShadowFile_cmd(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* x+ and x- commands - turn switches on or off */
    if ('+' == pszSaveCmdLine[1] || '-' == pszSaveCmdLine[1])
    {
        rc = OnOffCommand(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* Error: unknown/unsupported command... */
    ASSERT( cmd_argv[0] );
    logmsg( _("HHCPN139E Command \"%s\" not found; enter '?' for list.\n"),
              cmd_argv[0] );

ProcessPanelCommandExit:

    /* Free our saved copy */
    free(pszSaveCmdLine);

#if defined(OPTION_CONFIG_SYMBOLS)
    if (cl != pszCmdLine)
        free(cl);
#endif

    return rc;
}

///////////////////////////////////////////////////////////////////////
/* ? command - list all commands */

int ListAllCommands(int argc, char *argv[], char *cmdline)
{
    CMDTAB* pCmdTab;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg( _("HHCPN140I Valid panel commands are...\n\n") );
    logmsg( "  %-9.9s    %s \n", "Command", "Description..." );
    logmsg( "  %-9.9s    %s \n", "-------", "-----------------------------------------------" );

    /* List standard formatted commands from our routing table... */

    for (pCmdTab = Commands; pCmdTab->pszCommand; pCmdTab++)
    {
        // (don't display hidden internal commands)
        if ( strcasecmp( pCmdTab->pszCommand, TEST_CMD ) != 0 )
            logmsg( _("  %-9.9s    %s \n"), pCmdTab->pszCommand, pCmdTab->pszCmdDesc );
    }

    // List non-standard formatted commands...

    /* sf commands - shadow file add/remove/set/compress/display */

    logmsg( "  %-9.9s    %s \n", "sf+dev",    _("add shadow file") );
    logmsg( "  %-9.9s    %s \n", "sf-dev",    _("delete shadow file") );
    logmsg( "  %-9.9s    %s \n", "sf=dev ..", _("rename shadow file") );
    logmsg( "  %-9.9s    %s \n", "sfc",       _("compress shadow files") );
    logmsg( "  %-9.9s    %s \n", "sfd",       _("display shadow file stats") );

    logmsg("\n");

    /* x+ and x- commands - turn switches on or off */

    logmsg( "  %-9.9s    %s \n", "t{+/-}",    _("turn instruction tracing on/off") );
    logmsg( "  %-9.9s    %s \n", "s{+/-}",    _("turn instruction stepping on/off") );
    logmsg( "  %-9.9s    %s \n", "t{+/-}dev", _("turn CCW tracing on/off") );
    logmsg( "  %-9.9s    %s \n", "s{+/-}dev", _("turn CCW stepping on/off") );
#ifdef OPTION_CKD_KEY_TRACING
    logmsg( "  %-9.9s    %s \n", "t{+/-}CKD", _("turn CKD_KEY tracing on/off") );
#endif
    logmsg( "  %-9.9s    %s \n", "f{+/-}adr", _("mark frames unusable/usable") );

    return 0;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
// Layout of command help table...

typedef struct _HELPTAB
{
    const char* pszCommand;     /* command          */
    const char* pszCmdHelp;     /* help text        */
}
HELPTAB;

#define CMDHELP(cmd,help)  { cmd, help },

///////////////////////////////////////////////////////////////////////
// Define all help text here...

HELPTAB HelpTab[] =
{
/*        command         additional hep text...
        (max 9 chars)
*/
CMDHELP ( "*",         "The '*' comment command simply provides a convenient means\n"
                       "of entering comments into the console log without otherwise\n"
                       "doing anything. It is not processed in any way other than to\n"
                       "simply echo it to the console.\n"
                       )

CMDHELP ( "help",      "Enter \"help cmd\" where cmd is the command you need help\n"
                       "with. If the command has additional help text defined for it,\n"
                       "it will be displayed. Help text is usually limited to explaining\n"
                       "the format of the command and its various required or optional\n"
                       "parameters and is not meant to replace reading the documentation.\n"
                       )

#if defined( OPTION_W32_CTCI )
CMDHELP ( "tt32",      "Format:  \"tt32   debug | nodebug | stats <devnum>\".\n\n"

                       "Enables or disables global CTCI-W32 debug tracing\n"
                       "or displays TunTap32 stats for the specified CTC device.\n"
                       )
#endif /* defined( OPTION_W32_CTCI ) */

#if defined( OPTION_SCSI_TAPE )
CMDHELP ( "scsimount", "Format:    \"scsimount  [ no | yes | 0-99 ]\".\n\n"

                       "Displays or modifies the automatic SCSI tape mounts option.\n\n"

                       "When entered without any operands, it displays the current interval\n"
                       "and any pending tape mount requests. Entering 'no' (or 0 seconds)\n"
                       "disables automount detection.\n\n"

                       "Entering a value between 1-99 seconds (or 'yes') enables the option\n"
                       "and specifies how often to query SCSI tape drives to automatically\n"
                       "detect when a tape has been mounted (upon which an unsolicited\n"
                       "device-attention interrupt will be presented to the guest operating\n"
                       "system). 'yes' is equivalent to specifying a 5 second interval.\n"
                       )
#endif /* defined( OPTION_SCSI_TAPE ) */

CMDHELP ( "hst",       "Format: \"hst | hst n | hst l\". Command \"hst l\" or \"hst 0\" displays\n"
                       "list of last ten commands entered from command line\n"
                       "hst n, where n is a positive number retrieves n-th command from list\n"
                       "hst n, where n is a negative number retrieves n-th last command\n"
                       "hst without an argument works exactly as hst -1, it retrieves last command\n"
                       )
#if defined(OPTION_HAO)
// Hercules Automatic Operator
CMDHELP ( "hao",       "Format: \"hao  tgt <tgt> | cmd <cmd> | list <n> | del <n> | clear \".\n"
                       "  hao tgt <tgt> : define target rule (regex pattern) to react on\n"
                       "  hao cmd <cmd> : define command for previously defined rule\n"
                       "  hao list <n>  : list all rules/commands or only at index <n>\n"
                       "  hao del <n>   : delete the rule at index <n>\n"
                       "  hao clear     : delete all rules (stops automatic operator)\n"
                       )
#endif /* defined(OPTION_HAO) */

CMDHELP ( "cpu",       "Format: \"cpu hh\" where 'hh' is the hexadecimal cpu address of the cpu\n"
                       "in your multiprocessor configuration which you wish all panel commands\n"
                       "to apply to. For example, entering 'cpu 1F' followed by \"gpr\" will\n"
                       "display the general purpose registers for cpu 31 of your configuration.\n"
                       )

CMDHELP ( "cf",        "Configure current CPU online or offline:  Format->  \"cf [on|off]\"\n"
                       "Where the 'current' CPU is defined as whatever CPU was defined as\n"
                       "the panel command target cpu via the \"cpu\" panel command. (Refer\n"
                       "to the 'cpu' command for further information) Entering 'cf' by itself\n"
                       "simply displays the current online/offline status of the current cpu.\n"
                       "Otherwise the current cpu is configured online or offline as specified.\n"
                       "Use 'cfall' to configure/display all CPUs online/offline state.\n"
                       )

CMDHELP ( "start",     "Entering the 'start' command by itself simply starts a stopped\n"
                       "CPU, whereas 'start <devn>' presses the virtual start button on\n"
                       "printer device <devn>.\n"
                       )

CMDHELP ( "stop",      "Entering the 'stop' command by itself simply stops a running\n"
                       "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"
                       "printer device <devn>, usually causing an INTREQ.\n"
                       )

#ifdef _FEATURE_SYSTEM_CONSOLE
CMDHELP ( ".reply",    "To reply to a system control program (i.e. guest operating system)\n"
                       "message that gets issued to the hercules console, prefix the reply\n"
                       "with a period.\n"
                       )

CMDHELP ( "!message",  "To enter a system control program (i.e. guest operating system)\n"
                       "priority command on the hercules console, simply prefix the command\n"
                       "with an exclamation point '!'.\n"
                       )
#endif

CMDHELP ( "r",         "Format: \"r addr[.len]\" or \"r addr-addr\" to display real\n"
                       "storage, or \"r addr=value\" to alter real storage, where 'value'\n"
                       "is a hex string of up to 32 pairs of digits.\n"
                       )
CMDHELP ( "v",         "Format: \"v addr[.len]\" or \"v addr-addr\" to display virtual\n"
                       "storage, or \"v addr=value\" to alter virtual storage, where 'value'\n"
                       "is a hex string of up to 32 pairs of digits.\n"
                       )

CMDHELP ( "attach",    "Format: \"attach devn type [arg...]\n"
                       )

CMDHELP ( "define",    "Format: \"define olddevn newdevn\"\n"
                       )

CMDHELP ( "devinit",   "Format: \"devinit devn [arg...]\"\n"
                       "If no arguments are given then the same arguments are used\n"
                       "as were used the last time the device was created/initialized.\n"
                       )

CMDHELP ( "sh",        "Format: \"sh command [args...]\" where 'command' is any valid shell\n"
                       "command. The entered command and any arguments are passed as-is to the\n"
                       "shell for processing and the results are displayed on the console.\n"
                       )

CMDHELP ( "b",         "Format: \"b addr\" or \"b addr-addr\" where 'addr' is the instruction\n"
                       "address or range of addresses where you wish to halt execution. Once\n"
                       "the breakpoint is reached, instruction execution is temporarily halted\n"
                       "and the next instruction to be executed is displayed. You may then\n"
                       "examine registers and/or storage, etc. To continue execution after\n"
                       "reaching a breakpoint, enter the 'g' command.\n"
                       )

CMDHELP ( "b-",        "Format: \"b-\"  (removes any previously set breakpoint)\n"
                       )

CMDHELP ( "pgmtrace",  "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"
                       "interruption code in the range 0x01 to 0x40. Precede the interrupt code\n"
                       "with a '-' to stop tracing of that particular program interruption.\n"
                       )

CMDHELP ( "ostailor",  "Format: \"ostailor quiet | os/390 | z/os | vm | vse | linux | null\". Specifies\n"
                       "the intended operating system. The effect is to reduce control panel message\n"
                       "traffic by selectively suppressing program check trace messages which are\n"
                       "considered normal in the specified environment. 'quiet' suppresses all\n"
                       "exception messages, whereas 'null' suppresses none of them. The other options\n"
                       "suppress some messages and not others depending on the specified o/s. Prefix\n"
                       "values with '+' to combine them with existing values or '-' to exclude them.\n"
                       "SEE ALSO the 'pgmtrace' command which allows you to further fine tune\n"
                       "the tracing of program interrupt exceptions.\n"
                       )

CMDHELP ( "savecore",  "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and 'end'\n"
                       "define the starting and ending addresss of the range of real storage to be\n"
                       "saved to file 'filename'.  '*' for either the start address or end address\n"
                       "(the default) means: \"the first/last byte of the first/last modified page\n"
                       "as determined by the storage-key 'changed' bit\".\n"
                       )
CMDHELP ( "loadcore",  "Format: \"loadcore filename [address]\" where 'address' is the storage address\n"
                       "of where to begin loading memory. The file 'filename' is presumed to be a pure\n"
                       "binary image file previously created via the 'savecore' command. The default for\n"
                       "'address' is 0 (begining of storage).\n"
                       )
CMDHELP ( "loadtext",  "Format: \"loadtext filename [address]\". This command is essentially identical\n"
                       "to the 'loadcore' command except that it loads a text deck file with \"TXT\"\n"
                       "and \"END\" 80 byte records (i.e. an object deck).\n"
                       )

#if defined(OPTION_CONFIG_SYMBOLS)
CMDHELP ( "defsym",    "Format: \"defsym symbol [value]\". Defines symbol 'symbol' to contain value 'value'.\n"
                       "The symbol can then be the object of a substitution for later panel commands.\n"
                       "If 'value' contains blanks or spaces, then it should be enclosed within double\n"
                       "quotation marks (""). For more detailed information regarding symbol substitution\n"
                       "refer to the 'DEFSYM' configuration file statement in Hercules documentation.\n"
                       )
#endif

CMDHELP ( "script",    "Format: \"script filename [...filename...]\". Sequentially executes the commands contained\n"
                       "within the file -filename-. The script file may also contain \"script\" commands,\n"
                       "but the system ensures that no more than 10 levels of script are invoked at any\n"
                       "one time (to avoid a recursion loop)\n"
                       )

CMDHELP ( "cscript",   "Format: \"cscript\". This command will cancel the currently running script.\n"
                       "if no script is running, no action is taken\n"
                       )

CMDHELP ( "archmode",  "Format: \"archmode [S/370 | ESA/390 | z/Arch | ESAME]\". Entering the command\n"
                       "without any argument simply displays the current architecture mode. Entering\n"
                       "the command with an argument sets the architecture mode to the specified value.\n"
                       "Note: \"ESAME\" (Enterprise System Architecture, Modal Extensions) is simply a\n"
                       "synonym for \"z/Arch\". (they are identical to each other and mean the same thing)\n"
                       )

#if defined(FEATURE_ECPSVM)
CMDHELP ( "ecpsvm",   "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"
                       "Type \"ecpsvm help\" to see a list of available commands\n"
                       )
CMDHELP ( "evm",      "Format: \"evm\". This command is deprecated.\n"
                       "use \"ecpsvm\" instead\n"
                       )
#endif

CMDHELP ( "herclogo",  "Format: \"herclogo [<filename>]\". Load a new logo file for 3270 terminal sessions\n"
                       "If no filename is specified, the built-in logo is used instead\n"
                       )

#if defined(FISH_HANG)
CMDHELP ( "FishHangReport", "When built with --enable-fthreads --enable-fishhang, a detailed record of\n"
                       "every thread, lock and event that is created is maintained for debugging purposes.\n"
                       "If a lock is accessed before it has been initialized or if a thread exits while\n"
                       "still holding a lock, etc (including deadlock situations), the FishHang logic will\n"
                       "detect and report it. If you suspect one of hercules's threads is hung waiting for\n"
                       "a condition to be signalled for example, entering \"FishHangReport\" will display\n"
                       "the internal list of thread, locks and events to possibly help you determine where\n"
                       "it's hanging and what event (condition) it's hung on.\n"
                       )
#endif

#ifdef OPTION_MIPS_COUNTING
CMDHELP ( "maxrates",  "Format: \"maxrates [nnnn]\" where 'nnnn' is the desired reporting\n"
                       " interval in minutes. Acceptable values are from 1 to 1440. The default\n"
                       " is 1440 minutes (one day). Entering \"maxrates\" by itself displays\n"
                       " the current highest rates observed during the defined intervals.\n"
                       )
#endif // OPTION_MIPS_COUNTING

CMDHELP ( NULL, NULL )         /* (end of table) */
};

///////////////////////////////////////////////////////////////////////
/* help command - display additional help for a given command */

int HelpCommand(int argc, char *argv[], char *cmdline)
{
    HELPTAB* pHelpTab;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN141E Missing argument\n") );
        return -1;
    }

    for (pHelpTab = HelpTab; pHelpTab->pszCommand; pHelpTab++)
    {
        if (!strcasecmp(pHelpTab->pszCommand,argv[1]))
        {
            logmsg( _("%s"),pHelpTab->pszCmdHelp );
            return 0;
        }
    }

    logmsg( _("HHCPN142I No additional help available.\n") );
    return -1;
}

///////////////////////////////////////////////////////////////////////

#if defined(OPTION_DYNAMIC_LOAD)
DLL_EXPORT void *panel_command_r (void *cmdline)
#else
void *panel_command (void *cmdline)
#endif
{
#define MAX_CMD_LEN (32768)
    char  cmd[MAX_CMD_LEN];             /* Copy of panel command     */
    char *pCmdLine;
    unsigned i;

    pCmdLine = cmdline; ASSERT(pCmdLine);
    /* every command will be stored in history list */
    history_add(cmdline);

    /* Copy panel command to work area, skipping leading blanks */
    while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
    i = 0;
    while (*pCmdLine && i < (MAX_CMD_LEN-1))
    {
        cmd[i] = *pCmdLine;
        i++;
        pCmdLine++;
    }
    cmd[i] = 0;

    /* Ignore null commands (just pressing enter)
       unless instruction tracing is enabled. */
    if (!sysblk.inststep && 0 == cmd[0])
        return NULL;

    /* Echo the command to the control panel */
    logmsg( "%s\n", cmd);

#ifdef _FEATURE_SYSTEM_CONSOLE
    if ('.' == cmd[0] || '!' == cmd[0])
    {
        if (!cmd[1]) { cmd[1]=' '; cmd[2]=0; }
        scp_command (cmd+1, cmd[0] == '!');
        return NULL;
    }
#endif /*_FEATURE_SYSTEM_CONSOLE*/

    ProcessPanelCommand(cmd);
    return NULL;
}

///////////////////////////////////////////////////////////////////////
