/* DASDPDSU.C   (c) Copyright Roger Bowler, 1999-2002                */
/*              Hercules DASD Utilities: PDS unloader                */

/*-------------------------------------------------------------------*/
/* This program unloads members of a partitioned dataset from        */
/* a virtual DASD volume and copies each member to a flat file.      */
/*                                                                   */
/* The command format is:                                            */
/*      dasdpdsu ckdfile dsname [ascii]                              */
/* where: ckdfile is the name of the CKD image file                  */
/*        dsname is the name of the PDS to be unloaded               */
/*        ascii is an optional keyword which will cause the members  */
/*        to be unloaded as ASCII variable length text files.        */
/* Each member is copied to a file memname.mac in the current        */
/* working directory. If the ascii keyword is not specified then     */
/* the members are unloaded as fixed length binary files.            */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "dasdblks.h"

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static BYTE asciiflag = 0;              /* 1=Translate to ASCII      */
static BYTE eighthexFF[] =              /* End of directory marker   */
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
static int  extgui = 0;
#endif /*EXTERNALGUI*/

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

    /* Build the output file name */
    memset (ofname, 0, sizeof(ofname));
    strncpy (ofname, memname, 8);
    string_to_lower (ofname);
    strcat (ofname, ".mac");

    /* Open the output file */
    ofp = fopen (ofname, "w");
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

    fprintf (stderr,
            "Member %s TTR=%4.4X%2.2X\n",
            memname, trk, rec);

    /* Read the member */
    while (1)
    {
        /* Convert relative track to cylinder and head */
        rc = convert_tt (trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0) return -1;

//      fprintf (stderr,
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
            fprintf (stderr,
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
                fprintf (stderr,
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

/*-------------------------------------------------------------------*/
/* Subroutine to process a directory block                           */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      noext   Number of extents in dataset                         */
/*      extent  Dataset extent array                                 */
/*      dirblk  Pointer to directory block                           */
/*                                                                   */
/* Return value is 0 if OK, +1 if end of directory, or -1 if error   */
/*-------------------------------------------------------------------*/
static int
process_dirblk (CIFBLK *cif, int noext, DSXTENT extent[], BYTE *dirblk)
{
int             rc;                     /* Return code               */
int             size;                   /* Size of directory entry   */
int             k;                      /* Userdata halfword count   */
BYTE           *dirptr;                 /* -> Next byte within block */
int             dirrem;                 /* Number of bytes remaining */
PDSDIR         *dirent;                 /* -> Directory entry        */
BYTE            memname[9];             /* Member name (ASCIIZ)      */

    /* Load number of bytes in directory block */
    dirptr = dirblk;
    dirrem = (dirptr[0] << 8) | dirptr[1];
    if (dirrem < 2 || dirrem > 256)
    {
        fprintf (stderr, "Directory block byte count is invalid\n");
        return -1;
    }

    /* Point to first directory entry */
    dirptr += 2;
    dirrem -= 2;

    /* Process each directory entry */
    while (dirrem > 0)
    {
        /* Point to next directory entry */
        dirent = (PDSDIR*)dirptr;

        /* Test for end of directory */
        if (memcmp(dirent->pds2name, eighthexFF, 8) == 0)
            return +1;

        /* Extract the member name */
        make_asciiz (memname, sizeof(memname), dirent->pds2name, 8);

        /* Process the member */
        rc = process_member (cif, noext, extent, memname,
                            dirent->pds2ttrp);
        if (rc < 0) return -1;

        /* Load the user data halfword count */
        k = dirent->pds2indc & PDS2INDC_LUSR;

        /* Point to next directory entry */
        size = 12 + k*2;
        dirptr += size;
        dirrem -= size;
    }

    return 0;
} /* end function process_dirblk */

/*-------------------------------------------------------------------*/
/* DASDPDSU main entry point                                         */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
int             len;                    /* Record length             */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
int             rec;                    /* Record number             */
int             trk;                    /* Relative track number     */
BYTE           *fname;                  /* -> CKD image file name    */
BYTE            dsnama[45];             /* Dataset name (ASCIIZ)     */
int             noext;                  /* Number of extents         */
DSXTENT         extent[16];             /* Extent descriptor array   */
BYTE           *blkptr;                 /* -> PDS directory block    */
BYTE            dirblk[256];            /* Copy of directory block   */
CIFBLK         *cif;                    /* CKD image file descriptor */

    /* Display the program identification message */
    display_version (stderr, "Hercules PDS unload program ");

    /* Check the number of arguments */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    if (argc < 3 || argc > 4)
    {
        fprintf (stderr,
                "Usage: %s ckdfile pdsname [ascii]\n",
                argv[0]);
        return -1;
    }

    /* The first argument is the name of the CKD image file */
    fname = argv[1];

    /* The second argument is the dataset name */
    memset (dsnama, 0, sizeof(dsnama));
    strncpy (dsnama, argv[2], sizeof(dsnama)-1);
    string_to_upper (dsnama);

    /* The third argument is an optional keyword */
    if (argc > 3 && argv[3] != NULL)
    {
        if (strcasecmp(argv[3], "ascii") == 0)
            asciiflag = 1;
        else
        {
            fprintf (stderr,
                    "Keyword %s is not recognized\n",
                    argv[3]);
            return -1;
        }
    }

    /* Open the CKD image file */
    cif = open_ckd_image (fname, O_RDONLY|O_BINARY);
    if (cif == NULL) return -1;

    /* Build the extent array for the requested dataset */
    rc = build_extent_array (cif, dsnama, extent, &noext);
    if (rc < 0) return -1;

#ifdef EXTERNALGUI
    /* Calculate ending relative track */
    if (extgui)
    {
        int bcyl;  /* Extent begin cylinder     */
        int btrk;  /* Extent begin head         */
        int ecyl;  /* Extent end cylinder       */
        int etrk;  /* Extent end head           */
        int trks;  /* total tracks in dataset   */
        int i;     /* loop control              */

        for (i = 0, trks = 0; i < noext; i++)
        {
            bcyl = (extent[i].xtbcyl[0] << 8) | extent[i].xtbcyl[1];
            btrk = (extent[i].xtbtrk[0] << 8) | extent[i].xtbtrk[1];
            ecyl = (extent[i].xtecyl[0] << 8) | extent[i].xtecyl[1];
            etrk = (extent[i].xtetrk[0] << 8) | extent[i].xtetrk[1];

            trks += (((ecyl * cif->heads) + etrk) - ((bcyl * cif->heads) + btrk)) + 1;
        }

        fprintf(stderr,"ETRK=%d\n",trks-1);
    }
#endif /*EXTERNALGUI*/

    /* Point to the start of the directory */
    trk = 0;
    rec = 1;

    /* Read the directory */
    while (1)
    {
#ifdef EXTERNALGUI
        if (extgui) fprintf(stderr,"CTRK=%d\n",trk);
#endif /*EXTERNALGUI*/

        /* Convert relative track to cylinder and head */
        rc = convert_tt (trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0) return -1;

        /* Read a directory block */
        fprintf (stderr,
                "Reading directory block at cyl %d head %d rec %d\n",
                cyl, head, rec);

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

        /* Copy the directory block */
        memcpy (dirblk, blkptr, sizeof(dirblk));

        /* Process each member in the directory block */
        rc = process_dirblk (cif, noext, extent, dirblk);
        if (rc < 0) return -1;
        if (rc > 0) break;

        /* Point to the next directory block */
        rec++;

    } /* end while */

    fprintf (stderr,
            "End of directory\n");

    /* Close the CKD image file and exit */
    rc = close_ckd_image (cif);
    return rc;

} /* end function main */
