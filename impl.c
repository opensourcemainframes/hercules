/* IMPL.C       (c) Copyright Roger Bowler, 1999-2003                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* central processors, HTTP server, logger task and activates the    */
/* control panel which runs under the main thread when in foreground */
/* mode.                                                             */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"
#include "devtype.h"

#if defined(FISH_HANG)
extern  int   bFishHangAtExit;  // (set to true when shutting down)
extern  void  FishHangInit(char* pszFileCreated, int nLineCreated);
extern  void  FishHangReport();
extern  void  FishHangAtExit();
#endif // defined(FISH_HANG)

/* forward define process_script_file (ISW20030220-3) */
int process_script_file(char *,int);

/*-------------------------------------------------------------------*/
/* Signal handler for SIGINT signal                                  */
/*-------------------------------------------------------------------*/
static void sigint_handler (int signo)
{
//  logmsg ("config: sigint handler entered for thread %lu\n",/*debug*/
//          thread_id());                                     /*debug*/

    UNREFERENCED(signo);

    signal(SIGINT, sigint_handler);
    /* Ignore signal unless presented on console thread */
    if (thread_id() != sysblk.cnsltid)
        return;

    /* Exit if previous SIGINT request was not actioned */
    if (sysblk.sigintreq)
    {
        /* Release the configuration */
        release_config();
        exit(1);
    }

    /* Set SIGINT request pending flag */
    sysblk.sigintreq = 1;

    /* Activate instruction stepping */
    sysblk.inststep = 1;
    ON_IC_TRACE;
    return;
} /* end function sigint_handler */


#if !defined(NO_SIGABEND_HANDLER)
static void watchdog_thread()
{
S64 savecount[MAX_CPU_ENGINES];
int i;

#ifndef WIN32
    /* Set watchdog priority just below cpu priority
       such that it will not invalidly detect an
       inoperable cpu */
    if(sysblk.cpuprio >= 0)
        setpriority(PRIO_PROCESS, 0, sysblk.cpuprio+1);
#endif

    while(1)
    {
#ifdef FEATURE_CPU_RECONFIG
        for (i = 0; i < MAX_CPU_ENGINES; i++)
#else /*!FEATURE_CPU_RECONFIG*/
        for (i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
        {
            if(sysblk.regs[i].cpustate == CPUSTATE_STARTED
              && (!sysblk.regs[i].psw.wait
#if defined(_FEATURE_WAITSTATE_ASSIST)
              && !(sysblk.regs[i].sie_state && sysblk.regs[i].guestregs->psw.wait)
#endif
                                           ))
            {
                /* If the cpu is running but not executing
                   instructions then it must be malfunctioning */
                if(sysblk.regs[i].instcount == (U64)savecount[i])
                {
                    if(!try_obtain_lock(&sysblk.intlock))
                    {
                        /* Send signal to looping CPU */
                        signal_thread(sysblk.regs[i].cputid, SIGUSR1);
                        savecount[i] = -1;
                        release_lock(&sysblk.intlock);
                    }
                }
                else
                    /* Save current instcount */
                    savecount[i] = sysblk.regs[i].instcount;
            }
            else
                /* mark savecount invalid as CPU not in running state */
                savecount[i] = -1;
        }
        /* Sleep for 20 seconds */
        sleep(20);
    }
}
#endif /*!defined(NO_SIGABEND_HANDLER)*/


/*-------------------------------------------------------------------*/
/* Process .RC file thread                                           */
/*-------------------------------------------------------------------*/

void* process_rc_file (void* dummy)
{
BYTE   *rcname;                         /* hercules.rc name pointer  */

    UNREFERENCED(dummy);

    /* Obtain the name of the hercules.rc file or default */

    if(!(rcname = getenv("HERCULES_RC")))
        rcname = "hercules.rc";

    /* Run the script processor for this file */

    process_script_file(rcname,1);

    return NULL;
}


/*-------------------------------------------------------------------*/
/* IMPL main entry point                                             */
/*-------------------------------------------------------------------*/
static int daemon_mode = 0;
int main (int argc, char *argv[])
{
BYTE   *cfgfile;                        /* -> Configuration filename */
int     c;                              /* Work area for getopt      */
int     arg_error = 0;                  /* 1=Invalid arguments       */
char   *msgbuf;                         /*                           */
int     msgnum;                         /*                           */
int     msgcnt;                         /*                           */
TID     rctid;                          /* RC file thread identifier */

#if defined(FISH_HANG)
    /* "FishHang" debugs lock/cond/threading logic. Thus it must
     * be initialized BEFORE any lock/cond/threads are created.
     */
    FishHangInit(__FILE__,__LINE__);
#endif // defined(FISH_HANG)

    if(isatty(STDERR_FILENO))
        display_version (stderr, "Hercules ");
    else
        if(isatty(STDOUT_FILENO))
            display_version (stdout, "Hercules ");

    /* Clear the system configuration block */
    memset (&sysblk, 0, sizeof(SYSBLK));

    logger_init();

    /* Display the version identifier */
    display_version (stdout, "Hercules ");

#if defined(OPTION_DYNAMIC_LOAD)
    /* Initialize the hercules dynalic loader */
    hdl_main();
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* default to background mode when both stdout and stderr
       are redirected to a non-tty device */
    daemon_mode = !isatty(STDERR_FILENO);

#ifdef EXTERNALGUI
    /* Set GUI flag if specified as final argument */
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        hdl_load("dyngui",HDL_LOAD_NOUNLOAD);
        argc--;
    }
#endif /*EXTERNALGUI*/

#if defined(BUILTIN_STRERROR_R)
    strerror_r_init();
#endif /* defined(BUILTIN_STRERROR_R) */

    init_hostinfo();

    /* Get name of configuration file or default to hercules.cnf */
    if(!(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Process the command line options */
    while ((c = getopt(argc, argv, "f:l:d")) != EOF)
    {
    char *dllname, *strtok_str;

        switch (c) {
        case 'f':
            cfgfile = optarg;
            break;
        case 'l':
            for(dllname = strtok_r(optarg,", ",&strtok_str);
                dllname;
                dllname = strtok_r(NULL,", ",&strtok_str))
                hdl_load(dllname, HDL_LOAD_DEFAULT);
            break;
        case 'd':
            daemon_mode = 1;
            break;
        default:
            arg_error = 1;

        } /* end switch(c) */
    } /* end while */

    if (optind < argc)
        arg_error = 1;

    /* Terminate if invalid arguments were detected */
    if (arg_error)
    {
        fprintf (stderr,
                "usage: %s [-f config-filename]\n",
                argv[0]);
        exit(1);
    }

    /* Register the SIGINT handler */
    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        fprintf (stderr,
                "HHCIN001S Cannot register SIGINT handler: %s\n",
                strerror(errno));
        exit(1);
    }

    /* Ignore the SIGPIPE signal, otherwise Hercules may terminate with
       Broken Pipe error if the printer driver writes to a closed pipe */
    if ( signal (SIGPIPE, SIG_IGN) == SIG_ERR )
    {
        fprintf (stderr,
                "HHCIN002E Cannot suppress SIGPIPE signal: %s\n",
                strerror(errno));
    }

#if !defined(NO_SIGABEND_HANDLER)
    {
    struct sigaction sa;
        sa.sa_sigaction = (void*)&sigabend_handler;
#ifdef SA_NODEFER
        sa.sa_flags = SA_NODEFER;
#else
    sa.sa_flags = 0;
#endif

        if( sigaction(SIGILL, &sa, NULL)
         || sigaction(SIGFPE, &sa, NULL)
         || sigaction(SIGSEGV, &sa, NULL)
         || sigaction(SIGBUS, &sa, NULL)
         || sigaction(SIGUSR1, &sa, NULL)
         || sigaction(SIGUSR2, &sa, NULL) )
        {
            fprintf (stderr,
                    "HHCIN003S Cannot register SIGILL/FPE/SEGV/BUS/USR "
                    "handler: %s\n",
                    strerror(errno));
            exit(1);
        }
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* Build system configuration */
    build_config (cfgfile);

#if !defined(NO_SIGABEND_HANDLER)
    /* Start the watchdog */
    if ( create_thread (&sysblk.wdtid, &sysblk.detattr,
                        watchdog_thread, NULL) )
    {
        fprintf (stderr,
                "HHCIN004S Cannot create watchdog thread: %s\n",
                strerror(errno));
        exit(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

#if defined(OPTION_HTTP_SERVER)
    if(sysblk.httpport) {
        /* Start the http server connection thread */
        if (!sysblk.httproot)
        {
#if defined(WIN32)
            char process_dir[1024];
            if (get_process_directory(process_dir,1024) > 0)
                sysblk.httproot = strdup(process_dir);
            else
#endif /*defined(WIN32)*/
            sysblk.httproot = HTTP_ROOT;
        }
#if defined(WIN32)
        if (is_win32_directory(sysblk.httproot))
        {
            char posix_dir[1024];
            convert_win32_directory_to_posix_directory(sysblk.httproot,posix_dir);
            sysblk.httproot = strdup(posix_dir);
        }
#endif /*defined(WIN32)*/
        TRACE("HTTPROOT = %s\n",sysblk.httproot);
        if ( create_thread (&sysblk.httptid, &sysblk.detattr,
                            http_server, NULL) )
        {
            fprintf (stderr,
                    "HHCIN005S Cannot create http_server thread: %s\n",
                    strerror(errno));
            exit(1);
        }
    }
#endif /*defined(OPTION_HTTP_SERVER)*/

#ifdef OPTION_SHARED_DEVICES
    /* Start the shared server */
    if (sysblk.shrdport)
        if ( create_thread (&sysblk.shrdtid, &sysblk.detattr,
                            shared_server, NULL) )
        {
            fprintf (stderr,
                    "HHCIN006S Cannot create shared_server thread: %s\n",
                    strerror(errno));
            exit(1);
        }

    /* Retry pending connections */
    {
        DEVBLK *dev;
        TID     tid;

        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
            if (dev->connecting)
                if ( create_thread (&tid, &sysblk.detattr, *dev->hnd->init, dev) )
                {
                    fprintf (stderr,
                            "HHCIN007S Cannot create %4.4X connection thread: %s\n",
                            dev->devnum, strerror(errno));
                    exit(1);
                }
    }
#endif

    /* Start up the RC file processing thread */
    create_thread(&rctid,&sysblk.detattr,process_rc_file,NULL);

    /* Activate the control panel */
    if(!daemon_mode)
        panel_display ();
    else
        while(1)
#if defined(OPTION_DYNAMIC_LOAD)
            if(daemon_task)
                daemon_task ();
            else
#endif /*defined(OPTION_DYNAMIC_LOAD)*/
                if((msgcnt = log_read(&msgbuf, &msgnum, LOG_BLOCK)))
                    if(isatty(STDERR_FILENO))
                        fwrite(msgbuf,msgcnt,1,stderr);

    return 0;
} /* end function main */


/*-------------------------------------------------------------------*/
/* System cleanup                                                    */
/*-------------------------------------------------------------------*/
void system_cleanup (void)
{
}


/*-------------------------------------------------------------------*/
/* Shutdown hercules                                                 */
/*-------------------------------------------------------------------*/
void system_shutdown (void)
{
    /* ZZ FIXME: Using the shutdown flag does not serialize shutdown
                 it would be better to call a synchronous termination
                 routine, which only returns when the shutdown of
                 the function in question has been completed */
                 
    sysblk.shutdown = 1;

    /* ZZ FIXME: logger_term() should really be the last thing to do
                 as currently not all termination messages go to 
                 a redirected log, however the logger termination 
                 still has a problem where msgs could be lost during
                 termination (ie left in a buffer, not written to
                 the hardcopy log).  */
    logger_term();

    release_config();
}
