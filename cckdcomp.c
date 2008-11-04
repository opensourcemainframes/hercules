/* CCKDCOMP.C   (c) Copyright Roger Bowler, 1999-2007                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

// $Id$

/*-------------------------------------------------------------------*/
/* Remove all free space on a compressed ckd file                    */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.31  2007/12/01 23:31:57  fish
// Fix cckdcdsk/cckdcomp/cckdutil no message o/p issue
//
// Revision 1.30  2007/06/23 00:04:03  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.29  2006/12/08 09:43:17  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"

int syntax ();

/*-------------------------------------------------------------------*/
/* Main function for stand-alone compress                            */
/*-------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
int             level=-1;               /* Level for chkdsk          */
int             force=0;                /* 1=Compress if OPENED set  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */

    INITIALIZE_UTILITY("cckdcomp");

    /* parse the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '2':
            case '3':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax ();
                       force = 1;
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version 
                         (stderr, "Hercules cckd compress program ", FALSE);
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
        dev->fd = open (dev->filename, O_RDWR|O_BINARY);
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

        /* call chkdsk */
        if (cckd_chkdsk (dev, level) < 0)
        {
            cckdumsg (dev, 708, "chkdsk errors\n");
            close (dev->fd);
            continue;
        }
    
        /* call compress */
        rc = cckd_comp (dev);

        close (dev->fd);

    } /* for each arg */

    return 0;
}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/

int syntax()
{
    fprintf (stderr, "\ncckdcomp [-v] [-f] [-level] file1 [file2 ... ]\n"
                "\n"
                "          -v      display version and exit\n"
                "\n"
                "          -f      force check even if OPENED bit is on\n"
                "\n"
                "        chkdsk level is a digit 0 - 3:\n"
                "          -0  --  minimal checking\n"
                "          -1  --  normal  checking\n"
                "          -2  --  intermediate checking\n"
                "          -3  --  maximal checking\n"
                "         default  0\n"
                "\n");
    return -1;
}
