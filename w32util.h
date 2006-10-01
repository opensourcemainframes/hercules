//////////////////////////////////////////////////////////////////////////////////////////
//   w32util.h        Windows porting functions
//////////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2005-2006. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef _W32UTIL_H
#define _W32UTIL_H

#if defined( _MSVC_ )

#include "hercules.h"

#ifndef _W32UTIL_C_
  #ifndef _HUTIL_DLL_
    #define W32_DLL_IMPORT  DLL_IMPORT
  #else
    #define W32_DLL_IMPORT  extern
  #endif
#else
  #define   W32_DLL_IMPORT  DLL_EXPORT
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Translates a Win32 '[WSA]GetLastError()' value into a 'errno' value (if possible
// and/or if needed) that can then be used in the below 'w32_strerror' string function...

W32_DLL_IMPORT  int  w32_trans_w32error( const DWORD dwLastError );

//////////////////////////////////////////////////////////////////////////////////////////
// ("unsafe" version -- use "safer" 'w32_strerror_r' instead if possible)

W32_DLL_IMPORT  char*  w32_strerror( int errnum );

//////////////////////////////////////////////////////////////////////////////////////////
// Handles both regular 'errno' values as well as [WSA]GetLastError() values too...

W32_DLL_IMPORT int w32_strerror_r( int errnum, char* buffer, size_t buffsize );

//////////////////////////////////////////////////////////////////////////////////////////
// Return Win32 error message text associated with an error number value
// as returned by a call to either GetLastError() or WSAGetLastError()...

W32_DLL_IMPORT  char*  w32_w32errmsg( int errnum, char* pszBuffer, size_t nBuffSize );

//////////////////////////////////////////////////////////////////////////////////////////
// Large File Support...

#if (_MSC_VER < 1400)
  W32_DLL_IMPORT  __int64  w32_ftelli64 ( FILE* stream );
  W32_DLL_IMPORT    int    w32_fseeki64 ( FILE* stream, __int64 offset, int origin );
  W32_DLL_IMPORT    int    w32_ftrunc64 ( int fd, __int64 new_size );
#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_SOCKETPAIR )
  W32_DLL_IMPORT int socketpair( int domain, int type, int protocol, int socket_vector[2] );
#endif

#if !defined( HAVE_FORK )
  W32_DLL_IMPORT pid_t  fork( void );
#endif

#if !defined( HAVE_STRTOK_R )
W32_DLL_IMPORT char* strtok_r ( char* s, const char* sep, char** lasts);
#endif

#if !defined( HAVE_GETTIMEOFDAY )
  W32_DLL_IMPORT int gettimeofday ( struct timeval* pTV, void* pTZ);
#endif

#if !defined( HAVE_NANOSLEEP )
  W32_DLL_IMPORT int nanosleep ( const struct timespec* rqtp, struct timespec* rmtp );
#endif

#if !defined( HAVE_USLEEP )
  W32_DLL_IMPORT int usleep ( useconds_t  useconds );
#endif

// Can't use "HAVE_SLEEP" since Win32's "Sleep" causes HAVE_SLEEP to
// be erroneously #defined due to autoconf AC_CHECK_FUNCS case issues...

//#if !defined( HAVE_SLEEP )
  W32_DLL_IMPORT unsigned sleep ( unsigned seconds );
//#endif

#if !defined( HAVE_SCHED_YIELD )
  W32_DLL_IMPORT int sched_yield ( void );
#endif

#if !defined( HAVE_GETPGRP )
  #define  getpgrp  getpid
#endif

#if !defined( HAVE_SCANDIR )
  W32_DLL_IMPORT int scandir
  (
    const char *dir,
    struct dirent ***namelist,
    int (*filter)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **)
  );
#endif

#if !defined( HAVE_ALPHASORT )
  W32_DLL_IMPORT int alphasort ( const struct dirent **a, const struct dirent **b );
#endif

#if !defined(HAVE_SYS_RESOURCE_H)
  // Note: we only provide the absolute minimum required information
  #define  RUSAGE_SELF       0      // Current process
  #define  RUSAGE_CHILDREN  -1      // Children of the current process
  struct rusage                     // Resource utilization information
  {
    struct timeval  ru_utime;       // User time used
    struct timeval  ru_stime;       // System time used
  };
  W32_DLL_IMPORT int getrusage ( int who, struct rusage* r_usage );
#endif

#if !defined(HAVE_DECL_LOGIN_NAME_MAX) || !HAVE_DECL_LOGIN_NAME_MAX
  #define  LOGIN_NAME_MAX  UNLEN
#endif

#if !defined( HAVE_GETLOGIN )
  W32_DLL_IMPORT char* getlogin ( void );
#endif

#if !defined( HAVE_GETLOGIN_R )
  W32_DLL_IMPORT int getlogin_r ( char* name, size_t namesize );
#endif

#if !defined( HAVE_REALPATH )
  W32_DLL_IMPORT char* realpath ( const char* file_name, char* resolved_name );
#endif

// The inet_aton() function converts the specified string,
// in the Internet standard dot notation, to a network address,
// and stores the address in the structure provided.
//
// The inet_aton() function returns 1 if the address is successfully converted,
// or 0 if the conversion failed.

#if !defined( HAVE_INET_ATON )
  W32_DLL_IMPORT int inet_aton( const char* cp, struct in_addr* addr );
#endif

// Returns outpath as a host filesystem compatible filename path.
// This is a Cygwin-to-MSVC transitional period helper function.
// On non-Windows platforms it simply copies inpath to outpath.
// On Windows it converts inpath of the form "/cygdrive/x/foo.bar"
// to outpath in the form "x:/foo.bar" for Windows compatibility.
W32_DLL_IMPORT BYTE *hostpath( BYTE *outpath, const BYTE *inpath, size_t buffsize );

// Poor man's  "fcntl( fd, F_GETFL )"...
// (only returns access-mode flags and not any others)
W32_DLL_IMPORT int get_file_accmode_flags( int fd );

// Initialize/Deinitialize sockets package...
W32_DLL_IMPORT int  socket_init   ( void );
W32_DLL_IMPORT int  socket_deinit ( void );

// Set socket to blocking or non-blocking mode...
W32_DLL_IMPORT int socket_set_blocking_mode( int sfd, int blocking_mode );

// Determine whether a file descriptor is a socket or not...
// (returns 1==true if it's a socket, 0==false otherwise)
W32_DLL_IMPORT int socket_is_socket( int sfd );

// Retrieve directory where process was loaded from...
// (returns >0 == success, 0 == failure)
W32_DLL_IMPORT int get_process_directory( char* dirbuf, size_t bufsiz );

// Expand environment variables... (e.g. %SystemRoot%, etc); 0==success
W32_DLL_IMPORT int expand_environ_vars( const char* inbuff, char* outbuff, size_t outbufsiz );

// Initialize Hercules HOSTINFO structure
W32_DLL_IMPORT void w32_init_hostinfo( HOST_INFO* pHostInfo );

W32_DLL_IMPORT int   w32_socket   ( int af, int type, int protocol );
W32_DLL_IMPORT void  w32_FD_SET   ( int fd, fd_set* pSet );
W32_DLL_IMPORT int   w32_FD_ISSET ( int fd, fd_set* pSet );
W32_DLL_IMPORT int   w32_select   ( int nfds,
                     fd_set* pReadSet,
                     fd_set* pWriteSet,
                     fd_set* pExceptSet,
                     const struct timeval* pTimeVal,
                     const char* pszSourceFile,
                     int nLineNumber
                   );

W32_DLL_IMPORT FILE*  w32_fdopen ( int their_fd, const char* their_mode );
W32_DLL_IMPORT size_t w32_fwrite ( const void* buff, size_t size, size_t count, FILE* stream );
W32_DLL_IMPORT int    w32_fprintf( FILE* stream, const char* format, ... );
W32_DLL_IMPORT int    w32_fclose ( FILE* stream );
W32_DLL_IMPORT int    w32_get_stdin_char ( char* pCharBuff, size_t wait_millisecs );
W32_DLL_IMPORT pid_t  w32_poor_mans_fork ( char*  pszCommandLine, int* pnWriteToChildStdinFD );
W32_DLL_IMPORT void   w32_set_thread_name( TID tid, char* name );

//////////////////////////////////////////////////////////////////////////////////////////

#endif // defined(_MSVC_)

//////////////////////////////////////////////////////////////////////////////////////////
// Support for disabling of CRT Invalid Parameter Handler...

#if defined( _MSVC_ ) && defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

#define DISABLE_CRT_INVALID_PARAMETER_HANDLER()   DisableInvalidParameterHandling()
#define ENABLE_CRT_INVALID_PARAMETER_HANDLING()   EnableInvalidParameterHandling()

W32_DLL_IMPORT  void  DisableInvalidParameterHandling();
W32_DLL_IMPORT  void  EnableInvalidParameterHandling();

#else // !defined( _MSVC_ ) || !defined( _MSC_VER ) || ( _MSC_VER < 1400 )

#define DISABLE_CRT_INVALID_PARAMETER_HANDLER()   /* (no nothing) */
#define ENABLE_CRT_INVALID_PARAMETER_HANDLING()   /* (no nothing) */

#endif // defined( _MSVC_ ) && defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

//////////////////////////////////////////////////////////////////////////////////////////

#endif // _W32UTIL_H
