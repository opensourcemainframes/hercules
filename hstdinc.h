/* HSTDINC.H    (c) Copyright Roger Bowler, 1999-2007                */
/*              Hercules precompilation-eligible Header Files        */

/* This file contains #include statements for all of the header      */
/* files which are not dependent on the mainframe architectural      */
/* features selected and thus are eligible for precompilation        */

// $Id$
//
// $Log$
// Revision 1.14  2007/01/10 16:30:48  rbowler
// Include netinet/tcp.h for Unix keepalive settings
//
// Revision 1.13  2007/01/10 09:32:39  fish
// Enable connection keep-alive to try and detect 3270 clients that
// have died (MSVC only right now; don't know how to do it on *nix)
//
// Revision 1.12  2006/12/08 09:43:28  jj
// Add CVS message log
//

#ifndef _HSTDINC_H
#define _HSTDINC_H

#ifdef HAVE_CONFIG_H
  #include <config.h> // Hercules build configuration options/settings
#endif

///////////////////////////////////////////////////////////////////////
// Required and optional SYSTEM headers...
//////////////////////////////////////////////////////////////////////

#define _REENTRANT    /* Ensure that reentrant code is generated *JJ */
#define _THREAD_SAFE            /* Some systems use this instead *JJ */

#if defined(HAVE_STRSIGNAL) && defined(__GNUC__) && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE                 /* required by strsignal() *JJ */
#endif

/* Required headers  --  These we ALWAYS need to have... */

#ifdef _MSVC_
  // The following ensures certain functions get defined...
  // (such as TryEnterCriticalSection and InitializeCriticalSectionAndSpinCount)
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0403  // Windows 98 or WinNT SP4 or greater
  #endif
  #include <winsock2.h>         // Windows Sockets 2
  #include <mstcpip.h>          // (need struct tcp_keepalive)
#endif
#ifdef WIN32
  #include <windows.h>
#endif
#ifdef _MSVC_
  #include <tchar.h>
  #include <wincon.h>
  #include <conio.h>
  #include <io.h>
  #include <lmcons.h>
  #include <tlhelp32.h>
  #include <dbghelp.h>
  #include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#if !defined(_MSVC_)
  #include <sys/time.h>
  #include <sys/ioctl.h>
#endif
#include <sys/types.h>

/* Optional headers  --  These we can live without */

/* PROGRAMMING NOTE: On Darwin, <sys/socket.h> must be included before
   <net/if.h>, and on older Darwin systems, before <net/route.h> and
   <netinet/in.h> */
#ifdef HAVE_SYS_SOCKET_H
  #include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
  #include <arpa/inet.h>
#endif
#if defined(BUILD_HERCIFC)
  #ifdef HAVE_LINUX_IF_TUN_H
    #include <linux/if_tun.h>
  #endif
  #ifdef HAVE_NET_ROUTE_H
    #include <net/route.h>
  #endif
#endif
// (just make it easier to #include hercifc.h)
#if defined(BUILD_HERCIFC) || defined(_MSVC_) || !defined(HAVE_LINUX_IF_TUN_H) || !defined(HAVE_NET_IF_H)
  #define NEED_HERCIFC_H
#endif
#ifdef HAVE_NET_IF_H
  #include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
  #include <netinet/tcp.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
  #include <sys/mman.h>
#endif
#ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
#endif
#ifdef HAVE_SYS_MTIO_H
  #include <sys/mtio.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
  #include <sys/resource.h>
#endif
#ifdef HAVE_SYS_UN_H
  #include <sys/un.h>
#endif
#ifdef HAVE_SYS_UIO_H
  #include <sys/uio.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
  #include <sys/utsname.h>
#endif
#ifdef HAVE_SYS_WAIT_H
  #include <sys/wait.h>
#endif
#ifdef HAVE_BYTESWAP_H
 #ifndef NO_ASM_BYTESWAP
  #include <byteswap.h>
 #endif
#endif
#ifdef HAVE_BZLIB_H
  // windows.h #defines 'small' as char and bzlib.h
  // uses it for a variable name so we must #undef.
  #if defined(__CYGWIN__)
    #undef small
  #endif
  #include <bzlib.h>
  /* ISW 20050427 : CCKD_BZIP2/HET_BZIP2 are usually */
  /* controlled by config.h (automagic). If config.h */
  /* is not present however, then define them here.  */
  #if !defined(HAVE_CONFIG_H)
    #define CCKD_BZIP2
    #define HET_BZIP2
  #endif
#endif
#ifdef HAVE_DIRENT_H
  #include <dirent.h>
#endif
#ifdef OPTION_DYNAMIC_LOAD
  #ifdef HDL_USE_LIBTOOL
    #include <ltdl.h>
  #else
    #if defined(__MINGW__) || defined(_MSVC_)
      #include "w32dl.h"
    #else
      #include <dlfcn.h>
    #endif
  #endif
#endif
#ifdef HAVE_FENV_H
  #include <fenv.h>
#endif
#ifdef HAVE_ICONV
  #include <iconv.h>
#endif
#ifdef ENABLE_NLS
  #ifdef HAVE_LOCALE_H
    #include <locale.h>
  #else
    #error ENABLE_NLS requires <locale.h>
  #endif
  #ifdef HAVE_LIBINTL_H
    #include <libintl.h>
  #else
    #error ENABLE_NLS requires <libintl.h>
  #endif
#endif
#ifdef HAVE_INTTYPES_H
  #include <inttypes.h>
#endif
#ifdef HAVE_MALLOC_H
  #include <malloc.h>
#endif
#ifdef HAVE_MATH_H
  #include <math.h>
#endif
#ifdef HAVE_NETDB_H
  #include <netdb.h>
#endif
#ifdef HAVE_PWD_H
  #include <pwd.h>
#endif
#ifdef HAVE_REGEX_H
  #include <regex.h>
#endif
#ifdef HAVE_SCHED_H
  #include <sched.h>
#endif
#ifdef HAVE_SIGNAL_H
  #include <signal.h>
#endif
#ifdef HAVE_TIME_H
  #include <time.h>
#endif
#ifdef HAVE_TERMIOS_H
  #include <termios.h>
#endif
#ifdef HAVE_ZLIB_H
  #include <zlib.h>
#endif

// Some Hercules specific files, NOT guest arch dependent
#if defined(_MSVC_)
  #include "hercwind.h"         // Hercules definitions for Windows
#else
  #include <unistd.h>           // Unix standard definitions
#endif

#include "hostopts.h"           // Must come before htypes.h
#include "htypes.h"             // Hercules-wide data types

#endif // _HSTDINC_H
