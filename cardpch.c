/* CARDPCH.C    (c) Copyright Roger Bowler, 1999-2003                */
/*              ESA/390 Card Punch Device Handler                    */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* System/370 card punch devices.                                    */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "devtype.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define CARD_LENGTH     80
#define SPACE           ((BYTE)' ')
#define HEX40           ((BYTE)0x40)

/*-------------------------------------------------------------------*/
/* Subroutine to write data to the card punch                        */
/*-------------------------------------------------------------------*/
static void
write_buffer (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write data to the output file */
    rc = write (dev->fd, buf, len);

    /* Equipment check if error writing to output file */
    if (rc < len)
    {
        logmsg (_("HHCPU004E Error writing to %s: %s\n"),
                dev->filename,
                (errno == 0 ? "incomplete": strerror(errno)));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

} /* end function write_buffer */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int cardpch_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
int     i;                              /* Array subscript           */

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        logmsg (_("HHCPU001E File name missing or invalid\n"));
        return -1;
    }

    /* Save the file name in the device block */
    safe_strcpy (dev->filename, sizeof(dev->filename), argv[0]);

    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->ascii = 0;
    dev->crlf = 0;
    dev->cardpos = 0;
    dev->cardrem = CARD_LENGTH;

    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "ascii") == 0)
        {
            dev->ascii = 1;
            continue;
        }

        if (strcasecmp(argv[i], "ebcdic") == 0)
        {
            dev->ascii = 0;
            continue;
        }

        if (strcasecmp(argv[i], "crlf") == 0)
        {
            dev->crlf = 1;
            continue;
        }

        logmsg (_("HHCPU002E Invalid argument: %s\n"),
                argv[i]);
        return -1;
    }

    /* Set length of buffer */
    dev->bufsize = CARD_LENGTH + 2;

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
} /* end function cardpch_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void cardpch_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "PCH";
    snprintf (buffer, buflen, "%s%s%s",
                dev->filename,
                (dev->ascii ? " ascii" : " ebcdic"),
                ((dev->ascii && dev->crlf) ? " crlf" : ""));

} /* end function cardpch_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int cardpch_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function cardpch_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void cardpch_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             i;                      /* Loop counter              */
int             num;                    /* Number of bytes to move   */
BYTE            c;                      /* Output character          */

    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Open the device file if necessary */
    if (dev->fd < 0 && !IS_CCW_SENSE(code))
    {
        rc = open (dev->filename,
                    O_WRONLY | O_CREAT | O_TRUNC /* | O_SYNC */ |  O_BINARY,
                    S_IRUSR | S_IWUSR | S_IRGRP);
        if (rc < 0)
        {
            /* Handle open failure */
            logmsg (_("HHCPU003E Error opening file %s: %s\n"),
                    dev->filename, strerror(errno));

            /* Set unit check with intervention required */
            dev->sense[0] = SENSE_IR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }
        dev->fd = rc;
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    case 0x41:
    case 0x81:
    /*---------------------------------------------------------------*/
    /* WRITE, FEED, SELECT STACKER                                   */
    /*---------------------------------------------------------------*/
        /* Start a new record if not data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            dev->cardpos = 0;
            dev->cardrem = CARD_LENGTH;

        } /* end if(!data-chained) */

        /* Calculate number of bytes to write and set residual count */
        num = (count < dev->cardrem) ? count : dev->cardrem;
        *residual = count - num;

        /* Copy data from channel buffer to card buffer */
        for (i = 0; i < num; i++)
        {
            c = iobuf[i];

            if (dev->ascii)
            {
                c = guest_to_host(c);
                if (!isprint(c)) c = SPACE;
            }

            dev->buf[dev->cardpos] = c;
            dev->cardpos++;
            dev->cardrem--;
        } /* end for(i) */

        /* Perform end of record processing if not data-chaining */
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            if (dev->ascii)
            {
                /* Truncate trailing blanks from card buffer */
                for (i = dev->cardpos; i > 0; i--)
                    if (dev->buf[i-1] != SPACE) break;

                /* Append carriage return and line feed */
                if (dev->crlf) dev->buf[i++] = '\r';
                dev->buf[i++] = '\n';
            }
            else
            {
                /* Pad card image with blanks */
                for (i = dev->cardpos; i < CARD_LENGTH; i++)
                    dev->buf[i] = HEX40;
            }

            /* Write card image */
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

} /* end function cardpch_execute_ccw */


DEVHND cardpch_device_hndinfo = {
        &cardpch_init_handler,
        &cardpch_execute_ccw,
        &cardpch_close_device,
        &cardpch_query_device,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
