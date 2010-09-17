/* LOGGER.C     (c) Copyright Jan Jaeger, 2003-2010                  */
/*              System logger functions                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* If standard output or standard error is redirected then the log   */
/* is written to the redirection.                                    */
/* If standard output and standard error are both redirected then    */
/* the system log is written to the redirection of standard error    */
/* the redirection of standard output is ignored in this case,       */
/* and background mode is assumed.                                   */

/* Any thread can determine background mode by inspecting stderr     */
/* for isatty()                                                      */

#include "hstdinc.h"

#define _LOGGER_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "opcode.h"             /* Required for SETMODE macro        */
static COND  logger_cond;
static LOCK  logger_lock;
static TID   logger_tid;

static char *logger_buffer;
static int   logger_bufsize;

static int   logger_currmsg;
static int   logger_wrapped;

static int   logger_active=0;

static FILE *logger_syslog[2];          /* Syslog read/write pipe    */
       int   logger_syslogfd[2];        /*   pairs                   */
static FILE *logger_hrdcpy;             /* Hardcopy log or zero      */
static int   logger_hrdcpyfd;           /* Hardcopt fd or -1         */
static char  logger_filename[MAX_PATH];

/* Find the index for a specific line number in the log,             */
/* one being the most recent line                                    */
/* Example:                                                          */
/*   read the last 5 lines in the syslog:                            */
/*                                                                   */
/*   int msgnum;                                                     */
/*   int msgidx;                                                     */
/*   char *msgbuf;                                                   */
/*                                                                   */
/*        msgidx = log_line(5);                                      */
/*        while((msgcnt = log_read(&msgbuf, &msgidx, LOG_NOBLOCK)))  */
/*            function_to_process_log(msgbuf, msgcnt);               */
/*                                                                   */
DLL_EXPORT int log_line(int linenumber)
{
char *msgbuf[2] = {NULL, NULL}, *tmpbuf = NULL;
int  msgidx[2] = { -1, -1 };
int  msgcnt[2] = {0, 0};
int  i;

    if(!linenumber++)
        return logger_currmsg;

    /* Find the last two blocks in the log */
    do {
        msgidx[0] = msgidx[1];
        msgbuf[0] = msgbuf[1];
        msgcnt[0] = msgcnt[1];
    } while((msgcnt[1] = log_read(&msgbuf[1], &msgidx[1], LOG_NOBLOCK)));

    for(i = 0; i < 2 && linenumber; i++)
        if(msgidx[i] != -1)
        {
            for(; linenumber > 0; linenumber--)
            {
                if(!(tmpbuf = (void *)memrchr(msgbuf[i],'\n',msgcnt[i])))
                    break;
                msgcnt[i] = tmpbuf - msgbuf[i];
            }
            if(!linenumber)
                break;
        }

    while(i < 2 && tmpbuf && (*tmpbuf == '\r' || *tmpbuf == '\n'))
    {
        tmpbuf++;
        msgcnt[i]++;
    }

    return i ? msgcnt[i] + msgidx[0] : msgcnt[i];
}


/* log_read - read system log                                        */
/* parameters:                                                       */
/*   buffer   - pointer to bufferpointer                             */
/*              the bufferpointer will be returned                   */
/*   msgindex - an index used on multiple read requests              */
/*              a value of -1 ensures that reading starts at the     */
/*              oldest entry in the log                              */
/*   block    - LOG_NOBLOCK - non blocking request                   */
/*              LOG_BLOCK   - blocking request                       */
/* returns:                                                          */
/*   number of bytes in buffer or zero                               */
/*                                                                   */
/*                                                                   */
DLL_EXPORT int log_read(char **buffer, int *msgindex, int block)
{
int bytes_returned;

    obtain_lock(&logger_lock);

    if(*msgindex == logger_currmsg && block)
    {
        if(logger_active)
        {
            wait_condition(&logger_cond, &logger_lock);
        }
        else
        {
            *msgindex = logger_currmsg;
            *buffer = logger_buffer + logger_currmsg;
            release_lock(&logger_lock);
            return 0;
        }
    }

    if(*msgindex != logger_currmsg)
    {
        if(*msgindex < 0)
            *msgindex = logger_wrapped ? logger_currmsg : 0;

        if(*msgindex < 0 || *msgindex >= logger_bufsize)
            *msgindex = 0;

        *buffer = logger_buffer + *msgindex;

        if(*msgindex >= logger_currmsg)
        {
            bytes_returned = logger_bufsize - *msgindex;
            *msgindex = 0;
        }
        else
        {
            bytes_returned = logger_currmsg - *msgindex;
            *msgindex = logger_currmsg;
        }
    }
    else
        bytes_returned = 0;

    release_lock(&logger_lock);

    return bytes_returned;
}


static void logger_term(void *arg)
{
    UNREFERENCED(arg);

    if(logger_active)
    {
        char* term_msg = MSG(HHC02103, "I");
        size_t term_msg_len = strlen(term_msg);

        obtain_lock(&logger_lock);

        /* Flush all pending logger o/p before redirecting?? */
        fflush(stdout);

        /* Redirect all output to stderr */
        dup2(STDERR_FILENO, STDOUT_FILENO);

        /* Tell logger thread we want it to exit */
        logger_active = 0;

        /* Send the logger a message to wake it up */
        write_pipe( logger_syslogfd[LOG_WRITE], term_msg, term_msg_len );

        release_lock(&logger_lock);

        /* Wait for the logger to terminate */
        join_thread( logger_tid, NULL );
        detach_thread( logger_tid );
    }
}
static void logger_logfile_write( void* pBuff, size_t nBytes )
{
    char* pLeft = (char*)pBuff;
    int   nLeft = (int)nBytes;
#if defined( OPTION_MSGCLR )
    /* Remove "<pnl,..." color string if it exists */
    if (1
        && nLeft > 5
        && strncasecmp( pLeft, "<pnl", 4 ) == 0
        && (pLeft = memchr( pLeft+4, '>', nLeft-4 )) != NULL
        )
    {
        pLeft++;
        nLeft -= (pLeft - (char*)pBuff);
    }

#endif // defined( OPTION_MSGCLR )
    /* (ignore any errors; we did the best we could) */
    if (nLeft)
    {      
        if ( fwrite( pLeft, nLeft, 1, logger_hrdcpy ) != 1 )
        {
            fprintf(logger_hrdcpy, MSG(HHC02102, "E", "fwrite()",
                strerror(errno)));
        }
    }

    if ( sysblk.shutdown )
        fflush ( logger_hrdcpy );
}

#ifdef OPTION_TIMESTAMP_LOGFILE
/* ZZ FIXME:
 * This should really be part of logmsg, as the timestamps have currently
 * the time when the logger reads the message from the log pipe.  There can be 
 * quite a delay at times when there is a high system activity. Moving the timestamp 
 * to logmsg() will fix this.
 * The timestamp option should also NOT depend on anything like daemon mode.
 * logs entries should always be timestamped, in a fixed format, such that 
 * log readers may decide to skip the timestamp when displaying (ie panel.c).
 */
static void logger_logfile_timestamp()
{
    if (!sysblk.daemon_mode)
    {
        struct timeval  now;
        time_t          tt;
        char            hhmmss[10];

        gettimeofday( &now, NULL ); tt = now.tv_sec;
        strlcpy( hhmmss, ctime(&tt)+11, sizeof(hhmmss) );
        logger_logfile_write( hhmmss, strlen(hhmmss) );
    }
}
#endif


static void logger_thread(void *arg)
{
int bytes_read;

    UNREFERENCED(arg);

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set device thread priority; ignore any errors */
    if(setpriority(PRIO_PROCESS, 0, sysblk.devprio))
       WRMSG(HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

#if !defined( _MSVC_ )
    /* Redirect stdout to the logger */
    if(dup2(logger_syslogfd[LOG_WRITE],STDOUT_FILENO) == -1)
    {
        if(logger_hrdcpy)
            fprintf(logger_hrdcpy, MSG(HHC02102, "E", "dup2()", strerror(errno)));
        exit(1);
    }
#endif /* !defined( _MSVC_ ) */

    setvbuf (stdout, NULL, _IONBF, 0);

    /* call logger_term on system shutdown */
    hdl_adsc("logger_term",logger_term, NULL);

    obtain_lock(&logger_lock);

    logger_active = 1;

    /* Signal initialization complete */
    signal_condition(&logger_cond);

    release_lock(&logger_lock);

    /* ZZ FIXME:  We must empty the read pipe before we terminate */
    /* (Couldn't we just loop waiting for a 'select(,&readset,,,timeout)'
        to return zero?? Or use the 'poll' function similarly?? - Fish) */

    while(logger_active)
    {
        bytes_read = read_pipe(logger_syslogfd[LOG_READ],logger_buffer + logger_currmsg,
          ((logger_bufsize - logger_currmsg) > LOG_DEFSIZE ? LOG_DEFSIZE : logger_bufsize - logger_currmsg));

        if(bytes_read == -1)
        {
            int read_pipe_errno = HSO_errno;

            // (ignore any/all errors at shutdown)
            if (sysblk.shutdown) continue;

            if (HSO_EINTR == read_pipe_errno)
                continue;

            if(logger_hrdcpy)
                fprintf(logger_hrdcpy, MSG(HHC02102, "E", "read_pipe()", strerror(read_pipe_errno)));
            bytes_read = 0;
        }

        /* If Hercules is not running in daemon mode and panel
           initialization is not yet complete, write message
           to stderr so the user can see it on the terminal */
        if (!sysblk.daemon_mode)
        {
            if (!sysblk.panel_init)
            {
                char* pLeft2 = logger_buffer + logger_currmsg;
                int   nLeft2 = bytes_read;
#if defined( OPTION_MSGCLR )
            /* Remove "<pnl,..." color string if it exists */
                if (1
                    && nLeft2 > 5
                    && strncasecmp( pLeft2, "<pnl", 4 ) == 0
                    && (pLeft2 = memchr( pLeft2+4, '>', nLeft2-4 )) != NULL
                )
                {
                    pLeft2++;
                    nLeft2 -= (pLeft2 - (logger_buffer + logger_currmsg));
                }

#endif // defined( OPTION_MSGCLR )
                /* (ignore any errors; we did the best we could) */
                if (nLeft2)
                    fwrite( pLeft2, nLeft2, 1, stderr );
            }
        }

        /* Write log data to hardcopy file */
        if (logger_hrdcpy)
#if !defined( OPTION_TIMESTAMP_LOGFILE )
        {
            char* pLeft2 = logger_buffer + logger_currmsg;
            int   nLeft2 = bytes_read;
#if defined( OPTION_MSGCLR )
            /* Remove "<pnl,..." color string if it exists */
            if (1
                && nLeft2 > 5
                && strncasecmp( pLeft2, "<pnl", 4 ) == 0
                && (pLeft2 = memchr( pLeft2+4, '>', nLeft2-4 )) != NULL
            )
            {
                pLeft2++;
                nLeft2 -= (pLeft2 - (logger_buffer + logger_currmsg));
            }
            else
            {
                pLeft2 = logger_buffer + logger_currmsg;
                nLeft2 = bytes_read;
            }
#endif // defined( OPTION_MSGCLR )
            if (nLeft2)
                logger_logfile_write( pLeft2, nLeft2 );
        }
#else // defined( OPTION_TIMESTAMP_LOGFILE )
        {
            /* Need to prefix each line with a timestamp. */

            static int needstamp = 1;
            char*  pLeft  = logger_buffer + logger_currmsg;
            int    nLeft  = bytes_read;
            char*  pRight = NULL;
            int    nRight = 0;
            char*  pNL    = NULL;   /* (pointer to NEWLINE character) */

            if (needstamp)
            {
                if (!sysblk.logoptnotime) logger_logfile_timestamp();
                needstamp = 0;
            }

            while ( (pNL = memchr( pLeft, '\n', nLeft )) != NULL )
            {
                pRight  = pNL + 1;
                nRight  = nLeft - (pRight - pLeft);
                nLeft  -= nRight;

#if defined( OPTION_MSGCLR )
                /* Remove "<pnl...>" color string if it exists */
                {
                    char* pLeft2 = pLeft;
                    int   nLeft2 = nLeft;

                    if (1
                        && nLeft > 5
                        && strncasecmp( pLeft, "<pnl", 4 ) == 0
                        && (pLeft2 = memchr( pLeft+4, '>', nLeft-4 )) != NULL
                    )
                    {
                        pLeft2++;
                        nLeft2 -= (pLeft2 - pLeft);
                    }
                    else
                    {
                        pLeft2 = pLeft;
                        nLeft2 = nLeft;
                    }
                    if (nLeft2)
                        logger_logfile_write( pLeft2, nLeft2 );
                }
#else // !defined( OPTION_MSGCLR )

                if (nLeft)
                    logger_logfile_write( pLeft, nLeft );

#endif // defined( OPTION_MSGCLR )

                pLeft = pRight;
                nLeft = nRight;

                if (!nLeft)
                {
                    needstamp = 1;
                    break;
                }

                if (!sysblk.logoptnotime) logger_logfile_timestamp();
            }

            if (nLeft)
                logger_logfile_write( pLeft, nLeft );
        }
#endif // !defined( OPTION_TIMESTAMP_LOGFILE )

        /* Increment buffer index to next available position */
        logger_currmsg += bytes_read;
        if(logger_currmsg >= logger_bufsize)
        {
            logger_currmsg = 0;
            logger_wrapped = 1;
        }

        /* Notify all interested parties new log data is available */
        obtain_lock(&logger_lock);
        broadcast_condition(&logger_cond);
        release_lock(&logger_lock);
    }

    /* Logger is now terminating */
    obtain_lock(&logger_lock);

    /* Write final message to hardcopy file */
    if (logger_hrdcpy)
    {
        char* term_msg = MSG(HHC02103, "I");
        size_t term_msg_len = strlen(term_msg);
#ifdef OPTION_TIMESTAMP_LOGFILE
        if (!sysblk.logoptnotime) logger_logfile_timestamp();
#endif
        logger_logfile_write( term_msg, term_msg_len );
    }

    /* Redirect all msgs to stderr */
    logger_syslog[LOG_WRITE] = stderr;
    logger_syslogfd[LOG_WRITE] = STDERR_FILENO;
    fflush(stderr);

    /* Signal any waiting tasks */
    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}


DLL_EXPORT void logger_init(void)
{
    int rc;

    initialize_condition (&logger_cond);
    initialize_lock (&logger_lock);
    initialize_lock (&sysblk.msglock);

    obtain_lock(&logger_lock);

    if(fileno(stdin)>=0 ||
        fileno(stdout)>=0 ||
        fileno(stderr)>=0)
    {
        logger_syslog[LOG_WRITE] = stderr;

        /* If standard error is redirected, then use standard error
        as the log file. */
        if(!isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO))
        {
            /* Ignore standard output to the extent that it is
            treated as standard error */
            logger_hrdcpyfd = dup(STDOUT_FILENO);
            if(dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
            {
                fprintf(stderr, MSG(HHC02102, "E", "dup2()", strerror(errno)));
                exit(1);
            }
        }
        else
        {
            if(!isatty(STDOUT_FILENO))
            {
                logger_hrdcpyfd = dup(STDOUT_FILENO);
                if(dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
                {
                    fprintf(stderr, MSG(HHC02102, "E", "dup2()", strerror(errno)));
                    exit(1);
                }
            }
            if(!isatty(STDERR_FILENO))
            {
                logger_hrdcpyfd = dup(STDERR_FILENO);
                if(dup2(STDOUT_FILENO,STDERR_FILENO) == -1)
                {
                    fprintf(stderr, MSG(HHC02102, "E", "dup2()", strerror(errno)));
                    exit(1);
                }
            }
        }

        if(logger_hrdcpyfd == -1)
        {
            logger_hrdcpyfd = 0;
            fprintf(stderr, MSG(HHC02102, "E", "dup()", strerror(errno)));
        }

        if(logger_hrdcpyfd)
        {
            if(!(logger_hrdcpy = fdopen(logger_hrdcpyfd,"w")))
            fprintf(stderr, MSG(HHC02102, "E", "fdopen()", strerror(errno)));
        }

        if(logger_hrdcpy)
            setvbuf(logger_hrdcpy, NULL, _IONBF, 0);
    }
    else
    {
        logger_syslog[LOG_WRITE]=fopen("LOG","a");
    }

    logger_bufsize = LOG_DEFSIZE;

    if(!(logger_buffer = malloc(logger_bufsize)))
    {
        char buf[40];
        sprintf(buf, "malloc(%d)", logger_bufsize);
        fprintf(stderr, MSG(HHC02102, "E", buf, strerror(errno)));
        exit(1);
    }

    if(create_pipe(logger_syslogfd))
    {
        fprintf(stderr, MSG(HHC02102, "E", "create_pipe()", strerror(errno)));
        exit(1);  /* Hercules running without syslog */
    }

    setvbuf (logger_syslog[LOG_WRITE], NULL, _IONBF, 0);

    rc = create_thread (&logger_tid, JOINABLE,
                       logger_thread, NULL, "logger_thread");
    if (rc)
    {
        fprintf(stderr, MSG(HHC00102, "E", strerror(rc)));
        exit(1);
    }

    wait_condition(&logger_cond, &logger_lock);

    release_lock(&logger_lock);

}

DLL_EXPORT char *log_dsphrdcpy(void)
{
    return logger_filename;
}

DLL_EXPORT void log_sethrdcpy(char *filename)
{
FILE *temp_hrdcpy = logger_hrdcpy;
FILE *new_hrdcpy;
int   new_hrdcpyfd;

    if(!filename)
    {
        logger_filename[0] = '\0';

        if(!logger_hrdcpy)
        {
            WRMSG(HHC02100, "E");
            return;
        }
        else
        {
            obtain_lock(&logger_lock);
            logger_hrdcpy = 0;
            logger_hrdcpyfd = 0;
            release_lock(&logger_lock);
            fprintf(temp_hrdcpy,MSG(HHC02101, "I"));
            fclose(temp_hrdcpy);
            WRMSG(HHC02101, "I");
            return;
        }
    }
    else
    {
        char pathname[MAX_PATH];
        hostpath(pathname, filename, sizeof(pathname));
        
        logger_filename[0] = '\0';

        new_hrdcpyfd = open(pathname,
                O_WRONLY | O_CREAT | O_TRUNC /* O_SYNC */,
                            S_IRUSR  | S_IWUSR | S_IRGRP);
        if(new_hrdcpyfd < 0)
        {
            WRMSG(HHC02102, "E","open()",strerror(errno));
            return;
        }
        else
        {
            if(!(new_hrdcpy = fdopen(new_hrdcpyfd,"w")))
            {
                WRMSG(HHC02102,"E", "fdopen()", strerror(errno));
                return;
            }
            else
            {
                setvbuf(new_hrdcpy, NULL, _IONBF, 0);

                obtain_lock(&logger_lock);
                logger_hrdcpy = new_hrdcpy;
                logger_hrdcpyfd = new_hrdcpyfd;
                strcpy(logger_filename, filename);
                release_lock(&logger_lock);

                if(temp_hrdcpy)
                {
                    fprintf(temp_hrdcpy, MSG(HHC02104, "I", filename));
                    fclose(temp_hrdcpy);
                }
            }
        }
    }
}

/* log_wakeup - Wakeup any blocked threads.  Useful during shutdown. */
DLL_EXPORT void log_wakeup(void *arg)
{
    UNREFERENCED(arg);

    obtain_lock(&logger_lock);

    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}
