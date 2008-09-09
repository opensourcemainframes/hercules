/* CCKDCDSK.C   (c) Copyright Roger Bowler, 1999-2007                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

// $Id$

/*-------------------------------------------------------------------*/
/* Perform check function on a compressed ckd file                   */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.43  2008/09/04 22:03:15  gsmith
// Fix 64-bit length problem in cdsk_valid_trk - Tony Harminc
//
// Revision 1.42  2007/06/23 00:04:03  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.41  2006/12/08 09:43:17  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"

int syntax ();

/*-------------------------------------------------------------------*/
/* Main function for stand-alone chkdsk                              */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
int             level=1;                /* Chkdsk level checking     */
int             ro=0;                   /* 1=Open readonly           */
int             force=0;                /* 1=Check if OPENED bit on  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */

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

    /* parse the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax ();
                       force = 1;
                       break;
            case 'r':  if (argv[0][2] == 'o' && argv[0][3] == '\0')
                           ro = 1;
                       else return syntax ();
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version
                         (stderr, "Hercules cckd chkdsk program ", FALSE);
                       return 0;
            default:   return syntax ();
        }
    }

    if (argc < 1) return syntax ();

    for (i = 0; i < argc; i++)
    {
        memset (dev, 0, sizeof(DEVBLK));
        dev->batch = 1;

        /* open the file */
        hostpath(dev->filename, argv[i], sizeof(dev->filename));
        dev->fd = open (dev->filename, ro ? O_RDONLY|O_BINARY : O_RDWR|O_BINARY);
        if (dev->fd < 0)
        {
            cckdumsg (dev, 700, "open error: %s\n", strerror(errno));
            continue;
        }

        /* Check CCKD_OPENED bit if -f not specified */
        if (!force)
        {
            if (lseek (dev->fd, CCKD_DEVHDR_POS, SEEK_SET) < 0)
            {
                cckdumsg (dev, 702, "lseek error offset 0x%" I64_FMT "x: %s\n",
                          (long long)CCKD_DEVHDR_POS, strerror(errno));
                close (dev->fd);
                continue;
            }
            if ((rc = read (dev->fd, &cdevhdr, CCKD_DEVHDR_SIZE)) < CCKD_DEVHDR_SIZE)
            {
                cckdumsg (dev, 703, "read error rc=%d offset 0x%" I64_FMT "x len %d: %s\n",
                          rc, (long long)CCKD_DEVHDR_POS, CCKD_DEVHDR_SIZE,
                          rc < 0 ? strerror(errno) : "incomplete");
                close (dev->fd);
                continue;
            }
            if (cdevhdr.options & CCKD_OPENED)
            {
                cckdumsg (dev, 707, "OPENED bit is on, use -f\n");
                close (dev->fd);
                continue;
            }
        } /* if (!force) */

        rc = cckd_chkdsk (dev, level);

        close (dev->fd);
    } /* for each arg */

    return 0;
}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/
int syntax()
{
    fprintf (stderr, _("\ncckdcdsk [-v] [-f] [-level] [-ro] file1 [file2 ...]\n"
                "\n"
                "      -v      display version and exit\n"
                "\n"
                "      -f      force check even if OPENED bit is on\n"
                "\n"
                "    level is a digit 0 - 4:\n"
                "      -0  --  minimal checking (hdr, chdr, l1tab, l2tabs)\n"
                "      -1  --  normal  checking (hdr, chdr, l1tab, l2tabs, free spaces)\n"
                "      -2  --  extra   checking (hdr, chdr, l1tab, l2tabs, free spaces, trkhdrs)\n"
                "      -3  --  maximal checking (hdr, chdr, l1tab, l2tabs, free spaces, trkimgs)\n"
                "      -4  --  recover everything without using meta-data\n"
                "\n"
                "      -ro     open file readonly, no repairs\n"
                "\n"));
    return -1;
}
