/* CARDRDR.C    (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Card Reader Device Handler                   */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* card reader devices.                                              */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.45  2006/12/28 16:13:28  fish
// Fix PR# readers/104: "HHCRD011E Close error... No Error" in 'cardrdr_close_device' function.
//
// Revision 1.44  2006/12/08 09:43:17  jj
// Add CVS message log
//

#include "hstdinc.h"
#include "hercules.h"

#include "devtype.h"

#include "sockdev.h"


#if defined(WIN32) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
 SYSBLK *psysblk;
 #define sysblk (*psysblk)
#endif


/*-------------------------------------------------------------------*/
/* ISW 2003/03/07                                                    */
/* 3505 Byte 1 Sense Codes                                           */
/*-------------------------------------------------------------------*/
#define SENSE1_RDR_PERM         0x80 /* Permanent Err key depressed  */
#define SENSE1_RDR_AUTORETRY    0x40 /* Don't know                   */
#define SENSE1_RDR_MOTIONMF     0x20 /* Motion Malfunction           */
#define SENSE1_RDR_RAIC         0x10 /* Retry After Intreq Cleared   */
/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define CARD_SIZE        80
#define HEX40            ((BYTE)0x40)

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int cardrdr_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
int     i;                              /* Array subscript           */
int     fc;                             /* File counter              */

    int sockdev = 0;

    if (dev->bs)
    {
        if (!unbind_device(dev))
        {
            // (error message already issued)
            return -1;
        }
    }

    /* Initialize device dependent fields */

    dev->fd = -1;
    dev->fh = NULL;
    dev->multifile = 0;
    dev->ebcdic = 0;
    dev->ascii = 0;
    dev->trunc = 0;
    dev->cardpos = 0;
    dev->cardrem = 0;
    dev->autopad = 0;

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x2501;

    fc = 0;

    if (dev->more_files) free (dev->more_files);

    dev->more_files = malloc(sizeof(char*) * (fc + 1));

    if (!dev->more_files)
    {
        logmsg (_("HHCRD001E Out of memory\n"));
        return -1;
    }

    dev->more_files[fc] = NULL;

    /* Process the driver arguments starting with the SECOND
       argument. (The FIRST argument is the filename and is
       checked later further below.) */

    for (i = 1; i < argc; i++)
    {
        /* sockdev means the device file is actually
           a connected socket instead of a disk file.
           The file name is the socket_spec (host:port)
           to listen for connections on. */

        if (strcasecmp(argv[i], "sockdev") == 0)
        {
            sockdev = 1;
            continue;
        }

        /* multifile means to automatically open the next
           i/p file if multiple i/p files are defined.   */

        if (strcasecmp(argv[i], "multifile") == 0)
        {
            dev->multifile = 1;
            continue;
        }

        /* eof means that unit exception will be returned at
           end of file, instead of intervention required */

        if (strcasecmp(argv[i], "eof") == 0)
        {
            dev->rdreof = 1;
            continue;
        }

        /* intrq means that intervention required will be returned at
           end of file, instead of unit exception */

        if (strcasecmp(argv[i], "intrq") == 0)
        {
            dev->rdreof = 0;
            continue;
        }

        /* ebcdic means that the card image file consists of
           fixed length 80-byte EBCDIC card images with no
           line-end delimiters */

        if (strcasecmp(argv[i], "ebcdic") == 0)
        {
            dev->ebcdic = 1;
            continue;
        }

        /* ascii means that the card image file consists of
           variable length ASCII records delimited by either
           line-feed or carriage-return line-feed sequences */

        if (strcasecmp(argv[i], "ascii") == 0)
        {
            dev->ascii = 1;
            continue;
        }

        /* trunc means that records longer than 80 bytes will
           be silently truncated to 80 bytes when processing a
           variable length ASCII file.  The default behaviour
           is to present a data check if an overlength record
           is encountered.  The trunc option is ignored except
           when processing an ASCII card image file. */

        if (strcasecmp(argv[i], "trunc") == 0)
        {
            dev->trunc = 1;
            continue;
        }

        /* autopad means that if reading fixed sized records
         * (ebcdic) and end of file is reached in the middle of
         * a record, the record is automatically padded to 80 bytes.
         */

        if (strcasecmp(argv[i], "autopad") == 0)
        {
            dev->autopad = 1;
            continue;
        }

        // add additional file arguments

        if (strlen(argv[i]) > sizeof(dev->filename)-1)
        {
            logmsg (_("HHCRD002E File name too long (max=%ud): \"%s\"\n"),
                (unsigned int)sizeof(dev->filename)-1,argv[i]);
            return -1;
        }

        if (access(argv[i], R_OK | F_OK) != 0)
        {
            logmsg (_("HHCRD003E Unable to access file \"%s\": %s\n"),
                argv[i], strerror(errno));
            return -1;
        }

        dev->more_files[fc++] = strdup(argv[i]);
        dev->more_files = realloc(dev->more_files, sizeof(char*) * (fc + 1));

        if (!dev->more_files)
        {
            logmsg (_("HHCRD004E Out of memory\n"));
            return -1;
        }

        dev->more_files[fc] = NULL;
    }

    dev->current_file = dev->more_files;

    /* Check for conflicting arguments */

    if (dev->ebcdic && dev->ascii)
    {
        logmsg (_("HHCRD005E Specify 'ascii' or 'ebcdic' (or neither) but"
                  " not both\n"));
        return -1;
    }

    if (sockdev)
    {
        if (fc)
        {
            logmsg (_("HHCRD006E Only one filename (sock_spec) allowed for"
                      " socket devices\n"));
            return -1;
        }

        // If neither ascii nor ebcdic is specified, default to ascii.
        // This is required for socket devices because the open logic,
        // if neither is specified, attempts to determine whether the data
        // is actually ascii or ebcdic by reading the 1st 160 bytes of
        // data and then rewinding to the beginning of the file afterwards.
        //  Since you can't "rewind" a socket, we must therefore default
        // to one of them.

        if (!dev->ebcdic && !dev->ascii)
        {
            logmsg (_("HHCRD007I Defaulting to 'ascii' for socket device"
                      " %4.4X\n"),dev->devnum);
            dev->ascii = 1;
        }
    }

    if (dev->multifile && !fc)
    {
        logmsg (_("HHCRD008W 'multifile' option ignored: only one file"
                  " specified\n"));
        dev->multifile = 0;
    }

    /* The first argument is the file name */

    if (argc > 0)
    {
        /* Check for valid file name */

        if (strlen(argv[0]) > sizeof(dev->filename)-1)
        {
            logmsg (_("HHCRD009E File name too long (max=%ud): \"%s\"\n"),
                (unsigned int)sizeof(dev->filename)-1,argv[0]);
            return -1;
        }

        if (!sockdev)
        {
            /* Check for specification of no file mounted on reader */
            if (argv[0][0] == '*')
            {
                dev->filename[0] = '\0';
            }
            else if (access(argv[0], R_OK | F_OK) != 0)
            {
                logmsg (_("HHCRD010E Unable to access file \"%s\": %s\n"),
                    argv[0], strerror(errno));
                return -1;
            }
        }

        /* Save the file name in the device block */

        strcpy (dev->filename, argv[0]);
    }
    else
    {
        dev->filename[0] = '\0';
    }

    /* Set size of i/o buffer */

    dev->bufsize = CARD_SIZE;

    /* Set number of sense bytes */

    /* ISW 20030307 : Empirical knowledge : DOS/VS R34 Erep */
    /*                indicates 4 bytes in 3505 sense       */
    dev->numsense = 4;

    /* Initialize the device identifier bytes */

    dev->devid[0] = 0xFF;
    dev->devid[1] = 0x28; /* Control unit type is 2821-1 */
    dev->devid[2] = 0x21;
    dev->devid[3] = 0x01;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x01;
    dev->numdevid = 7;

    // If socket device, create a listening socket
    // to accept connections on.

    if (sockdev && !bind_device(dev,dev->filename))
    {
        // (error message already issued)
        return -1;
    }

    return 0;
} /* end function cardrdr_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void cardrdr_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    *class = "RDR";

    snprintf (buffer, buflen, "%s%s%s%s%s%s%s%s",
        ((dev->filename[0] == '\0') ? "*"          : (char *)dev->filename),
        (dev->bs ?                    " sockdev"   : ""),
        (dev->multifile ?             " multifile" : ""),
        (dev->ascii ?                 " ascii"     : ""),
        (dev->ebcdic ?                " ebcdic"    : ""),
        (dev->autopad ?               " autopad"   : ""),
        ((dev->ascii && dev->trunc) ? " trunc"     : ""),
        (dev->rdreof ?                " eof"       : " intrq"));

} /* end function cardrdr_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int cardrdr_close_device ( DEVBLK *dev )
{
    /* Close the device file */

    if (0
        || (  dev->bs && dev->fd >=  0   && close_socket( dev->fd ) <  0 )
        || ( !dev->bs && dev->fh != NULL &&    fclose(    dev->fh ) != 0 )
    )
    {
        int errnum = dev->bs ? get_HSO_errno() : errno;
        logmsg (_("HHCRD011E Close error on file \"%s\": %s\n"),
            dev->filename, strerror(errnum));
        dev->fd = -1;
        dev->fh = NULL;
        return -1;
    }

    if (dev->bs && (dev->bs->clientip || dev->bs->clientname))
    {
        logmsg (_("HHCRD012I %s (%s) disconnected from device %4.4X (%s)\n"),
            dev->bs->clientip, dev->bs->clientname, dev->devnum, dev->bs->spec);
    }

    dev->fd = -1;
    dev->fh = NULL;

    return 0;
} /* end function cardrdr_close_device */


/*-------------------------------------------------------------------*/
/* Clear the card reader                                             */
/*-------------------------------------------------------------------*/
static int clear_cardrdr ( DEVBLK *dev )
{
    /* Close the card image file */
    if (cardrdr_close_device(dev) != 0) return -1;

    if (dev->bs) return 0;

    /* Clear the file name */
    dev->filename[0] = '\0';

    /* If next file is available, open it */
    if (dev->current_file && *(dev->current_file))
    {
        strcpy(dev->filename, *(dev->current_file++));
    }
    else
    {
        /* Reset the device dependent flags */
        dev->multifile = 0;
        dev->ascii = 0;
        dev->ebcdic = 0;
//      dev->rdreof = 0;
        dev->trunc = 0;
        dev->autopad = 0;
    }

    return 0;
} /* end function clear_cardrdr */


/*-------------------------------------------------------------------*/
/* Open the card image file                                          */
/*-------------------------------------------------------------------*/
static int open_cardrdr ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     len;                            /* Length of data            */
BYTE    buf[160];                       /* Auto-detection buffer     */
char    pathname[MAX_PATH];             /* file path in host format  */

    *unitstat = 0;

    // Socket device?

    if (dev->bs)
    {
        // Intervention required if no one has connected yet

        if (dev->fd == -1)
        {
            if(dev->rdreof)
            {
                *unitstat=CSW_CE|CSW_DE|CSW_UX;
                return -1;
            }
            dev->sense[0] = SENSE_IR;
            dev->sense[1] = SENSE1_RDR_RAIC; /* Retry when IntReq Cleared */
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        return 0;
    }

    /* Intervention required if device has no file name */
    if (dev->filename[0] == '\0')
    {
        if(dev->rdreof)
        {
            *unitstat=CSW_CE|CSW_DE|CSW_UX;
            return -1;
        }
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_RDR_RAIC; /* Retry when IntReq Cleared */
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Open the device file */
    hostpath(pathname, dev->filename, sizeof(pathname));
    rc = open (pathname, O_RDONLY | O_BINARY);
    if (rc < 0)
    {
        /* Handle open failure */
        logmsg (_("HHCRD013E Error opening file %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Save the file descriptor in the device block */
    dev->fd = rc;
    dev->fh = fdopen(dev->fd, "rb");

    /* If neither EBCDIC nor ASCII was specified, attempt to
       detect the format by inspecting the first 160 bytes */
    if (dev->ebcdic == 0 && dev->ascii == 0)
    {
        /* Read first 160 bytes of file into the buffer */
        len = fread(buf, 1, sizeof(buf), dev->fh);
        if (len < 0)
        {
            /* Handle read error condition */
            logmsg (_("HHCRD014E Error reading file %s: %s\n"),
                    dev->filename, strerror(errno));

            /* Close the file */
            fclose(dev->fh);
            dev->fd = -1;
            dev->fh = NULL;

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Assume ASCII format if first 160 bytes contain only ASCII
           characters, carriage return, line feed, tab, or EOF */
        for (i = 0, dev->ascii = 1; i < len && buf[i] != '\x1A'; i++)
        {
            if ((buf[i] < 0x20 || buf[i] > 0x7F)
                && buf[i] != '\r' && buf[i] != '\n'
                && buf[i] != '\t')
            {
                dev->ascii = 0;
                dev->ebcdic = 1;
                break;
            }
        } /* end for(i) */

        /* Rewind to start of file */
        rc = fseek (dev->fh, 0, SEEK_SET);
        if (rc < 0)
        {
            /* Handle seek error condition */
            logmsg (_("HHCRD015E Seek error in file %s: %s\n"),
                    dev->filename, strerror(errno));

            /* Close the file */
            fclose (dev->fh);
            dev->fd = -1;
            dev->fh = NULL;

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

    } /* end if(auto-detect) */

    ASSERT(dev->fd != -1 && dev->fh);

    return 0;
} /* end function open_cardrdr */

/*-------------------------------------------------------------------*/
/* Read an 80-byte EBCDIC card image into the device buffer          */
/*-------------------------------------------------------------------*/
static int read_ebcdic ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */

    /* Read 80 bytes of card image data into the device buffer */
    if (dev->bs)
        rc = read_socket( dev->fd, dev->buf, CARD_SIZE );
    else
        rc = fread(dev->buf, 1, CARD_SIZE, dev->fh);

    if ((rc > 0) && (rc < CARD_SIZE) && dev->autopad)
    {
        memset(&dev->buf[rc], 0, CARD_SIZE - rc);
        rc = CARD_SIZE;
    }
    else if /* Check for End of file */
    (0
        || ( dev->bs && rc <= 0)
        || (!dev->bs && feof(dev->fh))
    )
    {
        /* Return unit exception or intervention required */
        if (dev->rdreof)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        }
        else
        {
            dev->sense[0] = SENSE_IR;
            dev->sense[1] = SENSE1_RDR_RAIC; /* Retry when IntReq Cleared */
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }

        /* Close the file and clear the file name and flags */
        if (clear_cardrdr(dev) != 0)
        {
            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        return -2;
    }

    /* Handle read error condition */
    if (rc < CARD_SIZE)
    {
        if (rc < 0)
            logmsg (_("HHCRD016E Error reading file %s: %s\n"),
                    dev->filename, strerror(errno));
        else
            logmsg (_("HHCRD017E Unexpected end of file on %s\n"),
                    dev->filename);

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    return 0;
} /* end function read_ebcdic */


/*-------------------------------------------------------------------*/
/* Read a variable length ASCII card image into the device buffer    */
/*-------------------------------------------------------------------*/
static int read_ascii ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
BYTE    c = 0;                          /* Input character           */

    /* Prefill the card image with EBCDIC blanks */
    memset (dev->buf, HEX40, CARD_SIZE);

    /* Read up to 80 bytes into device buffer */
    for (i = 0; ; )
    {
        /* Read next byte of card image */
        if (dev->bs)
        {
            BYTE b; rc = read_socket( dev->fd, &b, 1 );
            if (rc <= 0) rc = EOF; else c = b;
        }
        else
        {
            rc = getc(dev->fh);
            c = (BYTE)rc;
        }

        /* Handle end-of-file condition */
        if (rc == EOF || c == '\x1A')
        {
            /* End of record if there is any data in buffer */
            if (i > 0) break;

            /* Return unit exception or intervention required */
            if (dev->rdreof)
            {
                *unitstat = CSW_CE | CSW_DE | CSW_UX;
            }
            else
            {
                dev->sense[0] = SENSE_IR;
                dev->sense[1] = SENSE1_RDR_RAIC; /* Retry when IntReq Cleared */
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
            }

            /* Close the file and clear the file name and flags */
            if (clear_cardrdr(dev) != 0)
            {
                /* Set unit check with equipment check */
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                return -1;
            }

            return -2;
        }

        /* Handle read error condition */
        if (rc < 0)
        {
            logmsg (_("HHCRD018E Error reading file %s: %s\n"),
                    dev->filename, strerror(errno));

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Ignore carriage return */
        if (c == '\r') continue;

        /* Line-feed indicates end of variable length record */
        if (c == '\n') break;

        /* Expand tabs to spaces */
        if (c == '\t')
        {
            do {i++;} while ((i & 7) && (i < CARD_SIZE));
            continue;
        }

        /* Test for overlength record */
        if (i >= CARD_SIZE)
        {
            /* Ignore excess characters if trunc option specified */
            if (dev->trunc) continue;

            logmsg (_("HHCRD019E Card image exceeds %d bytes in file %s\n"),
                    CARD_SIZE, dev->filename);

            /* Set unit check with data check */
            dev->sense[0] = SENSE_DC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Convert character to EBCDIC and store in device buffer */
        dev->buf[i++] = host_to_guest(c);

    } /* end for(i) */

    return 0;
} /* end function read_ascii */


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void cardrdr_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     num;                            /* Number of bytes to move   */

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Open the device file if necessary */
    if ( !IS_CCW_SENSE(code) &&
        (dev->fd < 0 || (!dev->bs && !dev->fh)))
    {
        rc = open_cardrdr (dev, unitstat);
        if (rc) return;
    }

    /* Turn all read/feed commands into read, feed, select stacker 1 */
    if ((code & 0x17) == 0x02) code = 0x02;

    /* Turn all feed-only commands into NOP. This is ugly, and should
        really be thought out more. --JRM */
    if ((code & 0x37) == 0x23) code = 0x03;

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
        /* Read next card if not data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            for (;;)
            {
                /* Read ASCII or EBCDIC card image */
                if (dev->ascii)
                        rc = read_ascii (dev, unitstat);
                else
                        rc = read_ebcdic (dev, unitstat);

                if (0
                        || rc != -2
                        || !dev->multifile
                        || open_cardrdr (dev, unitstat) != 0
                        )
                break;
            }

            /* Return error status if read was unsuccessful */
            if (rc) break;

            /* Initialize number of bytes in current card */
            dev->cardpos = 0;
            dev->cardrem = CARD_SIZE;

        } /* end if(!data-chained) */

        /* Calculate number of bytes to read and set residual count */
        num = (count < dev->cardrem) ? count : dev->cardrem;
        *residual = count - num;
        if (count < dev->cardrem) *more = 1;

        /* Copy data from card image buffer into channel buffer */
        memcpy (iobuf, dev->buf + dev->cardpos, num);

        /* Update number of bytes remaining in card image buffer */
        dev->cardpos += num;
        dev->cardrem -= num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
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

        /* If sense is clear AND filename = "" OR sockdev and fd=-1 */
        /* Put an IR sense - so that an unsolicited sense can see the intreq */
        if(dev->sense[0]==0)
        {
            if(dev->filename[0]==0x00 ||
                    (dev->bs && dev->fd==-1))
            {
                dev->sense[0] = SENSE_IR;
                dev->sense[1] = SENSE1_RDR_RAIC; /* Retry when IntReq Cleared */
            }
        }

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

} /* end function cardrdr_execute_ccw */


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND cardrdr_device_hndinfo = {
        &cardrdr_init_handler,         /* Device Initialisation      */
        &cardrdr_execute_ccw,          /* Device CCW execute         */
        &cardrdr_close_device,         /* Device Close               */
        &cardrdr_query_device,         /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt3505_LTX_hdl_ddev
#define hdl_depc hdt3505_LTX_hdl_depc
#define hdl_reso hdt3505_LTX_hdl_reso
#define hdl_init hdt3505_LTX_hdl_init
#define hdl_fini hdt3505_LTX_hdl_fini
#endif


#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION


#if defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  #undef sysblk
  HDL_RESOLVER_SECTION;
  {
    HDL_RESOLVE_PTRVAR( psysblk, sysblk );
  }
  END_RESOLVER_SECTION
#endif


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(1442, cardrdr_device_hndinfo );
    HDL_DEVICE(2501, cardrdr_device_hndinfo );
    HDL_DEVICE(3505, cardrdr_device_hndinfo );
}
END_DEVICE_SECTION
#endif
