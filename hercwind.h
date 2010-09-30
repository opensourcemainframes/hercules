/* HERCWIND.H   (c) Copyright Roger Bowler, 2005-2009                */
/*              MSVC Environment Specific Definitions                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Header file containing additional data structures and function    */
/* prototypes required by Hercules in the MSVC environment           */
/*-------------------------------------------------------------------*/

// $Id$

#if !defined(_HERCWIND_H)
#define _HERCWIND_H

// PROGRAMMING NOTE: Cygwin has a bug in setvbuf requiring us
// to do an 'fflush()' after each stdout/err write, and it doesn't
// hurt doing it for the MSVC build either...
#define NEED_LOGMSG_FFLUSH

#if !defined( _MSVC_ )
  #error This file is only for building Hercules with MSVC
#endif

#if defined( _MSC_VER ) && (_MSC_VER < 1300)
  #error MSVC compiler versions less than 13.0 not supported.
#endif

///////////////////////////////////////////////////////////////////////
// The following is mostly for issuing "warning" messages since MS's
// compiler doesn't support #warning. Instead, we must use #pragma
// message as follows:
//
//     #pragma message( MSVC_MESSAGE_LINENUM "blah, blah..." )
//
// which results in:
//
//     foobar.c(123) : blah, blah...
//
// which is really handy when using their Visual Studio IDE since
// it allows us to quickly jump to that specific source statement
// with just the press of a function key...

#if !defined( MSVC_MESSAGE_LINENUM )
  #define MSVC_STRINGIZE( L )        #L
  #define MSVC_MAKESTRING( M, L )    M( L )
  #define MSVC_QUOTED_LINENUM        MSVC_MAKESTRING( MSVC_STRINGIZE, __LINE__ )
  #define MSVC_MESSAGE_LINENUM       __FILE__ "(" MSVC_QUOTED_LINENUM ") : "
#endif

///////////////////////////////////////////////////////////////////////
// Disable some warnings that tend to get in the way...
//
// FIXME: purposely disabling warning C4244 is dangerous IMO and might
// come back to haunt us in the future when we DO happen to introduce
// an unintentional coding error that results in unexpected data loss.
//
// We should instead take the time to fix all places where it's issued
// (being sure to add comments when we do) so that we can then rely on
// C4244 to warn us of real/actual coding errors.  -  Fish, April 2006

#pragma warning( disable: 4142 ) // C4142: benign redefinition of type
#pragma warning( disable: 4244 ) // C4244: conversion from 'type' to 'type', possible loss of data

///////////////////////////////////////////////////////////////////////

#ifdef                  _MAX_PATH
  #define   PATH_MAX    _MAX_PATH
#else
  #ifdef                FILENAME_MAX
    #define PATH_MAX    FILENAME_MAX
  #else
    #define PATH_MAX    260
  #endif
#endif

struct dirent {
        long            d_ino;
        char            d_name[FILENAME_MAX + 1];
};

typedef unsigned __int32 in_addr_t;
typedef unsigned char   u_char;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
typedef unsigned __int8  u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
typedef signed __int8   int8_t;
typedef signed __int16  int16_t;
typedef signed __int32  int32_t;
typedef signed __int64  int64_t;
typedef int             ssize_t;
typedef int             pid_t;
typedef int             mode_t;

#include <io.h>
#include <process.h>
#include <signal.h>
#include <direct.h>

#define STDIN_FILENO    fileno(stdin)
#define STDOUT_FILENO   fileno(stdout)
#define STDERR_FILENO   fileno(stderr)

/* Bit settings for open() and stat() functions */
#define S_IRUSR         _S_IREAD
#define S_IWUSR         _S_IWRITE
#define S_IRGRP         _S_IREAD
#define S_ISREG(m)      (((m) & _S_IFMT) == _S_IFREG)
#define S_ISDIR(m)      (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISCHR(m)      (((m) & _S_IFMT) == _S_IFCHR)
#define S_ISFIFO(m)     (((m) & _S_IFMT) == _S_IFIFO)

/* Bit settings for access() function */
#define F_OK            0
#define W_OK            2
#define R_OK            4

#define strcasecmp      stricmp
#define strncasecmp     strnicmp

#define snprintf        _snprintf
#define vsnprintf       _vsnprintf
#define strerror        w32_strerror
#define strerror_r      w32_strerror_r

#define srandom         srand 
#define random          rand 

#define inline          __inline
#define __inline__      __inline

#define HAVE_STRUCT_IN_ADDR_S_ADDR
#define HAVE_U_INT
#define HAVE_U_INT8_T
#define HAVE_LIBMSVCRT
#define HAVE_SYS_MTIO_H         // (ours is called 'w32mtio.h')

#ifndef MAX_CPU_ENGINES
#define MAX_CPU_ENGINES  8
#endif

#define OPTION_CONFIG_SYMBOLS
#define OPTION_ENHANCED_CONFIG_INCLUDE

#if !( defined( OPTION_FTHREADS ) || defined( OPTION_WTHREADS ) )
#define OPTION_FTHREADS 
#endif

#define HAVE_STRSIGNAL

#if !defined(OPTION_NO_EXTERNAL_GUI)
#if !defined(EXTERNALGUI)
#define EXTERNALGUI
#endif
#else
#undef  EXTERNALGUI
#endif

#define NO_SETUID
#define NO_SIGABEND_HANDLER

#undef  NO_ATTR_REGPARM         // ( ATTR_REGPARM(x) == __fastcall )
#define HAVE_ATTR_REGPARM       // ( ATTR_REGPARM(x) == __fastcall )
#define C99_FLEXIBLE_ARRAYS     // ("DEVBLK *memdev[];" supported)

//#include "getopt.h"
#define HAVE_GETOPT_LONG

#include <math.h>
#define HAVE_SQRTL
#define HAVE_LDEXPL
#define HAVE_FABSL
#define HAVE_FMODL
#define HAVE_FREXPL

// The following are needed by hostopts.h...

#define HAVE_DECL_SIOCSIFNETMASK  1     // (manually defined in tuntap.h)
#define HAVE_DECL_SIOCSIFHWADDR   1     // (manually defined in tuntap.h)
#define HAVE_DECL_SIOCADDRT       0     // (unsupported by CTCI-W32)
#define HAVE_DECL_SIOCDELRT       0     // (unsupported by CTCI-W32)
#define HAVE_DECL_SIOCDIFADDR     0     // (unsupported by CTCI-W32)

// SCSI tape handling transparency/portability

#define HAVE_DECL_MTEOTWARN       1     // (always true since I made it up!)
#define HAVE_DECL_MTEWARN         1     // (same as HAVE_DECL_MTEOTWARN)

// GNUWin32 PCRE (Perl-Compatible Regular Expressions) support...

#if defined(HAVE_PCRE)
  // (earlier packages failed to define this so we must do so ourselves)
  #define  PCRE_DATA_SCOPE  
      // extern __declspec(dllimport)
  #include PCRE_INCNAME                 // (passed by makefile)
  #define  OPTION_HAO                   // Hercules Automatic Operator
#endif

#if defined( _WIN64 )
  #define  SIZEOF_INT_P       8
  #define  SIZEOF_SIZE_T      8
#else
  #define  SIZEOF_INT_P       4
  #define  SIZEOF_SIZE_T      4
#endif

#define DBGTRACE DebugTrace

inline void DebugTrace(char* fmt, ...)
{
    const int chunksize = 512;
    int buffsize = 0;
    char* buffer = NULL;
    int rc = -1;
    va_list args;
    va_start( args, fmt );
    do
    {
        if (buffer) free( buffer );
        buffsize += chunksize;
        buffer = malloc( buffsize + 1 );
        if (!buffer) __debugbreak();
        rc = _vsnprintf_s( buffer, buffsize+1, buffsize, fmt, args);
    }
    while (rc < 0 || rc >= buffsize);
    OutputDebugStringA( buffer );
    free( buffer );
    va_end( args );
}

#endif /*!defined(_HERCWIND_H)*/
