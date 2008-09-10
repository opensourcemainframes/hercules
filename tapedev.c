/* TAPEDEV.C    (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* magnetic tape devices for the Hercules ESA/390 emulator.          */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Five emulated tape formats are supported:                         */
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
/*                                                                   */
/* 5. FAKETAPE  This is the format used by Fundamental Software      */
/*              on their FLEX-ES systems. It it similar to the AWS   */
/*              format. The entire tape is contained in a single     */
/*              flat file. A tape block is preceded by a 12-ASCII-   */
/*              hex-characters header which indicate the size of     */
/*              the previous and next blocks. Files are separated    */
/*              by tapemarks which consist of headers with a zero    */
/*              current block length. FakeTapes are both readable    */
/*              and writable.                                        */
/*                                                                   */
/*              Support for FAKETAPE is in the "FAKETAPE.C" member.  */
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
/* Flex FakeTape format (http://preview.tinyurl.com/67rgnp)          */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.149  2008/07/21 22:13:29  rbowler
// tapedev.c(964) : warning C4101: 'i' : unreferenced local variable
//
// Revision 1.148  2008/07/08 05:35:51  fish
// AUTOMOUNT redesign: support +allowed/-disallowed dirs
// and create associated 'automount' panel command - Fish
//
// Revision 1.147  2008/06/22 05:54:30  fish
// Fix print-formatting issue (mostly in tape modules)
// that can sometimes, in certain circumstances,
// cause herc to crash.  (%8.8lx --> I32_FMTX, etc)
//
// Revision 1.146  2008/05/28 16:46:29  fish
// Misleading VTAPE support renamed to AUTOMOUNT instead and fixed and enhanced so that it actually WORKS now.
//
// Revision 1.145  2008/05/25 06:36:43  fish
// VTAPE automount support (0x4B + 0xE4)
//
// Revision 1.144  2008/05/23 20:37:05  fish
// Fix "presuming" message in gettapetype function to use implemented default shortdesc in message.
//
// Revision 1.143  2008/05/22 21:17:29  fish
// Tape file extension neutrality support
//
// Revision 1.142  2008/05/22 20:15:12  fish
// Correct --blkid-24 typo (SHOULD be --blkid-22, not 24)
//
// Revision 1.141  2008/05/22 19:25:58  fish
// Flex FakeTape support
//
// Revision 1.140  2008/04/05 02:36:00  fish
// Wrap GENTMH_SCSI_ACTION... case in 'generic_tmhcall' function
// with #ifdef OPTION_SCSI_TAPE to fix undefined symbol error
// on non-scsi-tape build platforms
//
// Revision 1.139  2008/03/30 02:51:34  fish
// Fix SCSI tape EOV (end of volume) processing
//
// Revision 1.138  2008/03/29 08:36:46  fish
// More complete/extensive 3490/3590 tape support
//
// Revision 1.137  2008/03/28 02:09:42  fish
// Add --blkid-24 option support, poserror flag renamed to fenced,
// added 'generic', 'readblkid' and 'locateblk' tape media handler
// call vectors.
//
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
#include "tapedev.h"   /* Main tape handler header file              */

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

TAPEMEDIA_HANDLER  tmh_fake  =
{
    &generic_tmhcall,
    &open_faketape,
    &close_faketape,
    &read_faketape,
    &write_faketape,
    &rewind_faketape,
    &bsb_faketape,
    &fsb_faketape,
    &bsf_faketape,
    &fsf_faketape,
    &write_fakemark,
    &sync_faketape,
    &no_operation,              // (DSE)    ZZ FIXME: not coded yet
    &no_operation,              // (ERG)
    &is_tapeloaded_filename,
    &passedeot_faketape,
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
/* Device-Type Initialization Table    (DEV/CU MODEL, FEATURES, ETC) */
/*-------------------------------------------------------------------*/
/*
   PROGRAMMING NOTE: the MDR/OBR code (Device Characteristics bytes
   40-41) are apparently CRITICALLY IMPORTANT for proper tape drive
   functioning for certain operating systems. If the bytes are not
   provided (set to zero) or are set incorrectly, certain operating
   systems end up using unusual/undesirable Mode Set values in their
   Channel Programs (such as x'20' Write Immediate for example). I
   only note it here because these two particular bytes are rather
   innocuous looking based upon their name and sparsely documented
   and largely unexplained values, thereby possibly misleading one
   into believing they weren't important and thus could be safely
   set to zero if their values were unknown. Rest assured they are
   NOT unimportant! Quite the opposite: the are, for some operating
   systems, CRITICALLY IMPORTANT and must NOT be returned as zeros.

   The following were obtained from "EREP Release 3.5.0 Reference"
   (GC35-0152-03):

       Model                        MDR          OBR   
      -------                      -----        -----  

       3480                         0x41         0x80
       3490                         0x42         0x81

       3590                         0x46         0x83
       3590 (3591/3490 EMU)         0x47         0x84
       3590 (3590/3490 EMU)         0x48         0x85


   NOTE: only models 3480, 3490 and 3590 support the RDC (Read
   Device Characteristics) channel command, and thus they're the
   only ones we must know the MDR/OBR codes for (since the MDR/OBR
   codes are only used in the RDC CCW and not anwhere else). That
   is to say, NONE of the Channel Commands (CCWs) that all the
   OTHER models happen to support have an MDR/OBR code anywhere
   in their data. Only models 3480, 3490 and 3590 have MDR/OBR
   codes buried in their CCW data (specifically the RDC CCW data).

   Also note that, at the moment, we do not support emulating 3590's
   or 3591's running in 3490 Emulation Mode (i.e. 3591/3490 EMU or
   3590/3490 EMU). The user is free to use such a device with Herc-
   ules however, but if they do, it should be specified as a 3490.

---------------------------------------------------------------------*/

typedef struct DEVINITTAB               /* Initialization values     */
{
    U16         devtype;                /* Device type               */
    BYTE        devmodel;               /* Device model number       */
    U16         cutype;                 /* Control unit type         */
    BYTE        cumodel;                /* Control unit model number */
    U32         sctlfeat;               /* Storage control features  */
    BYTE        devclass;               /* Device class code         */
    BYTE        devtcode;               /* Device type code          */
    BYTE        MDR;                    /* Misc. Data Record ID      */
    BYTE        OBR;                    /* Outboard Recorder ID      */
    int         numdevid;               /* #of SNSID bytes (see NOTE)*/
    int         numsense;               /* #of SENSE bytes           */
    int         haverdc;                /* RDC Supported             */
    int         displayfeat;            /* Has LCD display           */
}
DEVINITTAB;
DEVINITTAB      DevInitTab[]  =         /* Initialization table      */
{
// PROGRAMMING NOTE: we currently do not support a #of Sense-ID bytes
// value (numdevid) greater than 7 since our current channel-subsystem
// design does not support the concept of hardware physical attachment
// "Nodes" (i.e. separate control-unit, device and interface elements).
// Supporting more than 7 bytes of Sense-ID information would require
// support for Node Descriptors (ND) and Node Element Descriptors (NED)
// and the associated commands (CCWs) to query them (Read Configuration
// Data (0xFA) , Set Interface Identifier (0x73) and associated support
// in the Read Subsystem Data (0x3E) command), which is vast overkill
// and a complete waste of time given our current overly-simple channel
// subsystem design.
//
//--------------------------------------------------------------------
//            3410/3411/3420/3422/3430/8809/9347/9348
//--------------------------------------------------------------------
//
// devtype/mod  cutype/mod    sctlfeat  cls typ MDR OBR sid sns rdc dsp
 { 0x3410,0x01, 0x3115,0x01, 0x00000000, 0,  0,  0,  0,  0,  9,  0,  0 },
 { 0x3411,0x01, 0x3115,0x01, 0x00000000, 0,  0,  0,  0,  0,  9,  0,  0 },
 { 0x3420,0x06, 0x3803,0x02, 0x00000000, 0,  0,  0,  0,  0, 24,  0,  0 }, // (DEFAULT: 3420)
 { 0x3422,0x01, 0x3422,0x01, 0x00000000, 0,  0,  0,  0,  7, 32,  0,  0 },
 { 0x3430,0x01, 0x3422,0x01, 0x00000000, 0,  0,  0,  0,  7, 32,  0,  0 },
 { 0x8809,0x01, 0x8809,0x01, 0x00000000, 0,  0,  0,  0,  0, 32,  0,  0 },
 { 0x9347,0x01, 0x9347,0x01, 0x00000000, 0,  0,  0,  0,  7, 32,  0,  0 },
 { 0x9348,0x01, 0x9348,0x01, 0x00000000, 0,  0,  0,  0,  7, 32,  0,  0 },

//--------------------------------------------------------------------
//                          3480/3490/3590
//--------------------------------------------------------------------
//
// PROGRAMMING NOTE: we currently do not support a #of Sense-ID bytes
// value (numdevid) greater than 7 since our current channel-subsystem
// design does not support the concept of hardware physical attachment
// "Nodes" (i.e. separate control-unit, device and interface elements).
// Supporting more than 7 bytes of Sense-ID information would require
// support for Node Descriptors (ND) and Node Element Descriptors (NED)
// and the associated commands (CCWs) to query them (Read Configuration
// Data (0xFA) , Set Interface Identifier (0x73) and associated support
// in the Read Subsystem Data (0x3E) command), which is vast overkill
// and a complete waste of time given our current overly-simple channel
// subsystem design.
//
// PROGRAMMING NOTE: if you change the below devtype/mod or cutype/mod
// values, be sure to ALSO change tapeccws.c's READ CONFIGURATION DATA
// (CCW opcode 0xFA) values as well!
//
// PROGRAMMING NOTE: the bit values of the 'sctlfeat' field are:
//
//     ....40..   (unknown)
//     ....08..   Set Special Intercept Condition (SIC) supported
//     ....04..   Channel Path No-Operation supported (always
//                on if Library Attachment Facility installed)
//     ....02..   Logical Write-Protect supported (always on
//                if Read Device Characteristics is supported)
//     ....01..   Extended Buffered Log support enabled (if 64
//                bytes of buffered log data, else 32 bytes)
//     ......80   Automatic Cartridge Loader installed/enabled
//     ......40   Improved Data Recording Capability (i.e.
//                compression support) installed/enabled
//     ......20   Suppress Volume Fencing
//     ......10   Library Interface online/enabled
//     ......08   Library Attachment Facility installed
//     ......04   (unknown)
//
// PROGRAMMING NOTE: the below "0x00004EC4" value for the 'sctlfeat'
// field for Model 3590 was determined empirically on a real machine.
//
// devtype/mod  cutype/mod    sctlfeat   cls  typ   MDR  OBR  sid sns  rdc dsp
 { 0x3480,0x31, 0x3480,0x31, 0x000002C0, 0x80,0x80, 0x41,0x80,  7, 24,  1,  1 },  // 0x31 = D31
 { 0x3490,0x50, 0x3490,0x50, 0x000002C0, 0x80,0x80, 0x42,0x81,  7, 32,  1,  1 },  // 0x50 = C10
 { 0x3590,0x10, 0x3590,0x50, 0x00004EC4, 0x80,0x80, 0x46,0x83,  7, 32,  1,  1 },  // 0x10 = B1A, 0x50 = A50
 { 0xFFFF,0xFF, 0xFFFF,0xFF, 0xFFFFFFFF, 0xFF,0xFF, 0xFF,0xFF, -1, -1, -1, -1 },  //**** END OF TABLE ****
 { 0x3420,0x06, 0x3803,0x02, 0x00000000,   0,   0,    0,   0,   0, 24,  0,  0 },  // (DEFAULT: 3420)
};

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int tapedev_init_handler (DEVBLK *dev, int argc, char *argv[])
{
int             rc;
DEVINITTAB*     pDevInitTab;

    /* Close current tape */
    if(dev->fd>=0)
    {
        dev->tmh->close(dev);
        dev->fd=-1;
    }

    autoload_close(dev);
    dev->tdparms.displayfeat=0;

    /* Determine the control unit type and model number */
    /* Support for 3490/3422/3430/8809/9347, etc.. */
    if (!sscanf( dev->typname, "%hx", &dev->devtype ))
        dev->devtype = 0x3420;

    // PROGAMMING NOTE: we use hard-coded values from our DevInitTab
    // for virtual (non-SCSI) devices and, for the time being, for non-
    // virtual (SCSI) devices too. Once we add direct SCSI I/O support
    // we will need to add code to get this information directly from
    // the actual SCSI device itself.
    for
    (
        pDevInitTab = &DevInitTab[0];
        pDevInitTab->devtype != 0xFFFF && pDevInitTab->devtype != dev->devtype;
        pDevInitTab++
    );

    if (pDevInitTab->devtype == 0xFFFF)         /* (entry not found?) */
    {
        logmsg ( _("Unsupported device type specified %4.4x\n"), dev->devtype );

        pDevInitTab++;                          /* (default entry; s/b same as 0x3420) */
        pDevInitTab->devtype = dev->devtype;    /* (don't know what else to do really) */
        pDevInitTab->cutype  = dev->devtype;    /* (don't know what else to do really) */
    }

    /* Allow SENSE ID for certain specific legacy devices if requested */

    dev->numdevid = pDevInitTab->numdevid;  // (default == from table)

    if (1
        && sysblk.legacysenseid             // (if option requested, AND is)
        && (0                               // (for allowable legacy device)
            || 0x3410 == dev->devtype
            || 0x3411 == dev->devtype
            || 0x3420 == dev->devtype
            || 0x8809 == dev->devtype
           )
    )
    {
        dev->numdevid = 7;                  // (allow for this legacy device)
    }

    /* Initialize the Sense-Id bytes if needed... */
    if (dev->numdevid > 0)
    {
        dev->devid[0] = 0xFF;

        dev->devid[1] = (pDevInitTab->cutype >> 8) & 0xFF;
        dev->devid[2] = (pDevInitTab->cutype >> 0) & 0xFF;
        dev->devid[3] =  pDevInitTab->cumodel;

        dev->devid[4] = (pDevInitTab->devtype >> 8) & 0xFF;
        dev->devid[5] = (pDevInitTab->devtype >> 0) & 0xFF;
        dev->devid[6] =  pDevInitTab->devmodel;

        /* Initialize the CIW information if needed... */
        if (dev->numdevid > 7)
        {
            // PROGRAMMING NOTE: see note near 'DEVINITTAB'
            // struct definition regarding requirements for
            // supporting more than 7 bytes of SNSID info.

            memcpy (&dev->devid[8],  "\x40\xFA\x00\xA0", 4);  // CIW Read Configuration Data  (0xFA)
            memcpy (&dev->devid[12], "\x41\x73\x00\x04", 4);  // CIW Set Interface Identifier (0x73)
            memcpy (&dev->devid[16], "\x42\x3E\x00\x60", 4);  // CIW Read Subsystem Data      (0x3E)
        }
    }

    /* Initialize the Read Device Characteristics (RDC) bytes... */
    if (pDevInitTab->haverdc)
    {
        dev->numdevchar = 64;

        memset (dev->devchar, 0, sizeof(dev->devchar));
        memcpy (dev->devchar, dev->devid+1, 6);

        // Bytes 6-9: Subsystem Facilities...

        dev->devchar[6] = (pDevInitTab->sctlfeat >> 24) & 0xFF;
        dev->devchar[7] = (pDevInitTab->sctlfeat >> 16) & 0xFF;
        dev->devchar[8] = (pDevInitTab->sctlfeat >>  8) & 0xFF;
        dev->devchar[9] = (pDevInitTab->sctlfeat >>  0) & 0xFF;

        // Bytes 10/11: Device Class/Type ...

        dev->devchar[10] = pDevInitTab->devclass;
        dev->devchar[11] = pDevInitTab->devtcode;

        // Bytes 24-29: cutype/model & devtype/model ...
        // (Note: undocumented; determined empirically)

        dev->devchar[24] = (pDevInitTab->cutype >> 8) & 0xFF;
        dev->devchar[25] = (pDevInitTab->cutype >> 0) & 0xFF;
        dev->devchar[26] =  pDevInitTab->cumodel;

        dev->devchar[27] = (pDevInitTab->devtype >> 8) & 0xFF;
        dev->devchar[28] = (pDevInitTab->devtype >> 0) & 0xFF;
        dev->devchar[29] =  pDevInitTab->devmodel;

        // Bytes 40-41: MDR/OBR code...

        dev->devchar[40] = pDevInitTab->MDR;
        dev->devchar[41] = pDevInitTab->OBR;
    }

    /* Initialize other fields */
//  dev->numdevid            = pDevInitTab->numdevid;   // (handled above)
    dev->numsense            = pDevInitTab->numsense;
    dev->tdparms.displayfeat = pDevInitTab->displayfeat;

    dev->fenced              = 0;   // (always, initially)
    dev->SIC_active          = 0;   // (always, initially)
    dev->SIC_supported       = 0;   // (until we're sure)
    dev->forced_logging      = 0;   // (always, initially)
#if defined( OPTION_TAPE_AUTOMOUNT )
    dev->noautomount         = 0;   // (always, initially)
#endif

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

    if (dev->devchar[8] & 0x08)     // SIC supported?
        dev->SIC_supported = 1;     // remember that fact

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
/*  Tape format determination REGEXPS. Used by gettapetype below     */
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
/*  Tape format determination REGEXPS. Used by gettapetype below     */
/*-------------------------------------------------------------------*/

struct  tape_format_entry   fmttab   [] =   /*    (table itself)     */
{
    /* This entry matches a filename ending with .aws    */
#define  AWSTAPE_FMTENTRY   0
#define  DEFAULT_FMTENTRY   AWSTAPE_FMTENTRY
    {
        "\\.aws$",
        TAPEDEVT_AWSTAPE,
        &tmh_aws,
        "AWS Format tape file",
        "AWS tape"
    },

    /* This entry matches a filename ending with .het    */
#define  HETTAPE_FMTENTRY   1
    {
        "\\.het$",
        TAPEDEVT_HETTAPE,
        &tmh_het,
        "Hercules Emulated Tape file",
        "HET tape"
    },

    /* This entry matches a filename ending with .tdf    */
#define  OMATAPE_FMTENTRY   2
    {
        "\\.tdf$",
        TAPEDEVT_OMATAPE,
        &tmh_oma,
        "Optical Media Attachment (OMA) tape",
        "OMA tape"
    },

    /* This entry matches a filename ending with .fkt    */
#define  FAKETAPE_FMTENTRY  3
    {
        "\\.fkt$",
        TAPEDEVT_FAKETAPE,
        &tmh_fake,
        "Flex FakeTape file",
        "FakeTape"
    },

#if defined(OPTION_SCSI_TAPE)

    /* This entry matches a filename starting with /dev/ */
#define  SCSITAPE_FMTENTRY  4
    {
        "^/dev/",
        TAPEDEVT_SCSITAPE,
        &tmh_scsi,
        "SCSI attached tape drive",
        "SCSI tape"
    },

#if defined(_MSVC_)

    /* (same idea but for Windows SCSI tape device names) */
#undef   SCSITAPE_FMTENTRY
#define  SCSITAPE_FMTENTRY  5
    {
        "^\\\\\\\\\\.\\\\Tape[0-9]",
        TAPEDEVT_SCSITAPE,
        &tmh_scsi,
        "SCSI attached tape drive",
        "SCSI tape"
    },

#endif // _MSVC_
#endif // OPTION_SCSI_TAPE
};


/*-------------------------------------------------------------------*/
/*  gettapetype_byname        determine tape device type by filename */
/*-------------------------------------------------------------------*/
/* returns fmttab entry# on success, -1 on error/unable to determine */
/*-------------------------------------------------------------------*/
int gettapetype_byname (DEVBLK *dev)
{
#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)
    regex_t     regwrk;                 /* REGEXP work area          */
    regmatch_t  regwrk2;                /* REGEXP match area         */
    char        errbfr[1024];           /* Working storage           */
    int         i;                      /* Loop control              */
#endif // HAVE_REGEX_H
    int         rc;                     /* various rtns return codes */

    /* Use the file name to determine the device type */

#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)

    for (i=0; i < arraysize( fmttab ); i++)
    {
        rc = regcomp (&regwrk, fmttab[i].fmtreg, REG_ICASE);
        if (rc < 0)
        {
            regerror (rc, &regwrk, errbfr, 1024);
            logmsg (_("HHCTA999E Device %4.4X: Unable to determine tape format type for %s: Internal error: Regcomp error %s on index %d\n"),
                dev->devnum, dev->filename, errbfr, i);
            return -1;
        }

        rc = regexec (&regwrk, dev->filename, 1, &regwrk2, 0);
        if (rc < 0)
        {
            regerror (rc, &regwrk, errbfr, 1024);
            regfree ( &regwrk );
            logmsg (_("HHCTA999E Device %4.4X: Unable to determine tape format type for %s: Internal error: Regexec error %s on index %d\n"),
                dev->devnum, dev->filename, errbfr, i);
            return -1;
        }

        regfree (&regwrk);

        if (rc == 0)  /* MATCH? */
            return i;

        ASSERT( rc == REG_NOMATCH );
    }

#else // !HAVE_REGEX_H

    if (1
        && (rc = strlen(dev->filename)) > 4
        && (rc = strcasecmp( &dev->filename[rc-4], ".aws" )) == 0
    )
    {
        return AWSTAPE_FMTENTRY;
    }

    if (1
        && (rc = strlen(dev->filename)) > 4
        && (rc = strcasecmp( &dev->filename[rc-4], ".het" )) == 0
    )
    {
        return HETTAPE_FMTENTRY;
    }

    if (1
        && (rc = strlen(dev->filename)) > 4
        && (rc = strcasecmp( &dev->filename[rc-4], ".tdf" )) == 0
    )
    {
        return OMATAPE_FMTENTRY;
    }

    if (1
        && (rc = strlen(dev->filename)) > 4
        && (rc = strcasecmp( &dev->filename[rc-4], ".fkt" )) == 0
    )
    {
        return FAKETAPE_FMTENTRY;
    }

#if defined(OPTION_SCSI_TAPE)
    if (1
        && (rc = strlen(dev->filename)) > 5
        && (rc = strncasecmp( dev->filename, "/dev/", 5 )) == 0
    )
    {
        if (strncasecmp( dev->filename+5, "st", 2 ) == 0)
            dev->stape_close_rewinds = 1; // (rewind at close)
        else
            dev->stape_close_rewinds = 0; // (otherwise don't)

        return SCSITAPE_FMTENTRY;
    }
#if defined(_MSVC_)
    if (1
        && strncasecmp(dev->filename, "\\\\.\\Tape", 8) == 0
        && isdigit(*(dev->filename+8))
        &&  0  ==  *(dev->filename+9)
    )
    {
        return SCSITAPE_FMTENTRY;
    }
#endif // _MSVC_
#endif // OPTION_SCSI_TAPE
#endif // HAVE_REGEX_H

    return -1;      /* -1 == "unable to determine" */

} /* end function gettapetype_byname */


/*-------------------------------------------------------------------*/
/*  gettapetype_bydata       determine tape device type by file data */
/*-------------------------------------------------------------------*/
/* returns fmttab entry# on success, -1 on error/unable to determine */
/*-------------------------------------------------------------------*/
int gettapetype_bydata (DEVBLK *dev)
{
    char        pathname[MAX_PATH];     /* file path in host format  */
    int         rc;                     /* various rtns return codes */

    /* Try to determine the type based on actual file contents */
    hostpath( pathname, dev->filename, sizeof(pathname) );
    rc = open ( pathname, O_RDONLY | O_BINARY );
    if (rc >= 0)
    {
        BYTE hdr[6];                    /* block header i/o buffer   */
        int fd = rc;                    /* save file descriptor      */

        /* Read the header. If bytes 0-3 are ASCII "0000", then the
         * tape is likely a Flex FakeTape. Otherwise if bytes 2-3 are
         * binary zero (x'0000'), it's likely an AWS type tape. If byte
         * 4 (first flag byte) has either of the ZLIB or BZIP2 flags on,
         * then it's a HET tape. Otherwise it's just an ordinary AWS tape.
         */
        rc = read (fd, hdr, sizeof(hdr));
             close(fd);
        if (rc >= 6)
        {
            /* Use the data to make the possible determination */
            if (memcmp(hdr, "@TDF", 4) == 0)
                return OMATAPE_FMTENTRY;

            if (hdr[0] == 0x30 && hdr[1] == 0x30 && hdr[2] == 0x30 && hdr[3] == 0x30)
                return FAKETAPE_FMTENTRY;

            if (hdr[2] == 0 && hdr[3] == 0)
            {
                if (hdr[4] & 0x03)  /* ZLIB and/or BZIP2 compressed? */
                    return HETTAPE_FMTENTRY;
                else
                    return AWSTAPE_FMTENTRY;    /* (default) */
            }
        }
    }
    return -1;      /* -1 == "unable to determine" */

} /* end function gettapetype_bydata */


/*-------------------------------------------------------------------*/
/*  gettapetype              determine tape device type              */
/*-------------------------------------------------------------------*/
/* returns fmttab entry# on success, -1 on error/unable to determine */
/*-------------------------------------------------------------------*/
int gettapetype (DEVBLK *dev, char **short_descr)
{
    char*       descr;                  /* Device descr from fmttab  */
    int         i = -1;                 /* fmttab entry#             */

    /* Try to determine device type by actual file contents first,
       but only if this isn't a SCSI tape device. (Thus we need to
       check by name first to determine if it's a SCSI device) */
#if defined(OPTION_SCSI_TAPE)
    i = gettapetype_byname( dev );          // (check if this is a SCSI)

    if (i != SCSITAPE_FMTENTRY)             // (if not, then check by..
#endif
    {
        int i2 = gettapetype_bydata( dev ); // ..reading the file data)
        if (i2 >= 0)                        // (if that worked..)
            i = i2;                         // (..then we know the type)
    }

    /* If we couldn't determine the device type based on the file's
       contents, then try again but this time based on its filename */
    if (i < 0)
        i = gettapetype_byname( dev );

    /* If still unknown, use a reasonable default value */
    if (i < 0)
    {
        i = DEFAULT_FMTENTRY;
        if (strcmp (dev->filename, TAPE_UNLOADED) != 0)
            logmsg (_("HHCTA999W Device %4.4X: Unable to determine tape format type for %s; presuming %s.\n"),
                 dev->devnum, dev->filename, fmttab[i].short_descr );
    }

    dev->tapedevt = fmttab[i].fmtcode;
    dev->tmh      = fmttab[i].tmh;
    descr         = fmttab[i].descr;
    *short_descr  = fmttab[i].short_descr;

    if (strcmp (dev->filename, TAPE_UNLOADED) != 0)
        logmsg (_("HHCTA998I Device %4.4X: %s is a %s\n"),
            dev->devnum, dev->filename, descr);

    return 0;   // (success)

} /* end function gettapetype */


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
#if defined( OPTION_TAPE_AUTOMOUNT )
    { "noautomount",NULL },
#endif
    { "--blkid-22", NULL },
    { "--blkid-24", NULL },   /* (synonym for --blkid-22) */
    { "--blkid-32", NULL },
    { "--no-erg",   NULL },
    { NULL,         NULL },   /* (end of table) */
};

/*-------------------------------------------------------------------*/
/*  The following table goes hand-in-hand with the 'ptab' PARSER     */
/*  table immediately above. Used by 'mountnewtape' function.        */
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
#if defined( OPTION_TAPE_AUTOMOUNT )
    TDPARM_NOAUTOMOUNT,
#endif
    TDPARM_BLKID22,
    TDPARM_BLKID24,
    TDPARM_BLKID32,
    TDPARM_NOERG
};

/*-------------------------------------------------------------------*/
/*        mountnewtape     --     mount a tape in the drive          */
/*-------------------------------------------------------------------*/
/*                                                                   */
/*  Syntax:       filename [options]                                 */
/*                                                                   */
/*  where options are any of the entries in the 'ptab' PARSER        */
/*  table defined further above. Some commonly used options are:     */
/*                                                                   */
/*    awstape          sets the HET parms to be compatible with the  */
/*                     R|P/390|'s tape file Format (HET files)       */
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
    char*       short_descr;            /* Short descr from fmttab   */
    int         i;                      /* Loop control              */
    int         rc, optrc;              /* various rtns return codes */
    union {                             /* Parser results            */
        U32     num;                    /* Parser results            */
        BYTE    str[ 80 ];              /* Parser results            */
    } res;                              /* Parser results            */

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

    /* Determine tape device type... */
    VERIFY( gettapetype( dev, &short_descr ) == 0 );

    /* (sanity check) */
    ASSERT(dev->tapedevt != TAPEDEVT_UNKNOWN);
    ASSERT(dev->tmh != NULL);
    ASSERT(short_descr != NULL);

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
    dev->eotmargin         = 128*1024; // 128K EOT margin (default)
    dev->tdparms.logical_readonly = 0; // read/write      (default)
#if defined( OPTION_TAPE_AUTOMOUNT )
    dev->noautomount       = 0;
#endif

#if defined(OPTION_SCSI_TAPE)
    // Real 3590's support Erase Gap and use 32-bit blockids.

    if (TAPEDEVT_SCSITAPE == dev->tapedevt
        &&     0x3590     == dev->devtype)
    {
        dev->stape_no_erg   = 0;        // (default for 3590 SCSI)
        dev->stape_blkid_32 = 1;        // (default for 3590 SCSI)
    }
#endif

#define  HHCTA078E()  logmsg (_("HHCTA078E Device %4.4X: option '%s' not valid for %s\n"), \
                              dev->devnum, argv[i], short_descr)

    /* Process remaining options */
    rc = 0;
    for (i = 1; i < argc; i++)
    {
        optrc = 0;
        switch (parser (&ptab[0], argv[i], &res))
        {
        case TDPARM_NONE:
            logmsg (_("HHCTA067E Device %4.4X: option '%s' unrecognized\n"),
                dev->devnum, argv[i]);
            optrc = -1;
            break;

        case TDPARM_AWSTAPE:
            if (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || TAPEDEVT_FAKETAPE == dev->tapedevt
            )
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.compress = FALSE;
            dev->tdparms.chksize = 4096;
            break;

        case TDPARM_IDRC:
        case TDPARM_COMPRESS:
            if (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || TAPEDEVT_FAKETAPE == dev->tapedevt
            )
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.compress = (res.num ? TRUE : FALSE);
            break;

        case TDPARM_METHOD:
            if (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || TAPEDEVT_FAKETAPE == dev->tapedevt
            )
            {
                HHCTA078E(); optrc = -1; break;
            }
            if (res.num < HETMIN_METHOD || res.num > HETMAX_METHOD)
            {
                logmsg(_("HHCTA068E Device %4.4X: option '%s': method must be within %u-%u\n"),
                    dev->devnum, argv[i], HETMIN_METHOD, HETMAX_METHOD);
                optrc = -1;
                break;
            }
            dev->tdparms.method = res.num;
            break;

        case TDPARM_LEVEL:
            if (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || TAPEDEVT_FAKETAPE == dev->tapedevt
            )
            {
                HHCTA078E(); optrc = -1; break;
            }
            if (res.num < HETMIN_LEVEL || res.num > HETMAX_LEVEL)
            {
                logmsg(_("HHCTA069E Device %4.4X: option '%s': level must be within %u-%u\n"),
                    dev->devnum, argv[i], HETMIN_LEVEL, HETMAX_LEVEL);
                optrc = -1;
                break;
            }
            dev->tdparms.level = res.num;
            break;

        case TDPARM_CHKSIZE:
            if (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || TAPEDEVT_FAKETAPE == dev->tapedevt
            )
            {
                HHCTA078E(); optrc = -1; break;
            }
            if (res.num < HETMIN_CHUNKSIZE || res.num > HETMAX_CHUNKSIZE)
            {
                logmsg (_("HHCTA070E Device %4.4X: option '%s': chunksize must be within %u-%u\n"),
                    dev->devnum, argv[i], HETMIN_CHUNKSIZE, HETMAX_CHUNKSIZE);
                optrc = -1;
                break;
            }
            dev->tdparms.chksize = res.num;
            break;

        case TDPARM_MAXSIZE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.maxsize=res.num;
            break;

        case TDPARM_MAXSIZEK:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.maxsize=res.num*1024;
            break;

        case TDPARM_MAXSIZEM:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.maxsize=res.num*1024*1024;
            break;

        case TDPARM_EOTMARGIN:
            dev->eotmargin=res.num;
            break;

        case TDPARM_STRICTSIZE:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.strictsize=res.num;
            break;

        case TDPARM_READONLY:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.logical_readonly=(res.num ? 1 : 0 );
            break;

        case TDPARM_RO:
        case TDPARM_NORING:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.logical_readonly=1;
            break;

        case TDPARM_RW:
        case TDPARM_RING:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.logical_readonly=0;
            break;

        case TDPARM_DEONIRQ:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->tdparms.deonirq=(res.num ? 1 : 0 );
            break;

#if defined( OPTION_TAPE_AUTOMOUNT )

        case TDPARM_NOAUTOMOUNT:
            if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->noautomount = 1;
            break;

#endif /* OPTION_TAPE_AUTOMOUNT */

#if defined(OPTION_SCSI_TAPE)
        case TDPARM_BLKID22:
        case TDPARM_BLKID24:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->stape_blkid_32 = 0;
            break;

        case TDPARM_BLKID32:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->stape_blkid_32 = 1;
            break;

        case TDPARM_NOERG:
            if (TAPEDEVT_SCSITAPE != dev->tapedevt)
            {
                HHCTA078E(); optrc = -1; break;
            }
            dev->stape_no_erg = 1;
            break;
#endif /* defined(OPTION_SCSI_TAPE) */

        default:
            logmsg(_("HHCTA071E Device %4.4X: option '%s': parse error\n"),
                dev->devnum, argv[i]);
            optrc = -1;
            break;

        } // end switch (parser (&ptab[0], argv[i], &res))

        if (optrc < 0)
            rc = -1;
        else
            logmsg (_("HHCTA066I Device %4.4X: option '%s' accepted.\n"),
                dev->devnum, argv[i]);

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
    char devparms[ MAX_PATH+1 + 128 ];
    char dispmsg [ 256 ];

    BEGIN_DEVICE_CLASS_QUERY( "TAPE", dev, class, buflen, buffer );

    *buffer = 0;
    devparms[0]=0;
    dispmsg [0]=0;

    GetDisplayMsg( dev, dispmsg, sizeof(dispmsg) );

    if (strchr(dev->filename,' ')) strlcat( devparms, "\"",          sizeof(devparms));
                                   strlcat( devparms, dev->filename, sizeof(devparms));
    if (strchr(dev->filename,' ')) strlcat( devparms, "\"",          sizeof(devparms));

#if defined( OPTION_TAPE_AUTOMOUNT )

    if (dev->noautomount)
        strlcat( devparms, " noautomount", sizeof(devparms));

#endif /* OPTION_TAPE_AUTOMOUNT */

    if ( strcmp( dev->filename, TAPE_UNLOADED ) == 0 )
    {
#if defined(OPTION_SCSI_TAPE)
        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
        {
            if (0x3590 == dev->devtype) // emulating 3590
            {
                if (!dev->stape_blkid_32 ) strlcat( devparms, " --blkid-22", sizeof(devparms) );
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
        char tapepos[64]; tapepos[0]=0;

        if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
        {
            snprintf( tapepos, sizeof(tapepos), "[%d:%08"I64_FMT"X] ",
                dev->curfilen, dev->nxtblkpos );
            tapepos[sizeof(tapepos)-1] = 0;
        }
#if defined(OPTION_SCSI_TAPE)
        else // (this is a SCSI tape drive)
        {
            if (STS_BOT( dev ))
            {
                dev->eotwarning = 0;
                strlcat(tapepos,"*BOT* ",sizeof(tapepos));
            }

            // If tape has a display, then GetDisplayMsg already
            // appended *FP* for us. Otherwise we need to do it.

            if ( !dev->tdparms.displayfeat )
                if (STS_WR_PROT( dev ))
                    strlcat(tapepos,"*FP* ",sizeof(tapepos));

            if (0x3590 == dev->devtype) // emulating 3590
            {
                if (!dev->stape_blkid_32 ) strlcat( devparms, " --blkid-22", sizeof(devparms) );
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

        case TAPEDEVT_HETTAPE:
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
                dev->eotwarning = 0;
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
#if defined(OPTION_SCSI_TAPE)
        case GENTMH_SCSI_ACTION_UPDATE_STATUS:
        {
            return update_status_scsitape( pGenParms->dev );
        }
#endif /* defined(OPTION_SCSI_TAPE) */

        default:
        {
            errno = EINVAL;         // (invalid arguments)
            return -1;              // (return failure)
        }
    }

    return -1;      // (never reached)
}
