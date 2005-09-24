/* HSCMISC.C    Misc. system command routines                        */
/*                                                                   */
/*              (c) Copyright Roger Bowler, 1999-2005                */
/*              (c) Copyright Jan Jaeger, 1999-2005                  */

#include "hstdinc.h"

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "inline.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

#if !defined(_HSCMISC_C)
#define _HSCMISC_C

/*-------------------------------------------------------------------*/
/*                System Shutdown Processing                         */
/*-------------------------------------------------------------------*/

/* The following 'sigq' functions are responsible for ensuring all of
   the CPUs are stopped ("quiesced") before continuing with Hercules
   shutdown processing, and should NEVER be called directly. Instead,
   they are called by the main 'do_shutdown' (or 'do_shutdown_wait')
   function(s) (defined further below) as needed and/or appropriate.
*/
static int wait_sigq_pending = 0;

static int is_wait_sigq_pending()
{
int pending;

    obtain_lock(&sysblk.intlock);
    pending = wait_sigq_pending;
    release_lock(&sysblk.intlock);

    return pending;
}

static void wait_sigq_resp()
{
int pending, i;
    /* Wait for all CPU's to stop */
    do
    {
        obtain_lock(&sysblk.intlock);
        wait_sigq_pending = 0;
        for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
          && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
            wait_sigq_pending = 1;
        pending = wait_sigq_pending;
        release_lock(&sysblk.intlock);

        if(pending)
            SLEEP(1);
    }
    while(is_wait_sigq_pending());
}

static void cancel_wait_sigq()
{
    obtain_lock(&sysblk.intlock);
    wait_sigq_pending = 0;
    release_lock(&sysblk.intlock);
}

/*                       do_shutdown_now

   This is the main shutdown processing function. It is NEVER called
   directly, but is instead ONLY called by either the 'do_shutdown'
   or 'do_shutdown_wait' functions after all CPUs have been stopped.

   It is responsible for releasing the device configuration and then
   calling the Hercules Dynamic Loader "hdl_shut" function to invoke
   all registered "Hercules at-exit/termination functions" (similar
   to 'atexit' but unique to Hercules) (to perform any other needed
   miscellaneous shutdown related processing).

   Only after the above three tasks have been completed (stopping the
   CPUs, releasing the device configuration, calling registered term-
   ination routines/functions) can Hercules then be safely terminated.

   Note too that, *technically*, this function *should* wait for *all*
   other threads to finish terminating first before either exiting or
   returning back to the caller, but we don't currently enforce that
   (since that's *really* what hdl_adsc + hdl_shut is designed for!).

   At the moment, as long as the three previously mentioned three most
   important shutdown tasks have been completed (stop cpus, release
   device config, call term funcs), then we consider the brunt of our
   shutdown processing to be completed and thus exit (or return back
   to the caller to let them exit instead). If there happen to be any
   threads still running when that happens, they will be automatically
   terminated by the operating sytem as normal when a process exits.

   SO... If there are any threads that must be terminated completely
   and cleanly before Hercules can safely terminate, then you better
   add code to this function to ENSURE that your thread is terminated
   properly! (and/or add a call to 'hdl_adsc' at the appropriate place
   in the startup sequence). For this purpose, the use of "join_thread"
   is *strongly* encouraged as it *ensures* that your thread will not
   continue until the thread in question has completely exited first.
*/
static void do_shutdown_now()
{
    logmsg("HHCIN900I Begin Hercules shutdown\n");

    ASSERT( !sysblk.shutfini );  // (sanity check)

    sysblk.shutfini = 0;  // (shutdown NOT finished yet)
    sysblk.shutdown = 1;  // (system shutdown initiated)

    logmsg("HHCIN901I Releasing configuration\n");
    {
        release_config();
    }
    logmsg("HHCIN902I Configuration release complete\n");

    logmsg("HHCIN903I Calling termination routines\n");
    {
        hdl_shut();
    }
    logmsg("HHCIN904I All termination routines complete\n");

    /*
    logmsg("HHCIN905I Terminating threads\n");
    {
        // (none we really care about at the moment...)
    }
    logmsg("HHCIN906I Threads terminations complete\n");
    */

    logmsg("HHCIN909I Hercules shutdown complete\n");
    sysblk.shutfini = 1;    // (shutdown is now complete)

    //                     PROGRAMMING NOTE

    // If we're NOT in "daemon_mode" (i.e. panel_display in control),
    // -OR- if a daemon_task DOES exist, then THEY are in control of
    // shutdown; THEY are responsible for exiting the system whenever
    // THEY feel it's proper to do so (by simply returning back to the
    // caller thereby allowing 'main' to return back to the operating
    // system).

    // OTHEWRWISE we ARE in "daemon_mode", but a daemon_task does NOT
    // exist, which means the main thread (tail end of 'impl.c') is
    // stuck in a loop reading log messages and writing them to the
    // logfile, so we need to do the exiting here since it obviously
    // cannot.

    if ( sysblk.daemon_mode
#if defined(OPTION_DYNAMIC_LOAD)
         && !daemon_task
#endif /*defined(OPTION_DYNAMIC_LOAD)*/
       )
    {
#if defined(FISH_HANG)
        FishHangAtExit();
#endif
#ifdef _MSVC_
        socket_deinit();
#endif
#ifdef DEBUG
        fprintf(stdout, _("DO_SHUTDOWN_NOW EXIT\n"));
#endif
        fprintf(stdout, _("HHCIN099I Hercules terminated\n"));
        fflush(stdout);
        exit(0);
    }
}

/*                     do_shutdown_wait

   This function simply waits for the CPUs to stop and then calls
   the above do_shutdown_now function to perform the actual shutdown
   (which releases the device configuration, etc)
*/
static void do_shutdown_wait()
{
    logmsg(_("HHCIN098I Shutdown initiated\n"));
    wait_sigq_resp();
    do_shutdown_now();
}

/*                 *****  do_shutdown  *****

   This is the main system shutdown function, and the ONLY function
   that should EVER be called to shut the system down. It calls one
   or more of the above static helper functions as needed.
*/
void do_shutdown()
{
TID tid;
    if(is_wait_sigq_pending())
        cancel_wait_sigq();
    else
        if(can_signal_quiesce() && !signal_quiesce(0,0))
            create_thread(&tid, &sysblk.detattr, do_shutdown_wait, NULL);
        else
            do_shutdown_now();
}

/*-------------------------------------------------------------------*/
/* Display general purpose registers                                 */
/*-------------------------------------------------------------------*/
void display_regs (REGS *regs)
{
int     i;

    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            logmsg ("GR%2.2d=%8.8"I32_FMT"X%s", i, regs->GR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            logmsg ("R%1.1X=%16.16"I64_FMT"X%s", i, regs->GR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");

} /* end function display_regs */


/*-------------------------------------------------------------------*/
/* Display control registers                                         */
/*-------------------------------------------------------------------*/
void display_cregs (REGS *regs)
{
int     i;

    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            logmsg ("CR%2.2d=%8.8"I32_FMT"X%s", i, regs->CR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            logmsg ("C%1.1X=%16.16"I64_FMT"X%s", i, regs->CR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");

} /* end function display_cregs */


/*-------------------------------------------------------------------*/
/* Display access registers                                          */
/*-------------------------------------------------------------------*/
void display_aregs (REGS *regs)
{
int     i;

    for (i = 0; i < 16; i++)
        logmsg ("AR%2.2d=%8.8"I32_FMT"X%s", i, regs->AR(i),
            ((i & 0x03) == 0x03) ? "\n" : "\t");

} /* end function display_aregs */


/*-------------------------------------------------------------------*/
/* Display floating point registers                                  */
/*-------------------------------------------------------------------*/
void display_fregs (REGS *regs)
{

    if(regs->CR(0) & CR0_AFP)

    logmsg ("FPR0=%8.8X %8.8X\t\tFPR1=%8.8X %8.8X\n"
            "FPR2=%8.8X %8.8X\t\tFPR3=%8.8X %8.8X\n"
            "FPR4=%8.8X %8.8X\t\tFPR5=%8.8X %8.8X\n"
            "FPR6=%8.8X %8.8X\t\tFPR7=%8.8X %8.8X\n"
            "FPR8=%8.8X %8.8X\t\tFPR9=%8.8X %8.8X\n"
            "FPRa=%8.8X %8.8X\t\tFPRb=%8.8X %8.8X\n"
            "FPRc=%8.8X %8.8X\t\tFPRd=%8.8X %8.8X\n"
            "FPRe=%8.8X %8.8X\t\tFPRf=%8.8X %8.8X\n",
            regs->fpr[0], regs->fpr[1], regs->fpr[2], regs->fpr[3],
            regs->fpr[4], regs->fpr[5], regs->fpr[6], regs->fpr[7],
            regs->fpr[8], regs->fpr[9], regs->fpr[10], regs->fpr[11],
            regs->fpr[12], regs->fpr[13], regs->fpr[14], regs->fpr[15],
            regs->fpr[16], regs->fpr[17], regs->fpr[18], regs->fpr[19],
            regs->fpr[20], regs->fpr[21], regs->fpr[22], regs->fpr[23],
            regs->fpr[24], regs->fpr[25], regs->fpr[26], regs->fpr[27],
            regs->fpr[28], regs->fpr[29], regs->fpr[30], regs->fpr[31]);
    else

    logmsg ("FPR0=%8.8X %8.8X\t\tFPR2=%8.8X %8.8X\n"
            "FPR4=%8.8X %8.8X\t\tFPR6=%8.8X %8.8X\n",
            regs->fpr[0], regs->fpr[1], regs->fpr[2], regs->fpr[3],
            regs->fpr[4], regs->fpr[5], regs->fpr[6], regs->fpr[7]);

} /* end function display_fregs */


/*-------------------------------------------------------------------*/
/* Display subchannel                                                */
/*-------------------------------------------------------------------*/
void display_subchannel (DEVBLK *dev)
{
    logmsg ("%4.4X:D/T=%4.4X",
            dev->devnum, dev->devtype);
    if (ARCH_370 == sysblk.arch_mode)
    {
        logmsg (" CSW=Flags:%2.2X CCW:%2.2X%2.2X%2.2X "
                "Stat:%2.2X%2.2X Count:%2.2X%2.2X\n",
                dev->csw[0], dev->csw[1], dev->csw[2], dev->csw[3],
                dev->csw[4], dev->csw[5], dev->csw[6], dev->csw[7]);
    } else {
        logmsg (" Subchannel_Number=%4.4X\n", dev->subchan);
        logmsg ("     PMCW=IntParm:%2.2X%2.2X%2.2X%2.2X Flags:%2.2X%2.2X"
                " Dev:%2.2X%2.2X"
                " LPM:%2.2X PNOM:%2.2X LPUM:%2.2X PIM:%2.2X\n"
                "          MBI:%2.2X%2.2X POM:%2.2X PAM:%2.2X"
                " CHPIDs:%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                " Misc:%2.2X%2.2X%2.2X%2.2X\n",
                dev->pmcw.intparm[0], dev->pmcw.intparm[1],
                dev->pmcw.intparm[2], dev->pmcw.intparm[3],
                dev->pmcw.flag4, dev->pmcw.flag5,
                dev->pmcw.devnum[0], dev->pmcw.devnum[1],
                dev->pmcw.lpm, dev->pmcw.pnom, dev->pmcw.lpum, dev->pmcw.pim,
                dev->pmcw.mbi[0], dev->pmcw.mbi[1],
                dev->pmcw.pom, dev->pmcw.pam,
                dev->pmcw.chpid[0], dev->pmcw.chpid[1],
                dev->pmcw.chpid[2], dev->pmcw.chpid[3],
                dev->pmcw.chpid[4], dev->pmcw.chpid[5],
                dev->pmcw.chpid[6], dev->pmcw.chpid[7],
                dev->pmcw.zone, dev->pmcw.flag25,
                dev->pmcw.flag26, dev->pmcw.flag27);

        logmsg ("     SCSW=Flags:%2.2X%2.2X SCHC:%2.2X%2.2X "
                "Stat:%2.2X%2.2X Count:%2.2X%2.2X "
                "CCW:%2.2X%2.2X%2.2X%2.2X\n",
                dev->scsw.flag0, dev->scsw.flag1,
                dev->scsw.flag2, dev->scsw.flag3,
                dev->scsw.unitstat, dev->scsw.chanstat,
                dev->scsw.count[0], dev->scsw.count[1],
                dev->scsw.ccwaddr[0], dev->scsw.ccwaddr[1],
                dev->scsw.ccwaddr[2], dev->scsw.ccwaddr[3]);
    }

} /* end function display_subchannel */


/*-------------------------------------------------------------------*/
/* Parse a storage range or storage alteration operand               */
/*                                                                   */
/* Valid formats for a storage range operand are:                    */
/*      startaddr                                                    */
/*      startaddr-endaddr                                            */
/*      startaddr.length                                             */
/* where startaddr, endaddr, and length are hexadecimal values.      */
/*                                                                   */
/* Valid format for a storage alteration operand is:                 */
/*      startaddr=hexstring (up to 32 pairs of digits)               */
/*                                                                   */
/* Return values:                                                    */
/*      0  = operand contains valid storage range display syntax;    */
/*           start/end of range is returned in saddr and eaddr       */
/*      >0 = operand contains valid storage alteration syntax;       */
/*           return value is number of bytes to be altered;          */
/*           start/end/value are returned in saddr, eaddr, newval    */
/*      -1 = error message issued                                    */
/*-------------------------------------------------------------------*/
static int parse_range (char *operand, U64 maxadr, U64 *sadrp,
                        U64 *eadrp, BYTE *newval)
{
U64     opnd1, opnd2;                   /* Address/length operands   */
U64     saddr, eaddr;                   /* Range start/end addresses */
int     rc;                             /* Return code               */
int     n;                              /* Number of bytes altered   */
int     h1, h2;                         /* Hexadecimal digits        */
char    *s;                             /* Alteration value pointer  */
BYTE    delim;                          /* Operand delimiter         */
BYTE    c;                              /* Character work area       */

    rc = sscanf(operand, "%"I64_FMT"x%c%"I64_FMT"x%c",
                &opnd1, &delim, &opnd2, &c);

    /* Process storage alteration operand */
    if (rc > 2 && delim == '=' && newval)
    {
        s = strchr (operand, '=');
        for (n = 0; n < 32;)
        {
            h1 = *(++s);
            if (h1 == '\0'  || h1 == '#' ) break;
            if (h1 == SPACE || h1 == '\t') continue;
            h1 = toupper(h1);
            h2 = *(++s);
            h2 = toupper(h2);
            h1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            h2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (h1 < 0 || h2 < 0 || n >= 32)
            {
                logmsg (_("HHCPN143E Invalid value: %s\n"), s);
                return -1;
            }
            newval[n++] = (h1 << 4) | h2;
        } /* end for(n) */
        saddr = opnd1;
        eaddr = saddr + n - 1;
    }
    else
    {
        /* Process storage range operand */
        saddr = opnd1;
        if (rc == 1)
        {
            /* If only starting address is specified, default to
               64 byte display, or less if near end of storage */
            eaddr = saddr + 0x3F;
            if (eaddr > maxadr) eaddr = maxadr;
        }
        else
        {
            /* Ending address or length is specified */
            if (rc != 3 || !(delim == '-' || delim == '.'))
            {
                logmsg (_("HHCPN144E Invalid operand: %s\n"), operand);
                return -1;
            }
            eaddr = (delim == '.') ? saddr + opnd2 - 1 : opnd2;
        }
        /* Set n=0 to indicate storage display only */
        n = 0;
    }

    /* Check for valid range */
    if (saddr > maxadr || eaddr > maxadr || eaddr < saddr)
    {
        logmsg (_("HHCPN145E Invalid range: %s\n"), operand);
        return -1;
    }

    /* Return start/end addresses and number of bytes altered */
    *sadrp = saddr;
    *eadrp = eaddr;
    return n;

} /* end function parse_range */


/*-------------------------------------------------------------------*/
/* get_connected_client   return IP address and hostname of the      */
/*                        client that is connected to this device    */
/*-------------------------------------------------------------------*/
void get_connected_client (DEVBLK* dev, char** pclientip, char** pclientname)
{
    *pclientip   = NULL;
    *pclientname = NULL;

    obtain_lock (&dev->lock);

    if (dev->bs             /* if device is a socket device,   */
        && dev->fd != -1)   /* and a client is connected to it */
    {
        *pclientip   = strdup(dev->bs->clientip);
        *pclientname = strdup(dev->bs->clientname);
    }

    release_lock (&dev->lock);
}

#endif /*!defined(_HSCMISC_C)*/


/*-------------------------------------------------------------------*/
/* Convert virtual address to absolute address                       */
/*                                                                   */
/* Input:                                                            */
/*      vaddr   Virtual address to be translated                     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*      acctype Type of access (ACCTYPE_INSTFETCH, ACCTYPE_READ,     */
/*              or ACCTYPE_LRA)                                      */
/* Output:                                                           */
/*      aaptr   Points to word in which abs address is returned      */
/*      siptr   Points to word to receive indication of which        */
/*              STD or ASCE was used to perform the translation      */
/* Return value:                                                     */
/*      0=translation successful, non-zero=exception code            */
/* Note:                                                             */
/*      To avoid unwanted alteration of the CPU register context     */
/*      during translation (for example, the TEA will be updated     */
/*      if a translation exception occurs), the translation is       */
/*      performed using a temporary copy of the CPU registers.       */
/*-------------------------------------------------------------------*/
static U16 ARCH_DEP(virt_to_abs) (RADR *raptr, int *siptr,
                        VADR vaddr, int arn, REGS *regs, int acctype)
{
RADR    raddr;
int     icode;
REGS    gregs, hgregs;

    // ZZFIXME:  Win32 builds emits bad code here
    //           so we have the next stmt:
    //           (is that still true??)
    if (!regs) return 0;

    gregs = *regs;
    gregs.ghostregs = 1;

    if(SIE_MODE(&gregs))
    {
        hgregs = *gregs.hostregs;
        gregs.hostregs = &hgregs;
        hgregs.guestregs = &gregs;
    }

    hgregs.ghostregs = 1;

    if( !(icode = setjmp(gregs.progjmp)) )
    {
        memcpy(&hgregs.progjmp,&gregs.progjmp,sizeof(jmp_buf));
        ARCH_DEP(logical_to_main) (vaddr, arn, &gregs, acctype, 0);
        raddr = SIE_MODE(&gregs) ? hgregs.dat.raddr : gregs.dat.raddr;
    }
    else
        return icode;

    *siptr = gregs.dat.stid;
    *raptr = raddr;

    return 0;

} /* end function virt_to_abs */


/*-------------------------------------------------------------------*/
/* Display real storage (up to 16 bytes, or until end of page)       */
/* Prefixes display by Rxxxxx: if draflag is 1                       */
/* Returns number of characters placed in display buffer             */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(display_real) (REGS *regs, RADR raddr, char *buf,
                                    int draflag)
{
RADR    aaddr;                          /* Absolute storage address  */
int     i, j;                           /* Loop counters             */
int     n = 0;                          /* Number of bytes in buffer */
char    hbuf[40];                       /* Hexadecimal buffer        */
BYTE    cbuf[17];                       /* Character buffer          */
BYTE    c;                              /* Character work area       */

    if (draflag)
    {
        n = sprintf (buf, "R:"F_RADR":", raddr);
    }

    aaddr = APPLY_PREFIXING (raddr, regs->PX);
    if (aaddr > regs->mainlim)
    {
        n += sprintf (buf+n, " Real address is not valid");
        return n;
    }

    n += sprintf (buf+n, "K:%2.2X=", STORAGE_KEY(aaddr, regs));

    memset (hbuf, SPACE, sizeof(hbuf));
    memset (cbuf, SPACE, sizeof(cbuf));

    for (i = 0, j = 0; i < 16; i++)
    {
        c = regs->mainstor[aaddr++];
        j += sprintf (hbuf+j, "%2.2X", c);
        if ((aaddr & 0x3) == 0x0) hbuf[j++] = SPACE;
        c = guest_to_host(c);
        if (!isprint(c)) c = '.';
        cbuf[i] = c;
        if ((aaddr & PAGEFRAME_BYTEMASK) == 0x000) break;
    } /* end for(i) */

    n += sprintf (buf+n, "%36.36s %16.16s", hbuf, cbuf);
    return n;

} /* end function display_real */


/*-------------------------------------------------------------------*/
/* Display virtual storage (up to 16 bytes, or until end of page)    */
/* Returns number of characters placed in display buffer             */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(display_virt) (REGS *regs, VADR vaddr, char *buf,
                                    int ar, int acctype)
{
RADR    raddr;                          /* Real address              */
int     n;                              /* Number of bytes in buffer */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */

    n = sprintf (buf, "%c:"F_VADR":", ar == USE_REAL_ADDR ? 'R' : 'V',
                             vaddr);
    xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid,
                                    vaddr, ar, regs, acctype);
    if (xcode == 0)
    {
        n += ARCH_DEP(display_real) (regs, raddr, buf+n, 0);
    }
    else
        n += sprintf (buf+n," Translation exception %4.4hX",xcode);

    return n;

} /* end function display_virt */


/*-------------------------------------------------------------------*/
/* Disassemble real                                                  */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(disasm_stor) (REGS *regs, char *opnd)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest real storage addr */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     stid = -1;
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
int     ilc;
BYTE    inst[6];                        /* Storage alteration value  */
BYTE    opcode;
U16     xcode;
char    type;

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    while((opnd && *opnd != '\0') &&
      (*opnd == ' ' || *opnd == '\t'))
        opnd++;

    if(REAL_MODE(&regs->psw))
        type = 'R';
    else
        type = 'V';

    switch(toupper(*opnd)) {
        case 'R': /* real */
        case 'V': /* virtual */
        case 'P': /* primary */
        case 'H': /* home */
          type = toupper(*opnd);
          opnd++;
    }

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, NULL);
    if (len < 0) return;

    /* Display real storage */
    for (i = 0; i < 999 && saddr <= eaddr; i++)
    {

        if(type == 'R')
            raddr = saddr;
        else
        {
            if((xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid, saddr, 0, regs, ACCTYPE_INSTFETCH) ))
            {
                logmsg(_("Storage not accessible code = %4.4X\n"), xcode);
                return;
            }
        }

        aaddr = APPLY_PREFIXING (raddr, regs->PX);
        if (aaddr > regs->mainlim)
        {
            logmsg(_("Adressing exception\n"));
            return;
        }

        opcode = regs->mainstor[aaddr];
        ilc = ILC(opcode);

        if (aaddr + ilc > regs->mainlim)
        {
            logmsg(_("Adressing exception\n"));
            return;
        }

        memcpy(inst, regs->mainstor + aaddr, ilc);
        logmsg("%c" F_RADR ": %2.2X%2.2X",
          stid == TEA_ST_PRIMARY ? 'P' :
          stid == TEA_ST_HOME ? 'H' :
          stid == TEA_ST_SECNDRY ? 'S' : 'R',
          raddr, inst[0], inst[1]);
        if(ilc > 2)
        {
            logmsg("%2.2X%2.2X", inst[2], inst[3]);
            if(ilc > 4)
                logmsg("%2.2X%2.2X ", inst[4], inst[5]);
            else
                logmsg("     ");
        }
        else
            logmsg("         ");
        DISASM_INSTRUCTION(inst);
        saddr += ilc;
    } /* end for(i) */

} /* end function disasm_stor */


/*-------------------------------------------------------------------*/
/* Process real storage alter/display command                        */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_real) (char *opnd, REGS *regs)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest real storage addr */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
BYTE    newval[32];                     /* Storage alteration value  */
char    buf[100];                       /* Message buffer            */

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, newval);
    if (len < 0) return;
    raddr = saddr;

    /* Alter real storage */
    if (len > 0)
    {
        for (i = 0; i < len && raddr+i <= regs->mainlim; i++)
        {
            aaddr = raddr + i;
            aaddr = APPLY_PREFIXING (aaddr, regs->PX);
            regs->mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */
    }

    /* Display real storage */
    for (i = 0; i < 999 && raddr <= eaddr; i++)
    {
        ARCH_DEP(display_real) (regs, raddr, buf, 1);
        logmsg ("%s\n", buf);
        raddr += 16;
    } /* end for(i) */

} /* end function alter_display_real */


/*-------------------------------------------------------------------*/
/* Process virtual storage alter/display command                     */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_virt) (char *opnd, REGS *regs)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest virt storage addr */
VADR    vaddr;                          /* Virtual storage address   */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     stid;                           /* Segment table indication  */
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
int     n;                              /* Number of bytes in buffer */
int     arn = 0;                        /* Access register number    */
U16     xcode;                          /* Exception code            */
BYTE    newval[32];                     /* Storage alteration value  */
char    buf[100];                       /* Message buffer            */

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, newval);
    if (len < 0) return;
    vaddr = saddr;

    /* Alter virtual storage */
    if (len > 0
        && ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr, arn,
                                   regs, ACCTYPE_LRA) == 0
        && ARCH_DEP(virt_to_abs) (&raddr, &stid, eaddr, arn,
                                   regs, ACCTYPE_LRA) == 0)
    {
        for (i = 0; i < len && raddr+i <= regs->mainlim; i++)
        {
            ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr+i, arn,
                                    regs, ACCTYPE_LRA);
            aaddr = APPLY_PREFIXING (raddr, regs->PX);
            regs->mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */
    }

    /* Display virtual storage */
    for (i = 0; i < 999 && vaddr <= eaddr; i++)
    {
        if (i == 0 || (vaddr & PAGEFRAME_BYTEMASK) < 16)
        {
            xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr, arn,
                                            regs, ACCTYPE_LRA);
            n = sprintf (buf, "V:"F_VADR" ", vaddr);
            if (stid == TEA_ST_PRIMARY)
                n += sprintf (buf+n, "(primary)");
            else if (stid == TEA_ST_SECNDRY)
                n += sprintf (buf+n, "(secondary)");
            else if (stid == TEA_ST_HOME)
                n += sprintf (buf+n, "(home)");
            else
                n += sprintf (buf+n, "(AR%2.2d)", arn);
            if (xcode == 0)
                n += sprintf (buf+n, " R:"F_RADR, raddr);
            logmsg ("%s\n", buf);
        }
        ARCH_DEP(display_virt) (regs, vaddr, buf, arn, ACCTYPE_LRA);
        logmsg ("%s\n", buf);
        vaddr += 16;
    } /* end for(i) */

} /* end function alter_display_virt */


/*-------------------------------------------------------------------*/
/* Display instruction                                               */
/*-------------------------------------------------------------------*/
void ARCH_DEP(display_inst) (REGS *regs, BYTE *inst)
{
QWORD   qword;                          /* Doubleword work area      */
BYTE    opcode;                         /* Instruction operation code*/
int     ilc;                            /* Instruction length        */
#ifdef DISPLAY_INSTRUCTION_OPERANDS
int     b1=-1, b2=-1, x1;               /* Register numbers          */
VADR    addr1 = 0, addr2 = 0;           /* Operand addresses         */
#endif /*DISPLAY_INSTRUCTION_OPERANDS*/
char    buf[100];                       /* Message buffer            */
int     n;                              /* Number of bytes in buffer */

  #if defined(_FEATURE_SIE)
    if(SIE_MODE(regs))
        logmsg(_("SIE: "));
  #endif /*defined(_FEATURE_SIE)*/

#if 0
#if _GEN_ARCH == 370
    logmsg("S/370 ");
#elif _GEN_ARCH == 390
    logmsg("ESA/390 ");
#else
    logmsg("Z/Arch ");
#endif
#endif

    /* Display the PSW */
    memset (qword, 0x00, sizeof(qword));
    copy_psw (regs, qword);
    n = sprintf (buf,
                "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X ",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
  #if defined(FEATURE_ESAME)
        n += sprintf (buf + n,
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X ",
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
  #endif /*defined(FEATURE_ESAME)*/

    /* Exit if instruction is not valid */
    if (inst == NULL)
    {
        logmsg (_("%sInstruction fetch error\n"), buf);
        display_regs (regs);
        return;
    }

    /* Extract the opcode and determine the instruction length */
    opcode = inst[0];
    ilc = ILC(opcode);

    /* Display the instruction */
    n += sprintf (buf+n, "INST=%2.2X%2.2X", inst[0], inst[1]);
    if (ilc > 2) n += sprintf (buf+n, "%2.2X%2.2X", inst[2], inst[3]);
    if (ilc > 4) n += sprintf (buf+n, "%2.2X%2.2X", inst[4], inst[5]);
    logmsg ("%s %s", buf,(ilc<4) ? "        " : (ilc<6) ? "    " : "");
    DISASM_INSTRUCTION(inst);


#ifdef DISPLAY_INSTRUCTION_OPERANDS

    /* Process the first storage operand */
    if (ilc > 2
        && opcode != 0x84 && opcode != 0x85
        && opcode != 0xA5 && opcode != 0xA7
        && opcode != 0xC0 && opcode != 0xEC)
    {
        /* Calculate the effective address of the first operand */
        b1 = inst[2] >> 4;
        addr1 = ((inst[2] & 0x0F) << 8) | inst[3];
        if (b1 != 0)
        {
            addr1 += regs->GR(b1);
            addr1 &= ADDRESS_MAXWRAP(regs);
        }

        /* Apply indexing for RX/RXE/RXF instructions */
        if ((opcode >= 0x40 && opcode <= 0x7F) || opcode == 0xB1
            || opcode == 0xE3 || opcode == 0xED)
        {
            x1 = inst[1] & 0x0F;
            if (x1 != 0)
            {
                addr1 += regs->GR(x1);
                addr1 &= ADDRESS_MAXWRAP(regs);
            }
        }
    }

    /* Process the second storage operand */
    if (ilc > 4
        && opcode != 0xC0 && opcode != 0xE3 && opcode != 0xEB
        && opcode != 0xEC && opcode != 0xED)
    {
        /* Calculate the effective address of the second operand */
        b2 = inst[4] >> 4;
        addr2 = ((inst[4] & 0x0F) << 8) | inst[5];
        if (b2 != 0)
        {
            addr2 += regs->GR(b2);
            addr2 &= ADDRESS_MAXWRAP(regs);
        }
    }

    /* Calculate the operand addresses for MVCL(E) and CLCL(E) */
    if (opcode == 0x0E || opcode == 0x0F
        || opcode == 0xA8 || opcode == 0xA9)
    {
        b1 = inst[1] >> 4;
        addr1 = regs->GR(b1) & ADDRESS_MAXWRAP(regs);
        b2 = inst[1] & 0x0F;
        addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
    }

    /* Calculate the operand addresses for RRE instructions */
    if ((opcode == 0xB2 &&
            ((inst[1] >= 0x20 && inst[1] <= 0x2F)
            || (inst[1] >= 0x40 && inst[1] <= 0x6F)
            || (inst[1] >= 0xA0 && inst[1] <= 0xAF)))
        || opcode == 0xB3
        || opcode == 0xB9)
    {
        b1 = inst[3] >> 4;
        addr1 = regs->GR(b1) & ADDRESS_MAXWRAP(regs);
        b2 = inst[3] & 0x0F;
        if (inst[1] >= 0x29 && inst[1] <= 0x2C)
            addr2 = regs->GR(b2) & ADDRESS_MAXWRAP_E(regs);
        else
        if (inst[1] >= 0x29 && inst[1] <= 0x2C)
            addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
        else
        addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
    }

    /* Display storage at first storage operand location */
    if (b1 >= 0)
    {
        if(REAL_MODE(&regs->psw))
            n = ARCH_DEP(display_virt) (regs, addr1, buf, USE_REAL_ADDR,
                                                ACCTYPE_READ);
        else
            n = ARCH_DEP(display_virt) (regs, addr1, buf, b1,
                                (opcode == 0x44 ? ACCTYPE_INSTFETCH :
                                 opcode == 0xB1 ? ACCTYPE_LRA :
                                                  ACCTYPE_READ));
        logmsg ("%s\n", buf);
    }

    /* Display storage at second storage operand location */
    if (b2 >= 0)
    {
        if(
            (REAL_MODE(&regs->psw)
            || (opcode == 0xB2 && inst[1] == 0x4B) /*LURA*/
            || (opcode == 0xB2 && inst[1] == 0x46) /*STURA*/
            || (opcode == 0xB9 && inst[1] == 0x05) /*LURAG*/
            || (opcode == 0xB9 && inst[1] == 0x25))) /*STURG*/
            n = ARCH_DEP(display_virt) (regs, addr2, buf, USE_REAL_ADDR,
                                                ACCTYPE_READ);
        else
            n = ARCH_DEP(display_virt) (regs, addr2, buf, b2,
                                        ACCTYPE_READ);

        logmsg ("%s\n", buf);
    }

#endif /*DISPLAY_INSTRUCTION_OPERANDS*/

    /* Display the general purpose registers */
    display_regs (regs);

    /* Display control registers if appropriate */
    if (!REAL_MODE(&regs->psw) || regs->ip[0] == 0xB2)
        display_cregs (regs);

    /* Display access registers if appropriate */
    if (!REAL_MODE(&regs->psw) && ACCESS_REGISTER_MODE(&regs->psw))
        display_aregs (regs);

} /* end function display_inst */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "hscmisc.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "hscmisc.c"
#endif


/*-------------------------------------------------------------------*/
/* Wrappers for architecture-dependent functions                     */
/*-------------------------------------------------------------------*/
void alter_display_real (char *opnd, REGS *regs)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_alter_display_real (opnd, regs); break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_alter_display_real (opnd, regs); break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_alter_display_real (opnd, regs); break;
#endif
    }

} /* end function alter_display_real */


void alter_display_virt (char *opnd, REGS *regs)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_alter_display_virt (opnd, regs); break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_alter_display_virt (opnd, regs); break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_alter_display_virt (opnd, regs); break;
#endif
    }

} /* end function alter_display_virt */


void display_inst(REGS *regs, BYTE *inst)
{
    switch(regs->arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_display_inst(regs,inst);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_display_inst(regs,inst);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_display_inst(regs,inst);
            break;
#endif
    }

}


void disasm_stor(REGS *regs, char *opnd)
{
    switch(regs->arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_disasm_stor(regs,opnd);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_disasm_stor(regs,opnd);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_disasm_stor(regs,opnd);
            break;
#endif
    }

}

/*-------------------------------------------------------------------*/
/* Execute a Unix or Windows command                                 */
/* Returns the system command status code                            */
/*-------------------------------------------------------------------*/
int herc_system (char* command)
{

#if HOW_TO_IMPLEMENT_SH_COMMAND == USE_ANSI_SYSTEM_API_FOR_SH_COMMAND

    return system(command);

#elif HOW_TO_IMPLEMENT_SH_COMMAND == USE_FORK_API_FOR_SH_COMMAND

extern char **environ;
int pid, status;

    if (command == 0)
        return 1;

    pid = fork();

    if (pid == -1)
        return -1;

    if (pid == 0)
    {
        char *argv[4];

        /* Redirect stderr (screen) to hercules log task */
        dup2(STDOUT_FILENO, STDERR_FILENO);

        /* Drop ROOT authority (saved uid) */
        SETMODE(TERM);

        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = command;
        argv[3] = 0;
        execve("/bin/sh", argv, environ);

        exit(127);
    }

    do
    {
        if (waitpid(pid, &status, 0) == -1)
        {
            if (errno != EINTR)
                return -1;
        } else
            return status;
    } while(1);
#else
  #error 'HOW_TO_IMPLEMENT_SH_COMMAND' not #defined correctly
#endif
} /* end function herc_system */

#endif /*!defined(_GEN_ARCH)*/
