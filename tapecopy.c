/* TAPECOPY.C   (c) Copyright Roger Bowler, 1999-2001                */
/*              Convert SCSI tape into AWSTAPE format                */

/*-------------------------------------------------------------------*/
/* This program reads a SCSI tape and produces a disk file with      */
/* each block of the tape prefixed by an AWSTAPE block header.       */
/* If no disk file name is supplied, then the program simply         */
/* prints a summary of the tape files and blocksizes.                */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Structure definition for AWSTAPE block header                     */
/*-------------------------------------------------------------------*/
typedef struct _AWSTAPE_BLKHDR {
        HWORD   curblkl;                /* Length of this block      */
        HWORD   prvblkl;                /* Length of previous block  */
        BYTE    flags1;                 /* Flags byte 1              */
        BYTE    flags2;                 /* Flags byte 2              */
    } AWSTAPE_BLKHDR;

/* Definitions for AWSTAPE_BLKHDR flags byte 1 */
#define AWSTAPE_FLAG1_NEWREC    0x80    /* Start of new record       */
#define AWSTAPE_FLAG1_TAPEMARK  0x40    /* Tape mark                 */
#define AWSTAPE_FLAG1_ENDREC    0x20    /* End of record             */

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static BYTE vollbl[] = "\xE5\xD6\xD3";  /* EBCDIC characters "VOL"   */
static BYTE hdrlbl[] = "\xC8\xC4\xD9";  /* EBCDIC characters "HDR"   */
static BYTE eoflbl[] = "\xC5\xD6\xC6";  /* EBCDIC characters "EOF"   */
static BYTE eovlbl[] = "\xC5\xD6\xE5";  /* EBCDIC characters "EOV"   */
static struct mt_tape_info tapeinfo[] = MT_TAPE_INFO;
static struct mt_tape_info densinfo[] = {
    {0x01, "NRZI (800 bpi)"},
    {0x02, "PE (1600 bpi)"},
    {0x03, "GCR (6250 bpi)"},
    {0x05, "QIC-45/60 (GCR, 8000 bpi)"},
    {0x06, "PE (3200 bpi)"},
    {0x07, "IMFM (6400 bpi)"},
    {0x08, "GCR (8000 bpi)"},
    {0x09, "GCR /37871 bpi)"},
    {0x0A, "MFM (6667 bpi)"},
    {0x0B, "PE (1600 bpi)"},
    {0x0C, "GCR (12960 bpi)"},
    {0x0D, "GCR (25380 bpi)"},
    {0x0F, "QIC-120 (GCR 10000 bpi)"},
    {0x10, "QIC-150/250 (GCR 10000 bpi)"},
    {0x11, "QIC-320/525 (GCR 16000 bpi)"},
    {0x12, "QIC-1350 (RLL 51667 bpi)"},
    {0x13, "DDS (61000 bpi)"},
    {0x14, "EXB-8200 (RLL 43245 bpi)"},
    {0x15, "EXB-8500 (RLL 45434 bpi)"},
    {0x16, "MFM 10000 bpi"},
    {0x17, "MFM 42500 bpi"},
    {0x24, "DDS-2"},
    {0x8C, "EXB-8505 compressed"},
    {0x90, "EXB-8205 compressed"},
    {0, NULL}};
static BYTE buf[65500];

/*-------------------------------------------------------------------*/
/* ASCII to EBCDIC translate tables                                  */
/*-------------------------------------------------------------------*/
#include "codeconv.h"

/*-------------------------------------------------------------------*/
/* Subroutine to print tape status                                   */
/*-------------------------------------------------------------------*/
static void print_status (BYTE *devname, long stat)
{
    printf ("%s status: %8.8lX", devname, stat);
    if (GMT_EOF(stat)) printf (" EOF");
    if (GMT_BOT(stat)) printf (" BOT");
    if (GMT_EOT(stat)) printf (" EOT");
    if (GMT_SM(stat)) printf (" SETMARK");
    if (GMT_EOD(stat)) printf (" EOD");
    if (GMT_WR_PROT(stat)) printf (" WRPROT");
    if (GMT_ONLINE(stat)) printf (" ONLINE");
    if (GMT_D_6250(stat)) printf (" 6250");
    if (GMT_D_1600(stat)) printf (" 1600");
    if (GMT_D_800(stat)) printf (" 800");
    if (GMT_DR_OPEN(stat)) printf (" NOTAPE");
    printf ("\n");

} /* end function print_status */

/*-------------------------------------------------------------------*/
/* Subroutine to obtain and print tape status                        */
/* Return value: 0=normal, 1=end of tape, -1=error                   */
/*-------------------------------------------------------------------*/
static int obtain_status (BYTE *devname, int devfd)
{
int             rc;                     /* Return code               */
struct mtget    stblk;                  /* Area for MTIOCGET ioctl   */

    rc = ioctl (devfd, MTIOCGET, (char*)&stblk);
    if (rc < 0)
    {
        printf ("tapecopy: Error reading status of %s: %s\n",
                devname, strerror(errno));
        return -1;
    }

    print_status (devname, stblk.mt_gstat);

    if (GMT_EOD(stblk.mt_gstat)) return 1;
    if (GMT_EOT(stblk.mt_gstat)) return 1;

    return 0;
} /* end function print_status */

/*-------------------------------------------------------------------*/
/* TAPECOPY main entry point                                         */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             len;                    /* Block length              */
int             prevlen;                /* Previous block length     */
BYTE           *devname;                /* -> Tape device name       */
BYTE           *filename;               /* -> Output file name       */
int             devfd;                  /* Tape file descriptor      */
int             outfd = -1;             /* Output file descriptor    */
int             fileno;                 /* Tape file number          */
int             blkcount;               /* Block count               */
int             minblksz;               /* Minimum block size        */
int             maxblksz;               /* Maximum block size        */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
struct mtget    stblk;                  /* Area for MTIOCGET ioctl   */
long            density;                /* Tape density code         */
BYTE            labelrec[81];           /* Standard label (ASCIIZ)   */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */

    /* Display the program identification message */
    display_version (stderr, "Hercules tape copy program ",
                     MSTRING(VERSION), __DATE__, __TIME__);

    /* The first argument is the tape device name */
    if (argc > 1 && argv[1] != NULL && strlen(argv[1]) > 5
        && memcmp (argv[1], "/dev/", 5) == 0)
    {
        devname = argv[1];
    }
    else
    {
        printf ("Usage: tapecopy /dev/st0 [outfile]\n");
        exit (1);
    }

    /* The second argument is the output file name */
    if (argc > 2 && argv[2] != NULL)
        filename = argv[2];
    else
        filename = NULL;

    /* Open the tape device */
    devfd = open (devname, O_RDONLY|O_BINARY);
    if (devfd < 0)
    {
        printf ("tapecopy: Error opening %s: %s\n",
                devname, strerror(errno));
        exit (3);
    }

    /* Obtain the tape status */
    rc = ioctl (devfd, MTIOCGET, (char*)&stblk);
    if (rc < 0)
    {
        printf ("tapecopy: Error reading status of %s: %s\n",
                devname, strerror(errno));
        exit (7);
    }

    /* Display tape status information */
    for (i = 0; tapeinfo[i].t_type != 0
                && tapeinfo[i].t_type != stblk.mt_type; i++);

    if (tapeinfo[i].t_name != NULL)
        printf ("%s device type: %s\n", devname, tapeinfo[i].t_name);
    else
        printf ("%s device type: 0x%lX\n", devname, stblk.mt_type);

    density = (stblk.mt_dsreg & MT_ST_DENSITY_MASK)
                >> MT_ST_DENSITY_SHIFT;

    for (i = 0; densinfo[i].t_type != 0
                && densinfo[i].t_type != density; i++);

    if (densinfo[i].t_name != NULL)
        printf ("%s tape density: %s\n", devname, densinfo[i].t_name);
    else
        printf ("%s tape density code: 0x%lX\n", devname, density);

    if (stblk.mt_gstat != 0)
    {
        print_status (devname, stblk.mt_gstat);
    }

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf ("tapecopy: Error setting attributes for %s: %s\n",
                devname, strerror(errno));
        exit (5);
    }

    /* Rewind the tape to the beginning */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf ("tapecopy: Error rewinding %s: %s\n",
                devname, strerror(errno));
        exit (6);
    }

    /* Open the output file */
    if (filename != NULL)
    {
        outfd = open (filename, O_WRONLY | O_CREAT | O_BINARY,
                        S_IRUSR | S_IWUSR | S_IRGRP);
        if (outfd < 0)
        {
            printf ("tapecopy: Error opening %s: %s\n",
                    filename, strerror(errno));
            exit (4);
        }
    }

    /* Copy blocks from tape to the output file */
    fileno = 1;
    blkcount = 0;
    minblksz = 0;
    maxblksz = 0;
    len = 0;

    while (1)
    {
        /* Save previous block length */
        prevlen = len;

        /* Read a block from the tape */
        len = read (devfd, buf, sizeof(buf));
        if (len < 0)
        {
            printf ("tapecopy: Error reading %s: %s\n",
                    devname, strerror(errno));
            obtain_status (devname, devfd);
            exit (8);
        }

        /* Check for tape mark */
        if (len == 0)
        {
            /* Print summary of current file */
            printf ("File %u: Blocks=%u, block size min=%u, max=%u\n",
                    fileno, blkcount, minblksz, maxblksz);

            /* Write tape mark to output file */
            if (outfd >= 0)
            {
                /* Build block header for tape mark */
                awshdr.curblkl[0] = 0;
                awshdr.curblkl[1] = 0;
                awshdr.prvblkl[0] = prevlen & 0xFF;
                awshdr.prvblkl[1] = (prevlen >> 8) & 0xFF;
                awshdr.flags1 = AWSTAPE_FLAG1_TAPEMARK;
                awshdr.flags2 = 0;

                /* Write block header to output file */
                rc = write (outfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
                if (rc < sizeof(AWSTAPE_BLKHDR))
                {
                    printf ("tapecopy: Error writing %s: %s\n",
                            filename, strerror(errno));
                    exit (9);
                } /* end if(rc) */

            } /* end if(outfd) */

            /* Reset counters for next file */
            fileno++;
            minblksz = 0;
            maxblksz = 0;
            blkcount = 0;

            /* Determine whether end of tape has been read */
            rc = obtain_status (devname, devfd);
            if (rc == 0) continue;
            if (rc > 0) printf ("End of tape\n");
            break;

        } /* end if(tapemark) */

        /* Count blocks and block sizes */
        blkcount++;
        if (len > maxblksz) maxblksz = len;
        if (minblksz == 0 || len < minblksz) minblksz = len;

        /* Print standard labels */
        if (len == 80 && blkcount < 4
            && (memcmp(buf, vollbl, 3) == 0
                || memcmp(buf, hdrlbl, 3) == 0
                || memcmp(buf, eoflbl, 3) == 0
                || memcmp(buf, eovlbl, 3) == 0))
        {
            for (i=0; i < 80; i++)
                labelrec[i] = ebcdic_to_ascii[buf[i]];
            labelrec[i] = '\0';
            printf ("%s\n", labelrec);
        }
        else
        {
            printf ("File %u: Block %u\r", fileno, blkcount);
        }

        /* Write block to output file */
        if (outfd >= 0)
        {
            /* Build the block header */
            awshdr.curblkl[0] = len & 0xFF;
            awshdr.curblkl[1] = (len >> 8) & 0xFF;
            awshdr.prvblkl[0] = prevlen & 0xFF;
            awshdr.prvblkl[1] = (prevlen >> 8) & 0xFF;
            awshdr.flags1 = AWSTAPE_FLAG1_NEWREC
                            | AWSTAPE_FLAG1_ENDREC;
            awshdr.flags2 = 0;

            /* Write block header to output file */
            rc = write (outfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
            if (rc < sizeof(AWSTAPE_BLKHDR))
            {
                printf ("tapecopy: Error writing %s: %s\n",
                        filename, strerror(errno));
                exit (10);
            } /* end if(rc) */

            /* Write data block to output file */
            rc = write (outfd, buf, len);
            if (rc < len)
            {
                printf ("tapecopy: Error writing %s: %s\n",
                        filename, strerror(errno));
                exit (11);
            } /* end if(rc) */

        } /* end if(outfd) */

    } /* end while */

    /* Close files and exit */
    close (devfd);
    if (filename != NULL) close (outfd);

    return 0;

} /* end function main */

