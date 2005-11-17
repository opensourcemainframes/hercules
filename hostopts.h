/*-------------------------------------------------------------------*/
/* HOSTOPTS.H   --   HOST-specific features and options              */
/*-------------------------------------------------------------------*/

//    This header file #included by 'featall.h' and 'hercules.h'

/*
   All HOST-operating-specific (Win32, Apple. Linux, etc) FEATures
   and OPTIONs that cannot be otherwise determined via configure.ac
   tests should be #defined here, and ONLY here!

   -----------------------------------------------------------------
   REMINDER: please do NOT use host-specific tests anywhere else in
   Hercules source code if you can help it! (e.g. #ifdef WIN32, etc)

   Instead, add a test to configure.ac which tests for the availability
   of the specific feature in question and then #defines a OPTION_XXX
   which can then be used in Hercules source code.
   -----------------------------------------------------------------

   The ONLY allowed exception is in the Hercules.h and htypes.h
   header files where different header files need to be #included
   (e.g. sockets) and/or typedef/#defines need to be made depending
   on the host build system type.

   ONLY IF such a configure.ac test is impractical or otherwise not
   possible should you then hard-code the OPTION_XXX setting here in
   this member (and ONLY in this member!) depending on the host o/s.

   Thus, all of the below hard-coded options are candidates for some
   future configure.ac test.

   Feel free to design one.

   Please. :)
*/

#ifndef _HOSTOPTS_H
#define _HOSTOPTS_H

/*-------------------------------------------------------------------*/
/* ZZ FIXME
                  'OPTION_SCSI_ERASE_TAPE'
                  'OPTION_SCSI_ERASE_GAP'

    NOTE: The following SHOULD in reality be some sort of test
    within configure.ac, but until we can devise some sort of
    simple configure test, we must hard-code them for now.

    According to the only docs I could find:

       MTERASE   Erase the media from current position.
                 If the field mt_count is nonzero, a full
                 erase is done (from current position to
                 end of media). If mt_count is zero, only
                 an erase gap is written. It is hard to
                 say which drives support only one but not
                 the other option

    HOWEVER, since it's hard to say which drivers support short
    erase-gaps and which support erase-tape (and HOW they support
    them if they do! For example, Cygwin is currently coded to
    perform whichever type of erase the drive happens to support;
    e.g. if you try to do an erase-gap but the drive doesn't support
    short erases, it will end up doing a LONG erase [of the entire
    tape]!! (and vice-versa: doing a long erase-tape on a drive
    that doesn't support it will cause [Cygwin] to do an erase-
    gap instead)).

    THUS, the SAFEST thing to do is to simply treat all "erases",
    whether short or long, as 'nop's for now (in order to prevent
    the accidental erasure of an entire tape!) Once we happen to
    know for DAMN SURE that a particular host o/s ALWAYS does what
    we want it to should we then change the below #defines. (and
    like I said, they really SHOULD be in some type of configure
    test/setting and not here).
*/
/*-------------------------------------------------------------------*\

                     File name comparisons
                  ('strcmp' vs. 'strcasecmp')

   On Windows, file names are not case sensitive. While the case
   of the file name may be preserved by the file system (and thus
   show file names in both upper/lower case in directory listings
   for example), the file system itself is NOT case-sensitive. File
   names "Foo", "foo", "fOo", "FoO", etc, all refer to the same file.

   On other platforms however (e.g. *nix), the file system IS case
   sensitive. File names "Foo", "foo", "fOo", "FoO", etc, all refer
   to different files on such systems. Thus we define a 'strfilecmp'
   macro to be used for filename comparisons and define it to be
   strcasecmp on Win32 platforms and strcmp for other platforms.

\*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/* Constants used in "#if OPTION_NAME == OPTION_VALUE" statements    */
/*-------------------------------------------------------------------*/


//       HOW_TO_IMPLEMENT_SH_COMMAND

#if defined( HAVE_FORK )
#define  USE_FORK_API_FOR_SH_COMMAND           4
#endif
#define  USE_ANSI_SYSTEM_API_FOR_SH_COMMAND    9


//       SET_CONSOLE_CURSOR_SHAPE_METHOD

#define  CURSOR_SHAPE_NOT_SUPPORTED             0
#define  CURSOR_SHAPE_VIA_SPECIAL_LINUX_ESCAPE  1



/*-------------------------------------------------------------------*/
/* Hard-coded Win32-specific features and options...                 */
/*-------------------------------------------------------------------*/
#if defined(WIN32)                      /* "Windows" options         */

#if defined(HDL_BUILD_SHARED) && defined(_MSVC_)
  #define  DLL_IMPORT   __declspec ( dllimport )
  #define  DLL_EXPORT   __declspec ( dllexport )
#else
  #define  DLL_IMPORT   extern
  #define  DLL_EXPORT
#endif

#define HTTP_SERVER_CONNECT_KLUDGE

#undef  OPTION_FISH_STUPID_GUI_PRTSPLR_EXPERIMENT     /* (Don't ask!) */
/*  Note:  OPTION_FISHIO  only possible with  OPTION_FTHREADS        */
#if defined(OPTION_FTHREADS)
  #define OPTION_FISHIO                 /* Use Fish's I/O scheduler  */
#else
  #undef  OPTION_FISHIO                 /* Use Herc's I/O scheduler  */
#endif
#define OPTION_W32_CTCI                 /* Fish's TunTap for CTCA's  */

#define OPTION_SCSI_TAPE                /* SCSI tape support         */
#ifdef _MSVC_
#define OPTION_SCSI_ERASE_TAPE          /* SUPPORTED!                */
#define OPTION_SCSI_ERASE_GAP           /* SUPPORTED!                */
#else // (mingw or cygwin?)
#undef  OPTION_SCSI_ERASE_TAPE          /* (NOT supported!)          */
#undef  OPTION_SCSI_ERASE_GAP           /* (NOT supported!)          */
#endif
#undef  OPTION_FBA_BLKDEVICE            /* (no FBA BLKDEVICE support)*/

#define OPTION_TUNTAP_SETNETMASK        /* TUNTAP_SetNetMask works   */
#define OPTION_TUNTAP_SETMACADDR        /* TUNTAP_SetMACAddr works   */
#define OPTION_TUNTAP_DELADD_ROUTES     /* Del/Add Routes    works   */

#define MAX_DEVICE_THREADS          0   /* (0 == unlimited)          */
#undef  MIXEDCASE_FILENAMES_ARE_UNIQUE  /* ("Foo" same as "fOo"!!)   */

#define DEFAULT_HERCPRIO    0
#define DEFAULT_TOD_PRIO    0
#define DEFAULT_CPU_PRIO    0
#define DEFAULT_DEV_PRIO   -8

#ifdef _MSVC_
  #define HOW_TO_IMPLEMENT_SH_COMMAND   USE_ANSI_SYSTEM_API_FOR_SH_COMMAND
#else
  #define HOW_TO_IMPLEMENT_SH_COMMAND   USE_FORK_API_FOR_SH_COMMAND
#endif


/*-------------------------------------------------------------------*/
/* Hard-coded Apple-specific features and options...                 */
/*-------------------------------------------------------------------*/
#elif defined(__APPLE__)                /* "Apple" options           */

#define DLL_IMPORT   extern
#define DLL_EXPORT

#undef  OPTION_SCSI_TAPE                /* No SCSI tape support      */
#undef  OPTION_SCSI_ERASE_TAPE          /* (NOT supported)           */
#undef  OPTION_SCSI_ERASE_GAP           /* (NOT supported)           */
#undef  OPTION_FBA_BLKDEVICE            /* (no FBA BLKDEVICE support)*/

#undef  OPTION_TUNTAP_SETNETMASK        /* TUNTAP_SetNetMask broken  */
#undef  OPTION_TUNTAP_SETMACADDR        /* TUNTAP_SetMACAddr broken  */
#undef  OPTION_TUNTAP_DELADD_ROUTES     /* Del/Add Routes    broken  */

#define MAX_DEVICE_THREADS          0   /* (0 == unlimited)          */
#define MIXEDCASE_FILENAMES_ARE_UNIQUE  /* ("Foo" and "fOo" unique)  */

#define DEFAULT_HERCPRIO    0
#define DEFAULT_TOD_PRIO  -20
#define DEFAULT_CPU_PRIO   15
#define DEFAULT_DEV_PRIO    8

#define HOW_TO_IMPLEMENT_SH_COMMAND       USE_ANSI_SYSTEM_API_FOR_SH_COMMAND
#define SET_CONSOLE_CURSOR_SHAPE_METHOD   CURSOR_SHAPE_NOT_SUPPORTED

/*-------------------------------------------------------------------*/
/* Hard-coded OTHER host-specific features and options...            */
/*-------------------------------------------------------------------*/
#else                                   /* "Other platform" options  */

#define DLL_IMPORT   extern
#define DLL_EXPORT

#define OPTION_SCSI_TAPE                /* SCSI tape support         */
#undef  OPTION_SCSI_ERASE_TAPE          /* (NOT supported)           */
#undef  OPTION_SCSI_ERASE_GAP           /* (NOT supported)           */
#define OPTION_FBA_BLKDEVICE            /* FBA block device support  */

#define OPTION_TUNTAP_SETNETMASK        /* TUNTAP_SetNetMask works   */
#define OPTION_TUNTAP_SETMACADDR        /* TUNTAP_SetMACAddr works   */
#define OPTION_TUNTAP_DELADD_ROUTES     /* Del/Add Routes    works   */

#define MAX_DEVICE_THREADS          0   /* (0 == unlimited)          */
#define MIXEDCASE_FILENAMES_ARE_UNIQUE  /* ("Foo" and "fOo" unique)  */

#define DEFAULT_HERCPRIO    0
#define DEFAULT_TOD_PRIO  -20
#define DEFAULT_CPU_PRIO   15
#define DEFAULT_DEV_PRIO    8

#define HOW_TO_IMPLEMENT_SH_COMMAND       USE_FORK_API_FOR_SH_COMMAND
#define SET_CONSOLE_CURSOR_SHAPE_METHOD   CURSOR_SHAPE_VIA_SPECIAL_LINUX_ESCAPE

#endif // (host-specific tests)

#endif // _HOSTOPTS_H
