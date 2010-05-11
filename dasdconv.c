/* DASDCONV.C   (c) Copyright Roger Bowler, 1999-2009                */
/*              Hercules DASD Utilities: DASD image converter        */

// $Id$

/*-------------------------------------------------------------------*/
/* This program converts a CKD disk image from HDR-30 format         */
/* to the AWSCKD format used by Hercules.                            */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdconv [options] infile outfile                            */
/*                                                                   */
/* options      -r means overwrite existing outfile                  */
/*              -q means suppress progress messages                  */
/*              -lfs creates one large output file (if supported)    */
/* infile       is the name of the HDR-30 format CKD image file      */
/*              ("-" means that the CKD image is read from stdin)    */
/*              If this module was compiled with HAVE_LIBZ option    */
/*              activated, then the input file may be compressed     */
/*              or uncompressed. Otherwise it must be uncompressed.  */
/* outfile      is the name of the AWSCKD image file to be created.  */
/*              If the image exceeds 2GB then multiple files will    */
/*              be created, with names suffixed _1, _2, etc.         */
/*              (except if the underlying file system supports files */
/*              larger than 2GB and the -lfs option is specified).   */
/*              This program will not overwrite an existing file.    */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"
#include "dasdblks.h"
#include "opcode.h"

/*-------------------------------------------------------------------*/
/* Definition of HDR-30 CKD image headers                            */
/*-------------------------------------------------------------------*/
typedef struct _H30CKD_TRKHDR {         /* Track header              */
        HWORD   devcode;                /* Device type code          */
        BYTE    resv02[14];             /* Reserved                  */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
        BYTE    resv14[28];             /* Reserved                  */
    } H30CKD_TRKHDR;

typedef struct _H30CKD_RECHDR {         /* Record header             */
        FWORD   resv00;                 /* Reserved                  */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
        BYTE    rec;                    /* Record number             */
        BYTE    klen;                   /* Key length                */
        HWORD   dlen;                   /* Data length               */
    } H30CKD_RECHDR;

#define H30CKD_TRKHDR_SIZE      ((ssize_t)sizeof(H30CKD_TRKHDR))
#define H30CKD_RECHDR_SIZE      ((ssize_t)sizeof(H30CKD_RECHDR))

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
BYTE eighthexFF[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
BYTE twelvehex00[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
BYTE ebcdicvol1[] = {0xE5, 0xD6, 0xD3, 0xF1};
BYTE gz_magic_id[] = {0x1F, 0x8B};
BYTE ckd_ident[] = {0x43, 0x4B, 0x44, 0x5F}; /* CKD_ in ASCII */

/*-------------------------------------------------------------------*/
/* Definition of file descriptor for gzip and non-gzip builds        */
/*-------------------------------------------------------------------*/
#if defined(HAVE_LIBZ)
  #define IFD           gzFile
  #define IFREAD        gzread
  #define IFCLOS        gzclose
#else /*!defined(HAVE_LIBZ)*/
  #define IFD           int
  #define IFREAD        read
  #define IFCLOS        close
#endif /*!defined(HAVE_LIBZ)*/

/*-------------------------------------------------------------------*/
/* Subroutine to exit the program                                    */
/*-------------------------------------------------------------------*/
void delayed_exit (int exit_code)
{
    /* Delay exiting is to give the system
     * time to display the error message. */
    usleep(100000);
    exit(exit_code);
}
#define  EXIT(rc)   delayed_exit(rc)   /* (use this macro to exit)   */

/*-------------------------------------------------------------------*/
/* Subroutine to display command syntax and exit                     */
/*-------------------------------------------------------------------*/
static void
argexit ( int code )
{
    fprintf (stderr,
            "Syntax:\tdasdconv [options] infile outfile\n"
            "where:\n\tinfile   = name of input HDR-30 CKD image file"
                                " (\"-\" means stdin)\n"
          #if defined(HAVE_LIBZ)
            "\t\t   (input file may be compressed)\n"
          #endif /*defined(HAVE_LIBZ)*/
            "\toutfile  = name of AWSCKD image file to be created\n"  
            "options:\n\t-r       = replace existing output file\n"  
            "\t-q       = suppress progress messages\n");
    if (sizeof(off_t) > 4) fprintf(stderr,
            "\t-lfs     = build one large output file\n");
    EXIT(code);
} /* end function argexit */

/*-------------------------------------------------------------------*/
/* Subroutine to read data from input file                           */
/* Input:                                                            */
/*      ifd     Input file descriptor                                */
/*      ifname  Input file name                                      */
/*      buf     Address of input buffer                              */
/*      reqlen  Number of bytes requested                            */
/*      offset  Current offset in file (for error message only)      */
/*-------------------------------------------------------------------*/
static void
read_input_data (IFD ifd, char *ifname, BYTE *buf, int reqlen,
                U32 offset)
{
int     rc;                             /* Return code               */
int     len = 0;                        /* Number of bytes read      */

    while (len < reqlen)
    {
        rc = IFREAD (ifd, buf + len, reqlen - len);
        if (rc == 0) break;
        if (rc < 0)
        {
            fprintf (stderr,
                    "%s read error: %s\n",
                    ifname, strerror(errno));
            EXIT(3);
        }
        len += rc;
    } /* end while */

    if (len < reqlen)
    {
        fprintf (stderr,
                "Unexpected end-of-file on %s\n"
                "Expected %d bytes at offset %8.8X,"
                " found %d bytes\n",
                ifname, reqlen, offset, len);
        EXIT(3);
    }

} /* end function read_input_data */

/*-------------------------------------------------------------------*/
/* Subroutine to locate next record in HDR-30 CKD track image        */
/* Input:                                                            */
/*      buf     Pointer to start of track image buffer               */
/* Input+Output:                                                     */
/*      ppbuf   Pointer to current position in track image buffer    */
/*      plen    Length remaining in track image buffer               */
/* Output:                                                           */
/*      pkl     Key length                                           */
/*      pkp     Address of record key                                */
/*      pdl     Data length                                          */
/*      pdp     Address of record data                               */
/*      pcc     Cylinder number                                      */
/*      phh     Head number                                          */
/*      prn     Record number                                        */
/* Return value:                                                     */
/*      0=OK, 1=End of track, >1=Track format error detected         */
/*-------------------------------------------------------------------*/
static int 
find_input_record (BYTE *buf, BYTE **ppbuf, int *plen,
                BYTE *pkl, BYTE **pkp, U16 *pdl, BYTE **pdp,
                U32 *pcc, U32 *phh, BYTE *prn)
{
H30CKD_RECHDR  *hrec;                   /* Input record header       */
U16             dlen;                   /* Data length               */
BYTE            klen;                   /* Key length                */
int             n;                      /* Integer work area         */

    UNREFERENCED(buf);

    /* End of track if not enough bytes remain in buffer */
    if (*plen < H30CKD_RECHDR_SIZE) return 1;

    /* Point to record header */
    hrec = (H30CKD_RECHDR*)(*ppbuf);
     
    /* End of track if record header is all zero */
    if (memcmp(*ppbuf, twelvehex00, 12) == 0) return 1;

    /* Extract the key length and data length */
    klen = hrec->klen;
    FETCH_HW (dlen, hrec->dlen);

    /* Check that the reserved bytes are all zero */
    if (memcmp(hrec->resv00, twelvehex00, sizeof(hrec->resv00)) != 0)
        return 2;

    /* Check that the key and data do not overflow the buffer */
    if (*plen < H30CKD_RECHDR_SIZE + klen + dlen)
        return 3;

    /* Return the cylinder, head, and record number */
    FETCH_HW (*pcc, hrec->cyl);
    FETCH_HW (*phh, hrec->head);
    *prn = hrec->rec;

    /* Point past the record header to the key */
    *plen -= H30CKD_RECHDR_SIZE;
    *ppbuf += H30CKD_RECHDR_SIZE;

    /* Return the key length and key pointer */
    *pkl = klen;
    *pkp = *ppbuf;

    /* Point past the key to the data */
    *plen -= klen;
    *ppbuf += klen;

    /* Return the data length and data pointer */
    *pdl = dlen;
    *pdp = *ppbuf;
     
    /* Point past the data to the next record header */
    *plen -= dlen;
    *ppbuf += dlen;

    /* Ensure next header starts on a fullword boundary */
    if ((klen + dlen) & 3)
    {
        n = 4 - ((klen + dlen) % 4);
        *plen -= n;
        *ppbuf += n;
    }

    /* Issue progress message */
   #if 0
    fprintf (stderr,
        "+%4.4X cyl=%4.4X head=%4.4X rec=%2.2X"
        " kl=%2.2X dl=%4.4X trkbal=%d\n",
        (BYTE*)hrec-buf, *pcc, *phh, *prn, klen, dlen, *plen);
   #endif

    return 0;
} /* end function find_input_record */

/*-------------------------------------------------------------------*/
/* Subroutine to open input HDR-30 CKD image file                    */
/* Input:                                                            */
/*      ifname  Input HDR-30 CKD image file name                     */
/* Output:                                                           */
/*      devt    Device type                                          */
/*      vcyls   Number of primary cylinders on volume                */
/*      itrkl   Input HDR-30 CKD image track length                  */
/*      itrkb   -> Track image buffer (containing 1st track image)   */
/*      volser  Volume serial number (6 bytes ASCII + X'00')         */
/* Return value:                                                     */
/*      Input file descriptor                                        */
/*-------------------------------------------------------------------*/
static IFD 
open_input_image (char *ifname, U16 *devt, U32 *vcyls,
                U32 *itrkl, BYTE **itrkb, BYTE *volser)
{
int             rc;                     /* Return code               */
H30CKD_TRKHDR   h30trkhdr;              /* Input track header        */
IFD             ifd;                    /* Input file descriptor     */
int             len;                    /* Length of input           */
U16             code;                   /* Device type code          */
U16             dt;                     /* Device type               */
U32             cyls;                   /* Device size (pri+alt cyls)*/
U32             alts;                   /* Number of alternate cyls  */
BYTE           *itrkbuf;                /* -> Input track buffer     */
U32             itrklen;                /* Input track length        */
BYTE           *pbuf;                   /* Current byte in input buf */
BYTE            klen;                   /* Key length                */
U16             dlen;                   /* Data length               */
BYTE           *kptr;                   /* -> Key in input buffer    */
BYTE           *dptr;                   /* -> Data in input buffer   */
U32             cyl;                    /* Cylinder number           */
U32             head;                   /* Head number               */
BYTE            rec;                    /* Record number             */
char            pathname[MAX_PATH];     /* file path in host format  */

    hostpath(pathname, (char *)ifname, sizeof(pathname));

    /* Open the HDR-30 CKD image file */
  #if defined(HAVE_LIBZ)
    if (strcmp(ifname, "-") == 0)
        ifd = gzdopen (STDIN_FILENO, "rb");
    else
        ifd = gzopen (pathname, "rb");

    if (ifd == NULL)
    {
        fprintf (stderr,
                "Cannot open %s: %s\n",
                ifname,
                errno == 0 ? "gzopen error" : strerror(errno));
        EXIT(3);
    }
  #else /*!defined(HAVE_LIBZ)*/
    if (strcmp(ifname, "-") == 0)
        ifd = STDIN_FILENO;
    else
    {
        ifd = open (pathname, O_RDONLY | O_BINARY);

        if (ifd < 0)
        {
            fprintf (stderr,
                    "Cannot open %s: %s\n",
                    ifname, strerror(errno));
            EXIT(3);
        }
    }
  #endif /*!defined(HAVE_LIBZ)*/

    /* Read the first track header */
    read_input_data (ifd, ifname, (BYTE*)&h30trkhdr,
                    H30CKD_TRKHDR_SIZE, 0);

  #if !defined(HAVE_LIBZ)
    /* Reject input if compressed and we lack gzip support */
    if (memcmp(h30trkhdr.devcode, gz_magic_id, sizeof(gz_magic_id)) == 0) 
    {
        fprintf (stderr,
                "Input file %s appears to be a .gz file\n"
                "but this program was compiled without compression support\n",
                ifname);
        EXIT(3);
    }
  #endif /*!defined(HAVE_LIBZ)*/

    /* Reject input if it is already in CKD or CCKD format */
    if (memcmp((BYTE*)&h30trkhdr, ckd_ident, sizeof(ckd_ident)) == 0)
    {
        fprintf (stderr,
                "Input file %s is already in CKD format, use dasdcopy\n",
                ifname);
        EXIT(3);
    }

    /* Extract the device type code from the track header */
    FETCH_HW (code, h30trkhdr.devcode);

    /* Determine the input device type and size from the device code */
    switch (code) {
    case 0x01: dt=0x3330; cyls=411; alts=7; break;      /* 3330      */
    case 0x02: dt=0x3330; cyls=815; alts=7; break;      /* 3330-11   */
    case 0x03: dt=0x3340; cyls=351; alts=1; break;      /* 3340-35   */
    case 0x04: dt=0x3340; cyls=701; alts=1; break;      /* 3340-70   */
    case 0x05: dt=0x3350; cyls=562; alts=7; break;      /* 3350      */
    case 0x06: dt=0x3375; cyls=962; alts=3; break;      /* 3375      */
    case 0x08: dt=0x3380; cyls=888; alts=3; break;      /* 3380-A,D,J*/
    case 0x09: dt=0x3380; cyls=1774; alts=4; break;     /* 3380-E    */
    case 0x0A: dt=0x3380; cyls=2660; alts=5; break;     /* 3380-K    */
    case 0x0B: dt=0x3390; cyls=1117; alts=4; break;     /* 3390-1    */
    case 0x0C: dt=0x3390; cyls=2230; alts=4; break;     /* 3390-2    */
    case 0x0D: dt=0x3390; cyls=3343; alts=4; break;     /* 3390-3    */
    case 0x12: dt=0x2314; cyls=203; alts=3; break;      /* 2314      */
    case 0x13: dt=0x3390; cyls=10038; alts=21; break;   /* 3390-9    */
    case 0x14: dt=0x9345; cyls=1454; alts=14; break;    /* 9345-1    */
    case 0x15: dt=0x9345; cyls=2170; alts=14; break;    /* 9345-2    */
    default:
        fprintf (stderr,
                "Unknown device code %4.4X" \
                " at offset 00000000 in input file %s\n",
                code, ifname);
        EXIT(3);
    } /* end switch(code) */

    /* Use the device type to determine the input image track size */
    switch (dt) {
    case 0x2314: itrklen = 0x2000; break;
    case 0x3330: itrklen = 0x3400; break;
    case 0x3340: itrklen = 0x2400; break;
    case 0x3350: itrklen = 0x4C00; break;
    case 0x3375: itrklen = 0x9000; break;
    case 0x3380: itrklen = 0xBC00; break;
    case 0x3390: itrklen = 0xE400; break;
    case 0x9345: itrklen = 0xBC00; break;
    default:
        fprintf (stderr, "Unknown device type: %4.4X\n", dt);
        EXIT(3);
    } /* end switch(dt) */

    /* Obtain the input track buffer */
    itrkbuf = malloc (itrklen);
    if (itrkbuf == NULL)
    {
        fprintf (stderr,
                "Cannot obtain storage for input track buffer: %s\n",
                strerror(errno));
        EXIT(3);
    }

    /* Copy the first track header to the input track buffer */
    memcpy (itrkbuf, &h30trkhdr, H30CKD_TRKHDR_SIZE);

    /* Read the remainder of the first track into the buffer */
    read_input_data (ifd, ifname,
                     itrkbuf + H30CKD_TRKHDR_SIZE,
                     itrklen - H30CKD_TRKHDR_SIZE,
                     H30CKD_TRKHDR_SIZE);

    /* Initialize the volume serial number */
    strcpy ((char *)volser, "(NONE)");

    /* Search for volume label in record 3 of first track */
    pbuf = itrkbuf + H30CKD_TRKHDR_SIZE;
    len = itrklen - H30CKD_TRKHDR_SIZE;
    while (1)
    {
        /* Find next input record */
        rc = find_input_record (itrkbuf, &pbuf, &len,
                &klen, &kptr, &dlen, &dptr,
                &cyl, &head, &rec);
     
        /* Give up if error or end of track */
        if (rc != 0) break;

        /* Process when record 3 is found */
        if (cyl == 0 && head == 0 && rec == 3)
        {
            /* Extract volser if it is a volume label */
            if (klen == 4 && memcmp(kptr, ebcdicvol1, 4) == 0
            && dlen == 80 && memcmp(dptr, ebcdicvol1, 4) == 0)
                make_asciiz ((char *)volser, 7, dptr+4, 6);
            break;
        }
    } /* end while */

    /* Set output variables and return the input file descriptor */
    *devt = dt;
    *vcyls = cyls - alts;
    *itrkl = itrklen;
    *itrkb = itrkbuf;
    return ifd;

} /* end function open_input_image */

/*-------------------------------------------------------------------*/
/* Subroutine to create an AWSCKD DASD image file                    */
/* Input:                                                            */
/*      ifd     Input HDR-30 image file descriptor                   */
/*      ifname  Input file name                                      */
/*      itrklen Length of input track buffer                         */
/*      itrkbuf Address of input track buffer                        */
/*      repl    1=replace existing file, 0=do not replace            */
/*      ofname  Output AWSCKD file name                              */
/*      fseqn   Sequence number of this file (1=first)               */
/*      devtype Device type                                          */
/*      heads   Number of heads per cylinder                         */
/*      trksize AWSCKD image track length                            */
/*      obuf    Address of output AWSCKD track image buffer          */
/*      start   Starting cylinder number for this file               */
/*      end     Ending cylinder number for this file                 */
/*      volcyls Total number of cylinders on volume                  */
/*      volser  Volume serial number                                 */
/*-------------------------------------------------------------------*/
static void
convert_ckd_file (IFD ifd, char *ifname, int itrklen, BYTE *itrkbuf,
                int repl, int quiet,
                char *ofname, int fseqn, U16 devtype, U32 heads,
                U32 trksize, BYTE *obuf, U32 start, U32 end,
                U32 volcyls, BYTE *volser)
{
int             rc;                     /* Return code               */
int             ofd;                    /* Output file descriptor    */
CKDDASD_DEVHDR  devhdr;                 /* Output device header      */
CKDDASD_TRKHDR *trkhdr;                 /* -> Output track header    */
CKDDASD_RECHDR *rechdr;                 /* -> Output record header   */
U32             cyl;                    /* Cylinder number           */
U32             head;                   /* Head number               */
int             fileseq;                /* CKD header sequence number*/
int             highcyl;                /* CKD header high cyl number*/
BYTE           *opos;                   /* -> Byte in output buffer  */
BYTE            klen;                   /* Key length                */
U16             dlen;                   /* Data length               */
BYTE            rec;                    /* Record number             */
BYTE           *iptr;                   /* -> Byte in input buffer   */
BYTE           *kptr;                   /* -> Key in input buffer    */
BYTE           *dptr;                   /* -> Data in input buffer   */
int             ilen;                   /* Bytes left in input buffer*/
H30CKD_TRKHDR  *ith;                    /* -> Input track header     */
U32             ihc, ihh;               /* Input trk header cyl,head */
U32             offset;                 /* Current input file offset */
char            pathname[MAX_PATH];     /* file path in host format  */

    UNREFERENCED(volser);

    /* Set file sequence number to zero if this is the only file */
    if (fseqn == 1 && end + 1 == volcyls)
        fileseq = 0;
    else
        fileseq = fseqn;

    /* Set high cylinder number to zero if this is the last file */
    if (end + 1 == volcyls)
        highcyl = 0;
    else
        highcyl = end;

    /* Create the AWSCKD image file */
    hostpath(pathname, (char *)ofname, sizeof(pathname));
    ofd = open (pathname,
                O_WRONLY | O_CREAT | O_BINARY | (repl ? 0 : O_EXCL),
                S_IRUSR | S_IWUSR | S_IRGRP);

    if (ofd < 0)
    {
        fprintf (stderr, "%s open error: %s\n",
                ofname, strerror(errno));
        EXIT(8);
    }

    /* Create the device header */
    memset(&devhdr, 0, CKDDASD_DEVHDR_SIZE);
    memcpy(devhdr.devid, "CKD_P370", 8);
    devhdr.heads[3] = (heads >> 24) & 0xFF;
    devhdr.heads[2] = (heads >> 16) & 0xFF;
    devhdr.heads[1] = (heads >> 8) & 0xFF;
    devhdr.heads[0] = heads & 0xFF;
    devhdr.trksize[3] = (trksize >> 24) & 0xFF;
    devhdr.trksize[2] = (trksize >> 16) & 0xFF;
    devhdr.trksize[1] = (trksize >> 8) & 0xFF;
    devhdr.trksize[0] = trksize & 0xFF;
    devhdr.devtype = devtype & 0xFF;
    devhdr.fileseq = fileseq;
    devhdr.highcyl[1] = (highcyl >> 8) & 0xFF;
    devhdr.highcyl[0] = highcyl & 0xFF;

    /* Write the device header */
    rc = write (ofd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "%s device header write error: %s\n",
                ofname, errno ? strerror(errno) : "incomplete");
        EXIT(1);
    }

    /* Write each cylinder */
    for (cyl = start; cyl <= end; cyl++)
    {
        /* Display progress message every 10 cylinders */
        if ((cyl % 10) == 0)
        {
#ifdef EXTERNALGUI
            if (extgui)
                fprintf (stderr, "CYL=%u\n", cyl);
            else
#endif /*EXTERNALGUI*/
            if (quiet == 0)
                fprintf (stderr, "Writing cylinder %u\r", cyl);
        }

        for (head = 0; head < heads; head++)
        {
            /* Calculate the current offset in the file */
            offset = ((cyl*heads)+head)*itrklen;

            /* Read the input track image (except cyl 0 head 0 
               already read by the open_input_image procedure) */
            if (cyl > 0 || head > 0)
            {
                read_input_data (ifd, ifname,
                                 itrkbuf, itrklen,
                                 offset);
            } /* end if(cyl>0||head>0) */

            /* Validate the track header */
            ith = (H30CKD_TRKHDR*)itrkbuf;
            FETCH_HW (ihc, ith->cyl);
            FETCH_HW (ihh, ith->head);
            if (ihc != cyl || ihh != head)
            {
                fprintf (stderr,
                        "Invalid track header found at offset %8.8X\n"
                        "in input file %s\n"
                        "Expected cyl=%4.4X head=%4.4X\n"
                        "   Found cyl=%4.4X head=%4.4X\n",
                        offset, ifname,
                        cyl, head, ihc, ihh);
                EXIT(8);
            }
             
            /* Clear the output track image to zeroes */
            memset (obuf, 0, trksize);

            /* Build the output track header */
            trkhdr = (CKDDASD_TRKHDR*)obuf;
            trkhdr->bin = 0;
            STORE_HW (trkhdr->cyl, cyl);
            STORE_HW (trkhdr->head, head);
            opos = obuf + CKDDASD_TRKHDR_SIZE;

            /* Copy each record from the input buffer */
            iptr = itrkbuf + H30CKD_TRKHDR_SIZE;
            ilen = itrklen - H30CKD_TRKHDR_SIZE;
            while (1)
            {
                /* Locate the next input record */
                rc = find_input_record (itrkbuf, &iptr, &ilen,
                        &klen, &kptr, &dlen, &dptr,
                        &ihc, &ihh, &rec);

                /* Exit at end of track */
                if (rc == 1) break;

                /* Error if invalid record header detected */
                if (rc > 1)
                {
                    fprintf (stderr,
                            "Invalid record header (reason %d)\n"
                            "at offset %4.4X"
                            " in track at cyl %4.4X head %4.4X\n"
                            "at offset %8.8X in input file %s\n",
                            rc, (unsigned int)(iptr-itrkbuf), cyl, head,
                            offset, ifname);
                    EXIT(9);
                }

                /* Build AWSCKD record header in output buffer */
                rechdr = (CKDDASD_RECHDR*)opos;
                opos += CKDDASD_RECHDR_SIZE;
                STORE_HW (rechdr->cyl, ihc);
                STORE_HW (rechdr->head, ihh);
                rechdr->rec = rec;
                rechdr->klen = klen;
                STORE_HW (rechdr->dlen, dlen);

                /* Copy key and data to output buffer */
                if (klen != 0)
                {
                    memcpy (opos, kptr, klen);
                    opos += klen;
                }
                if (dlen != 0)
                {
                    memcpy (opos, dptr, dlen);
                    opos += dlen;
                }

            } /* end while */

            /* Build the end of track marker */
            memcpy (opos, eighthexFF, 8);

            /* Write the track to the file */
            rc = write (ofd, obuf, trksize);
            if (rc < 0 || (U32)rc < trksize)
            {
                fprintf (stderr,
                        "%s cylinder %u head %u write error: %s\n",
                        ofname, cyl, head,
                        errno ? strerror(errno) : "incomplete");
                EXIT(1);
            }

        } /* end for(head) */

    } /* end for(cyl) */

    /* Close the AWSCKD image file */
    rc = close (ofd);
    if (rc < 0)
    {
        fprintf (stderr, "%s close error: %s\n",
                ofname, strerror(errno));
        EXIT(10);
    }

    /* Display completion message */
    fprintf (stderr,
            "%u cylinders successfully written to file %s\n",
            cyl - start, ofname);

} /* end function convert_ckd_file */

/*-------------------------------------------------------------------*/
/* Subroutine to create an AWSCKD DASD image                         */
/* Input:                                                            */
/*      lfs     Build one large output file                          */
/*      ifd     Input HDR-30 image file descriptor                   */
/*      ifname  Input file name                                      */
/*      itrklen Length of input track buffer                         */
/*      itrkbuf Address of input track buffer                        */
/*      repl    1=replace existing file, 0=do not replace            */
/*      ofname  Output AWSCKD image file name                        */
/*      devtype Device type                                          */
/*      heads   Number of heads per cylinder                         */
/*      maxdlen Maximum R1 record data length                        */
/*      volcyls Total number of cylinders on volume                  */
/*      volser  Volume serial number                                 */
/*                                                                   */
/* If the total number of cylinders exceeds the capacity of a 2GB    */
/* file, then multiple CKD image files will be created, with the     */
/* suffix _1, _2, etc suffixed to the specified file name.           */
/* Otherwise a single file is created without a suffix.              */
/*-------------------------------------------------------------------*/
static void
convert_ckd (int lfs, IFD ifd, char *ifname, int itrklen,
            BYTE *itrkbuf, int repl, int quiet,
            char *ofname, U16 devtype, U32 heads,
            U32 maxdlen, U32 volcyls, BYTE *volser)
{
int             i;                      /* Array subscript           */
char            *s;                     /* String pointer            */
int             fileseq;                /* File sequence number      */
char            sfname[260];            /* Suffixed name of this file*/
char            *suffix;                /* -> Suffix character       */
U32             endcyl;                 /* Last cylinder of this file*/
U32             cyl;                    /* Cylinder number           */
U32             cylsize;                /* Cylinder size in bytes    */
BYTE           *obuf;                   /* -> Output track buffer    */
U32             mincyls;                /* Minimum cylinder count    */
U32             maxcyls;                /* Maximum cylinder count    */
U32             maxcpif;                /* Maximum number of cylinders
                                           in each CKD image file    */
int             rec0len = 8;            /* Length of R0 data         */
U32             trksize;                /* AWSCKD image track length */

    /* Compute the AWSCKD image track length */
    trksize = sizeof(CKDDASD_TRKHDR)
                + sizeof(CKDDASD_RECHDR) + rec0len
                + sizeof(CKDDASD_RECHDR) + maxdlen
                + sizeof(eighthexFF);
    trksize = ROUND_UP(trksize,512);

    /* Compute minimum and maximum number of cylinders */
    cylsize = trksize * heads;
    mincyls = 1;

    if (!lfs)
    {
        maxcpif = 0x80000000 / cylsize;
        maxcyls = maxcpif * CKD_MAXFILES;
    }
    else
        maxcpif = maxcyls = volcyls;

    if (maxcyls > 65536) maxcyls = 65536;

    /* Check for valid number of cylinders */
    if (volcyls < mincyls || volcyls > maxcyls)
    {
        fprintf (stderr,
                "Cylinder count %u is outside range %u-%u\n",
                volcyls, mincyls, maxcyls);
        EXIT(4);
    }

    /* Obtain track data buffer */
    obuf = malloc(trksize);
    if (obuf == NULL)
    {
        fprintf (stderr, "Cannot obtain track buffer: %s\n",
                strerror(errno));
        EXIT(6);
    }

    /* Display progress message */
    fprintf (stderr,
            "Converting %4.4X volume %s: %u cyls, "
            "%u trks/cyl, %u bytes/track\n",
            devtype, volser, volcyls, heads, trksize);
#ifdef EXTERNALGUI
    if (extgui)
        fprintf (stderr, "CYLS=%u\n", volcyls);
#endif /*EXTERNALGUI*/

    /* Copy the unsuffixed AWSCKD image file name */
    strcpy (sfname, ofname);
    suffix = NULL;

    /* Create the suffixed file name if volume will exceed 2GB */
    if (volcyls > maxcpif)
    {
        /* Look for last slash marking end of directory name */
        s = strrchr (ofname, '/');
        if (s == NULL) s = ofname;

        /* Insert suffix before first dot in file name, or
           append suffix to file name if there is no dot */
        s = strchr (s, '.');
        if (s != NULL)
        {
            i = s - ofname;
            strcpy (sfname + i, "_1");
            strcat (sfname, ofname + i);
            suffix = sfname + i + 1;
        }
        else
        {
            strcat (sfname, "_1");
            suffix = sfname + strlen(sfname) - 1;
        }
    }

    /* Create the AWSCKD image files */
    for (cyl = 0, fileseq = 1; cyl < volcyls;
            cyl += maxcpif, fileseq++)
    {
        /* Insert the file sequence number in the file name */
        if (suffix) *suffix = '0' + fileseq;

        /* Calculate the ending cylinder for this file */
        if (cyl + maxcpif < volcyls)
            endcyl = cyl + maxcpif - 1;
        else
            endcyl = volcyls - 1;

        /* Create an AWSCKD image file */
        convert_ckd_file (ifd, ifname, itrklen, itrkbuf, repl, quiet,
                        sfname, fileseq, devtype, heads, trksize,
                        obuf, cyl, endcyl, volcyls, volser);
    }

    /* Release the output track buffer */
    free (obuf);

} /* end function convert_ckd */

/*-------------------------------------------------------------------*/
/* DASDCONV program main entry point                                 */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
IFD             ifd;                    /* Input file descriptor     */
int             repl = 0;               /* 1=replace existing file   */
int             quiet = 0;              /* 1=suppress progress msgs  */
BYTE           *itrkbuf;                /* -> Input track buffer     */
U32             itrklen;                /* Input track length        */
U32             volcyls;                /* Total cylinders on volume */
U32             heads = 0;              /* Number of tracks/cylinder */
U32             maxdlen = 0;            /* Maximum R1 data length    */
U16             devtype;                /* Device type               */
char            ifname[256];            /* Input file name           */
char            ofname[256];            /* Output file name          */
BYTE            volser[7];              /* Volume serial (ASCIIZ)    */
int             lfs = 0;                /* 1 = Build large file      */

    INITIALIZE_UTILITY("dasdconv");

    /* Display the program identification message */
    display_version (stderr,
                     "Hercules DASD CKD image conversion program",
                     FALSE);

    /* Process the options in the argument list */
    for (; argc > 1; argc--, argv++)
    {
        if (strcmp(argv[1], "-") == 0) break;
        if (argv[1][0] != '-') break;
        if (strcmp(argv[1], "-r") == 0)
            repl = 1;
        else if (strcmp(argv[1], "-q") == 0)
            quiet = 1;
        else
        if (sizeof(off_t) > 4 && strcmp(argv[1], "-lfs") == 0)
            lfs = 1;
        else
            argexit(5);
    }
    if (argc != 3)
        argexit(5);

    /* The first argument is the input file name */
    if (argv[1] == NULL || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(ifname)-1)
        argexit(1);
    strcpy (ifname, argv[1]);

    /* The second argument is the output file name */
    if (argv[2] == NULL || strlen(argv[2]) == 0
        || strlen(argv[2]) > sizeof(ofname)-1)
        argexit(2);
    strcpy (ofname, argv[2]);

    /* Read the first track of the input file, and determine
       the device type and size from the track header */
    ifd = open_input_image (ifname, &devtype, &volcyls,
                &itrklen, &itrkbuf, volser);

    /* Use the device type to determine track characteristics */
    switch (devtype) {
    case 0x2314: heads = 20; maxdlen = 7294; break;
    case 0x3330: heads = 19; maxdlen = 13030; break;
    case 0x3340: heads = 12; maxdlen = 8368; break;
    case 0x3350: heads = 30; maxdlen = 19069; break;
    case 0x3375: heads = 12; maxdlen = 35616; break;
    case 0x3380: heads = 15; maxdlen = 47476; break;
    case 0x3390: heads = 15; maxdlen = 56664; break;
    case 0x9345: heads = 15; maxdlen = 46456; break;
    default:
        fprintf (stderr, "Unknown device type: %4.4X\n", devtype);
        EXIT(3);
    } /* end switch(devtype) */

    /* Create the device */
    convert_ckd (lfs, ifd, ifname, itrklen, itrkbuf, repl, quiet,
                ofname, devtype, heads, maxdlen, volcyls, volser);

    /* Release the input buffer and close the input file */
    free (itrkbuf);
    IFCLOS (ifd);
     
    /* Display completion message */
    fprintf (stderr, "DASD conversion successfully completed.\n");

    return 0;

} /* end function main */

