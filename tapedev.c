/* TAPEDEV.C    (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* 3420 magnetic tape devices for the Hercules ESA/390 emulator.     */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Four emulated tape formats are supported:                         */
/*                                                                   */
/* 1. AWSTAPE   This is the format used by the P/390.                */
/*              The entire tape is contained in a single flat file.  */
/*              A tape block consists of one or more block segments. */
/*              Each block segment is preceded by a 6-byte header.   */
/*              Files are separated by tapemarks, which consist      */
/*              of headers with zero block length.                   */
/*              AWSTAPE files are readable and writable.             */
/*                                                                   */
/*              Support for AWSTAPE is in the "AWSTAPE.C" member.    */
/*                                                                   */
/*                                                                   */
/* 2. OMATAPE   This is the Optical Media Attach device format.      */
/*              Each physical file on the tape is represented by     */
/*              a separate flat file.  The collection of files that  */
/*              make up the physical tape is obtained from an ASCII  */
/*              text file called the "tape description file", whose  */
/*              file name is always tapes/xxxxxx.tdf (where xxxxxx   */
/*              is the volume serial number of the tape).            */
/*              Three formats of tape files are supported:           */
/*              * FIXED files contain fixed length EBCDIC blocks     */
/*                with no headers or delimiters. The block length    */
/*                is specified in the TDF file.                      */
/*              * TEXT files contain variable length ASCII blocks    */
/*                delimited by carriage return line feed sequences.  */
/*                The data is translated to EBCDIC by this module.   */
/*              * HEADER files contain variable length blocks of     */
/*                EBCDIC data prefixed by a 16-byte header.          */
/*              The TDF file and all of the tape files must reside   */
/*              reside under the same directory which is normally    */
/*              on CDROM but can be on disk.                         */
/*              OMATAPE files are supported as read-only media.      */
/*                                                                   */
/*              OMATAPE tape Support is in the "OMATAPE.C" member.   */
/*                                                                   */
/*                                                                   */
/* 3. SCSITAPE  This format allows reading and writing of 4mm or     */
/*              8mm DAT tape, 9-track open-reel tape, or 3480-type   */
/*              cartridge on an appropriate SCSI-attached drive.     */
/*              All SCSI tapes are processed using the generalized   */
/*              SCSI tape driver (st.c) which is controlled using    */
/*              the MTIOCxxx set of IOCTL commands.                  */
/*              PROGRAMMING NOTE: the 'tape' portability macros for  */
/*              physical (SCSI) tapes MUST be used for all tape i/o! */
/*                                                                   */
/*              SCSI tape Support is in the "SCSITAPE.C" member.     */
/*                                                                   */
/*                                                                   */
/* 4. HET       This format is based on the AWSTAPE format but has   */
/*              been extended to support compression.  Since the     */
/*              basic file format has remained the same, AWSTAPEs    */
/*              can be read/written using the HET routines.          */
/*                                                                   */
/*              Support for HET is in the "HETTAPE.C" member.        */
/*                                                                   */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      3480 commands contributed by Jan Jaeger                      */
/*      Sense byte improvements by Jan Jaeger                        */
/*      3480 Read Block ID and Locate CCWs by Brandon Hill           */
/*      Unloaded tape support by Brandon Hill                    v209*/
/*      HET format support by Leland Lucius                      v209*/
/*      JCS - minor changes by John Summerfield                  2003*/
/*      PERFORM SUBSYSTEM FUNCTION / CONTROL ACCESS support by       */
/*      Adrian Trenkwalder (with futher enhancements by Fish)        */
/*      **INCOMPLETE** 3590 support by Fish (David B. Trout)         */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Reference information:                                            */
/* SC53-1200 S/370 and S/390 Optical Media Attach/2 User's Guide     */
/* SC53-1201 S/370 and S/390 Optical Media Attach/2 Technical Ref    */
/* SG24-2506 IBM 3590 Tape Subsystem Technical Guide                 */
/* GA32-0331 IBM 3590 Hardware Reference                             */
/* GA32-0329 IBM 3590 Introduction and Planning Guide                */
/* SG24-2594 IBM 3590 Multiplatform Implementation                   */
/* ANSI INCITS 131-1994 (R1999) SCSI-2 Reference                     */
/* GA32-0127 IBM 3490E Hardware Reference                            */
/* GC35-0152 EREP Release 3.5.0 Reference                            */
/* SA22-7204 ESA/390 Common I/O-Device Commands                      */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.136  2008/03/27 07:14:17  fish
// SCSI MODS: groundwork: part 3: final shuffling around.
// Moved functions from one module to another and resequenced
// functions within each. NO CODE WAS ACTUALLY CHANGED.
// Next commit will begin the actual changes.
//
// Revision 1.135  2008/03/26 07:23:51  fish
// SCSI MODS part 2: split tapedev.c: aws, het, oma processing moved
// to separate modules, CCW processing moved to separate module.
//
// Revision 1.134  2008/03/25 11:41:31  fish
// SCSI TAPE MODS part 1: groundwork: non-functional changes:
// rename some functions, comments, general restructuring, etc.
// New source modules awstape.c, omatape.c, hettape.c and
// tapeccws.c added, but not yet used (all will be used in a future
// commit though when tapedev.c code is eventually split)
//
// Revision 1.133  2008/03/13 01:44:17  kleonard
// Fix residual read-only setting for tape device
//
// Revision 1.132  2008/03/04 01:10:29  ivan
// Add LEGACYSENSEID config statement to allow X'E4' Sense ID on devices
// that originally didn't support it. Defaults to off for compatibility reasons
//
// Revision 1.131  2008/03/04 00:25:25  ivan
// Ooops.. finger check on 8809 case for numdevid.. Thanks Roger !
//
// Revision 1.130  2008/03/02 12:00:04  ivan
// Re-disable Sense ID on 3410, 3420, 8809 : report came in that it breaks MTS
//
// Revision 1.129  2007/12/14 17:48:52  rbowler
// Enable SENSE ID CCW for 2703,3410,3420
//
// Revision 1.128  2007/11/29 03:36:40  fish
// Re-sequence CCW opcode 'case' statements to be in ascending order.
// COSMETIC CHANGE ONLY. NO ACTUAL LOGIC WAS CHANGED.
//
// Revision 1.127  2007/11/13 15:10:52  rbowler
// fsb_awstape support for segmented blocks
//
// Revision 1.126  2007/11/11 20:46:50  rbowler
// read_awstape support for segmented blocks
//
// Revision 1.125  2007/11/09 14:59:34  rbowler
// Move misplaced comment and restore original programming style
//
// Revision 1.124  2007/11/02 16:04:15  jmaynard
// Removing redundant #if !(defined OPTION_SCSI_TAPE).
//
// Revision 1.123  2007/09/01 06:32:24  fish
// Surround 3590 SCSI test w/#ifdef (OPTION_SCSI_TAPE)
//
// Revision 1.122  2007/08/26 14:37:17  fish
// Fix missed unfixed 31 Aug 2006 non-SCSI tape Locate bug
//
// Revision 1.121  2007/07/24 23:06:32  fish
// Force command-reject for 3590 Medium Sense and Mode Sense
//
// Revision 1.120  2007/07/24 22:54:49  fish
// (comment changes only)
//
// Revision 1.119  2007/07/24 22:46:09  fish
// Default to --blkid-32 and --no-erg for 3590 SCSI
//
// Revision 1.118  2007/07/24 22:36:33  fish
// Fix tape Synchronize CCW (x'43') to do actual commit
//
// Revision 1.117  2007/07/24 21:57:29  fish
// Fix Win32 SCSI tape "Locate" and "ReadBlockId" SNAFU
//
// Revision 1.116  2007/06/23 00:04:18  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.115  2007/04/06 15:40:25  fish
// Fix Locate Block & Read BlockId for SCSI tape broken by 31 Aug 2006 preliminary-3590-support change
//
// Revision 1.114  2007/02/25 21:10:44  fish
// Fix het_locate to continue on tapemark
//
// Revision 1.113  2007/02/03 18:58:06  gsmith
// Fix MVT tape CMDREJ error
//
// Revision 1.112  2006/12/28 03:04:17  fish
// PR# tape/100: Fix crash in "open_omatape()" in tapedev.c if bad filespec entered in OMA (TDF)  file
//
// Revision 1.111  2006/12/11 17:25:59  rbowler
// Change locblock from long to U32 to correspond with dev->blockid
//
// Revision 1.110  2006/12/08 09:43:30  jj
// Add CVS message log
//
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* This module's header file                  */

/*-------------------------------------------------------------------*/

//#define  ENABLE_TRACING_STMTS     // (Fish: DEBUGGING)

#ifdef ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
  // (TRACE, ASSERT, and VERIFY macros are #defined in hmacros.h)
#else
  #undef  TRACE
  #undef  ASSERT
  #undef  VERIFY
  #define TRACE       1 ? ((void)0) : logmsg
  #define ASSERT(a)
  #define VERIFY(a)   ((void)(a))
#endif

/*-------------------------------------------------------------------*/

#if defined(WIN32) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  SYSBLK *psysblk;
  #define sysblk (*psysblk)
#endif

/*-------------------------------------------------------------------*/

DEVHND  tapedev_device_hndinfo   =
{
    &tapedev_init_handler,             /* Device Initialisation      */
    &tapedev_execute_ccw,              /* Device CCW execute         */
    &tapedev_close_device,             /* Device Close               */
    &tapedev_query_device,             /* Device Query               */
    NULL,                              /* Device Start channel pgm   */
    NULL,                              /* Device End channel pgm     */
    NULL,                              /* Device Resume channel pgm  */
    NULL,                              /* Device Suspend channel pgm */
    NULL,                              /* Device Read                */
    NULL,                              /* Device Write               */
    NULL,                              /* Device Query used          */
    NULL,                              /* Device Reserve             */
    NULL,                              /* Device Release             */
    NULL,                              /* Device Attention           */
    TapeImmedCommands,                 /* Immediate CCW Codes        */
    NULL,                              /* Signal Adapter Input       */
    NULL,                              /* Signal Adapter Output      */
    NULL,                              /* Hercules suspend           */
    NULL                               /* Hercules resume            */
};

/*-------------------------------------------------------------------*/
/* Libtool static name colision resolution...                        */
/* Note: lt_dlopen will look for symbol & modulename_LTX_symbol      */
/*-------------------------------------------------------------------*/

#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)

  #define  hdl_ddev   hdt3420_LTX_hdl_ddev
  #define  hdl_depc   hdt3420_LTX_hdl_depc
  #define  hdl_reso   hdt3420_LTX_hdl_reso
  #define  hdl_init   hdt3420_LTX_hdl_init
  #define  hdl_fini   hdt3420_LTX_hdl_fini

#endif

/*-------------------------------------------------------------------*/

#if defined(OPTION_DYNAMIC_LOAD)

HDL_DEPENDENCY_SECTION;
{
    HDL_DEPENDENCY ( HERCULES );
    HDL_DEPENDENCY ( DEVBLK   );
    HDL_DEPENDENCY ( SYSBLK   );
}
END_DEPENDENCY_SECTION

/*-------------------------------------------------------------------*/

HDL_DEVICE_SECTION;
{
    HDL_DEVICE ( 3410, tapedev_device_hndinfo );
    HDL_DEVICE ( 3411, tapedev_device_hndinfo );
    HDL_DEVICE ( 3420, tapedev_device_hndinfo );
    HDL_DEVICE ( 3422, tapedev_device_hndinfo );
    HDL_DEVICE ( 3430, tapedev_device_hndinfo );
    HDL_DEVICE ( 3480, tapedev_device_hndinfo );
    HDL_DEVICE ( 3490, tapedev_device_hndinfo );
    HDL_DEVICE ( 3590, tapedev_device_hndinfo );
    HDL_DEVICE ( 8809, tapedev_device_hndinfo );
    HDL_DEVICE ( 9347, tapedev_device_hndinfo );
    HDL_DEVICE ( 9348, tapedev_device_hndinfo );
}
END_DEVICE_SECTION

/*-------------------------------------------------------------------*/

#if defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)

  #undef sysblk

  HDL_RESOLVER_SECTION;
  {
    HDL_RESOLVE_PTRVAR ( psysblk, sysblk );
  }
  END_RESOLVER_SECTION

#endif // defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)

#endif // defined(OPTION_DYNAMIC_LOAD)

/*-------------------------------------------------------------------*/
/*  (see 'tapedev.h' for layout of TAPEMEDIA_HANDLER structure)      */
/*-------------------------------------------------------------------*/

TAPEMEDIA_HANDLER  tmh_aws  =
{
    &generic_tmhcall,
    &open_awstape,
    &close_awstape,
    &read_awstape,
    &write_awstape,
    &rewind_awstape,
    &bsb_awstape,
    &fsb_awstape,
    &bsf_awstape,
    &fsf_awstape,
    &write_awsmark,
    &sync_awstape,
    &no_operation,              // (DSE)    ZZ FIXME: not coded yet
    &no_operation,              // (ERG)
    &is_tapeloaded_filename,
    &passedeot_awstape,
    &readblkid_virtual,
    &locateblk_virtual
};

/*-------------------------------------------------------------------*/

TAPEMEDIA_HANDLER  tmh_het   =
{
    &generic_tmhcall,
    &open_het,
    &close_het,
    &read_het,
    &write_het,
    &rewind_het,
    &bsb_het,
    &fsb_het,
    &bsf_het,
    &fsf_het,
    &write_hetmark,
    &sync_het,
    &no_operation,              // (DSE)    ZZ FIXME: not coded yet
    &no_operation,              // (ERG)
    &is_tapeloaded_filename,
    &passedeot_het,
    &readblkid_virtual,
    &locateblk_virtual
};

/*-------------------------------------------------------------------*/

TAPEMEDIA_HANDLER  tmh_oma   =
{
    &generic_tmhcall,
    &open_omatape,
    &close_omatape,
    &read_omatape,
    &write_READONLY5,           // WRITE
    &rewind_omatape,
    &bsb_omatape,
    &fsb_omatape,
    &bsf_omatape,
    &fsf_omatape,
    &write_READONLY,            // WTM
    &write_READONLY,            // SYNC
    &write_READONLY,            // DSE
    &write_READONLY,            // ERG
    &is_tapeloaded_filename,
    &return_false1,             // passedeot
    &readblkid_virtual,
    &locateblk_virtual
};

/*-------------------------------------------------------------------*/

#if defined(OPTION_SCSI_TAPE)

TAPEMEDIA_HANDLER  tmh_scsi   =
{
    &generic_tmhcall,
    &open_scsitape,
    &close_scsitape,
    &read_scsitape,
    &write_scsitape,
    &rewind_scsitape,
    &bsb_scsitape,
    &fsb_scsitape,
    &bsf_scsitape,
    &fsf_scsitape,
    &write_scsimark,
    &sync_scsitape,
    &dse_scsitape,
    &erg_scsitape,
    &is_tape_mounted_scsitape,
    &passedeot_scsitape,
    &readblkid_scsitape,
    &locateblk_scsitape
};

#endif /* defined(OPTION_SCSI_TAPE) */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int tapedev_init_handler (DEVBLK *dev, int argc, char *argv[])
{
U16             cutype;                 /* Control unit type         */
BYTE            cumodel;                /* Control unit model number */
BYTE            devmodel;               /* Device model number       */
BYTE            devclass;               /* Device class              */
BYTE            devtcode;               /* Device type code          */
U32             sctlfeat;               /* Storage control features  */
int             haverdc;                /* RDC Supported             */
int             rc;

    /* Determine the control unit type and model number */
    /* Support for 3490/3422/3430/8809/9347, etc.. */
    /* Close current tape */
    if(dev->fd>=0)
    {
        dev->tmh->close(dev);
        dev->fd=-1;
    }
    autoload_close(dev);
    haverdc=0;
    dev->tdparms.displayfeat=0;

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x3420;

    switch(dev->devtype)
    {
    case 0x3480:
        cutype = 0x3480;
        cumodel = 0x31;
        devmodel = 0x31; /* Model D31 */
        devclass = 0x80;
        devtcode = 0x80;
        sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                               /* Autoloader installed */
                               /* IDRC Supported */
        dev->numdevid = 7;
        dev->numsense = 24;
        haverdc=1;
        dev->tdparms.displayfeat=1;
        break;
   case 0x3490:
        cutype = 0x3490;
        cumodel = 0x50; /* Model C10 */
        devmodel = 0x50;
        devclass = 0x80;
        devtcode = 0x80; /* Valid for 3490 too */
        sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                               /* Autoloader installed */
                               /* IDRC Supported */
        dev->numdevid = 7;
        dev->numsense = 32;
        haverdc=1;
        dev->tdparms.displayfeat=1;
        break;
   case 0x3590:
        cutype = 0x3590;
        cumodel = 0x50; /* Model C10 ?? */
        devmodel = 0x50;
        devclass = 0x80;
        devtcode = 0x80; /* Valid for 3590 too */
        sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                               /* Autoloader installed */
                               /* IDRC Supported */
        dev->numdevid = 7;
        dev->numsense = 32;
        haverdc=1;
        dev->tdparms.displayfeat=1;
        break;
    case 0x3420:
        cutype = 0x3803;
        cumodel = 0x02;
        devmodel = 0x06;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = sysblk.legacysenseid ? 7 : 0;
        dev->numsense = 24;
        break;
    case 0x9347:
        cutype = 0x9347;
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = 7;
        dev->numsense = 32;
        break;
    case 0x9348:
        cutype = 0x9348;
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = 7;
        dev->numsense = 32;
        break;
    case 0x8809:
        cutype = 0x8809;
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = sysblk.legacysenseid ? 7 : 0;
        dev->numsense = 32;
        break;
    case 0x3410:
    case 0x3411:
        dev->devtype = 0x3411;  /* a 3410 is a 3411 */
        cutype = 0x3115; /* Model 115 IFA */
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        /* disable senseid again.. Breaks MTS */
        dev->numdevid = sysblk.legacysenseid ? 7 : 0;
        dev->numsense = 9;
        break;
    case 0x3422:
        cutype = 0x3422;
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = 7;
        dev->numsense = 32;
        break;
    case 0x3430:
        cutype = 0x3422;
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = 7;
        dev->numsense = 32;
        break;
    default:
        logmsg(_("Unsupported device type specified %4.4x\n"),dev->devtype);
        cutype = dev->devtype; /* don't know what to do really */
        cumodel = 0x01;
        devmodel = 0x01;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
        dev->numdevid = 0; /* We don't know */
        dev->numsense = 1;
        break;
    } // end switch(dev->devtype)

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = cutype >> 8;
    dev->devid[2] = cutype & 0xFF;
    dev->devid[3] = cumodel;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = devmodel;

    /* Initialize the device characteristics bytes */
    if (haverdc)
    {
        memset (dev->devchar, 0, sizeof(dev->devchar));
        memcpy (dev->devchar, dev->devid+1, 6);
        dev->devchar[6] = (sctlfeat >> 24) & 0xFF;
        dev->devchar[7] = (sctlfeat >> 16) & 0xFF;
        dev->devchar[8] = (sctlfeat >> 8) & 0xFF;
        dev->devchar[9] = sctlfeat & 0xFF;
        dev->devchar[10] = devclass;
        dev->devchar[11] = devtcode;
        dev->devchar[40] = 0x41;
        dev->devchar[41] = 0x80;
        dev->numdevchar = 64;
    }

    /* Initialize SCSI tape control fields */
#if defined(OPTION_SCSI_TAPE)
    dev->sstat               = GMT_DR_OPEN(-1);
    dev->stape_getstat_sstat = GMT_DR_OPEN(-1);
#endif

    /* Clear the DPA */
    memset (dev->pgid, 0, sizeof(dev->pgid));
    /* Clear Drive password - Adrian */
    memset (dev->drvpwd, 0, sizeof(dev->drvpwd));

    /* Request the channel to merge data chained write CCWs into
       a single buffer before passing data to the device handler */
    dev->cdwmerge = 1;

    /* Tape is a syncio type 2 device */
    dev->syncio = 2;

    /* ISW */
    /* Build a 'clear' sense */
    memset (dev->sense, 0, sizeof(dev->sense));
    dev->sns_pending = 0;

    // Initialize the [non-SCSI] auto-loader...

    // PROGRAMMING NOTE: we don't [yet] know at this early stage
    // what type of tape device we're dealing with (SCSI (non-virtual)
    // or non-SCSI (virtual)) since 'mountnewtape' hasn't been called
    // yet (which is the function that determines which media handler
    // should be used and is the one that initializes dev->tapedevt)

    // The only thing we know (or WILL know once 'autoload_init'
    // is called) is whether or not there was a [non-SCSI] auto-
    // loader defined for the device. That's it and nothing more.

    autoload_init( dev, argc, argv );

    // Was an auto-loader defined for this device?
    if ( !dev->als )
    {
        // No. Just mount whatever tape there is (if any)...
        rc = mountnewtape( dev, argc, argv );
    }
    else
    {
        // Yes. Try mounting the FIRST auto-loader slot...
        if ( (rc = autoload_mount_first( dev )) != 0 )
        {
            // If that doesn't work, try subsequent slots...
            while
            (
                dev->als
                &&
                (rc = autoload_mount_next( dev )) != 0
            )
            {
                ;  // (nop; just go on to next slot)
            }
            rc = dev->als ? rc : -1;
        }
    }
    return rc;

} /* end function tapedev_init_handler */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int tapedev_close_device ( DEVBLK *dev )
{
    autoload_close(dev);
    dev->tmh->close(dev);
    ASSERT( dev->fd < 0 );

    dev->curfilen  = 1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;
    dev->curblkrem = 0;
    dev->curbufoff = 0;
    dev->blockid   = 0;
    dev->fenced = 0;

    return 0;
} /* end function tapedev_close_device */


/*-------------------------------------------------------------------*/
/*  The following table goes hand-in-hand with the 'enum' values     */
/*  that immediately follow.  Used by 'mountnewtape' function.       */
/*-------------------------------------------------------------------*/

PARSER  ptab  [] =
{
    { "awstape",    NULL },
    { "idrc",       "%d" },
    { "compress",   "%d" },
    { "method",     "%d" },
    { "level",      "%d" },
    { "chunksize",  "%d" },
    { "maxsize",    "%d" },
    { "maxsizeK",   "%d" },
    { "maxsizeM",   "%d" },
    { "eotmargin",  "%d" },
    { "strictsize", "%d" },
    { "readonly",   "%d" },
    { "ro",         NULL },
    { "noring",     NULL },
    { "rw",         NULL },
    { "ring",       NULL },
    { "deonirq",    "%d" },
    { "--blkid-24", NULL },
    { "--blkid-32", NULL },
    { "--no-erg",   NULL },
    { NULL,         NULL },   /* (end of table) */
};

/*-------------------------------------------------------------------*/
/*  The following table goes hand-in-hand with the 'ptab' PARSER     */
/*  table immediately above.                                         */
/*-------------------------------------------------------------------*/

enum
{
    TDPARM_NONE,
    TDPARM_AWSTAPE,
    TDPARM_IDRC,
    TDPARM_COMPRESS,
    TDPARM_METHOD,
    TDPARM_LEVEL,
    TDPARM_CHKSIZE,
    TDPARM_MAXSIZE,
    TDPARM_MAXSIZEK,
    TDPARM_MAXSIZEM,
    TDPARM_EOTMARGIN,
    TDPARM_STRICTSIZE,
    TDPARM_READONLY,
    TDPARM_RO,
    TDPARM_NORING,
    TDPARM_RW,
    TDPARM_RING,
    TDPARM_DEONIRQ,
    TDPARM_BLKID24,
    TDPARM_BLKID32,
    TDPARM_NOERG
};

/*-------------------------------------------------------------------*/
/*  Tape format determination REGEXPS. Used by mountnewtape below    */
/*-------------------------------------------------------------------*/

struct tape_format_entry                    /*   (table layout)      */
{
    char*               fmtreg;             /* A regular expression  */
    int                 fmtcode;            /* the device code       */
    TAPEMEDIA_HANDLER*  tmh;                /* The media dispatcher  */
    char*               descr;              /* readable description  */
    char*               short_descr;        /* (same but shorter)    */
};

/*-------------------------------------------------------------------*/
/*  Tape format determination REGEXPS. Used by mountnewtape below    */
/*-------------------------------------------------------------------*/

struct  tape_format_entry   fmttab   [] =   /*    (table itself)     */
{
    /* This entry matches a filename ending with .tdf    */
    {
        "\\.tdf$",
        TAPEDEVT_OMATAPE,
        &tmh_oma,
        "Optical Media Attachment (OMA) tape",
        "OMA tape"
    },

#if defined(OPTION_SCSI_TAPE)

    /* This entry matches a filename starting with /dev/ */
    {
        "^/dev/",
        TAPEDEVT_SCSITAPE,
        &tmh_scsi,
        "SCSI attached tape drive",
        "SCSI tape"
    },

#if defined(_MSVC_)

    /* (same idea but for Windows SCSI tape device names) */
    {
        "^\\\\\\\\\\.\\\\Tape[0-9]",
        TAPEDEVT_SCSITAPE,
        &tmh_scsi,
        "SCSI attached tape drive",
        "SCSI tape"
    },

#endif // _MSVC_
#endif // OPTION_SCSI_TAPE

    /* This entry matches a filename ending with .het    */
    {
        "\\.het$",
        TAPEDEVT_HET,
        &tmh_het,
        "Hercules Emulated Tape file",
        "HET tape"
    },

    /* Catch-all entry that matches anything else */
    {
        NULL,
        TAPEDEVT_AWSTAPE,
        &tmh_aws,
        "AWS Format tape file",
        "AWS tape"
    }
};


/*-------------------------------------------------------------------*/
/*        mountnewtape     --     mount a tape in the drive          */
/*-------------------------------------------------------------------*/
/*                                                                   */
/*  Syntax:       filename [parms]                                   */
/*                                                                   */
/*  where parms are any of the entries defined in the 'ptab' PARSER  */
/*  table defined further above. Some commonly used parms are:       */
/*                                                                   */
/*    awstape          set the HET parms be compatible with the      */
/*                     R|P/390|IS tape file Format (HET files)       */
/*                                                                   */
/*    idrc|compress    0|1: Write tape blocks with compression       */
/*                     (std deviation: Read backward allowed on      */
/*                     compressed HET tapes while it is not on       */
/*                     IDRC formated 3480 tapes)                     */
/*                                                                   */
/*    --no-erg         for SCSI tape only, means the hardware does   */
/*                     not support the "Erase Gap" command and all   */
/*                     such i/o's should return 'success' instead.   */
/*                                                                   */
/*    --blkid-32       for SCSI tape only, means the hardware        */
/*                     only supports full 32-bit block-ids.          */
/*                                                                   */
/*-------------------------------------------------------------------*/
int  mountnewtape ( DEVBLK *dev, int argc, char **argv )
{
#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)

    regex_t     regwrk;                 /* REGEXP work area          */
    regmatch_t  regwrk2;                /* REGEXP match area         */
    char        errbfr[1024];           /* Working storage           */

#endif // HAVE_REGEX_H

    char*       descr;                  /* Device descr from fmttab  */
    char*       short_descr;            /* Short descr from fmttab   */
    int         i;                      /* Loop control              */
    int         rc;                     /* various rtns return codes */

    union                               /* Parser results            */
    {
        U32     num;                    /* Parser results            */
        BYTE    str[ 80 ];              /* Parser results            */
    }
    res;                                /* Parser results            */

    /* Release the previous OMA descriptor array if allocated */
    if (dev->omadesc != NULL)
    {
        free (dev->omadesc);
        dev->omadesc = NULL;
    }

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
        strcpy (dev->filename, TAPE_UNLOADED);
    else
        /* Save the file name in the device block */
        strcpy (dev->filename, argv[0]);

    /* Use the file name to determine the device type */
    for(i=0;;i++)
    {
        dev->tapedevt=fmttab[i].fmtcode;
        dev->tmh=fmttab[i].tmh;
        if(fmttab[i].fmtreg==NULL)
        {
            break;
        }
#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)
        rc=regcomp(&regwrk,fmttab[i].fmtreg,REG_ICASE);
        if(rc<0)
        {
            regerror(rc,&regwrk,errbfr,1024);
            logmsg (_("HHCTA999E Device %4.4X: Unable to determine tape format type for %s: Internal error: Regcomp error %s on index %d\n"),dev->devnum,dev->filename,errbfr,i);
                return -1;
        }
        rc=regexec(&regwrk,dev->filename,1,&regwrk2,0);
        if(rc==REG_NOMATCH)
        {
            regfree(&regwrk);
            continue;
        }
        if(rc==0)
        {
            regfree(&regwrk);
            break;
        }
        regerror(rc,&regwrk,errbfr,1024);
        logmsg (_("HHCTA999E Device %4.4X: Unable to determine tape format type for %s: Internal error: Regexec error %s on index %d\n"),dev->devnum,dev->filename,errbfr,i);
        regfree(&regwrk);
        return -1;
#else // !HAVE_REGEX_H
        switch ( dev->tapedevt )
        {
        case TAPEDEVT_OMATAPE: // filename ends with ".tdf"
            if ( (rc = strlen(dev->filename)) <= 4 )
                rc = -1;
            else
                rc = strcasecmp( &dev->filename[rc-4], ".tdf" );
            break;
#if defined(OPTION_SCSI_TAPE)
        case TAPEDEVT_SCSITAPE: // filename starts with "\\.\Tape" or "/dev/"
#if defined(_MSVC_)
            if (1
                && strncasecmp(dev->filename, "\\\\.\\Tape", 8) == 0
                && isdigit(*(dev->filename+8))
                &&  0  ==  *(dev->filename+9)
            )
                rc = 0;
            else
#endif // _MSVC_
            {
                if ( (rc = strlen(dev->filename)) <= 5 )
                    rc = -1;
                else
                    rc = strncasecmp( dev->filename, "/dev/", 5 );
                if (0 == rc)
                {
                    if (strncasecmp( dev->filename+5, "st", 2 ) == 0)
                        dev->stape_close_rewinds = 1; // (rewind at close)
                    else
                        dev->stape_close_rewinds = 0; // (otherwise don't)
                }
            }
            break;
#endif // OPTION_SCSI_TAPE
        case TAPEDEVT_HET:      // filename ends with ".het"
            if ( (rc = strlen(dev->filename)) <= 4 )
                rc = -1;
            else
                rc = strcasecmp( &dev->filename[rc-4], ".het" );
            break;
        default:                // (should not occur)
            ASSERT(0);
            logmsg (_("HHCTA999E Device %4.4X: Unable to determine tape format type for %s\n"),dev->devnum,dev->filename);
            return -1;
        }
        if (!rc) break;
#endif // HAVE_REGEX_H
    }
    descr       = fmttab[i].descr;       // (save device description)
    short_descr = fmttab[i].short_descr; // (save device description)
    if (strcmp (dev->filename, TAPE_UNLOADED)!=0)
    {
        logmsg (_("HHCTA998I Device %4.4X: %s is a %s\n"),dev->devnum,dev->filename,descr);
    }

    /* Initialize device dependent fields */
    dev->fd                = -1;
#if defined(OPTION_SCSI_TAPE)
    dev->sstat               = GMT_DR_OPEN(-1);
    dev->stape_getstat_sstat = GMT_DR_OPEN(-1);
#endif
    dev->omadesc           = NULL;
    dev->omafiles          = 0;
    dev->curfilen          = 1;
    dev->nxtblkpos         = 0;
    dev->prvblkpos         = -1;
    dev->curblkrem         = 0;
    dev->curbufoff         = 0;
    dev->readonly          = 0;
    dev->hetb              = NULL;
    dev->tdparms.compress  = HETDFLT_COMPRESS;
    dev->tdparms.method    = HETDFLT_METHOD;
    dev->tdparms.level     = HETDFLT_LEVEL;
    dev->tdparms.chksize   = HETDFLT_CHKSIZE;
    dev->tdparms.maxsize   = 0;        // no max size     (default)
    dev->tdparms.eotmargin = 128*1024; // 128K EOT margin (default)
    dev->tdparms.logical_readonly = 0; // read/write      (default)

#if defined(OPTION_SCSI_TAPE)
    // Real 3590's use 32-bit blockids, and don't support Erase Gap.

    if (TAPEDEVT_SCSITAPE == dev->tapedevt
        &&     0x3590     == dev->devtype)
    {
        dev->stape_no_erg   = 1;        // (default for 3590 SCSI)
        dev->stape_blkid_32 = 1;        // (default for 3590 SCSI)
    }
#endif

    /* Process remaining parameters */
    rc = 0;
    for (i = 1; i < argc; i++)
    {
        logmsg (_("HHCTA066I %s device %4.4X parameter: '%s'\n"), short_descr, dev->devnum, argv[i]);
        switch (parser (&ptab[0], argv[i], &res))
        {
        case TDPARM_NONE:
            logmsg (_("HHCTA067E Device %4.4X: %s - Unrecognized parameter: '%s'\n"),
                dev->devnum,dev->filename,argv[i]);
            rc = -1;
            break;

        case TDPARM_AWSTAPE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.compress = FALSE;
            dev->tdparms.chksize = 4096;
            break;

        case TDPARM_IDRC:
        case TDPARM_COMPRESS:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.compress = (res.num ? TRUE : FALSE);
            break;

        case TDPARM_METHOD:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            if (res.num < HETMIN_METHOD || res.num > HETMAX_METHOD)
            {
                logmsg(_("HHCTA068E Method must be within %u-%u\n"),
                    HETMIN_METHOD, HETMAX_METHOD);
                rc = -1;
                break;
            }
            dev->tdparms.method = res.num;
            break;

        case TDPARM_LEVEL:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            if (res.num < HETMIN_LEVEL || res.num > HETMAX_LEVEL)
            {
                logmsg(_("HHCTA069E Level must be within %u-%u\n"),
                    HETMIN_LEVEL, HETMAX_LEVEL);
                rc = -1;
                break;
            }
            dev->tdparms.level = res.num;
            break;

        case TDPARM_CHKSIZE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            if (res.num < HETMIN_CHUNKSIZE || res.num > HETMAX_CHUNKSIZE)
            {
                logmsg (_("HHCTA070E Chunksize must be within %u-%u\n"),
                    HETMIN_CHUNKSIZE, HETMAX_CHUNKSIZE);
                rc = -1;
                break;
            }
            dev->tdparms.chksize = res.num;
            break;

        case TDPARM_MAXSIZE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.maxsize=res.num;
            break;

        case TDPARM_MAXSIZEK:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.maxsize=res.num*1024;
            break;

        case TDPARM_MAXSIZEM:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.maxsize=res.num*1024*1024;
            break;

        case TDPARM_EOTMARGIN:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.eotmargin=res.num;
            break;

        case TDPARM_STRICTSIZE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.strictsize=res.num;
            break;

        case TDPARM_READONLY:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.logical_readonly=(res.num ? 1 : 0 );
            break;

        case TDPARM_RO:
        case TDPARM_NORING:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.logical_readonly=1;
            break;

        case TDPARM_RW:
        case TDPARM_RING:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.logical_readonly=0;
            break;

        case TDPARM_DEONIRQ:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for SCSI tape\n"), argv[i]);
                rc = -1;
                break;
            }
            dev->tdparms.deonirq=(res.num ? 1 : 0 );
            break;

#if defined(OPTION_SCSI_TAPE)
        case TDPARM_BLKID24:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for %s\n"),
                    argv[i], short_descr );
                rc = -1;
                break;
            }
            dev->stape_blkid_32 = 0;
            break;

        case TDPARM_BLKID32:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for %s\n"),
                    argv[i], short_descr );
                rc = -1;
                break;
            }
            dev->stape_blkid_32 = 1;
            break;

        case TDPARM_NOERG:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                logmsg (_("HHCTA078E Option '%s' not valid for %s\n"),
                    argv[i], short_descr );
                rc = -1;
                break;
            }
            dev->stape_no_erg = 1;
            break;
#endif /* defined(OPTION_SCSI_TAPE) */

        default:
            logmsg(_("HHCTA071E Error in '%s' parameter\n"), argv[i]);
            rc = -1;
            break;
        } // end switch (parser (&ptab[0], argv[i], &res))
    } // end for (i = 1; i < argc; i++)

    if (0 != rc)
        return -1;

    /* Adjust the display if necessary */
    if(dev->tdparms.displayfeat)
    {
        if(strcmp(dev->filename,TAPE_UNLOADED)==0)
        {
            /* NO tape is loaded */
            if(TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype)
            {
                /* A new tape SHOULD be mounted */
                dev->tapedisptype   = TAPEDISPTYP_MOUNT;
                dev->tapedispflags |= TAPEDISPFLG_REQAUTOMNT;
                strlcpy( dev->tapemsg1, dev->tapemsg2, sizeof(dev->tapemsg1) );
            }
            else if(TAPEDISPTYP_UNMOUNT == dev->tapedisptype)
            {
                dev->tapedisptype = TAPEDISPTYP_IDLE;
            }
        }
        else
        {
            /* A tape IS already loaded */
            dev->tapedisptype = TAPEDISPTYP_IDLE;
        }
    }
    UpdateDisplay(dev);
    ReqAutoMount(dev);
    return 0;

} /* end function mountnewtape */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void tapedev_query_device ( DEVBLK *dev, char **class,
                int buflen, char *buffer )
{
    char devparms[ PATH_MAX+1 + 64 ];
    char dispmsg [ 256 ];

    BEGIN_DEVICE_CLASS_QUERY( "TAPE", dev, class, buflen, buffer );

    *buffer = 0;
    devparms[0]=0;
    dispmsg [0]=0;

    GetDisplayMsg( dev, dispmsg, sizeof(dispmsg) );

    if ( strcmp( dev->filename, TAPE_UNLOADED ) == 0 )
    {
        strlcat( devparms, dev->filename, sizeof(devparms));

#if defined(OPTION_SCSI_TAPE)
        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
        {
            if (0x3590 == dev->devtype) // emulating 3590
            {
                if (!dev->stape_blkid_32 ) strlcat( devparms, " --blkid-24", sizeof(devparms) );
            }
            else // emulating 3480, 3490
            {
                if ( dev->stape_blkid_32 ) strlcat( devparms, " --blkid-32", sizeof(devparms) );
            }
            if ( dev->stape_no_erg ) strlcat( devparms, " --no-erg", sizeof(devparms) );
        }
#endif
        snprintf(buffer, buflen, "%s%s%s",
            devparms,
            dev->tdparms.displayfeat ? ", Display: " : "",
            dev->tdparms.displayfeat ?    dispmsg    : "");
    }
    else // (filename was specified)
    {
        char tapepos[32]; tapepos[0]=0;

        if (strchr(dev->filename,' ')) strlcat( devparms, "\"",          sizeof(devparms));
                                       strlcat( devparms, dev->filename, sizeof(devparms));
        if (strchr(dev->filename,' ')) strlcat( devparms, "\"",          sizeof(devparms));

        if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
        {
            snprintf( tapepos, sizeof(tapepos), "[%d:%8.8lX] ",
                dev->curfilen, (unsigned long int)dev->nxtblkpos );
            tapepos[sizeof(tapepos)-1] = 0;
        }
#if defined(OPTION_SCSI_TAPE)
        else // (this is a SCSI tape drive)
        {
            if (STS_BOT( dev )) strlcat(tapepos,"*BOT* ",sizeof(tapepos));

            // If tape has a display, then GetDisplayMsg already
            // appended *FP* for us. Otherwise we need to do it.

            if ( !dev->tdparms.displayfeat )
                if (STS_WR_PROT( dev ))
                    strlcat(tapepos,"*FP* ",sizeof(tapepos));

            if (0x3590 == dev->devtype) // emulating 3590
            {
                if (!dev->stape_blkid_32 ) strlcat( devparms, " --blkid-24", sizeof(devparms) );
            }
            else // emulating 3480, 3490
            {
                if ( dev->stape_blkid_32 ) strlcat( devparms, " --blkid-32", sizeof(devparms) );
            }
            if ( dev->stape_no_erg ) strlcat( devparms, " --no-erg", sizeof(devparms) );
        }
#endif

        if ( TAPEDEVT_SCSITAPE != dev->tapedevt
#if defined(OPTION_SCSI_TAPE)
            || !STS_NOT_MOUNTED(dev)
#endif
        )
        {
            // Not a SCSI tape,  -or-  mounted SCSI tape...

            snprintf (buffer, buflen, "%s%s %s%s%s",

                devparms, (dev->readonly ? " ro" : ""),

                tapepos,
                dev->tdparms.displayfeat ? "Display: " : "",
                dev->tdparms.displayfeat ?  dispmsg    : "");
        }
        else /* ( TAPEDEVT_SCSITAPE == dev->tapedevt && STS_NOT_MOUNTED(dev) ) */
        {
            // UNmounted SCSI tape...

            snprintf (buffer, buflen, "%s%s (%sNOTAPE)%s%s",

                devparms, (dev->readonly ? " ro" : ""),

                dev->fd < 0              ?   "closed; "  : "",
                dev->tdparms.displayfeat ? ", Display: " : "",
                dev->tdparms.displayfeat ?    dispmsg    : ""  );
        }
    }

    buffer[buflen-1] = 0;

} /* end function tapedev_query_device */


/*-------------------------------------------------------------------*/
/* Issue a message on the console indicating the display status      */
/*-------------------------------------------------------------------*/
void UpdateDisplay( DEVBLK *dev )
{
    if ( dev->tdparms.displayfeat )
    {
        char msgbfr[256];

        GetDisplayMsg( dev, msgbfr, sizeof(msgbfr) );

        if ( dev->prev_tapemsg )
        {
            if ( strcmp( msgbfr, dev->prev_tapemsg ) == 0 )
                return;
            free( dev->prev_tapemsg );
            dev->prev_tapemsg = NULL;
        }

        dev->prev_tapemsg = strdup( msgbfr );

        logmsg(_("HHCTA100I %4.4X: Now Displays: %s\n"),
            dev->devnum, msgbfr );
    }
#if defined(OPTION_SCSI_TAPE)
    else
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            int_scsi_status_update( dev, 1 );
#endif
}


/*-------------------------------------------------------------------*/
/* Issue Automatic Mount Requests as defined by the display          */
/*-------------------------------------------------------------------*/
void ReqAutoMount( DEVBLK *dev )
{
    char   volser[7];
    BYTE   tapeloaded, autoload, mountreq, unmountreq, stdlbled, ascii, scratch;
    char*  lbltype;
    char*  tapemsg = "";
    char*  eyecatcher =
"*******************************************************************************";

    ///////////////////////////////////////////////////////////////////

    // The Automatic Cartridge Loader or "ACL" (sometimes also referred
    // to as an "Automatic Cartridge Feeder" (ACF) too) automatically
    // loads the next cartridge [from the magazine] whenever a tape is
    // unloaded, BUT ONLY IF the 'Index Automatic Load' bit (bit 7) of
    // the FCB (Format Control Byte, byte 0) was on whenever the Load
    // Display ccw was sent to the drive. If the bit was not on when
    // the Load Display ccw was issued, then the requested message (if
    // any) is displayed until the next tape mount/dismount and the ACL
    // is NOT activated (i.e. the next tape is NOT automatically loaded).
    // If the bit was on however, then, as stated, the ACF component of
    // the drive will automatically load the next [specified] cartridge.

    // Whenever the ACL facility is activated (via bit 7 of byte 0 of
    // the Load Display ccw), then only bytes 1-8 of the "Display Until
    // Mounted" message (or bytes 9-17 of a "Display Until Dismounted
    // Then Mounted" message) are displayed to let the operator know
    // which tape is currently being processed by the autoloader and
    // thus is basically for informational purposes only (the operator
    // does NOT need to do anything since the auto-loader is handling
    // tape mounts for them automatically; i.e. the message is NOT an
    // operator mount/dismount request).

    // If the 'Index Automatic Load' bit was not set in the Load Display
    // CCW however, then the specified "Display Until Mounted", "Display
    // Until Unmounted" or "Display Until Unmounted Then Display Until
    // Mounted" message is meant as a mount, unmount, or unmount-then-
    // mount request for the actual [human being] operator, and thus
    // they DO need to take some sort of action (since the ACL automatic
    // loader facility is not active; i.e. the message is a request to
    // the operator to manually unload, load or unload then load a tape).

    // THUS... If the TAPEDISPFLG_AUTOLOADER flag is set (indicating
    // the autoloader is (or should be) active), then the message we
    // issue is simply for INFORMATIONAL purposes only (i.e. "FYI: the
    // following tape is being *automatically* loaded; you don't need
    // to actually do anything")

    // If the TAPEDISPFLG_AUTOLOADER is flag is NOT set however, then
    // we need to issue a message notifying the operator of what they
    // are *expected* to do (e.g. either unload, load or unload/load
    // the specified tape volume).

    // Also please note that while there are no formally established
    // standards regarding the format of the Load Display CCW message
    // text, there are however certain established conventions (estab-
    // lished by IBM naturally). If the first character is an 'M', it
    // means "Please MOUNT the indicated volume". An 'R' [apparently]
    // means "Retain", and, similarly, 'K' means "Keep" (same thing as
    // "Retain"). If the LAST character is an 'S', then it means that
    // a Standard Labeled volume is being requested, whereas an 'N'
    // (or really, anything OTHER than an 'S' (except 'A')) means an
    // unlabeled (or non-labeled) tape volume is being requested. An
    // 'A' as the last character means a Standard Labeled ASCII tape
    // is being requested. If the message is "SCRTCH" (or something
    // similar), then a either a standard labeled or unlabeled scratch
    // tape is obviously being requested (there doesn't seem to be any
    // convention/consensus regarding the format for requesting scratch
    // tapes; some shops for example use 'XXXSCR' to indicate that a
    // scratch tape from tape pool 'XXX' should be mounted).

    ///////////////////////////////////////////////////////////////////

    /* Open the file/drive if needed (kick off auto-mount if needed) */
    if (dev->fd < 0)
    {
        BYTE unitstat = 0, code = 0;

        dev->tmh->open( dev, &unitstat, code );

#if defined(OPTION_SCSI_TAPE)
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
        {
            // PROGRAMMING NOTE: it's important to do TWO refreshes here
            // to cause the auto-mount thread to get created. Doing only
            // one doesn't work and doing two shouldn't cause any harm.

            GENTMH_PARMS  gen_parms;

            gen_parms.action  = GENTMH_SCSI_ACTION_UPDATE_STATUS;
            gen_parms.dev     = dev;

            // (refresh potentially stale status)
            VERIFY( dev->tmh->generic( &gen_parms ) == 0 );

            // (force auto-mount thread creation)
            VERIFY( dev->tmh->generic( &gen_parms ) == 0 );
        }
#endif /* defined(OPTION_SCSI_TAPE) */
    }

    /* Disabled when [non-SCSI] ACL in use */
    if ( dev->als )
        return;

    /* Do we actually have any work to do? */
    if ( !( dev->tapedispflags & TAPEDISPFLG_REQAUTOMNT ) )
        return;     // (nothing to do!)

    /* Reset work flag */
    dev->tapedispflags &= ~TAPEDISPFLG_REQAUTOMNT;

    /* If the drive doesn't have a display,
       then it can't have an auto-loader either */
    if ( !dev->tdparms.displayfeat )
        return;

    /* Determine if mount or unmount request
       and get pointer to correct message */

    tapeloaded = dev->tmh->tapeloaded( dev, NULL, 0 ) ? TRUE : FALSE;

    mountreq   = FALSE;     // (default)
    unmountreq = FALSE;     // (default)

    if (tapeloaded)
    {
        // A tape IS already loaded...

        // 1st byte of message1 non-blank, *AND*,
        // unmount request or,
        // unmountmount request and not message2-only flag?

        if (' ' != *(tapemsg = dev->tapemsg1) &&
            (0
                || TAPEDISPTYP_UNMOUNT == dev->tapedisptype
                || (1
                    && TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype
                    && !(dev->tapedispflags & TAPEDISPFLG_MESSAGE2)
                   )
            )
        )
            unmountreq = TRUE;
    }
    else
    {
        // NO TAPE is loaded yet...

        // mount request and 1st byte of msg1 non-blank, *OR*,
        // unmountmount request and 1st byte of msg2 non-blank?

        if (
        (1
            && TAPEDISPTYP_MOUNT == dev->tapedisptype
            && ' ' != *(tapemsg = dev->tapemsg1)
        )
        ||
        (1
            && TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype
            && ' ' != *(tapemsg = dev->tapemsg2)
        ))
            mountreq = TRUE;
    }

    /* Extract volser from message */
    strncpy( volser, tapemsg+1, 6 ); volser[6]=0;

    /* Set some boolean flags */
    autoload = ( dev->tapedispflags & TAPEDISPFLG_AUTOLOADER )    ?  TRUE  :  FALSE;
    stdlbled = ( 'S' == tapemsg[7] )                              ?  TRUE  :  FALSE;
    ascii    = ( 'A' == tapemsg[7] )                              ?  TRUE  :  FALSE;
    scratch  = ( 'S' == tapemsg[0] )                              ?  TRUE  :  FALSE;

    lbltype = stdlbled ? "SL" : "UL";

#if defined(OPTION_SCSI_TAPE)
#if 1
    // ****************************************************************
    // ZZ FIXME: ZZ TODO:   ***  Programming Note  ***

    // Since we currently don't have any way of activating a SCSI tape
    // drive's REAL autoloader mechanism whenever we receive an auto-
    // mount message [from the guest o/s via the Load Display CCW], we
    // issue a normal operator mount request message instead (in order
    // to ask the [Hercules] operator (a real human being) to please
    // perform the automount for us instead since we can't [currently]
    // do it for them automatically since we don't currently have any
    // way to send the real request on to the real SCSI device).

    // Once ASPI code eventually gets added to Herc (and/or something
    // similar for the Linux world), then the following workaround can
    // be safely removed.

    autoload = FALSE;       // (temporarily forced; see above)

    // ****************************************************************
#endif
#endif /* defined(OPTION_SCSI_TAPE) */

    if ( autoload )
    {
        // ZZ TODO: Here is where we'd issue i/o (ASPI?) to the actual
        // hardware autoloader facility (i.e. the SCSI medium changer)
        // to unload and/or load the tape(s) if this were a SCSI auto-
        // loading tape drive.

        if ( unmountreq )
        {
            if ( scratch )
                logmsg(_("AutoMount: %s%s scratch tape being auto-unloaded on %4.4X = %s\n"),
                    ascii ? "ASCII " : "",lbltype,
                    dev->devnum, dev->filename);
            else
                logmsg(_("AutoMount: %s%s tape volume \"%s\" being auto-unloaded on %4.4X = %s\n"),
                    ascii ? "ASCII " : "",lbltype,
                    volser, dev->devnum, dev->filename);
        }
        if ( mountreq )
        {
            if ( scratch )
                logmsg(_("AutoMount: %s%s scratch tape being auto-loaded on %4.4X = %s\n"),
                    ascii ? "ASCII " : "",lbltype,
                    dev->devnum, dev->filename);
            else
                logmsg(_("AutoMount: %s%s tape volume \"%s\" being auto-loaded on %4.4X = %s\n"),
                    ascii ? "ASCII " : "",lbltype,
                    volser, dev->devnum, dev->filename);
        }
    }
    else
    {
        // If this is a mount or unmount request, inform the
        // [Hercules] operator of the action they're expected to take...

        if ( unmountreq )
        {
            char* keep_or_retain = "";

            if ( 'K' == tapemsg[0] ) keep_or_retain = "and keep ";
            if ( 'R' == tapemsg[0] ) keep_or_retain = "and retain ";

            if ( scratch )
            {
                logmsg(_("\n%s\nAUTOMOUNT: Unmount %sof %s%s scratch tape requested on %4.4X = %s\n%s\n\n"),
                    eyecatcher,
                    keep_or_retain,
                    ascii ? "ASCII " : "",lbltype,
                    dev->devnum, dev->filename,
                    eyecatcher );
            }
            else
            {
                logmsg(_("\n%s\nAUTOMOUNT: Unmount %sof %s%s tape volume \"%s\" requested on %4.4X = %s\n%s\n\n"),
                    eyecatcher,
                    keep_or_retain,
                    ascii ? "ASCII " : "",lbltype,
                    volser,
                    dev->devnum, dev->filename,
                    eyecatcher );
            }
        }
        if ( mountreq )
        {
            if ( scratch )
                logmsg(_("\n%s\nAUTOMOUNT: Mount for %s%s scratch tape requested on %4.4X = %s\n%s\n\n"),
                    eyecatcher,
                    ascii ? "ASCII " : "",lbltype,
                    dev->devnum, dev->filename,
                    eyecatcher );
            else
                logmsg(_("\n%s\nAUTOMOUNT: Mount for %s%s tape volume \"%s\" requested on %4.4X = %s\n%s\n\n"),
                    eyecatcher,
                    ascii ? "ASCII " : "",lbltype,
                    volser,
                    dev->devnum, dev->filename,
                    eyecatcher );
        }
    }

} /* end function ReqAutoMount */


/*-------------------------------------------------------------------*/
/*      Get 3480/3490/3590 Display text in 'human' form              */
/* If not a 3480/3490/3590, then just update status if a SCSI tape   */
/*-------------------------------------------------------------------*/
void GetDisplayMsg( DEVBLK *dev, char *msgbfr, size_t  lenbfr )
{
    msgbfr[0]=0;

    if ( !dev->tdparms.displayfeat )
    {
        // (drive doesn't have a display)
#if defined(OPTION_SCSI_TAPE)
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            int_scsi_status_update( dev, 1 );
#endif
        return;
    }

    if ( !IS_TAPEDISPTYP_SYSMSG( dev ) )
    {
        // -------------------------
        //   Display Host message
        // -------------------------

        // "When bit 3 (alternate) is set to 1, then
        //  bits 4 (blink) and 5 (low/high) are ignored."

        strlcpy( msgbfr, "\"", lenbfr );

        if ( dev->tapedispflags & TAPEDISPFLG_ALTERNATE )
        {
            char  msg1[9];
            char  msg2[9];

            strlcpy ( msg1,   dev->tapemsg1, sizeof(msg1) );
            strlcat ( msg1,   "        ",    sizeof(msg1) );
            strlcpy ( msg2,   dev->tapemsg2, sizeof(msg2) );
            strlcat ( msg2,   "        ",    sizeof(msg2) );

            strlcat ( msgbfr, msg1,             lenbfr );
            strlcat ( msgbfr, "\" / \"",        lenbfr );
            strlcat ( msgbfr, msg2,             lenbfr );
            strlcat ( msgbfr, "\"",             lenbfr );
            strlcat ( msgbfr, " (alternating)", lenbfr );
        }
        else
        {
            if ( dev->tapedispflags & TAPEDISPFLG_MESSAGE2 )
                strlcat( msgbfr, dev->tapemsg2, lenbfr );
            else
                strlcat( msgbfr, dev->tapemsg1, lenbfr );

            strlcat ( msgbfr, "\"",          lenbfr );

            if ( dev->tapedispflags & TAPEDISPFLG_BLINKING )
                strlcat ( msgbfr, " (blinking)", lenbfr );
        }

        if ( dev->tapedispflags & TAPEDISPFLG_AUTOLOADER )
            strlcat( msgbfr, " (AUTOLOADER)", lenbfr );

        return;
    }

    // ----------------------------------------------
    //   Display SYS message (Unit/Device message)
    // ----------------------------------------------

    // First, build the system message, then move it into
    // the caller's buffer...

    strlcpy( dev->tapesysmsg, "\"", sizeof(dev->tapesysmsg) );

    switch ( dev->tapedisptype )
    {
    case TAPEDISPTYP_IDLE:
    case TAPEDISPTYP_WAITACT:
    default:
        // Blank display if no tape loaded...
        if ( !dev->tmh->tapeloaded( dev, NULL, 0 ) )
        {
            strlcat( dev->tapesysmsg, "        ", sizeof(dev->tapesysmsg) );
            break;
        }

        // " NT RDY " if tape IS loaded, but not ready...
        // (IBM docs say " NT RDY " means "Loaded but not ready")

        ASSERT( dev->tmh->tapeloaded( dev, NULL, 0 ) );

        if (0
            || dev->fd < 0
#if defined(OPTION_SCSI_TAPE)
            || (1
                && TAPEDEVT_SCSITAPE == dev->tapedevt
                && !STS_ONLINE( dev )
               )
#endif
        )
        {
            strlcat( dev->tapesysmsg, " NT RDY ", sizeof(dev->tapesysmsg) );
            break;
        }

        // Otherwise tape is loaded and ready  -->  "READY"

        ASSERT( dev->tmh->tapeloaded( dev, NULL, 0 ) );

        strlcat ( dev->tapesysmsg, " READY  ", sizeof(dev->tapesysmsg) );
        strlcat( dev->tapesysmsg, "\"", sizeof(dev->tapesysmsg) );

        if (0
            || dev->readonly
#if defined(OPTION_SCSI_TAPE)
            || (1
                &&  TAPEDEVT_SCSITAPE == dev->tapedevt
                &&  STS_WR_PROT( dev )
               )
#endif
        )
            // (append "file protect" indicator)
            strlcat ( dev->tapesysmsg, " *FP*", sizeof(dev->tapesysmsg) );

        // Copy system message to caller's buffer
        strlcpy( msgbfr, dev->tapesysmsg, lenbfr );
        return;

    case TAPEDISPTYP_ERASING:
        strlcat ( dev->tapesysmsg, " ERASING", sizeof(dev->tapesysmsg) );
        break;

    case TAPEDISPTYP_REWINDING:
        strlcat ( dev->tapesysmsg, "REWINDNG", sizeof(dev->tapesysmsg) );
        break;

    case TAPEDISPTYP_UNLOADING:
        strlcat ( dev->tapesysmsg, "UNLOADNG", sizeof(dev->tapesysmsg) );
        break;

    case TAPEDISPTYP_CLEAN:
        strlcat ( dev->tapesysmsg, "*CLEAN  ", sizeof(dev->tapesysmsg) );
        break;
    }

    strlcat( dev->tapesysmsg, "\"", sizeof(dev->tapesysmsg) );

    // Copy system message to caller's buffer
    strlcpy( msgbfr, dev->tapesysmsg, lenbfr );

} /* end function GetDisplayMsg */


/*-------------------------------------------------------------------*/
/*                         IsAtLoadPoint                             */
/*-------------------------------------------------------------------*/
/* Called by the device-type-specific 'build_sense_xxxx' functions   */
/* (indirectly via the 'build_senseX' function) when building sense  */
/* for any i/o error (non-"TAPE_BSENSE_STATUSONLY" type call)       */
/*-------------------------------------------------------------------*/
int IsAtLoadPoint (DEVBLK *dev)
{
int ldpt=0;
    if ( dev->fd >= 0 )
    {
        /* Set load point indicator if tape is at load point */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            if (dev->nxtblkpos==0)
            {
                ldpt=1;
            }
            break;

        case TAPEDEVT_HET:
            if (dev->hetb->cblk == 0)
            {
                ldpt=1;
            }
            break;

#if defined(OPTION_SCSI_TAPE)
        case TAPEDEVT_SCSITAPE:
            int_scsi_status_update( dev, 0 );   // (internal call)
            if ( STS_BOT( dev ) )
            {
                ldpt=1;
            }
            break;
#endif /* defined(OPTION_SCSI_TAPE) */

        case TAPEDEVT_OMATAPE:
            if (dev->nxtblkpos == 0 && dev->curfilen == 1)
            {
                ldpt=1;
            }
            break;
        } /* end switch(dev->tapedevt) */
    }
    else // ( dev->fd < 0 )
    {
        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
            ldpt=0; /* (tape cannot possibly be at loadpoint
                        if the device cannot even be opened!) */
        else if ( strcmp( dev->filename, TAPE_UNLOADED ) != 0 )
        {
            /* If the tape has a filename but the tape is not yet */
            /* opened, then we are at loadpoint                   */
            ldpt=1;
        }
    }
    return ldpt;

} /* end function IsAtLoadPoint */


/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**                   AUTOLOADER FUNCTIONS                          **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

/*-------------------------------------------------------------------*/
/*                         autoload_init                             */
/*-------------------------------------------------------------------*/
/*  initialise the Autoloader feature                                */
/*-------------------------------------------------------------------*/
void autoload_init(DEVBLK *dev,int ac,char **av)
{
    char        bfr[4096];
    char    *rec;
    FILE        *aldf;
    char    *verb;
    int        i;
    char    *strtokw;
    char     pathname[MAX_PATH];

    autoload_close(dev);
    if(ac<1)
    {
        return;
    }
    if(av[0][0]!='@')
    {
        return;
    }
    logmsg(_("TAPE: Autoloader file request fn=%s\n"),&av[0][1]);
    hostpath(pathname, &av[0][1], sizeof(pathname));
    if(!(aldf=fopen(pathname,"r")))
    {
        return;
    }
    for(i=1;i<ac;i++)
    {
        autoload_global_parms(dev,av[i]);
    }
    while((rec=fgets(bfr,4096,aldf)))
    {
        for(i=(strlen(rec)-1);isspace(rec[i]) && i>=0;i--)
        {
            rec[i]=0;
        }
        if(strlen(rec)==0)
        {
            continue;
        }
        verb=strtok_r(rec," \t",&strtokw);
        if(verb==NULL)
        {
            continue;
        }
        if(verb[0]==0)
        {
            continue;
        }
        if(verb[0]=='#')
        {
            continue;
        }
        if(strcmp(verb,"*")==0)
        {
            while((verb=strtok_r(NULL," \t",&strtokw)))
            {
                autoload_global_parms(dev,verb);
            }
            continue;
        }
        autoload_tape_entry(dev,verb,&strtokw);
    } // end while((rec=fgets(bfr,4096,aldf)))
    fclose(aldf);
    return;

} /* end function autoload_init */


/*-------------------------------------------------------------------*/
/*                      autoload_close                               */
/*-------------------------------------------------------------------*/
/*  terminate autoloader operations: release all storage that        */
/*  was allocated by the autoloader facility                         */
/*-------------------------------------------------------------------*/
void autoload_close(DEVBLK *dev)
{
    int        i;
    if(dev->al_argv!=NULL)
    {
        for(i=0;i<dev->al_argc;i++)
        {
            free(dev->al_argv[i]);
            dev->al_argv[i]=NULL;
        }
        free(dev->al_argv);
        dev->al_argv=NULL;
        dev->al_argc=0;
    }
    dev->al_argc=0;
    if(dev->als!=NULL)
    {
        for(i=0;i<dev->alss;i++)
        {
            autoload_clean_entry(dev,i);
        }
        free(dev->als);
        dev->als=NULL;
        dev->alss=0;
    }
} /* end function autoload_close */


/*-------------------------------------------------------------------*/
/*                    autoload_clean_entry                           */
/*-------------------------------------------------------------------*/
/*  release storage allocated for an autoloader slot                 */
/*  (except the slot itself)                                         */
/*-------------------------------------------------------------------*/
void autoload_clean_entry(DEVBLK *dev,int ix)
{
    int i;
    for(i=0;i<dev->als[ix].argc;i++)
    {
        free(dev->als[ix].argv[i]);
        dev->als[ix].argv[i]=NULL;
    }
    dev->als[ix].argc=0;
    if(dev->als[ix].filename!=NULL)
    {
        free(dev->als[ix].filename);
        dev->als[ix].filename=NULL;
    }
} /* end function autoload_clean_entry */


/*-------------------------------------------------------------------*/
/*                    autoload_global_parms                          */
/*-------------------------------------------------------------------*/
/*  Appends a blank delimited word to the list of parameters         */
/*  that will be passed for every tape mounted by the autoloader     */
/*-------------------------------------------------------------------*/
void autoload_global_parms(DEVBLK *dev,char *par)
{
    logmsg(_("TAPE Autoloader - Adding global parm %s\n"),par);
    if(dev->al_argv==NULL)
    {
        dev->al_argv=malloc(sizeof(char *)*256);
        dev->al_argc=0;
    }
    dev->al_argv[dev->al_argc]=(char *)malloc(strlen(par)+sizeof(char));
    strcpy(dev->al_argv[dev->al_argc],par);
    dev->al_argc++;

} /* end function autoload_global_parms */


/*-------------------------------------------------------------------*/
/*                    autoload_tape_entry                            */
/*-------------------------------------------------------------------*/
/*  populate an autoloader slot  (creates new slot if needed)        */
/*-------------------------------------------------------------------*/
void autoload_tape_entry(DEVBLK *dev,char *fn,char **strtokw)
{
    char *p;
    TAPEAUTOLOADENTRY tae;
    logmsg(_("TAPE Autoloader: Adding tape entry %s\n"),fn);
    memset(&tae,0,sizeof(tae));
    tae.filename=malloc(strlen(fn)+sizeof(char)+1);
    strcpy(tae.filename,fn);
    while((p=strtok_r(NULL," \t",strtokw)))
    {
        if(tae.argv==NULL)
        {
            tae.argv=malloc(sizeof(char *)*256);
        }
        tae.argv[tae.argc]=malloc(strlen(p)+sizeof(char)+1);
        strcpy(tae.argv[tae.argc],p);
        tae.argc++;
    }
    if(dev->als==NULL)
    {
        dev->als=malloc(sizeof(tae));
        dev->alss=0;
    }
    else
    {
        dev->als=realloc(dev->als,sizeof(tae)*(dev->alss+1));
    }
    memcpy(&dev->als[dev->alss],&tae,sizeof(tae));
    dev->alss++;

} /* end function autoload_tape_entry */


/*-------------------------------------------------------------------*/
/*             autoload_wait_for_tapemount_thread                    */
/*-------------------------------------------------------------------*/
void *autoload_wait_for_tapemount_thread(void *db)
{
int     rc  = -1;
DEVBLK *dev = (DEVBLK*) db;

    obtain_lock(&dev->lock);
    {
        while
        (
            dev->als
            &&
            (rc = autoload_mount_next( dev )) != 0
        )
        {
            release_lock( &dev->lock );
            SLEEP(AUTOLOAD_WAIT_FOR_TAPEMOUNT_INTERVAL_SECS);
            obtain_lock( &dev->lock );
        }
    }
    release_lock(&dev->lock);
    if ( rc == 0 )
        device_attention(dev,CSW_DE);
    return NULL;

} /* end function autoload_wait_for_tapemount_thread */


/*-------------------------------------------------------------------*/
/*                     autoload_mount_first                          */
/*-------------------------------------------------------------------*/
/*  mount in the drive the tape which is                             */
/*  positionned in the 1st autoloader slot                           */
/*-------------------------------------------------------------------*/
int autoload_mount_first(DEVBLK *dev)
{
    dev->alsix=0;
    return(autoload_mount_tape(dev,0));
}


/*-------------------------------------------------------------------*/
/*                     autoload_mount_next                           */
/*-------------------------------------------------------------------*/
/*  mount in the drive the tape whch is                              */
/*  positionned in the slot after the currently mounted tape.        */
/*  if this is the last tape, close the autoloader                   */
/*-------------------------------------------------------------------*/
int autoload_mount_next(DEVBLK *dev)
{
    if(dev->alsix>=dev->alss)
    {
        autoload_close(dev);
        return -1;
    }
    dev->alsix++;
    return(autoload_mount_tape(dev,dev->alsix));
}


/*-------------------------------------------------------------------*/
/*                     autoload_mount_tape                           */
/*-------------------------------------------------------------------*/
/*  mount in the drive the tape which is                             */
/*  positionned in the autoloader slot #alix                         */
/*-------------------------------------------------------------------*/
int autoload_mount_tape(DEVBLK *dev,int alix)
{
    char        **pars;
    int        pcount=1;
    int        i;
    int        rc;
    if(alix>=dev->alss)
    {
        return -1;
    }
    pars=malloc(sizeof(BYTE *)*256);
    pars[0]=dev->als[alix].filename;
    for(i=0;i<dev->al_argc;i++,pcount++)
    {
        pars[pcount]=malloc(strlen(dev->al_argv[i])+10);
        strcpy(pars[pcount],dev->al_argv[i]);
        if(pcount>255)
        {
            break;
        }
    }
    for(i=0;i<dev->als[alix].argc;i++,pcount++)
    {
        pars[pcount]=malloc(strlen(dev->als[alix].argv[i])+10);
        strcpy(pars[pcount],dev->als[alix].argv[i]);
        if(pcount>255)
        {
            break;
        }
    }
    rc=mountnewtape(dev,pcount,pars);
    for(i=1;i<pcount;i++)
    {
        free(pars[i]);
    }
    free(pars);
    return(rc);

} /* end function autoload_mount_tape */


/*-------------------------------------------------------------------*/
/* is_tapeloaded_filename                                            */
/*-------------------------------------------------------------------*/
int is_tapeloaded_filename ( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);
    // true 1 == tape loaded, false 0 == tape not loaded
    return strcmp( dev->filename, TAPE_UNLOADED ) != 0 ? 1 : 0;
}

/*-------------------------------------------------------------------*/
/* return_false1                                                     */
/*-------------------------------------------------------------------*/
int return_false1 ( DEVBLK *dev )
{
    UNREFERENCED(dev);
    return 0;
}

/*-------------------------------------------------------------------*/
/* write_READONLY                                                    */
/*-------------------------------------------------------------------*/
int write_READONLY ( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
    build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
    return -1;
}

/*-------------------------------------------------------------------*/
/* write_READONLY5                                                   */
/*-------------------------------------------------------------------*/
int write_READONLY5 ( DEVBLK *dev, BYTE *bfr, U16 blklen, BYTE *unitstat, BYTE code )
{
    UNREFERENCED(bfr);
    UNREFERENCED(blklen);
    build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
    return -1;
}

/*-------------------------------------------------------------------*/
/* no_operation                                                      */
/*-------------------------------------------------------------------*/
int no_operation ( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
    build_senseX( TAPE_BSENSE_STATUSONLY, dev, unitstat, code );
    return 0;
}

/*-------------------------------------------------------------------*/
/* readblkid_virtual                                                 */
/*-------------------------------------------------------------------*/
int readblkid_virtual ( DEVBLK* dev, BYTE* logical, BYTE* physical )
{
    // NOTE: returned value is always in guest BIG-ENDIAN format...

    BYTE  blockid[4];

    if (0x3590 == dev->devtype)
    {
        // Full 32-bit block-id...

        blockid[0] = (dev->blockid >> 24) & 0xFF;
        blockid[1] = (dev->blockid >> 16) & 0xFF;
        blockid[2] = (dev->blockid >> 8 ) & 0xFF;
        blockid[3] = (dev->blockid      ) & 0xFF;
    }
    else // (3480 et. al)
    {
        // "22-bit" block-id...

        blockid[0] = 0x01;  // ("wrap" value)
        blockid[1] = (dev->blockid >> 16) & 0x3F;
        blockid[2] = (dev->blockid >> 8 ) & 0xFF;
        blockid[3] = (dev->blockid      ) & 0xFF;
    }

    // NOTE: For virtual tape devices, we return the same value
    // for both the logical "Channel block ID" value as well as
    // the physical "Device block ID" value...

    if (logical)  memcpy( logical,  &blockid[0], 4 );
    if (physical) memcpy( physical, &blockid[0], 4 );

    return 0;
}

/*-------------------------------------------------------------------*/
/* locateblk_virtual                                                 */
/*-------------------------------------------------------------------*/
int locateblk_virtual ( DEVBLK* dev, U32 blockid, BYTE *unitstat, BYTE code )
{
    // NOTE: 'blockid' passed in host (little-endian) format...

    int rc;

    /* Do it the hard way: rewind to load-point and then
       keep doing fsb, fsb, fsb... until we find our block
    */
    if ((rc = dev->tmh->rewind( dev, unitstat, code)) >= 0)
    {
        /* Reset position counters to start of file */

        dev->curfilen   =  1;
        dev->nxtblkpos  =  0;
        dev->prvblkpos  = -1;
        dev->blockid    =  0;

        /* Do it the hard way */

        while ( dev->blockid < blockid && ( rc >= 0 ) )
            rc = dev->tmh->fsb( dev, unitstat, code );
    }

    return rc;
}

/*-------------------------------------------------------------------*/
/* generic_tmhcall              generic media-type-handler call...   */
/*-------------------------------------------------------------------*/
int generic_tmhcall ( GENTMH_PARMS* pGenParms )
{
    if (!pGenParms)
    {
        errno = EINVAL;             // (invalid arguments)
        return -1;                  // (return failure)
    }

    switch (pGenParms->action)
    {
        case GENTMH_SCSI_ACTION_UPDATE_STATUS:
        {
            return update_status_scsitape( pGenParms->dev );
        }

        default:
        {
            errno = EINVAL;         // (invalid arguments)
            return -1;              // (return failure)
        }
    }

    return -1;      // (never reached)
}
