/* VERSION.C    (c) Copyright Roger Bowler, 1999-2009                */
/*              Hercules Version Display Module                      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name, version, build    */
/* date and time, and copyright notice to the indicated file.        */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.52  2009/01/23 13:12:19  bernard
// copyright notice
//
// Revision 1.51  2009/01/14 15:23:20  jj
// Move modpath logic to hsccmd.c
//
// Revision 1.50  2007/12/07 12:50:22  rbowler
// Show multi_byte assist status at startup
//
// Revision 1.49  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.48  2006/12/08 09:43:31  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _VERSION_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "machdep.h"

/*--------------------------------------------------*/
/*   "Unusual" (i.e. noteworthy) build options...   */
/*--------------------------------------------------*/

static const char *build_info[] = {

#if defined(_MSVC_)
    "Windows (MSVC) "
  #if defined(DEBUG)
    "** DEBUG ** "
  #endif
    "build for " MSTRING(HOST_ARCH)
  #if defined(CUSTOM_BUILD_STRING)
    ": \"" CUSTOM_BUILD_STRING "\""
  #endif
    ,
#elif defined(CUSTOM_BUILD_STRING)
    CUSTOM_BUILD_STRING,
#endif

#if !defined(_ARCHMODE2)
    "Mode:"
#else
    "Modes:"
#endif
#if defined(_370)
    " " _ARCH_370_NAME
#endif
#if defined(_390)
    " " _ARCH_390_NAME
#endif
#if defined(_900)
    " " _ARCH_900_NAME
#endif
    ,

    "Max CPU Engines: " MSTRING(MAX_CPU_ENGINES),

#if !defined(_MSVC_)
  #if defined(NO_SETUID)
    "No setuid support",
  #else
    "Using "
    #if defined(HAVE_SETRESUID)
      "setresuid()"
    #elif defined(HAVE_SETREUID)
      "setreuid()"
    #else
      "(UNKNOWN)"
    #endif
    " for setting privileges",
  #endif
#endif

#if defined(OPTION_FTHREADS)
    "Using fthreads instead of pthreads",
#endif
#if defined(OPTION_DYNAMIC_LOAD)
    "Dynamic loading support",
#else
    "No Dynamic loading support",
#endif
#if defined(HDL_BUILD_SHARED)
    "Using shared libraries",
#else
    "Using static libraries",
#endif

#if !defined(EXTERNALGUI)
    "No External GUI support",
#endif

#if defined(OPTION_HTTP_SERVER)
    "HTTP Server support",
#if defined(PKGDATADIR) && defined(DEBUG)
    "HTTP document default root directory is "PKGDATADIR,
#endif
#endif

#if defined(NO_IEEE_SUPPORT)
    "No IEEE support",
#else
    #if !defined(HAVE_SQRTL)
        "No sqrtl support",
    #endif
#endif

#if defined(NO_SIGABEND_HANDLER)
    "No SIGABEND handler",
#endif

#if !defined(CCKD_BZIP2)
    "No CCKD BZIP2 support",
#endif

#if !defined(HAVE_LIBZ)
    "No ZLIB support",
#endif

#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)
    "Regular Expressions support",
#endif

#if defined(OPTION_HAO)
    "Automatic Operator support",
#endif

#if !defined(HET_BZIP2)
    "No HET BZIP2 support",
#endif

#if defined(ENABLE_NLS)
    "National Language Support",
#endif

    "Machine dependent assists:"
#if !defined( ASSIST_CMPXCHG1  ) \
 && !defined( ASSIST_CMPXCHG4  ) \
 && !defined( ASSIST_CMPXCHG8  ) \
 && !defined( ASSIST_CMPXCHG16 ) \
 && !defined( ASSIST_FETCH_DW  ) \
 && !defined( ASSIST_STORE_DW  ) \
 && !defined( MULTI_BYTE_ASSIST)
    " (none)",
#else
  #if defined( ASSIST_CMPXCHG1 )
                    " cmpxchg1"
  #endif
  #if defined( ASSIST_CMPXCHG4 )
                    " cmpxchg4"
  #endif
  #if defined( ASSIST_CMPXCHG8 )
                    " cmpxchg8"
  #endif
  #if defined( ASSIST_CMPXCHG16 )
                    " cmpxchg16"
  #endif
  #if defined( ASSIST_FETCH_DW )
                    " fetch_dw"
  #endif
  #if defined( ASSIST_STORE_DW )
                    " store_dw"
  #endif
  #if defined( MULTI_BYTE_ASSIST )
                    " multi_byte"
  #endif
    ,
#endif

};

/*-------------------------------------------------------------------*/
/* Retrieve ptr to build information strings array...                */
/*         (returns #of entries in array)                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  get_buildinfo_strings(const char*** pppszBldInfoStr)
{
    if (!pppszBldInfoStr) return 0;
    *pppszBldInfoStr = build_info;
    return ( sizeof(build_info) / sizeof(build_info[0]) );
}


/*-------------------------------------------------------------------*/
/* Display version and copyright                                     */
/*-------------------------------------------------------------------*/
DLL_EXPORT void display_version_2 (FILE *f, char *prog, const char verbose,int httpfd)
{
    unsigned int i;
    const char** ppszBldInfoStr = NULL;

#if defined(EXTERNALGUI)
    /* If external gui being used, set stdout & stderr streams
       to unbuffered so we don't have to flush them all the time
       in order to ensure consistent sequence of log messages.
    */
    if (extgui)
    {
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
#endif /*EXTERNALGUI*/

        /* Log version */

    if ( f != stdout )
        if(httpfd<0)
            fprintf (f, MSG(HHC01413, "I", prog, VERSION));
        else
            hprintf (httpfd, MSG(HHC01413, "I", prog, VERSION));
    else
        WRMSG (HHC01413, "I", prog, VERSION);

    /* Log Copyright */

    if ( f != stdout )
        if(httpfd<0)
            fprintf (f, MSG(HHC01414, "I", HERCULES_COPYRIGHT));
        else
            hprintf (httpfd, MSG(HHC01414, "I", HERCULES_COPYRIGHT));
    else
        WRMSG (HHC01414, "I", HERCULES_COPYRIGHT);

    /* If we're being verbose, display the rest of the info */
    if (verbose)
    {
        /* Log build date/time */

        if ( f != stdout )
            if(httpfd<0)
                fprintf (f, MSG(HHC01415, "I", __DATE__, __TIME__));
            else
                hprintf (httpfd, MSG(HHC01415, "I", __DATE__, __TIME__));
        else
            WRMSG (HHC01415, "I", __DATE__, __TIME__);

        /* Log "unusual" build options */

        if ( f != stdout )
            if(httpfd<0)
                fprintf (f, MSG(HHC01416, "I"));
            else
                hprintf (httpfd, MSG(HHC01416, "I"));
        else
            WRMSG (HHC01416, "I");

        if (!(i = get_buildinfo_strings( &ppszBldInfoStr )))
        {
            if ( f != stdout )
                if(httpfd<0)
                    fprintf (f, MSG(HHC01417, "I", "(none)"));
                else
                    hprintf (httpfd, MSG(HHC01417, "I", "(none)"));
            else
                WRMSG (HHC01417, "I", "(none)");
        }
        else
        {
            for(; i; i--, ppszBldInfoStr++ )
            {
                if ( f != stdout )
                    if(httpfd<0)
                        fprintf (f, MSG(HHC01417, "I", *ppszBldInfoStr));
                    else
                        hprintf (httpfd, MSG(HHC01417, "I", *ppszBldInfoStr));
                else
                    WRMSG (HHC01417, "I", *ppszBldInfoStr);
            }
        }

        if(f != stdout)
            if(httpfd<0)
                display_hostinfo( &hostinfo, f, -1 );
            else
                display_hostinfo( &hostinfo, (FILE *)-1,httpfd );
        else
            display_hostinfo( &hostinfo, f, -1 );
    }

} /* end function display_version */

DLL_EXPORT void display_version(FILE *f,char *prog,const char verbose)
{
    display_version_2(f,prog,verbose,-1);
}
