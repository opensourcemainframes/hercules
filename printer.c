/* PRINTER.C    (c) Copyright Roger Bowler, 1999-2002                */
/*              ESA/390 Line Printer Device Handler                  */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* System/370 line printer devices.                                  */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define LINE_LENGTH     150
#define SPACE           ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Subroutine to open the printer file or pipe                       */
/*-------------------------------------------------------------------*/
static int
open_printer (DEVBLK *dev)
{
int             fd;                     /* File descriptor           */
#if !defined(WIN32)
int             rc;                     /* Return code               */
int             pipefd[2];              /* Pipe descriptors          */
pid_t           pid;                    /* Child process identifier  */
#endif /* !defined(WIN32) */

    /* Regular open if 1st char of filename is not vertical bar */
    if (dev->filename[0] != '|')
    {
        fd = open (dev->filename,
                    O_WRONLY | O_CREAT | O_TRUNC /* | O_SYNC */,
                    S_IRUSR | S_IWUSR | S_IRGRP);
        if (fd < 0)
        {
            logmsg ("HHC413I Error opening file %s: %s\n",
                    dev->filename, strerror(errno));
            return -1;
        }

        /* Save file descriptor in device block and return */
        dev->fd = fd;
        return 0;
    }

    /* Filename is in format |xxx, set up pipe to program xxx */

#if defined(WIN32)
    logmsg ("HHC414I %4.4X print to pipe not supported under Windows\n",
            dev->devnum);
    return -1;
#else /* !defined(WIN32) */

    /* Create a pipe */
    rc = pipe (pipefd);
    if (rc < 0)
    {
        logmsg ("HHC415I %4.4X device initialization error: pipe: %s\n",
                dev->devnum, strerror(errno));
        return -1;
    }

    /* Fork a child process to receive the pipe data */
    pid = fork();
    if (pid < 0)
    {
        logmsg ("HHC416I %4.4X device initialization error: fork: %s\n",
                dev->devnum, strerror(errno));
        return -1;
    }

    /* The child process executes the pipe receiver program */
    if (pid == 0)
    {
        /* Log start of child process */
        logmsg ("HHC417I %4.4X pipe receiver (pid=%d) starting\n",
                dev->devnum, getpid());

        /* Close the write end of the pipe */
        close (pipefd[1]);

        /* Duplicate the read end of the pipe onto STDIN */
        if (pipefd[0] != STDIN_FILENO)
        {
            rc = dup2 (pipefd[0], STDIN_FILENO);
            if (rc != STDIN_FILENO)
            {
                logmsg ("HHC418I %4.4X dup2 error: %s\n",
                        dev->devnum, strerror(errno));
                _exit(127);
            }
            /* Close the original descriptor now duplicated to STDIN */
            close (pipefd[0]);

        } /* end if(pipefd[0] != STDIN_FILENO) */

        /* Redirect STDOUT to the control panel message pipe */
        rc = dup2 (fileno(sysblk.msgpipew), STDOUT_FILENO);
        if (rc != STDOUT_FILENO)
        {
            logmsg ("HHC419I %4.4X dup2 error: %s\n",
                    dev->devnum, strerror(errno));
            _exit(127);
        }

        /* Redirect STDERR to the control panel message pipe */
        rc = dup2 (fileno(sysblk.msgpipew), STDERR_FILENO);
        if (rc != STDERR_FILENO)
        {
            logmsg ("HHC420I %4.4X dup2 error: %s\n",
                    dev->devnum, strerror(errno));
            _exit(127);
        }

        /* Relinquish any ROOT authority before calling shell */
        SETMODE(TERM);

        /* Execute the specified pipe receiver program */
        rc = system (dev->filename+1);
        if (rc != 0)
        {
            logmsg ("HHC422I %4.4X Unable to execute %s: %s\n",
                    dev->devnum, dev->filename+1, strerror(errno));
            close (STDIN_FILENO);
            _exit(rc);
        }

        /* Log end of child process */
        logmsg ("HHC423I %4.4X pipe receiver (pid=%d) terminating\n",
                dev->devnum, getpid());

        /* The child process terminates using _exit instead of exit
           to avoid invoking the panel atexit cleanup routine */
        close (STDIN_FILENO);
        _exit(0);

    } /* end if(pid==0) */

    /* The parent process continues as the pipe sender */

    /* Close the read end of the pipe */
    close (pipefd[0]);

    /* Save pipe write descriptor in the device block */
    dev->fd = pipefd[1];

    return 0;
#endif /* !defined(WIN32) */
} /* end function open_printer */

/*-------------------------------------------------------------------*/
/* Subroutine to write data to the printer                           */
/*-------------------------------------------------------------------*/
static void
write_buffer (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write data to the printer file */
    rc = write (dev->fd, buf, len);

    /* Equipment check if error writing to printer file */
    if (rc < len)
    {
        logmsg ("HHC414I %4.4X Error writing to %s: %s\n",
                dev->devnum, dev->filename,
                (errno == 0 ? "incomplete": strerror(errno)));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

} /* end function write_buffer */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int printer_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
int     i;                              /* Array subscript           */

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        fprintf (stderr,
                "HHC411I File name missing or invalid\n");
        return -1;
    }

    /* Save the file name in the device block */
    strcpy (dev->filename, argv[0]);

    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->printpos = 0;
    dev->printrem = LINE_LENGTH;
    dev->diaggate = 0;
    dev->fold = 0;
    dev->crlf = 0;

    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "crlf") == 0)
        {
            dev->crlf = 1;
            continue;
        }

        fprintf (stderr, "HHC412I Invalid argument: %s\n",
                argv[i]);
        return -1;
    }

    /* Set length of print buffer */
    dev->bufsize = LINE_LENGTH + 8;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = 0x28; /* Control unit type is 2821-1 */
    dev->devid[2] = 0x21;
    dev->devid[3] = 0x01;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x01;
    dev->numdevid = 7;

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    return 0;
} /* end function printer_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void printer_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "PRT";
    snprintf (buffer, buflen, "%s%s",
                dev->filename,
                (dev->crlf ? " crlf" : ""));

} /* end function printer_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int printer_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function printer_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void printer_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             i;                      /* Loop counter              */
int             num;                    /* Number of bytes to move   */
char           *eor;                    /* -> end of record string   */
BYTE            c;                      /* Print character           */

    /* Reset flags at start of CCW chain */
    if (chained == 0)
    {
        dev->diaggate = 0;
    }

    /* Open the device file if necessary */
    if (dev->fd < 0 && !IS_CCW_SENSE(code))
    {
        rc = open_printer (dev);
        if (rc < 0)
        {
            /* Set unit check with intervention required */
            dev->sense[0] = SENSE_IR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE WITHOUT SPACING                                         */
    /*---------------------------------------------------------------*/
        eor = "\r";
        goto write;

    case 0x09:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 1 LINE                                        */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    case 0x11:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 2 LINES                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n" : "\n\n";
        goto write;

    case 0x19:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 3 LINES                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n\n" : "\n\n\n";
        goto write;

    case 0x89:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 1                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\f" : "\f";
        goto write;

    case 0xC9:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 9                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    case 0xE1:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 12                                  */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    write:
        /* Start a new record if not data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            dev->printpos = 0;
            dev->printrem = LINE_LENGTH;

        } /* end if(!data-chained) */

        /* Calculate number of bytes to write and set residual count */
        num = (count < dev->printrem) ? count : dev->printrem;
        *residual = count - num;

        /* Copy data from channel buffer to print buffer */
        for (i = 0; i < num; i++)
        {
            c = ebcdic_to_ascii[iobuf[i]];

            if (dev->fold) c = toupper(c);
            if (c == 0) c = SPACE;

            dev->buf[dev->printpos] = c;
            dev->printpos++;
            dev->printrem--;
        } /* end for(i) */

        /* Perform end of record processing if not data-chaining */
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            /* Truncate trailing blanks from print line */
            for (i = dev->printpos; i > 0; i--)
                if (dev->buf[i-1] != SPACE) break;

            /* Append carriage return and line feed(s) */
            strcpy (dev->buf + i, eor);
            i += strlen(eor);

            /* Write print line */
            write_buffer (dev, dev->buf, i, unitstat);
            if (*unitstat != 0) break;

        } /* end if(!data-chaining) */

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x06:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC CHECK READ                                         */
    /*---------------------------------------------------------------*/
        /* If not 1403, reject if not preceded by DIAGNOSTIC GATE */
        if (dev->devtype != 0x1403 && dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x07:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC GATE                                               */
    /*---------------------------------------------------------------*/
        /* Command reject if 1403, or if chained to another CCW
           except a no-operation at the start of the CCW chain */
        if (dev->devtype == 1403 || ccwseq > 1
            || (chained && prevcode != 0x03))
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set diagnostic gate flag */
        dev->diaggate = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0A:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC READ UCS BUFFER                                    */
    /*---------------------------------------------------------------*/
        /* Reject if 1403 or not preceded by DIAGNOSTIC GATE */
        if (dev->devtype == 0x1403 || dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x12:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC READ FCB                                           */
    /*---------------------------------------------------------------*/
        /* Reject if 1403 or not preceded by DIAGNOSTIC GATE */
        if (dev->devtype == 0x1403 || dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0B:
    /*---------------------------------------------------------------*/
    /* SPACE 1 LINE IMMEDIATE                                        */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x13:
    /*---------------------------------------------------------------*/
    /* SPACE 2 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n" : "\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x1B:
    /*---------------------------------------------------------------*/
    /* SPACE 3 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n\n" : "\n\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x23:
    /*---------------------------------------------------------------*/
    /* UNFOLD                                                        */
    /*---------------------------------------------------------------*/
        dev->fold = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* FOLD                                                          */
    /*---------------------------------------------------------------*/
        dev->fold = 1;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x73:
    /*---------------------------------------------------------------*/
    /* BLOCK DATA CHECK                                              */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x7B:
    /*---------------------------------------------------------------*/
    /* ALLOW DATA CHECK                                              */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x8B:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 1 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\f" : "\f";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xCB:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 9 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE3:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 12 IMMEDIATE                                  */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x63:
    /*---------------------------------------------------------------*/
    /* LOAD FORMS CONTROL BUFFER                                     */
    /*---------------------------------------------------------------*/
        /* Return normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xEB:
    /*---------------------------------------------------------------*/
    /* UCS GATE LOAD                                                 */
    /*---------------------------------------------------------------*/
        /* Command reject if not first command in chain */
        if (chained != 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xF3:
    /*---------------------------------------------------------------*/
    /* LOAD UCS BUFFER AND FOLD                                      */
    /*---------------------------------------------------------------*/
        /* For 1403, command reject if not chained to UCS GATE */
        if (dev->devtype == 0x1403 && prevcode != 0xEB)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set fold indicator and return normal status */
        dev->fold = 1;
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xFB:
    /*---------------------------------------------------------------*/
    /* LOAD UCS BUFFER (NO FOLD)                                     */
    /*---------------------------------------------------------------*/
        /* For 1403, command reject if not chained to UCS GATE */
        if (dev->devtype == 0x1403 && prevcode != 0xEB)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reset fold indicator and return normal status */
        dev->fold = 0;
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function printer_execute_ccw */

