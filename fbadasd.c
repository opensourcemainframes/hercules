/* FBADASD.C    (c) Copyright Roger Bowler, 1999-2011                */
/*              ESA/390 FBA Direct Access Storage Device Handler     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* fixed block architecture direct access storage devices.           */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      0671 device support by Jay Maynard                           */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _FBADASD_C_
#define _HDASD_DLL_

#include "hercules.h"
#include "dasdblks.h"  // (need #define DEFAULT_FBA_TYPE)
#include "sr.h"

/*-------------------------------------------------------------------*/
/* Bit definitions for Define Extent file mask                       */
/*-------------------------------------------------------------------*/
#define FBAMASK_CTL             0xC0    /* Operation control bits... */
#define FBAMASK_CTL_INHFMT      0x00    /* ...inhibit format writes  */
#define FBAMASK_CTL_INHWRT      0x40    /* ...inhibit all writes     */
#define FBAMASK_CTL_ALLWRT      0xC0    /* ...allow all writes       */
#define FBAMASK_CTL_RESV        0x80    /* ...reserved bit setting   */
#define FBAMASK_CE              0x08    /* CE field extent           */
#define FBAMASK_DIAG            0x04    /* Permit diagnostic command */
#define FBAMASK_RESV            0x33    /* Reserved bits - must be 0 */

/*-------------------------------------------------------------------*/
/* Bit definitions for Locate operation byte                         */
/*-------------------------------------------------------------------*/
#define FBAOPER_RESV            0xE0    /* Reserved bits - must be 0 */
                                        /* Note : Bit 3 seems to be  */
                                        /* The "suppress" bit but is */
                                        /* apparently ignored        */
                                        /* It is therefore valid but */
                                        /* not used.                 */
#define FBAOPER_CODE            0x0F    /* Operation code bits...    */
#define FBAOPER_WRITE           0x01    /* ...write data             */
#define FBAOPER_READREP         0x02    /* ...read replicated data   */
#define FBAOPER_FMTDEFC         0x04    /* ...format defective block */
#define FBAOPER_WRTVRFY         0x05    /* ...write data and verify  */
#define FBAOPER_READ            0x06    /* ...read data              */

#define FBA_BLKGRP_SIZE  (120 * 512)    /* Size of block group       */

static int fba_read (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat);

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int fbadasd_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
int     rc;                             /* Return code               */
struct  stat statbuf;                   /* File information          */
int     startblk;                       /* Device origin block number*/
int     numblks;                        /* Device block count        */
BYTE    c;                              /* Character work area       */
char   *cu = NULL;                      /* Specified control unit    */
char   *kw;                             /* Argument keyword          */
int     cfba = 0;                       /* 1 = Compressed fba        */
int     i;                              /* Loop index                */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed device header  */
char    pathname[MAX_PATH];             /* file path in host format  */
char   *strtok_str = NULL;              /* save last position        */

    if (!dev->typname || !sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = DEFAULT_FBA_TYPE;

    /* reset excps count */
    dev->excps = 0;

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) >= sizeof(dev->filename))
    {
        WRMSG (HHC00500, "E", SSID_TO_LCSS(dev->ssid), dev->devnum);
        return -1;
    }

    /* Save the file name in the device block */
    hostpath(dev->filename, argv[0], sizeof(dev->filename));

#if defined( OPTION_SHOWDVOL1 )
    /* Initialize 'dev->dasdvol' field (VOL1 label == volser) */
    {
        CIFBLK* pCIF;
        char dasdvol[7] = {0};
        if (!dev->showdvol1) /* (first time here for this open?) */
        {
            char* sfname = NULL;
            if (dev->dasdsfn)
            {
                /* (N.B. must NOT have end quote, ONLY begin quote) */
                int sfnlen = (int)(strlen( "sf=\"" ) + strlen( dev->dasdsfn ) + 1);
                sfname = malloc( sfnlen );
                strlcpy( sfname, "sf=\"", sfnlen );
                strlcat( sfname, dev->dasdsfn, sfnlen );
            }
            pCIF = open_fba_image( dev->filename, sfname, O_RDONLY | O_BINARY,
                IMAGE_OPEN_DVOL1 | IMAGE_OPEN_QUIET );
            if (sfname)
                free( sfname );
            if (pCIF)
            {
                const int vol1sector = 1;
                const BYTE vol1[4] = { 0xE5, 0xD6, 0xD3, 0xF1 }; /* "VOL1" Standard OS volume marker */
                const BYTE cmse[4] = { 0xC3, 0xD4, 0xE2, 0x7E }; /* "CMS=" CMS */
                const BYTE cms1[4] = { 0xC3, 0xD4, 0xE2, 0xF1 }; /* "CMS1" CMS EDF */
                const BYTE dir1[4] = { 0xC4, 0xC9, 0xD9, 0xF1 }; /* "DIR1" Recognized by VM */
                const BYTE lnx1[4] = { 0xD3, 0xD5, 0xE7, 0xF1 }; /* "LNX1" Linux */
                BYTE vol1data[10];
                BYTE unitstat;
                pCIF->devblk.fbamask   = 0;
                pCIF->devblk.fbaxblkn  = 0;
                pCIF->devblk.fbaxfirst = 0;
                pCIF->devblk.fbaxlast  = pCIF->devblk.fbanumblk - 1;
                pCIF->devblk.fbarba    = vol1sector * pCIF->devblk.fbablksiz;
                rc = fba_read( &pCIF->devblk, vol1data, 10, &unitstat );
                if (rc == 10 && (0
                    || memcmp( vol1data, vol1, 4 ) == 0
                    || memcmp( vol1data, cmse, 4 ) == 0
                    || memcmp( vol1data, cms1, 4 ) == 0
                    || memcmp( vol1data, dir1, 4 ) == 0
                    || memcmp( vol1data, lnx1, 4 ) == 0
                ))
                    make_asciiz( dasdvol, 7, vol1data+4, 6 );
                close_image_file( pCIF );
                strlcpy( dev->dasdvol, dasdvol, sizeof( dev->dasdvol ));
            }
        }
    }
#endif /* defined( OPTION_SHOWDVOL1 ) */

    /* Device is shareable */
    dev->shared = 1;

    /* Check for possible remote device */
    if (stat(dev->filename, &statbuf) < 0)
    {
        rc = shared_fba_init ( dev, argc, argv);
        if (rc < 0)
        {
            WRMSG (HHC00501, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename);
            return -1;
        }
        else
            return rc;
    }

    /* Open the device file */
    dev->fd = HOPEN (dev->filename, O_RDWR|O_BINARY);
    if (dev->fd < 0)
    {
        dev->fd = HOPEN (dev->filename, O_RDONLY|O_BINARY);
        if (dev->fd < 0)
        {
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "open()", strerror(errno));
            return -1;
        }
    }

    /* Read the first block to see if it's compressed */
    rc = read (dev->fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < (int)CKDDASD_DEVHDR_SIZE)
    {
        /* Handle read error condition */
        if (rc < 0)
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "read()", strerror(errno));
        else
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "read()", "unexpected end of file");
        close (dev->fd);
        dev->fd = -1;
        return -1;
    }

    /* Processing for compressed fba dasd */
    if (memcmp (&devhdr.devid, "FBA_C370", 8) == 0)
    {
        cfba = 1;

        /* Read the compressed device header */
        rc = read (dev->fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
        if (rc < (int)CKDDASD_DEVHDR_SIZE)
        {
            /* Handle read error condition */
            if (rc < 0)
                WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "read()", strerror(errno));
            else
                WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "read()", "unexpected end of file");
            close (dev->fd);
            dev->fd = -1;
            return -1;
        }

        /* Set block size, device origin, and device size in blocks */
        dev->fbablksiz = 512;
        dev->fbaorigin = 0;
        dev->fbanumblk = ((U32)(cdevhdr.cyls[3]) << 24)
                       | ((U32)(cdevhdr.cyls[2]) << 16)
                       | ((U32)(cdevhdr.cyls[1]) << 8)
                       |  (U32)(cdevhdr.cyls[0]);

        /* Default to synchronous I/O */
//FIXME: syncio is reported to be broken for fba
//      dev->syncio = 1;

        /* process the remaining arguments */
        for (i = 1; i < argc; i++)
        {
            if (strlen (argv[i]) > 3
             && memcmp ("sf=", argv[i], 3) == 0)
            {
                if ('\"' == argv[i][3]) argv[i]++;
                hostpath(pathname, argv[i]+3, sizeof(pathname));
                dev->dasdsfn = strdup(pathname);
                if (dev->dasdsfn)
                {
                    /* Set the pointer to the suffix character */
                    dev->dasdsfx = strrchr (dev->dasdsfn, PATHSEPC);
                    if (dev->dasdsfx == NULL)
                        dev->dasdsfx = dev->dasdsfn + 1;
                    dev->dasdsfx = strchr (dev->dasdsfx, '.');
                    if (dev->dasdsfx == NULL)
                        dev->dasdsfx = dev->dasdsfn + strlen(dev->dasdsfn);
                    dev->dasdsfx--;
                }
                continue;
            }
            if (strlen (argv[i]) > 3
             && memcmp("cu=", argv[i], 3) == 0)   /* support for cu= added but  */
            {                                     /* is ignored for the present */
                kw = strtok_r (argv[i], "=", &strtok_str );
                cu = strtok_r (NULL, " \t", &strtok_str );
                continue;
            }
            if (strcasecmp ("nosyncio", argv[i]) == 0
             || strcasecmp ("nosyio",   argv[i]) == 0)
            {
                dev->syncio = 0;
                continue;
            }
            if (strcasecmp ("syncio", argv[i]) == 0
             || strcasecmp ("syio",   argv[i]) == 0)
            {
                dev->syncio = 1;
                continue;
            }

            WRMSG (HHC00503, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[i], i + 1);
            return -1;
        }
    }

    /* Processing for regular fba dasd */
    else
    {
        /* Determine the device size */
        rc = fstat (dev->fd, &statbuf);
        if (rc < 0)
        {
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "fstat()", strerror(errno));
            close (dev->fd);
            dev->fd = -1;
            return -1;
        }
#if defined(OPTION_FBA_BLKDEVICE) && defined(BLKGETSIZE)
        if(S_ISBLK(statbuf.st_mode))
        {
            rc=ioctl(dev->fd,BLKGETSIZE,&statbuf.st_size);
            if(rc<0)
            {
                WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "ioctl()", strerror(errno));
                close (dev->fd);
                dev->fd = -1;
                return -1;
            }
            dev->fbablksiz = 512;
            dev->fbaorigin = 0;
            dev->fbanumblk = statbuf.st_size;
            if (!dev->quiet)
                WRMSG (HHC00504, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename);
        }
        else
#endif // defined(OPTION_FBA_BLKDEVICE) && defined(BLKGETSIZE)
        {
            /* Set block size, device origin, and device size in blocks */
            dev->fbablksiz = 512;
            dev->fbaorigin = 0;
            dev->fbanumblk = statbuf.st_size / dev->fbablksiz;
        }

        /* The second argument is the device origin block number */
        if (argc >= 2)
        {
            if (sscanf(argv[1], "%u%c", &startblk, &c) != 1
             || startblk >= dev->fbanumblk)
            {
                WRMSG (HHC00505, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, argv[1]);
                close (dev->fd);
                dev->fd = -1;
                return -1;
            }
            dev->fbaorigin = (off_t)startblk;
            dev->fbanumblk -= startblk;
        }

        /* The third argument is the device block count */
        if (argc >= 3 && strcmp(argv[2],"*") != 0)
        {
            if (sscanf(argv[2], "%u%c", &numblks, &c) != 1
             || numblks > dev->fbanumblk)
            {
                WRMSG (HHC00506, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, argv[2]);
                close (dev->fd);
                dev->fd = -1;
                return -1;
            }
            dev->fbanumblk = numblks;
        }
    }
    dev->fbaend = (dev->fbaorigin + dev->fbanumblk) * dev->fbablksiz;

    if (!dev->quiet)
        WRMSG (HHC00507, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, dev->fbaorigin, dev->fbanumblk);

    /* Set number of sense bytes */
    dev->numsense = 24;

    /* Locate the FBA dasd table entry */
    dev->fbatab = dasd_lookup (DASD_FBADEV, NULL, dev->devtype, dev->fbanumblk);
    if (dev->fbatab == NULL)
    {
        WRMSG (HHC00508, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, dev->devtype);
        close (dev->fd);
        dev->fd = -1;
        return -1;
    }

    /* Build the devid area */
    dev->numdevid = dasd_build_fba_devid (dev->fbatab,(BYTE *)&dev->devid);

    /* Build the devchar area */
    dev->numdevchar = dasd_build_fba_devchar (dev->fbatab,
                                 (BYTE *)&dev->devchar,dev->fbanumblk);

    /* Initialize current blkgrp and cache entry */
    dev->bufcur = dev->cache = -1;

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    /* Call the compressed init handler if compressed fba */
    if (cfba)
        return cckddasd_init_handler (dev, argc, argv);

    return 0;
} /* end function fbadasd_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void fbadasd_query_device (DEVBLK *dev, char **devclass,
                int buflen, char *buffer)
{
    CCKDDASD_EXT    *cckd;
    char            devname[ 6 + 1 + MAX_PATH + 1 ] = {0};

    BEGIN_DEVICE_CLASS_QUERY( "DASD", dev, devclass, buflen, buffer );

    switch (sysblk.showdvol1)
    {
    default:
    case SHOWDVOL1_NO:
        MSGBUF( devname, "%s", dev->filename );
        break;
    case SHOWDVOL1_YES:
        MSGBUF( devname, "%-6.6s %s", dev->dasdvol, dev->filename );
        break;
    case SHOWDVOL1_ONLY:
        MSGBUF( devname, "%-6.6s", dev->dasdvol );
        break;
    }

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        snprintf( buffer, buflen-1, "%s [%"I64_FMT"d,%d] IO[%"I64_FMT"u]",
                  devname,
                  dev->fbaorigin, dev->fbanumblk,
                  dev->excps);
    }
    else
    {
        snprintf( buffer, buflen-1, "%s [%"I64_FMT"d,%d] [%d sfs] IO[%"I64_FMT"u]",
                  devname,
                  dev->fbaorigin, dev->fbanumblk,
                  cckd->sfn,
                  dev->excps);
    }

} /* end function fbadasd_query_device */

/*-------------------------------------------------------------------*/
/* Calculate length of an FBA block group                            */
/*-------------------------------------------------------------------*/
static int fba_blkgrp_len (DEVBLK *dev, int blkgrp)
{
off_t   offset;                         /* Offset of block group     */

    offset = blkgrp * FBA_BLKGRP_SIZE;
    if (dev->fbaend - offset < FBA_BLKGRP_SIZE)
        return (int)(dev->fbaend - offset);
    else
        return FBA_BLKGRP_SIZE;
}

/*-------------------------------------------------------------------*/
/* Read fba block(s)                                                 */
/*-------------------------------------------------------------------*/
static int fba_read (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
int     rc;                             /* Return code               */
int     blkgrp;                         /* Block group number        */
int     blklen;                         /* Length left in block group*/
int     off;                            /* Device buffer offset      */
int     bufoff;                         /* Buffer offset             */
int     copylen;                        /* Length left to copy       */

    /* Command reject if referencing outside the volume */
    if (dev->fbarba < dev->fbaorigin * dev->fbablksiz
     || dev->fbarba + len > dev->fbaend)
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read the block group */
    blkgrp = dev->fbarba / FBA_BLKGRP_SIZE;
    rc = (dev->hnd->read) (dev, blkgrp, unitstat);
    if (rc < 0)
        return -1;

    off = dev->fbarba % FBA_BLKGRP_SIZE;
    blklen = dev->buflen - off;

    /* Initialize target buffer offset and length to copy */
    bufoff = 0;
    copylen = len;

    /* Access multiple block groups asynchronously */
    if (dev->syncio_active && copylen > blklen)
    {
        dev->syncio_retry = 1;
        return -1;
    }

    /* Copy from the device buffer to the target buffer */
    while (copylen > 0)
    {
        int len = copylen < blklen ? copylen : blklen;

        /* Copy to the target buffer */
        if (buf) memcpy (buf + bufoff, dev->buf + off, len);

        /* Update offsets and lengths */
        bufoff += len;
        copylen -= blklen;

        /* Read the next block group if still more to copy */
        if (copylen > 0)
        {
            blkgrp++;
            off = 0;
            rc = (dev->hnd->read) (dev, blkgrp, unitstat);
            if (rc < 0)
                return -1;
            blklen = fba_blkgrp_len (dev, blkgrp);
        }
    }

    /* Update the rba */
    dev->fbarba += len;

    return len;
} /* end function fba_read */

/*-------------------------------------------------------------------*/
/* Write fba block(s)                                                */
/*-------------------------------------------------------------------*/
static int fba_write (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
int     rc;                             /* Return code               */
int     blkgrp;                         /* Block group number        */
int     blklen;                         /* Length left in block group*/
int     off;                            /* Target buffer offset      */
int     bufoff;                         /* Source buffer offset      */
int     copylen;                        /* Length left to copy       */

    /* Command reject if referencing outside the volume */
    if (dev->fbarba < dev->fbaorigin * dev->fbablksiz
     || dev->fbarba + len > dev->fbaend)
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read the block group */
    blkgrp = dev->fbarba / FBA_BLKGRP_SIZE;
    rc = (dev->hnd->read) (dev, blkgrp, unitstat);
    if (rc < 0)
        return -1;

    off = dev->fbarba % FBA_BLKGRP_SIZE;
    blklen = dev->buflen - off;

    /* Initialize source buffer offset and length to copy */
    bufoff = 0;
    copylen = len;

    /* Access multiple block groups asynchronously */
    if (dev->syncio_active && copylen > blklen)
    {
        dev->syncio_retry = 1;
        return -1;
    }

    /* Copy to the device buffer from the target buffer */
    while (copylen > 0)
    {
        int len = copylen < blklen ? copylen : blklen;

        /* Write to the block group */
        rc = (dev->hnd->write) (dev, blkgrp, off, buf + bufoff,
                                len, unitstat);
        if (rc < 0)
            return -1;

        /* Update offsets and lengths */
        bufoff += len;
        copylen -= len;
        blkgrp++;
        off = 0;
        blklen = fba_blkgrp_len (dev, blkgrp);
    }

    /* Update the rba */
    dev->fbarba += len;

    return len;
} /* end function fba_write */

/*-------------------------------------------------------------------*/
/* FBA read block group exit                                         */
/*-------------------------------------------------------------------*/
static
int fbadasd_read_blkgrp (DEVBLK *dev, int blkgrp, BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             i, o;                   /* Cache indexes             */
int             len;                    /* Length to read            */
off_t           offset;                 /* File offsets              */

    /* Return if reading the same block group */
    if (blkgrp >= 0 && blkgrp == dev->bufcur)
        return 0;

    /* Write the previous block group if modified */
    if (dev->bufupd)
    {
        /* Retry if synchronous I/O */
        if (dev->syncio_active)
        {
            dev->syncio_retry = 1;
            return -1;
        }

        dev->bufupd = 0;

        /* Seek to the old block group offset */
        offset = (off_t)(((S64)dev->bufcur * FBA_BLKGRP_SIZE) + dev->bufupdlo);
        offset = lseek (dev->fd, offset, SEEK_SET);
        if (offset < 0)
        {
            /* Handle seek error condition */
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "lseek()", strerror(errno));
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            cache_lock(CACHE_DEVBUF);
            cache_setflag(CACHE_DEVBUF, dev->cache, ~FBA_CACHE_ACTIVE, 0);
            cache_unlock(CACHE_DEVBUF);
            dev->bufupdlo = dev->bufupdhi = 0;
            dev->bufcur = dev->cache = -1;
            return -1;
        }

        /* Write the portion of the block group that was modified */
        rc = write (dev->fd, dev->buf + dev->bufupdlo,
                    dev->bufupdhi - dev->bufupdlo);
        if (rc < dev->bufupdhi - dev->bufupdlo)
        {
            /* Handle write error condition */
            WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "write()", strerror(errno));
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            cache_lock(CACHE_DEVBUF);
            cache_setflag(CACHE_DEVBUF, dev->cache, ~FBA_CACHE_ACTIVE, 0);
            cache_unlock(CACHE_DEVBUF);
            dev->bufupdlo = dev->bufupdhi = 0;
            dev->bufcur = dev->cache = -1;
            return -1;
        }

        dev->bufupdlo = dev->bufupdhi = 0;
    }

    cache_lock (CACHE_DEVBUF);

    /* Make the previous cache entry inactive */
    if (dev->cache >= 0)
        cache_setflag(CACHE_DEVBUF, dev->cache, ~FBA_CACHE_ACTIVE, 0);
    dev->bufcur = dev->cache = -1;

    /* Return on special case when called by the close handler */
    if (blkgrp < 0)
    {
        cache_unlock (CACHE_DEVBUF);
        return 0;
    }

fba_read_blkgrp_retry:

    /* Search the cache */
    i = cache_lookup (CACHE_DEVBUF, FBA_CACHE_SETKEY(dev->devnum, blkgrp), &o);

    /* Cache hit */
    if (i >= 0)
    {
        cache_setflag(CACHE_DEVBUF, i, ~0, FBA_CACHE_ACTIVE);
        cache_setage(CACHE_DEVBUF, i);
        cache_unlock(CACHE_DEVBUF);

        logdevtr (dev, MSG(HHC00516, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, blkgrp, i));

        dev->cachehits++;
        dev->cache = i;
        dev->buf = cache_getbuf(CACHE_DEVBUF, dev->cache, 0);
        dev->bufcur = blkgrp;
        dev->bufoff = 0;
        dev->bufoffhi = fba_blkgrp_len (dev, blkgrp);
        dev->buflen = fba_blkgrp_len (dev, blkgrp);
        dev->bufsize = cache_getlen(CACHE_DEVBUF, dev->cache);
        return 0;
    }

    /* Retry if synchronous I/O */
    if (dev->syncio_active)
    {
        cache_unlock(CACHE_DEVBUF);
        dev->syncio_retry = 1;
        return -1;
    }

    /* Wait if no available cache entry */
    if (o < 0)
    {
        logdevtr (dev, MSG(HHC00517, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, blkgrp));
        dev->cachewaits++;
        cache_wait(CACHE_DEVBUF);
        goto fba_read_blkgrp_retry;
    }

    /* Cache miss */
    logdevtr (dev, MSG(HHC00518, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, blkgrp, o));

    dev->cachemisses++;

    /* Make this cache entry active */
    cache_setkey (CACHE_DEVBUF, o, FBA_CACHE_SETKEY(dev->devnum, blkgrp));
    cache_setflag(CACHE_DEVBUF, o, 0, FBA_CACHE_ACTIVE|DEVBUF_TYPE_FBA);
    cache_setage (CACHE_DEVBUF, o);
    dev->buf = cache_getbuf(CACHE_DEVBUF, o, FBA_BLKGRP_SIZE);
    cache_unlock (CACHE_DEVBUF);

    /* Get offset and length */
    offset = (off_t)((S64)blkgrp * FBA_BLKGRP_SIZE);
    len = fba_blkgrp_len (dev, blkgrp);

    logdevtr (dev, MSG(HHC00519, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                        blkgrp, offset, fba_blkgrp_len(dev, blkgrp)));

    /* Seek to the block group offset */
    offset = lseek (dev->fd, offset, SEEK_SET);
    if (offset < 0)
    {
        /* Handle seek error condition */
        WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "lseek()", strerror(errno));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        cache_lock(CACHE_DEVBUF);
        cache_release(CACHE_DEVBUF, o, 0);
        cache_unlock(CACHE_DEVBUF);
        return -1;
    }

    /* Read the block group */
    rc = read (dev->fd, dev->buf, len);
    if (rc < len)
    {
        /* Handle read error condition */
        WRMSG (HHC00502, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "read()", rc < 0 ? strerror(errno) : "unexpected end of file");
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        cache_lock(CACHE_DEVBUF);
        cache_release(CACHE_DEVBUF, o, 0);
        cache_unlock(CACHE_DEVBUF);
        return -1;
    }

    dev->cache = o;
    dev->buf = cache_getbuf(CACHE_DEVBUF, dev->cache, 0);
    dev->bufcur = blkgrp;
    dev->bufoff = 0;
    dev->bufoffhi = fba_blkgrp_len (dev, blkgrp);
    dev->buflen = fba_blkgrp_len (dev, blkgrp);
    dev->bufsize = cache_getlen(CACHE_DEVBUF, dev->cache);

    return 0;

} /* end function fbadasd_read_blkgrp */

/*-------------------------------------------------------------------*/
/* FBA update block group exit                                       */
/*-------------------------------------------------------------------*/
static
int fbadasd_update_blkgrp (DEVBLK *dev, int blkgrp, int off,
                          BYTE *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Read the block group */
    if (blkgrp != dev->bufcur)
    {
        rc = (dev->hnd->read) (dev, blkgrp, unitstat);
        if (rc < 0)
        {
            dev->bufcur = dev->cache = -1;
            return -1;
        }
    }

    /* Copy to the device buffer */
    if (buf) memcpy (dev->buf + off, buf, len);

    /* Update high/low offsets */
    if (!dev->bufupd || off < dev->bufupdlo)
        dev->bufupdlo = off;
    if (off + len > dev-> bufupdhi)
        dev->bufupdhi = off + len;

    /* Indicate block group has been modified */
    if (!dev->bufupd)
    {
        dev->bufupd = 1;
        shared_update_notify (dev, blkgrp);
    }

    return len;
} /* end function fbadasd_update_blkgrp */

/*-------------------------------------------------------------------*/
/* Channel program end exit                                          */
/*-------------------------------------------------------------------*/
static
void fbadasd_end (DEVBLK *dev)
{
BYTE            unitstat;

    /* Forces updated buffer to be written */
    fbadasd_read_blkgrp (dev, -1, &unitstat);
}

/*-------------------------------------------------------------------*/
/* Release cache entries                                             */
/*-------------------------------------------------------------------*/
int fbadasd_purge_cache (int *answer, int ix, int i, void *data)
{
U16             devnum;                 /* Cached device number      */
int             blkgrp;                 /* Cached block group        */
DEVBLK         *dev = data;             /* -> device block           */

    UNREFERENCED(answer);
    FBA_CACHE_GETKEY(i, devnum, blkgrp);
    if (dev->devnum == devnum)
        cache_release (ix, i, CACHE_FREEBUF);
    return 0;
}

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int fbadasd_close_device ( DEVBLK *dev )
{
BYTE            unitstat;

    /* Forces updated buffer to be written */
    fbadasd_read_blkgrp (dev, -1, &unitstat);

    /* Free the cache */
    cache_lock(CACHE_DEVBUF);
    cache_scan(CACHE_DEVBUF, fbadasd_purge_cache, dev);
    cache_unlock(CACHE_DEVBUF);

    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function fbadasd_close_device */

/*-------------------------------------------------------------------*/
/* Return used blocks                                                */
/*-------------------------------------------------------------------*/
static
int fbadasd_used (DEVBLK *dev)
{
    return dev->fbanumblk;
}

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void fbadasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     num;                            /* Number of bytes to move   */
BYTE    hexzeroes[512];                 /* Bytes for zero fill       */
int     rem;                            /* Byte count for zero fill  */
int     repcnt;                         /* Replication count         */

    /* Reset extent flag at start of CCW chain */
    if (chained == 0)
        dev->fbaxtdef = 0;

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ IPL                                                      */
    /*---------------------------------------------------------------*/
        /* Must be first CCW or chained from a previous READ IPL CCW */
        if (chained && prevcode != 0x02)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Zeroize the file mask and set extent for entire device */
        dev->fbamask = 0;
        dev->fbaxblkn = 0;
        dev->fbaxfirst = 0;
        dev->fbaxlast = dev->fbanumblk - 1;

        /* Seek to start of block zero */
        dev->fbarba = dev->fbaorigin * dev->fbablksiz;

        /* Overrun if data chaining */
        if ((flags & CCW_FLAGS_CD))
        {
            dev->sense[0] = SENSE_OR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate number of bytes to read and set residual count */
        num = (count < dev->fbablksiz) ? count : dev->fbablksiz;
        *residual = count - num;
        if (count < dev->fbablksiz) *more = 1;

        /* Read physical block into channel buffer */
        rc = fba_read (dev, iobuf, num, unitstat);
        if (rc < num) break;

        /* Set extent defined flag */
        dev->fbaxtdef = 1;

        /* Set normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x41:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
        /* Reject if previous command was not LOCATE */
        if (prevcode != 0x43)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if locate command did not specify write operation */
        if ((dev->fbaoper & FBAOPER_CODE) != FBAOPER_WRITE
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_WRTVRFY)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Prepare a block of zeroes for write padding */
        memset( hexzeroes, 0, sizeof(hexzeroes) );

        /* Write physical blocks of data to the device */
        while (dev->fbalcnum > 0)
        {
            /* Exit if data chaining and this CCW is exhausted */
            if ((flags & CCW_FLAGS_CD) && count == 0)
                break;

            /* Overrun if data chaining within a physical block */
            if ((flags & CCW_FLAGS_CD) && count < dev->fbablksiz)
            {
                dev->sense[0] = SENSE_OR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Calculate number of bytes to write */
            num = (count < dev->fbablksiz) ? count : dev->fbablksiz;
            if (num < dev->fbablksiz) *more = 1;

            /* Write physical block from channel buffer */
            if (num > 0)
            {
                rc = fba_write (dev, iobuf, num, unitstat);
                if (rc < num) break;
            }

            /* Fill remainder of block with zeroes */
            if (num < dev->fbablksiz)
            {
                rem = dev->fbablksiz - num;
                rc = fba_write (dev, hexzeroes, rem, unitstat);
                if (rc < rem) break;
            }

            /* Prepare to write next block */
            count -= num;
            iobuf += num;
            dev->fbalcnum--;

        } /* end while */

        /* Set residual byte count */
        *residual = count;
        if (dev->fbalcnum > 0) *more = 1;

        /* Set ending status */
        *unitstat |= CSW_CE | CSW_DE;
        break;

    case 0x42:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
        /* Reject if previous command was not LOCATE or READ IPL */
        if (prevcode != 0x43 && prevcode != 0x02)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if locate command did not specify read operation */
        if (prevcode != 0x02
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_READ
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_READREP)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Read physical blocks of data from device */
        while (dev->fbalcnum > 0 && count > 0)
        {
            /* Overrun if data chaining within a physical block */
            if ((flags & CCW_FLAGS_CD) && count < dev->fbablksiz)
            {
                dev->sense[0] = SENSE_OR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Calculate number of bytes to read */
            num = (count < dev->fbablksiz) ? count : dev->fbablksiz;
            if (num < dev->fbablksiz) *more = 1;

            /* Read physical block into channel buffer */
            rc = fba_read (dev, iobuf, num, unitstat);
            if (rc < num) break;

            /* Prepare to read next block */
            count -= num;
            iobuf += num;
            dev->fbalcnum--;

        } /* end while */

        /* Set residual byte count */
        *residual = count;
        if (dev->fbalcnum > 0) *more = 1;

        /* Set ending status */
        *unitstat |= CSW_CE | CSW_DE;

        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* LOCATE                                                        */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 8) ? count : 8;
        *residual = count - num;

        /* Control information length must be at least 8 bytes */
        if (count < 8)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* LOCATE must be preceded by DEFINE EXTENT or READ IPL */
        if (dev->fbaxtdef == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Save and validate the operation byte */
        dev->fbaoper = iobuf[0];
        if (dev->fbaoper & FBAOPER_RESV)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Validate and process operation code */
        if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_WRITE
            || (dev->fbaoper & FBAOPER_CODE) == FBAOPER_WRTVRFY)
        {
            /* Reject command if file mask inhibits all writes */
            if ((dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_INHWRT)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }
        else if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_READ
                || (dev->fbaoper & FBAOPER_CODE) == FBAOPER_READREP)
        {
        }
        else if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_FMTDEFC)
        {
            /* Reject command if file mask inhibits format writes */
            if ((dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_INHFMT)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }
        else /* Operation code is invalid */
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 1 contains the replication count */
        repcnt = iobuf[1];

        /* Bytes 2-3 contain the block count */
        dev->fbalcnum = fetch_hw(iobuf + 2);

        /* Bytes 4-7 contain the displacement of the first block
           relative to the start of the dataset */
        dev->fbalcblk = fetch_fw(iobuf + 4);

        /* Verify that the block count is non-zero, and that
           the starting and ending blocks fall within the extent */
        if (   dev->fbalcnum == 0
            || dev->fbalcnum >  dev->fbaxlast + 1
            || dev->fbalcblk <  dev->fbaxfirst
            || dev->fbalcblk >  dev->fbaxlast + 1 - dev->fbalcnum)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* For replicated data, verify that the replication count
           is non-zero and is a multiple of the block count */
        if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_READREP)
        {
            if (repcnt == 0 || repcnt % dev->fbalcnum != 0)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Position device to start of block */
        dev->fbarba = (dev->fbaorigin
                     + dev->fbaxblkn
                     + dev->fbalcblk - dev->fbaxfirst
                      ) * dev->fbablksiz;

        logdevtr (dev, MSG(HHC00520, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                            dev->fbarba, dev->fbarba));

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x63:
    /*---------------------------------------------------------------*/
    /* DEFINE EXTENT                                                 */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 16) ? count : 16;
        *residual = count - num;

        /* Control information length must be at least 16 bytes */
        if (count < 16)
        {
            WRMSG(HHC00509, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, count);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            WRMSG(HHC00510, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Save and validate the file mask */
        dev->fbamask = iobuf[0];
        if ((dev->fbamask & (FBAMASK_RESV | FBAMASK_CE))
            || (dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_RESV)
        {
            WRMSG(HHC00511, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, dev->fbamask);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

//      VM/ESA sends 0x00000200 0x00000000 0x00000000 0x0001404F
//      /* Verify that bytes 1-3 are zeroes */
//      if (iobuf[1] != 0 || iobuf[2] != 0 || iobuf[3] != 0)
//      {
//          LOGMSG(_("fbadasd: invalid reserved bytes %2.2X %2.2X %2.2X\n"),
//                  iobuf[1], iobuf[2], iobuf[3]);
//          dev->sense[0] = SENSE_CR;
//          *unitstat = CSW_CE | CSW_DE | CSW_UC;
//          break;
//      }

        /* Bytes 4-7 contain the block number of the first block
           of the extent relative to the start of the device */
        dev->fbaxblkn = fetch_fw(iobuf + 4);

        /* Bytes 8-11 contain the block number of the first block
           of the extent relative to the start of the dataset */
        dev->fbaxfirst = fetch_fw(iobuf + 8);

        /* Bytes 12-15 contain the block number of the last block
           of the extent relative to the start of the dataset */
        dev->fbaxlast = fetch_fw(iobuf + 12);

        /* Validate the extent description by checking that the
           ending block is not less than the starting block and
           that the ending block does not exceed the device size */
        if (dev->fbaxlast < dev->fbaxfirst
         || dev->fbaxblkn > (U32)dev->fbanumblk
         || dev->fbaxlast - dev->fbaxfirst >= dev->fbanumblk - dev->fbaxblkn)
        {
            WRMSG(HHC00512, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, dev->fbaxfirst, dev->fbaxlast, dev->fbaxblkn, dev->fbanumblk);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set extent defined flag and return normal status */
        dev->fbaxtdef = 1;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x64:
    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x94:
    /*---------------------------------------------------------------*/
    /* DEVICE RELEASE                                                */
    /*---------------------------------------------------------------*/
        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        if (dev->hnd->release) (dev->hnd->release) (dev);

        obtain_lock (&dev->lock);
        dev->reserved = 0;
        release_lock (&dev->lock);

        /* Return sense information */
        goto sense;

    case 0xB4:
    /*---------------------------------------------------------------*/
    /* DEVICE RESERVE                                                */
    /*---------------------------------------------------------------*/
        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reserve device to the ID of the active channel program */

        obtain_lock (&dev->lock);
        dev->reserved = 1;
        release_lock (&dev->lock);

        if (dev->hnd->reserve) (dev->hnd->reserve) (dev);

        /* Return sense information */
        goto sense;

    case 0x14:
    /*---------------------------------------------------------------*/
    /* UNCONDITIONAL RESERVE                                         */
    /*---------------------------------------------------------------*/
        /* Reject if this is not the first CCW in the chain */
        if (ccwseq > 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reserve device to the ID of the active channel program */

        obtain_lock (&dev->lock);
        dev->reserved = 1;
        release_lock (&dev->lock);

        if (dev->hnd->reserve) (dev->hnd->reserve) (dev);

        /* Return sense information */
        goto sense;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
    sense:
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset(dev->sense, 0, sizeof(dev->sense));

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

    case 0xA4:
    /*---------------------------------------------------------------*/
    /* READ AND RESET BUFFERED LOG                                   */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 24) ? count : 24;
        *residual = count - num;
        if (count < 24) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memset( iobuf, 0, num );

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

} /* end function fbadasd_execute_ccw */

/*-------------------------------------------------------------------*/
/* Read Standard Block (used by Diagnose instructions)               */
/*-------------------------------------------------------------------*/
DLL_EXPORT void fbadasd_read_block
      ( DEVBLK *dev, int blknum, int blksize, int blkfactor,
        BYTE *iobuf, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     sector;       /* First sector being read                     */

    /* Unit check if block number is invalid */
    sector = blknum * blkfactor;
    if (sector >= dev->fbanumblk)
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Seek to start of desired block */
    dev->fbarba = ( dev->fbaorigin + sector ) * dev->fbablksiz;

    /* Read block into I/O buffer */
    rc = fba_read (dev, iobuf, blksize, unitstat);
    if (rc < blksize)
    {
       dev->sense[0] = SENSE_CR;
       *unitstat = CSW_CE | CSW_DE | CSW_UC;
       return;
    }

    /* Return unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function fbadasd_read_block */

/*-------------------------------------------------------------------*/
/* Write Standard Block (used by Diagnose instructions)              */
/*-------------------------------------------------------------------*/
DLL_EXPORT void fbadasd_write_block (
        DEVBLK *dev, int blknum, int blksize, int blkfactor,
        BYTE *iobuf, BYTE *unitstat, U16 *residual )
{
int     rc;           /* Return code from write function             */
int     sector;       /* First sector being read                     */
#if 0
U64     rba;          /* Large file size offset                      */
#endif

    /* Unit check if block number is invalid */
    sector = blknum * blkfactor;
    if (sector >= dev->fbanumblk || sector < 0 )
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Seek to start of desired block */
    dev->fbarba = (off_t)(( dev->fbaorigin + sector ) * dev->fbablksiz);

    /* Read block into I/O buffer */
    rc = fba_write (dev, iobuf, blksize, unitstat);
    if (rc < blksize)
    {
       dev->sense[0] = SENSE_CR;
       *unitstat = CSW_CE | CSW_DE | CSW_UC;
       return;
    }

    /* Return unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function fbadasd_write_block */

/* Deprecated: Will be replaced by fbadasd_read/write_block functions */
/*-------------------------------------------------------------------*/
/* Synchronous Fixed Block I/O (used by Diagnose instruction)        */
/*-------------------------------------------------------------------*/
DLL_EXPORT void fbadasd_syncblk_io ( DEVBLK *dev, BYTE type, int blknum,
        int blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     blkfactor;                      /* Number of device blocks
                                           per logical block         */

    /* Calculate the blocking factor */
    blkfactor = blksize / dev->fbablksiz;

    /* Unit check if block number is invalid */
    if (blknum * blkfactor >= dev->fbanumblk)
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Seek to start of desired block */
    dev->fbarba = dev->fbaorigin * dev->fbablksiz;

    /* Process depending on operation type */
    switch (type) {

    case 0x01:
        /* Write block from I/O buffer */
        rc = fba_write (dev, iobuf, blksize, unitstat);
        if (rc < blksize) return;
        break;

    case 0x02:
        /* Read block into I/O buffer */
        rc = fba_read (dev, iobuf, blksize, unitstat);
        if (rc < blksize) return;
        break;

    } /* end switch(type) */

    /* Return unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function fbadasd_syncblk_io */

/*-------------------------------------------------------------------*/
/* Hercules suspend/resume text unit key values                      */
/*-------------------------------------------------------------------*/
#define SR_DEV_FBA_BUFCUR       ( SR_DEV_FBA | 0x001 )
#define SR_DEV_FBA_BUFOFF       ( SR_DEV_FBA | 0x002 )
#define SR_DEV_FBA_ORIGIN       ( SR_DEV_FBA | 0x003 )
#define SR_DEV_FBA_NUMBLK       ( SR_DEV_FBA | 0x004 )
#define SR_DEV_FBA_RBA          ( SR_DEV_FBA | 0x005 )
#define SR_DEV_FBA_END          ( SR_DEV_FBA | 0x006 )
#define SR_DEV_FBA_DXBLKN       ( SR_DEV_FBA | 0x007 )
#define SR_DEV_FBA_DXFIRST      ( SR_DEV_FBA | 0x008 )
#define SR_DEV_FBA_DXLAST       ( SR_DEV_FBA | 0x009 )
#define SR_DEV_FBA_LCBLK        ( SR_DEV_FBA | 0x00a )
#define SR_DEV_FBA_LCNUM        ( SR_DEV_FBA | 0x00b )
#define SR_DEV_FBA_BLKSIZ       ( SR_DEV_FBA | 0x00c )
#define SR_DEV_FBA_XTDEF        ( SR_DEV_FBA | 0x00d )
#define SR_DEV_FBA_OPER         ( SR_DEV_FBA | 0x00e )
#define SR_DEV_FBA_MASK         ( SR_DEV_FBA | 0x00f )

/*-------------------------------------------------------------------*/
/* Hercules suspend                                                  */
/*-------------------------------------------------------------------*/
int fbadasd_hsuspend(DEVBLK *dev, void *file) {
    if (dev->bufcur >= 0)
    {
        SR_WRITE_VALUE(file, SR_DEV_FBA_BUFCUR, dev->bufcur, sizeof(dev->bufcur));
        SR_WRITE_VALUE(file, SR_DEV_FBA_BUFOFF, dev->bufoff, sizeof(dev->bufoff));
    }
    SR_WRITE_VALUE(file, SR_DEV_FBA_ORIGIN, dev->fbaorigin, sizeof(dev->fbaorigin));
    SR_WRITE_VALUE(file, SR_DEV_FBA_NUMBLK, dev->fbanumblk, sizeof(dev->fbanumblk));
    SR_WRITE_VALUE(file, SR_DEV_FBA_RBA, dev->fbarba, sizeof(dev->fbarba));
    SR_WRITE_VALUE(file, SR_DEV_FBA_END, dev->fbaend, sizeof(dev->fbaend));
    SR_WRITE_VALUE(file, SR_DEV_FBA_DXBLKN, dev->fbaxblkn, sizeof(dev->fbaxblkn));
    SR_WRITE_VALUE(file, SR_DEV_FBA_DXFIRST, dev->fbaxfirst, sizeof(dev->fbaxfirst));
    SR_WRITE_VALUE(file, SR_DEV_FBA_DXLAST, dev->fbaxlast, sizeof(dev->fbaxlast));
    SR_WRITE_VALUE(file, SR_DEV_FBA_LCBLK, dev->fbalcblk, sizeof(dev->fbalcblk));
    SR_WRITE_VALUE(file, SR_DEV_FBA_LCNUM, dev->fbalcnum, sizeof(dev->fbalcnum));
    SR_WRITE_VALUE(file, SR_DEV_FBA_BLKSIZ, dev->fbablksiz, sizeof(dev->fbablksiz));
    SR_WRITE_VALUE(file, SR_DEV_FBA_XTDEF, dev->fbaxtdef, 1);
    SR_WRITE_VALUE(file, SR_DEV_FBA_OPER, dev->fbaoper, sizeof(dev->fbaoper));
    SR_WRITE_VALUE(file, SR_DEV_FBA_MASK, dev->fbamask, sizeof(dev->fbamask));
    return 0;
}

/*-------------------------------------------------------------------*/
/* Hercules resume                                                   */
/*-------------------------------------------------------------------*/
int fbadasd_hresume(DEVBLK *dev, void *file)
{
int     rc;
size_t  key, len;
BYTE byte;

    do {
        SR_READ_HDR(file, key, len);
        switch (key) {
        case SR_DEV_FBA_BUFCUR:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            rc = (dev->hnd->read) ? (dev->hnd->read)(dev, rc, &byte) : -1;
            if ((int)rc < 0) return -1;
            break;
        case SR_DEV_FBA_BUFOFF:
            SR_READ_VALUE(file, len, &dev->bufoff, sizeof(dev->bufoff));
            break;
        case SR_DEV_FBA_ORIGIN:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            if ((off_t)rc != dev->fbaorigin)
            {
                WRMSG(HHC00513, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, rc, (int)dev->fbaorigin);
                return -1;
            }
            break;
        case SR_DEV_FBA_NUMBLK:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            if ((int)rc != dev->fbanumblk)
            {
                WRMSG(HHC00514, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, rc, dev->fbanumblk);
                return -1;
            }
            break;
        case SR_DEV_FBA_RBA:
            SR_READ_VALUE(file, len, &dev->fbarba, sizeof(dev->fbarba));
            break;
        case SR_DEV_FBA_END:
            SR_READ_VALUE(file, len, &dev->fbaend, sizeof(dev->fbaend));
            break;
        case SR_DEV_FBA_DXBLKN:
            SR_READ_VALUE(file, len, &dev->fbaxblkn, sizeof(dev->fbaxblkn));
            break;
        case SR_DEV_FBA_DXFIRST:
            SR_READ_VALUE(file, len, &dev->fbaxfirst, sizeof(dev->fbaxfirst));
            break;
        case SR_DEV_FBA_DXLAST:
            SR_READ_VALUE(file, len, &dev->fbaxlast, sizeof(dev->fbaxlast));
            break;
        case SR_DEV_FBA_LCBLK:
            SR_READ_VALUE(file, len, &dev->fbalcblk, sizeof(dev->fbalcblk));
            break;
        case SR_DEV_FBA_LCNUM:
            SR_READ_VALUE(file, len, &dev->fbalcnum, sizeof(dev->fbalcnum));
            break;
        case SR_DEV_FBA_BLKSIZ:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            if ((int)rc != dev->fbablksiz)
            {
                WRMSG(HHC00515, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, rc, dev->fbablksiz);
                return -1;
            }
            break;
        case SR_DEV_FBA_XTDEF:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->fbaxtdef = rc;
            break;
        case SR_DEV_FBA_OPER:
            SR_READ_VALUE(file, len, &dev->fbaoper, sizeof(dev->fbaoper));
            break;
        case SR_DEV_FBA_MASK:
            SR_READ_VALUE(file, len, &dev->fbamask, sizeof(dev->fbamask));
            break;
        default:
            SR_READ_SKIP(file, len);
            break;
        } /* switch (key) */
    } while ((key & SR_DEV_MASK) == SR_DEV_FBA);
    return 0;
}

DLL_EXPORT DEVHND fbadasd_device_hndinfo = {
        &fbadasd_init_handler,         /* Device Initialisation      */
        &fbadasd_execute_ccw,          /* Device CCW execute         */
        &fbadasd_close_device,         /* Device Close               */
        &fbadasd_query_device,         /* Device Query               */
        NULL,                          /* Device Extended Query      */
        NULL,                          /* Device Start channel pgm   */
        &fbadasd_end,                  /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        &fbadasd_end,                  /* Device Suspend channel pgm */
        NULL,                          /* Device Halt channel pgm    */
        &fbadasd_read_blkgrp,          /* Device Read                */
        &fbadasd_update_blkgrp,        /* Device Write               */
        &fbadasd_used,                 /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Ouput       */
        NULL,                          /* QDIO subsys desc           */
        &fbadasd_hsuspend,             /* Hercules suspend           */
        &fbadasd_hresume               /* Hercules resume            */
};
