/* CCKD2CKD.C   (c) Copyright Roger Bowler, 1999-2002                */
/*       Copy a Compressed CKD Direct Access Storage Device file to  */
/*       a regular CKD Direct Access Storage Device file.            */

/*-------------------------------------------------------------------*/
/* This module creates a regular ckd dasd emulation file from a      */
/* compressed ckd dasd emulation file.                               */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/
void syntax ();
int abbrev (char *, char *);
void status (int, int);
int null_trk (int, unsigned char *, int, int);
int valid_trk (int, unsigned char *, int, int);

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/
int         errs = 0;
static BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Build a ckd file from a compressed ckd file                       */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[])
{
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
CCKD_L1ENT     *l1;                     /* -> Primary lookup table   */
CCKD_L2ENT      l2[256];                /* Secondary lookup table    */
int             rc;                     /* Return code               */
int             i,j;                    /* Indices                   */
int             cyls=-1;                /* Cylinders in output file  */
unsigned long   trks;                   /* Tracks in output file     */
int             heads;                  /* Heads per cylinder        */
int             trksz;                  /* Track size                */
char           *ifile;                  /* Input file name           */
int             ifd;                    /* Input file descriptor     */
char           *ofile;                  /* Output file name          */
char           *sfxptr;                 /* -> Last char of file name */
int             ofd=-1;                 /* Output file descriptor    */
int             bytes_per_cyl;          /* Bytes per cylinder        */
int             cyls_per_file;          /* Cyls per output file      */
int             trks_per_file;          /* Trks per output file      */
int             files=0;                /* Number output files       */
int             highcyl=0;              /* High cyl on output file   */
int             fileseq=0;              /* Output file sequence nbr  */
int             trk;                    /* Current track             */
int             compress;               /* Compression method        */
unsigned char  *buf;                    /* Input buffer              */
unsigned long   buflen;                 /* Input buffer length       */
unsigned char  *buf2;                   /* Uncompressed buffer       */
unsigned long   buf2len;                /* Uncompressed buffer length*/
unsigned char  *obuf;                   /* Output buffer             */
int             obuflen;                /* Output buffer length      */
int             quiet=0;                /* Don't display status      */
int             valid=1;                /* Validate track images     */
int             swapend=0;              /* Need to swap byte order   */
int             maxerrs=5;              /* Max errors allowed        */
int             limited=0;              /* 1=Limit cyls copied       */

    /* Display the program identification message */
    display_version (stdout, "Hercules cckd to ckd copy program ");

    /* parse the arguments */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case 'h':  syntax ();

            case 'c':  if (abbrev(argv[0], "-cyl"))
                       {
                           if (argc < 2) syntax ();
                           argc--; argv++;
                           cyls = atoi (argv[0]);
                           if (cyls >= 0) limited = 1;
                           break;
                       }
                       syntax ();

            case 'm':  if (abbrev(argv[0], "-maxerrs"))
                       {
                           if (argc < 2) syntax ();
                           argc--; argv++;
                           maxerrs = atoi (argv[0]);
                           break;
                       }
                       syntax ();

            case 'n':  if (abbrev(argv[0], "-novalidate"))
                       {
                           valid = 0;
                           break;
                       }
                       syntax ();

            case 'q':  if (abbrev(argv[0], "-quiet"))
                       {
                           quiet = 1;
                           break;
                       }
                       syntax ();

            case 'v':  if (abbrev(argv[0], "-validate"))
                       {
                           valid = 1;
                           break;
                       }
                       syntax ();

            default:   syntax ();
        }
    }
    if (argc != 2) syntax ();
    ifile = argv[0]; ofile = argv[1];

    /* open the input file */
    ifd = open (ifile, O_RDONLY|O_BINARY);
    if (ifd < 0)
    {
        fprintf (stderr,
                 "cckd2ckd: error opening input file %s: %s\n",
                 ifile, strerror(errno));
        exit (1);
    }

    /* read the CKD device header */
    rc = read (ifd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                 ifile, strerror(errno));
        exit (2);
    }
    if (memcmp(devhdr.devid, "CKD_C370", 8) != 0)
    {
        fprintf (stderr,
         "cckd2ckd: input file %s is not a compressed ckd file\n",
         ifile);
        exit (3);
    }
    memcpy (devhdr.devid, "CKD_P370", 8);

    /* get heads and track size */
    heads = ((U32)(devhdr.heads[3]) << 24)
            | ((U32)(devhdr.heads[2]) << 16)
            | ((U32)(devhdr.heads[1]) << 8)
            | (U32)(devhdr.heads[0]);
    trksz = ((U32)(devhdr.trksize[3]) << 24)
            | ((U32)(devhdr.trksize[2]) << 16)
            | ((U32)(devhdr.trksize[1]) << 8)
            | (U32)(devhdr.trksize[0]);

    /* read the compressed CKD device header */
    rc = read (ifd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                 ifile, strerror(errno));
        exit (4);
    }

    /* check the byte order of the file vs the machine */
    if (((cdevhdr.options & CCKD_BIGENDIAN) != 0 && cckd_endian() == 0) ||
        ((cdevhdr.options & CCKD_BIGENDIAN) == 0 && cckd_endian() != 0))
        swapend = 1;
    if (swapend) cckd_swapend_chdr (&cdevhdr);

    /* get area for primary lookup table and read it in */
    l1 = malloc (cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    if (l1 == NULL)
    {
        fprintf (stderr, "lookup table malloc error: %s\n",
                 strerror(errno));
        exit (5);
    } 
    rc = read (ifd, l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    if (rc != cdevhdr.numl1tab * CCKD_L1ENT_SIZE)
    {
        fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                 ifile, strerror(errno));
        exit (6);
    }
    if (swapend) cckd_swapend_l1 (l1, cdevhdr.numl1tab);

    /* get number of cylinders if not specified */
    if (cyls == -1)
    {
        cyls = ((U32)(cdevhdr.cyls[3]) << 24)
               | ((U32)(cdevhdr.cyls[2]) << 16)
               | ((U32)(cdevhdr.cyls[1]) << 8)
               | (U32)(cdevhdr.cyls[0]);
    }

    /* if number of cylinders specified is zero then
       calculate the minimum number of cylinders */
    if (cyls == 0)
    {   /* find last used level 1 table entry */
        for (i = cdevhdr.numl1tab - 1; i > 0; i--)
           if (l1[i] != 0) break;
        /* get the last secondary lookup table */
        if (l1[i] == 0)
            memset (&l2, 0, CCKD_L2TAB_SIZE);
        else
        {
            rc = lseek (ifd, l1[i], SEEK_SET);
            if (rc == -1)
            {
                fprintf (stderr, "cckd2ckd: %s lseek error: %s\n",
                         ifile, strerror(errno));
                exit (7);
            }
            rc = read (ifd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                         ifile, strerror(errno));
                exit (8);
            }
        }
        /* find the last used entry in the level 2 table */
        for (j = 255; j > 0; j--)
           if (l2[j].pos != 0) break;
        /* calculate last used cylinder */
        trks = 256 * i + (j + 1);
        cyls = trks / heads;
        if (trks % heads > 0) cyls++;
        if (cyls == 0) cyls = 1;
    }

    /* perform some file calculations */
    trks = cyls * heads;
    bytes_per_cyl = trksz * heads;
    cyls_per_file = (2147483647 - CKDDASD_DEVHDR_SIZE) /
                     bytes_per_cyl;
    trks_per_file = cyls_per_file * heads;
    files = (trks + trks_per_file - 1) / trks_per_file;
    highcyl = cyls_per_file - 1;

#ifdef EXTERNALGUI
    /* Tell the GUI how many tracks we'll be processing. */
    if (extgui) fprintf (stderr, "TRKS=%ld\n", trks);
#endif /*EXTERNALGUI*/

    /* don't print status if stdout is not a tty */
#ifdef EXTERNALGUI
    if (!extgui)
#endif /*EXTERNALGUI*/
    if (!isatty (fileno(stdout))) quiet = 1;

    /* Locate the last character of the file name */
    sfxptr = strrchr (ofile, '/');
    if (sfxptr == NULL) sfxptr = ofile + 1;
    sfxptr = strchr (sfxptr, '.');
    if (sfxptr == NULL) sfxptr = ofile + strlen(ofile);
    sfxptr--;

    /* get buffers */
    buf = malloc (trksz);
    buf2 = malloc (trksz);
    if (buf == NULL || buf2 == NULL)
    {
        fprintf (stderr, "cckd2ckd: buffer malloc error: %s\n",
                 strerror(errno));
        exit (9);
    }

    /* process each entry in the primary lookup table */
    for (i = 0; i * 256 < trks || l1[i] != 0; i++)
    {
        if (limited && i * 256 >= trks) break;
        /* get the secondary lookup table */
        if (i >= cdevhdr.numl1tab || l1[i] == 0)
            memset (&l2, 0, CCKD_L2TAB_SIZE);
        else
        {
            rc = lseek (ifd, l1[i], SEEK_SET);
            if (rc == -1)
            {
                fprintf (stderr, "cckd2ckd: %s lseek error: %s\n",
                         ifile, strerror(errno));
                exit (10);
            }
            rc = read (ifd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                         ifile, strerror(errno));
                exit (11);
            }
            if (swapend) cckd_swapend_l2 ((CCKD_L2ENT *)&l2);
        }

        /* process each entry in the secondary lookup table */
        for (j = 0;
             j < 256 && (i * 256 + j < trks || (l1[i] != 0 && l2[j].pos != 0));
             j++)

        {
            trk = i * 256 + j;
            if (limited && trk >= trks) break;

            /* check for new output file */
            if (trk % trks_per_file == 0)
            {
                if (files > 1) fileseq++;
                if (fileseq > 1)
                { /* close the current file */
                    rc = close (ofd);
                    if (rc < 0)
                    {
                        fprintf (stderr,
                                 "cckd2ckd: %s close error: %s\n",
                                 ofile, strerror(errno));
                        exit(12);
                    }
                    *sfxptr = '0' + fileseq;
                }
                /* update the devhdr */
                devhdr.fileseq = fileseq;
                if (fileseq > 0 && fileseq < files)
                {
                    devhdr.highcyl[1] = (highcyl >> 8) & 0xFF;
                    devhdr.highcyl[0] = highcyl & 0xFF;
                    highcyl += cyls_per_file;
                }
                else
                {
                    devhdr.highcyl[1] = 0;
                    devhdr.highcyl[0] = 0;
                }
                /* open the output file */
                ofd = open (ofile,
                            O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                            S_IRUSR | S_IWUSR | S_IRGRP);
                if (ofd < 0)
                {
                    fprintf (stderr,
                             "cckd2ckd: %s open error: %s\n",
                             ofile, strerror(errno));
                    exit (13);
                }

                /* write the devhdr */
                rc = write (ofd, &devhdr, CKDDASD_DEVHDR_SIZE);
                if (rc != CKDDASD_DEVHDR_SIZE)
                {
                    fprintf (stderr,
                             "cckd2ckd: %s write error: %s\n",
                             ofile, strerror(errno));
                    exit (14);
                }
            }

            /* read next track image */
            if (l2[j].pos == 0)
            {
                obuflen = null_trk (trk, buf, heads, l2[j].len);
                obuf = buf;
            }
            else
            {
                rc = lseek (ifd, l2[j].pos, SEEK_SET);
                if (rc == -1)
                {
                    fprintf (stderr, "cckd2ckd: %s lseek error: %s\n",
                             ifile, strerror(errno));
                    exit (15);
                }

                rc = read (ifd, buf, l2[j].len);
                if (rc != l2[j].len)
                {
                    fprintf (stderr, "cckd2ckd: %s read error: %s\n",
                             ifile, strerror(errno));
                    exit (16);
                }

                /* uncompress the track image */
                compress = buf[0];
                buf[0] = 0;
                switch (compress)
                {
                    case CCKD_COMPRESS_NONE:
                        obuflen = l2[j].len;
                        obuf = buf;
                        break;

                    case CCKD_COMPRESS_ZLIB:
                        /* Uncompress the track image using zlib.
                           Note that the trk hdr is not compressed. */
                        buflen = l2[j].len - CKDDASD_TRKHDR_SIZE;
                        buf2len = trksz - CKDDASD_TRKHDR_SIZE;
                        memcpy(buf2, buf, CKDDASD_TRKHDR_SIZE);
                        rc = uncompress(&buf2[CKDDASD_TRKHDR_SIZE],
                                        &buf2len,
                                        &buf[CKDDASD_TRKHDR_SIZE],
                                        buflen);
                        if (rc != Z_OK)
                        {
                            fprintf (stderr,
                                     "*** uncompress error for"
                                     " track %d: %d\n", trk, rc);
                            fprintf (stderr,
                                     "    null track substituted\n");
                            errs++;
                            obuflen = null_trk (trk, buf, heads, 0);
                            obuf = buf;
                        }
                        else
                        {
                            obuflen = buf2len + CKDDASD_TRKHDR_SIZE;
                            obuf = buf2;
                        }
                        break;

#ifdef CCKD_BZIP2
                    case CCKD_COMPRESS_BZIP2:
                        /* Decompress the track image using bzip2
                           Note that the trk hdr is not compressed. */
                        buflen = l2[j].len - CKDDASD_TRKHDR_SIZE;
                        buf2len = trksz - CKDDASD_TRKHDR_SIZE;
                        memcpy(buf2, buf, CKDDASD_TRKHDR_SIZE);
                        rc = BZ2_bzBuffToBuffDecompress (
                                        &buf2[CKDDASD_TRKHDR_SIZE],
                                        (unsigned int *)&buf2len,
                                        &buf[CKDDASD_TRKHDR_SIZE],
                                        buflen, 0, 0);
                        if (rc != BZ_OK)
                        {
                            fprintf (stderr,
                                     "*** decompress error for"
                                     " track %d: %d\n", trk, rc);
                            fprintf (stderr,
                                     "    null track substituted\n");
                            errs++;
                            obuflen = null_trk (trk, buf, heads, 0);
                            obuf = buf;
                        }
                        else
                        {
                            obuflen = buf2len + CKDDASD_TRKHDR_SIZE;
                            obuf = buf2;
                        }
                        break;
#endif

                    default:
                        fprintf (stderr,
                                 "*** unknown compression for"
                                 " track %d: %d\n", trk, compress);
                        fprintf (stderr,
                                 "    null track substituted\n");
                        errs++;
                        obuflen = null_trk (trk, buf, heads, 0);
                        obuf = buf;
                        break;
                }
            }

            /* validate the track image */
            if (valid) obuflen = valid_trk(trk, obuf, heads, obuflen);

            /* clear the remainder of the buffer */
            memset (&obuf[obuflen], 0, trksz - obuflen);

            /* write the track image */
            rc = write (ofd, obuf, trksz);
            if (rc != trksz)
            {
                fprintf (stderr,
                         "cckd2ckd: %s write error: %s\n",
                         ofile, strerror(errno));
                exit (17);
            }

            /* update status information */
            if (!quiet) status (trk+1, trks);

            /* check for max errors exceeded */
            if (maxerrs > 0 && errs >= maxerrs)
            {
                fprintf (stderr,
                         "cckd2ckd: Terminated due to errors\n");
                exit (18);
            }
        }
    }

    /* free all our malloc()ed storage */
    free (buf);
    free (buf2);
    free (l1);

    /* close the files */
    rc = close (ofd);
    if (rc < 0)
    {
        fprintf (stderr,
                 "cckd2ckd: %s close error: %s\n",
                 ofile, strerror(errno));
        exit(19);
    }
    rc = close (ifd);
    if (rc < 0)
    {
        fprintf (stderr,
                 "cckd2ckd: %s close error: %s\n",
                 ifile, strerror(errno));
        exit(20);
    }

    if (quiet == 0 || errs > 0)
        printf ("cckd2ckd: copy %s\n",
                errs ? "completed with errors"
                     : "successful!!         ");

    return 0;

}

void syntax ()
{
    printf ("usage:  cckd2ckd [-options] input-file output-file\n"
            "\n"
            "     input-file   --   input compressed ckd dasd file\n"
            "     output-file  --   output ckd dasd file\n"
            "\n"
            "   options:\n"
            "     -cyl cyls         number cylinders to copy if\n"
            "                       entire file is not to be copied\n"
            "                       0 - copy to last used cylinder\n"
            "     -maxerrs errs     max number of errors before copy\n"
            "                       is terminated; if 0 then errors\n"
            "                       are ignored.  Default is 5\n"
            "     -quiet            quiet mode, don't display status\n"
            "     -validate         validate track images [default]\n"
            "     -novalidate       don't validate track images\n");
    exit (21);
} /* end function syntax */

int abbrev (char *tst, char *cmp)
{
    size_t len = strlen(tst);
    return ((len >= 1) && (!strncmp(tst, cmp, len)));
} /* end function abbrev */

void status (int i, int n)
{
static char indic[] = "|/-\\";

#ifdef EXTERNALGUI
    if (extgui) fprintf (stderr, "TRK=%d\n", i);
    else
#endif /*EXTERNALGUI*/
    printf ("\r%c %3d%% track %6d of %6d\r",
            indic[i%4], (i*100)/n, i, n);
} /* end function status */


/*-------------------------------------------------------------------*/
/* Build a null track                                                */
/*-------------------------------------------------------------------*/
int null_trk (int trk, unsigned char *buf, int heads, int len)
{
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
char            cchh[4];                /* Cyl, head big-endian      */
int             size;                   /* Size of null record       */

    /* cylinder and head calculations */
    cyl = trk / heads;
    head = trk % heads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* a null track has a 5 byte track hdr, 8 byte r0 count,
       8 byte r0 data, 8 byte r1 count and 8 ff's */
    memset(buf, 0, 37);
    memcpy (&buf[1], cchh, sizeof(cchh));
    memcpy (&buf[5], cchh, sizeof(cchh));
    buf[12] = 8;
    if (len == 0)
    {
        memcpy (&buf[21], cchh, sizeof(cchh));
        buf[25] = 1;
        memcpy (&buf[29], eighthexFF, 8);
        size = CCKD_NULLTRK_SIZE1;
    }
    else
    {
        memcpy (&buf[21], eighthexFF, 8);
        size = CCKD_NULLTRK_SIZE0;
    }
    return size;
} /* end function null_trk */


/*-------------------------------------------------------------------*/
/* Validate a track image                                            */
/*-------------------------------------------------------------------*/
int valid_trk (int trk, unsigned char *buf, int heads, int len)
{
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
char            cchh[4], cchh2[4];      /* Cyl, head big-endian      */
int             r;                      /* Record number             */
int             sz;                     /* Track size                */
int             kl,dl;                  /* Key/Data lengths          */

    /* cylinder and head calculations */
    cyl = trk / heads;
    head = trk % heads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* validate home address */
    if (buf[0] !=0 || memcmp (&buf[1], cchh, 4) != 0)
    {
        fprintf (stderr, "*** track %d HA validation error !! "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 trk, buf[0], buf[1], buf[2], buf[3], buf[4]);
        fprintf (stderr, "    null track substituted\n");
        errs++;
        return null_trk (trk, buf, heads, 0);
    }

    /* validate record 0 */
    memcpy (cchh2, &buf[5], 4); cchh2[0] &= 0x7f; /* fix for ovflow */
    if (memcmp (cchh, cchh2, 4) != 0   ||  buf[9] != 0 ||
        buf[10] != 0   || buf[11] != 0 || buf[12] != 8)
    {
        fprintf (stderr, "\r*** track %d R0 validation error !! "

                 "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 trk, buf[5], buf[6], buf[7], buf[8], buf[9],
                 buf[10], buf[11], buf[12]);
        fprintf (stderr, "    null track substituted\n");
        errs++;
        return null_trk (trk, buf, heads, 0);
    }

    /* validate records 1 thru n */
    for (r = 1, sz = 21;
         memcmp (&buf[sz], eighthexFF, 8) != 0;
         sz += 8 + kl + dl, r++)
    {
        kl = buf[sz+5];
        dl = buf[sz+6] * 256 + buf[sz+7];

        /* fix for track overflow bit */
        memcpy (cchh2, &buf[sz], 4); cchh2[0] &= 0x7f;

        /* fix for funny formatted vm disks */
        if (r == 1) memcpy (cchh, cchh2, 4);

        if (memcmp (cchh, cchh2, 4) != 0 || buf[sz+4] == 0 ||

            sz + 8 + kl + dl >= len)
        {
            fprintf (stderr, "\r*** track %d R%d validation error !! "

                     "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                     trk, r, buf[sz], buf[sz+1], buf[sz+2], buf[sz+3],
                     buf[sz+4], buf[sz+5], buf[sz+6], buf[sz+7]);
            errs++;
            if (r > 1)
            {
                printf ("    track truncated              \n");

                memcpy (&buf[sz], eighthexFF, 8);
                return sz + 8;
            }
            else
            {
                printf ("    null track substituted       \n");

                return null_trk (trk, buf, heads, 0);
            }
        }
    }
    sz += 8;

    if (sz != len)
    {
        fprintf (stderr, "\r*** track %d size mismatch !! "

                 "size found %d, expected %d\n",
                 trk, sz, len);
        errs++;
    }

    return sz;
} /* end function valid_trk */
