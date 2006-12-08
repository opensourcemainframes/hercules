/* DASDISUP.C   (c) Copyright Roger Bowler, 1999-2006                */
/*              Hercules DASD Utilities: IEHIOSUP                    */

// $Id$

/*-------------------------------------------------------------------*/
/* This program performs the IEHIOSUP function of OS/360.            */
/* It adjusts the TTRs in the XCTL tables in each of the             */
/* Open/Close/EOV modules in SYS1.SVCLIB.                            */
/*                                                                   */
/* The command format is:                                            */
/*      dasdisup ckdfile                                             */
/* where: ckdfile is the name of the CKD image file                  */
/*-------------------------------------------------------------------*/

// $Log$

#include "hstdinc.h"

#include "hercules.h"
#include "dasdblks.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define HEX00           ((BYTE)'\x00')  /* EBCDIC low value          */
#define HEX40           ((BYTE)'\x40')  /* EBCDIC space              */
#define HEXFF           ((BYTE)'\xFF')  /* EBCDIC high value         */
#define OVERPUNCH_ZERO  ((BYTE)'\xC0')  /* EBCDIC 12-0 card punch    */

/*-------------------------------------------------------------------*/
/* Definition of member information array entry                      */
/*-------------------------------------------------------------------*/
typedef struct _MEMINFO {
        BYTE    memname[8];             /* Member name (EBCDIC)      */
        BYTE    ttrtext[3];             /* TTR of first text record  */
        BYTE    dwdsize;                /* Text size in doublewords  */
        BYTE    alias;                  /* 1=This is an alias        */
        BYTE    notable;                /* 1=Member has no XCTL table*/
        BYTE    multitxt;               /* 1=Member has >1 text rcd  */
    } MEMINFO;

#define MAX_MEMBERS             1000    /* Size of member info array */

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/

/* List of first loads for Open/Close/EOV routines */
static char *firstload[] = {
        "IGC0001I",                     /* Open (SVC19)              */
        "IGC0002{",                     /* Close (SVC20)             */
        "IGC0002A",                     /* Stow (SVC21)              */
        "IGC0002B",                     /* OpenJ (SVC22)             */
        "IGC0002C",                     /* TClose (SVC23)            */
        "IGC0002I",                     /* Scratch (SVC29)           */
        "IGC0003A",                     /* FEOV (SVC31)              */
        "IGC0003B",                     /* Allocate (SVC32)          */
        "IGC0005E",                     /* EOV (SVC55)               */
        "IGC0008A",                     /* Setprt (SVC81)            */
        "IGC0008F",                     /* Atlas (SVC86)             */
        "IGC0009C",                     /* TSO (SVC93)               */
        "IGC0009D",                     /* TSO (SVC94)               */
        NULL };                         /* End of list               */

/* List of second loads for Open/Close/EOV routines */
static char *secondload[] = {
        "IFG019", "IGG019",             /* Open (SVC19)              */
        "IFG020", "IGG020",             /* Close (SVC20)             */
        "IGG021",                       /* Stow (SVC21)              */
        "IFG023", "IGG023",             /* TClose (SVC23)            */
        "IGG029",                       /* Scratch (SVC29)           */
        "IGG032",                       /* Allocate (SVC32)          */
        "IFG055", "IGG055",             /* EOV (SVC55)               */
        "IGG081",                       /* Setprt (SVC81)            */
        "IGG086",                       /* Atlas (SVC86)             */
        "IGG093",                       /* TSO (SVC93)               */
        "IGG094",                       /* TSO (SVC94)               */
        NULL };                         /* End of list               */

static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

#if 0
/*-------------------------------------------------------------------*/
/* Subroutine to process a member                                    */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      noext   Number of extents in dataset                         */
/*      extent  Dataset extent array                                 */
/*      memname Member name (ASCIIZ)                                 */
/*      ttr     Member TTR                                           */
/*                                                                   */
/* Return value is 0 if successful, or -1 if error                   */
/*-------------------------------------------------------------------*/
static int
process_member (CIFBLK *cif, int noext, DSXTENT extent[],
                BYTE *memname, BYTE *ttr)
{
int             rc;                     /* Return code               */
int             len;                    /* Record length             */
int             trk;                    /* Relative track number     */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
int             rec;                    /* Record number             */
BYTE           *buf;                    /* -> Data block             */
FILE           *ofp;                    /* Output file pointer       */
BYTE            ofname[256];            /* Output file name          */
int             offset;                 /* Offset of record in buffer*/
BYTE            card[81];               /* Logical record (ASCIIZ)   */
char            pathname[MAX_PATH];     /* ofname in host path format*/

    /* Build the output file name */
    memset (ofname, 0, sizeof(ofname));
    strncpy (ofname, memname, 8);
    string_to_lower (ofname);
    strcat (ofname, ".mac");

    /* Open the output file */
    hostpath(pathname, ofname, sizeof(pathname));
    ofp = fopen (pathname, "w");
    if (ofp == NULL)
    {
        fprintf (stderr,
                "Cannot open %s: %s\n",
                ofname, strerror(errno));
        return -1;
    }

    /* Point to the start of the member */
    trk = (ttr[0] << 8) | ttr[1];
    rec = ttr[2];

    fprintf (stdout,
            "Member %s TTR=%4.4X%2.2X\n",
            memname, trk, rec);

    /* Read the member */
    while (1)
    {
        /* Convert relative track to cylinder and head */
        rc = convert_tt (trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0) return -1;

//      fprintf (stdout,
//              "CCHHR=%4.4X%4.4X%2.2X\n",
//              cyl, head, rec);

        /* Read a data block */
        rc = read_block (cif, cyl, head, rec, NULL, NULL, &buf, &len);
        if (rc < 0) return -1;

        /* Move to next track if record not found */
        if (rc > 0)
        {
            trk++;
            rec = 1;
            continue;
        }

        /* Exit at end of member */
        if (len == 0) break;

        /* Check length of data block */
        if (len % 80 != 0)
        {
            fprintf (stdout,
                    "Bad block length %d at cyl %d head %d rec %d\n",
                    len, cyl, head, rec);
            return -1;
        }

        /* Process each record in the data block */
        for (offset = 0; offset < len; offset += 80)
        {
            if (asciiflag)
            {
                make_asciiz (card, sizeof(card), buf + offset, 72);
                fprintf (ofp, "%s\n", card);
            }
            else
            {
                fwrite (buf+offset, 80, 1, ofp);
            }

            if (ferror(ofp))
            {
                fprintf (stdout,
                        "Error writing %s: %s\n",
                        ofname, strerror(errno));
                return -1;
            }
        } /* end for(offset) */

        /* Point to the next data block */
        rec++;

    } /* end while */

    /* Close the output file and exit */
    fclose (ofp);
    return 0;

} /* end function process_member */
#endif

/*-------------------------------------------------------------------*/
/* Subroutine to process a directory block                           */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      noext   Number of extents in dataset                         */
/*      extent  Dataset extent array                                 */
/*      dirblk  Pointer to directory block                           */
/* Input/output:                                                     */
/*      memtab  Member information array                             */
/*      nmem    Number of entries in member information array        */
/*                                                                   */
/* Directory information for each member is extracted from the       */
/* directory block and saved in the member information array.        */
/*                                                                   */
/* Return value is 0 if OK, +1 if end of directory, or -1 if error   */
/*-------------------------------------------------------------------*/
static int
process_dirblk (CIFBLK *cif, int noext, DSXTENT extent[],
                BYTE *dirblk, MEMINFO memtab[], int *nmem)
{
int             n;                      /* Member array subscript    */
int             i;                      /* Array subscript           */
int             totlen;                 /* Total module length       */
int             txtlen;                 /* Length of 1st text block  */
int             size;                   /* Size of directory entry   */
int             k;                      /* Userdata halfword count   */
BYTE           *dirptr;                 /* -> Next byte within block */
int             dirrem;                 /* Number of bytes remaining */
PDSDIR         *dirent;                 /* -> Directory entry        */
char            memnama[9];             /* Member name (ASCIIZ)      */

    UNREFERENCED(cif);
    UNREFERENCED(noext);
    UNREFERENCED(extent);

    /* Load number of bytes in directory block */
    dirptr = dirblk;
    dirrem = (dirptr[0] << 8) | dirptr[1];
    if (dirrem < 2 || dirrem > 256)
    {
        fprintf (stdout, _("HHCDS003E Directory block byte count is invalid\n"));
        return -1;
    }

    /* Point to first directory entry */
    dirptr += 2;
    dirrem -= 2;

    /* Process each directory entry */
    for (n = *nmem; dirrem > 0; )
    {
        /* Point to next directory entry */
        dirent = (PDSDIR*)dirptr;

        /* Test for end of directory */
        if (memcmp(dirent->pds2name, eighthexFF, 8) == 0)
            return +1;

        /* Load the user data halfword count */
        k = dirent->pds2indc & PDS2INDC_LUSR;

        /* Point to next directory entry */
        size = 12 + k*2;
        dirptr += size;
        dirrem -= size;

        /* Extract the member name */
        make_asciiz (memnama, sizeof(memnama), dirent->pds2name, 8);
        if (dirent->pds2name[7] == OVERPUNCH_ZERO)
            memnama[7] = '{';

        /* Find member in first load table */
        for (i = 0; firstload[i] != NULL; i++)
            if (strcmp(memnama, firstload[i]) == 0) break;

        /* If not in first table, find in second table */
        if (firstload[i] == NULL)
        {
            for (i = 0; secondload[i] != NULL; i++)
                if (memcmp(memnama, secondload[i], 6) == 0) break;

            /* If not in second table then skip member */
            if (secondload[i] == NULL)
            {
                fprintf (stdout,
                        _("HHCDS018I %s %s skipped\n"),
                        memnama,
                        ((dirent->pds2indc & PDS2INDC_ALIAS) ?
                        "Alias" : "Member"));
                continue;
            }

        } /* end if(firstload[i]==NULL) */

        /* Check that member information array is not full */
        if (n >= MAX_MEMBERS)
        {
            fprintf (stdout,
                    _("HHCDS004E Number of members exceeds MAX_MEMBERS\n"));
            return -1;
        }

        /* Check that user data contains at least 1 TTR */
        if (((dirent->pds2indc & PDS2INDC_NTTR) >> PDS2INDC_NTTR_SHIFT)
                < 1)
        {
            fprintf (stdout,
                    _("HHCDS005E Member %s TTR count is zero\n"), memnama);
            return -1;
        }

        /* Extract the total module length */
        totlen = (dirent->pds2usrd[10] << 16)
                | (dirent->pds2usrd[11] << 8)
                | dirent->pds2usrd[12];

        /* Extract the length of the first text block */
        txtlen = (dirent->pds2usrd[13] << 8)
                | dirent->pds2usrd[14];

        /* Save member information in the array */
        memcpy (memtab[n].memname, dirent->pds2name, 8);
        memcpy (memtab[n].ttrtext, dirent->pds2usrd + 0, 3);
        memtab[n].dwdsize = totlen / 8;

        /* Flag the member if it is an alias */
        memtab[n].alias = (dirent->pds2indc & PDS2INDC_ALIAS) ? 1 : 0;

        /* Flag member if 7th character of name is non-numeric */
        memtab[n].notable = (memnama[6] < '0' || memnama[6] > '9') ?
                                                                1 : 0;

        /* Check that the member has a single text record */
        if ((dirent->pds2usrd[8] & 0x01) == 0 || totlen != txtlen)
        {
            fprintf (stdout,
                    _("HHCDS006W Member %s is not single text record\n"),
                    memnama);
            memtab[n].multitxt = 1;
        }

        /* Check that the total module length does not exceed X'7F8' */
        if (totlen > 255*8)
        {
            fprintf (stdout,
                    _("HHCDS007W Member %s size %4.4X "
                    "exceeds X\'7F8\' bytes\n"),
                    memnama, totlen);
        }

        /* Check that the total module length is a multiple of 8 */
        if (totlen & 0x7)
        {
            fprintf (stdout,
                    _("HHCDS008W Member %s size %4.4X "
                    "is not a multiple of 8\n"),
                    memnama, totlen);
        }

        /* Increment number of entries in table */
        *nmem = ++n;

    } /* end for */

    return 0;

} /* end function process_dirblk */

/*-------------------------------------------------------------------*/
/* Subroutine to resolve TTRs embedded within a member               */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      noext   Number of extents in dataset                         */
/*      extent  Dataset extent array                                 */
/*      memp    Array entry for member whose TTRs are to be resolved */
/*      memtab  Member information array                             */
/*      nmem    Number of entries in member information array        */
/*                                                                   */
/* Return value is 0 if OK, -1 if error                              */
/*-------------------------------------------------------------------*/
static int
resolve_xctltab (CIFBLK *cif, int noext, DSXTENT extent[],
                MEMINFO *memp, MEMINFO memtab[], int nmem)
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             len;                    /* Record length             */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
int             rec;                    /* Record number             */
int             trk;                    /* Relative track number     */
int             xctloff;                /* Offset to XCTL table      */
int             warn;                   /* 1=Flag TTRL difference    */
BYTE           *blkptr;                 /* -> Text record data       */
char            memnama[9];             /* Member name (ASCIIZ)      */
BYTE            svcnum[3];              /* SVC number (EBCDIC)       */
BYTE            prefix[3];              /* IGG/IFG prefix (EBCDIC)   */
BYTE            refname[8];             /* Referred name (EBCDIC)    */
char            refnama[9];             /* Referred name (ASCIIZ)    */

    /* Extract the member name */
    make_asciiz (memnama, sizeof(memnama), memp->memname, 8);
    if (memp->memname[7] == OVERPUNCH_ZERO)
        memnama[7] = '{';

    /* Skip the member if it is an alias */
    if (memp->alias)
    {
        fprintf (stdout, _("HHCDS009I Alias %s skipped\n"), memnama);
        return 0;
    }

    /* Skip the member if it has no XCTL table */
    if (memp->notable)
    {
        fprintf (stdout, _("HHCDS010I Member %s skipped\n"), memnama);
        return 0;
    }

    /* Error if member is not a single text record */
    if (memp->multitxt)
    {
        fprintf (stdout,
                _("HHCDS011E Member %s has multiple text records\n"),
                memnama);
        return -1;
    }

    /* Convert relative track to cylinder and head */
    trk = (memp->ttrtext[0] << 8) | memp->ttrtext[1];
    rec = memp->ttrtext[2];
    rc = convert_tt (trk, noext, extent, cif->heads, &cyl, &head);
    if (rc < 0)
    {
        fprintf (stdout,
                _("HHCDS012E Member %s has invalid TTR %4.4X%2.2X\n"),
                memnama, trk, rec);
        return -1;
    }

    fprintf (stdout,
            _("HHCDS013I Processing member %s text record TTR=%4.4X%2.2X "
            "CCHHR=%4.4X%4.4X%2.2X\n"),
            memnama, trk, rec, cyl, head, rec);

    /* Read the text record */
    rc = read_block (cif, cyl, head, rec,
                    NULL, NULL, &blkptr, &len);
    if (rc != 0)
    {
        fprintf (stdout,
                _("HHCDS014E Member %s error reading TTR %4.4X%2.2X\n"),
                memnama, trk, rec);
        return -1;
    }

    /* Check for incorrect length record */
    if (len < 8 || len > 1024 || (len & 0x7))
    {
        fprintf (stdout,
                _("HHCDS015E Member %s TTR %4.4X%2.2X "
                "text record length %4.4X is not valid\n"),
                memnama, trk, rec, len);
        return -1;
    }

    /* Check that text record length matches directory entry */
    if (len != memp->dwdsize * 8)
    {
        fprintf (stdout,
                _("HHCDS016E Member %s TTR %4.4X%2.2X "
                "text record length %4.4X does not match "
                "length %4.4X in directory\n"),
                memnama, trk, rec, len, memp->dwdsize * 8);
        return -1;
    }

    /* Extract the SVC number and the XCTL table offset
       from the last 4 bytes of the text record */
    memcpy (svcnum, blkptr + len - 4, sizeof(svcnum));
    xctloff = blkptr[len-1] * 8;

    /* For the first load of SVC 19, 20, 23, and 55, and for
       IFG modules, the table is in two parts.  The parts are
       separated by a X'FFFF' delimiter.  The first part refers
       to IFG modules, the second part refers to IGG modules */
    if ((memcmp(memnama, "IGC", 3) == 0
         && (memcmp(svcnum, "\xF0\xF1\xF9", 3) == 0
            || memcmp(svcnum, "\xF0\xF2\xF0", 3) == 0
            || memcmp(svcnum, "\xF0\xF2\xF3", 3) == 0
            || memcmp(svcnum, "\xF0\xF5\xF5", 3) == 0))
        || memcmp(memnama, "IFG", 3) == 0)
        convert_to_ebcdic (prefix, sizeof(prefix), "IFG");
    else
        convert_to_ebcdic (prefix, sizeof(prefix), "IGG");

    /* Process each entry in the XCTL table */
    while (1)
    {
        /* Exit at end of XCTL table */
        if (blkptr[xctloff] == HEX00 && blkptr[xctloff+1] == HEX00)
            break;

        /* Switch prefix at end of first part of table */
        if (blkptr[xctloff] == HEXFF && blkptr[xctloff+1] == HEXFF)
        {
            xctloff += 2;
            convert_to_ebcdic (prefix, sizeof(prefix), "IGG");
            continue;
        }

        /* Error if XCTL table overflows text record */
        if (xctloff >= len - 10)
        {
            fprintf (stdout,
                    _("HHCDS017E Member %s TTR %4.4X%2.2X "
                    "XCTL table improperly terminated\n"),
                    memnama, trk, rec);
            return -1;
        }

        /* Skip this entry if the suffix is blank */
        if (blkptr[xctloff] == HEX40
            && blkptr[xctloff+1] == HEX40)
        {
            xctloff += 6;
            continue;
        }

        /* Build the name of the member referred to */
        memcpy (refname, prefix, 3);
        memcpy (refname + 3, svcnum, 3);
        memcpy (refname + 6, blkptr+xctloff, 2);
        make_asciiz (refnama, sizeof(refnama), refname, 8);

        /* Display XCTL table entry */
        fprintf (stdout,
                _("HHCDS019I In member %s: %s TTRL=%2.2X%2.2X%2.2X%2.2X"),
                memnama, refnama,
                blkptr[xctloff+2], blkptr[xctloff+3],
                blkptr[xctloff+4], blkptr[xctloff+5]);

        /* Find the referred member in the member array */
        for (i = 0; i < nmem; i++)
        {
            if (memcmp(memtab[i].memname, refname, 8) == 0)
                break;
        } /* end for(i) */

        /* Loop if member not found */
        if (i == nmem)
        {
            fprintf (stdout, " ** Member %s not found **\n", refnama);
            xctloff += 6;
            continue;
        }

        /* Loop if TTRL in the XCTL table matches actual TTRL */
        if (memcmp(blkptr+xctloff+2, memtab[i].ttrtext, 3) == 0
            && blkptr[xctloff+5] == memtab[i].dwdsize)
        {
            fprintf (stdout, "\n");
            xctloff += 6;
            continue;
        }

        /* Flag entries whose L differs */
        if (blkptr[xctloff+5] != memtab[i].dwdsize)
            warn = 1;
        else
            warn = 0;

        /* Replace TTRL in the XCTL table by the actual TTRL */
        memcpy (blkptr+xctloff+2, memtab[i].ttrtext, 3);
        blkptr[xctloff+5] = memtab[i].dwdsize;

        fprintf (stdout,
                " replaced by TTRL=%2.2X%2.2X%2.2X%2.2X %s\n",
                blkptr[xctloff+2], blkptr[xctloff+3],
                blkptr[xctloff+4], blkptr[xctloff+5],
                (warn ? "****" : ""));

        /* Flag the track as modified to force rewrite */
        cif->trkmodif = 1;

        /* Point to next entry in XCTL table */
        xctloff += 6;

    } /* end while */

    return 0;
} /* end function resolve_xctltab */

/*-------------------------------------------------------------------*/
/* DASDISUP main entry point                                         */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             len;                    /* Record length             */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
int             rec;                    /* Record number             */
int             trk;                    /* Relative track number     */
char           *fname;                  /* -> CKD image file name    */
char           *sfname;                 /* -> CKD shadow file name   */
int             noext;                  /* Number of extents         */
DSXTENT         extent[16];             /* Extent descriptor array   */
BYTE           *blkptr;                 /* -> PDS directory block    */
CIFBLK         *cif;                    /* CKD image file descriptor */
MEMINFO        *memtab;                 /* -> Member info array      */
int             nmem = 0;               /* Number of array entries   */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, HERC_LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
#endif /*EXTERNALGUI*/

    /* Display the program identification message */
    display_version (stderr,
                     "Hercules IEHIOSUP program ", FALSE);

    /* Check the number of arguments */
    if (argc < 2 || argc > 3)
    {
        fprintf (stdout,
                "Usage: %s ckdfile [sf=shadow-file-name]\n",
                argv[0]);
        return -1;
    }

    /* The first argument is the name of the CKD image file */
    fname = argv[1];

    /* The next argument, if there, is the name of the shadow file */
    if (argc > 2) sfname = argv[2];
    else sfname = NULL;

    /* Obtain storage for the member information array */
    memtab = (MEMINFO*) malloc (sizeof(MEMINFO) * MAX_MEMBERS);
    if (memtab == NULL)
    {
        fprintf (stdout,
                _("HHCDS001E Cannot obtain storage for member array: %s\n"),
                strerror(errno));
        return -1;
    }

    /* Open the CKD image file */
    cif = open_ckd_image (fname, sfname, O_RDWR|O_BINARY, 0);
    if (cif == NULL) return -1;

    /* Build the extent array for the SVCLIB dataset */
    rc = build_extent_array (cif, "SYS1.SVCLIB", extent, &noext);
    if (rc < 0) return -1;

    /* Point to the start of the directory */
    trk = 0;
    rec = 1;

    /* Read the directory */
    while (1)
    {
        /* Convert relative track to cylinder and head */
        rc = convert_tt (trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0) return -1;

        /* Read a directory block */
        rc = read_block (cif, cyl, head, rec,
                        NULL, NULL, &blkptr, &len);
        if (rc < 0) return -1;

        /* Move to next track if block not found */
        if (rc > 0)
        {
            trk++;
            rec = 1;
            continue;
        }

        /* Exit at end of directory */
        if (len == 0) break;

        /* Extract information for each member in directory block */
        rc = process_dirblk (cif, noext, extent, blkptr,
                            memtab, &nmem);
        if (rc < 0) return -1;
        if (rc > 0) break;

        /* Point to the next directory block */
        rec++;

    } /* end while */

    fprintf (stdout,
            _("HHCDS002I End of directory: %d members selected\n"),
            nmem);

#ifdef EXTERNALGUI
    if (extgui) fprintf (stderr,"NMEM=%d\n",nmem);
#endif /*EXTERNALGUI*/

    /* Read each member and resolve the embedded TTRs */
    for (i = 0; i < nmem; i++)
    {
#ifdef EXTERNALGUI
        if (extgui) fprintf (stderr,"MEM=%d\n",i);
#endif /*EXTERNALGUI*/
        rc = resolve_xctltab (cif, noext, extent, memtab+i,
                                memtab, nmem);

    } /* end for(i) */

    /* Close the CKD image file and exit */
    rc = close_ckd_image (cif);
    free (memtab);
    return rc;

} /* end function main */
