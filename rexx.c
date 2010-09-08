/* REXX.C       (c)Copyright Jan Jaeger, 2010                        */
/*              REXX Interpreter Support                             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _REXX_C_
#define _HENGINE_DLL_

#include "hercules.h"

#if defined(HAVE_REGINA_REXXSAA_H)

#define INCL_REXXSAA

#if defined( _MSVC_ )
#include "rexxsaa.h"
#else
#include <regina/rexxsaa.h>
#endif

#define hSubcom "HERCULES"

static int rexx_initialised = FALSE;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)

#if defined ( _MSVC_ )
#define REGINA_LIBRARY "regina.dll"
#else
#define REGINA_LIBRARY "libregina.so"
#endif

typedef APIRET APIENTRY rRexxStart( LONG, PRXSTRING, PCSZ, PRXSTRING, PCSZ, LONG, PRXSYSEXIT, PSHORT, PRXSTRING ) ;
typedef APIRET APIENTRY rRexxRegisterSubcomExe( PCSZ, RexxSubcomHandler *, PUCHAR ) ; 

static rRexxRegisterSubcomExe *hRexxRegisterSubcomExe = NULL;
static rRexxStart             *hRexxStart = NULL;

#else

#define hRexxRegisterSubcomExe RexxRegisterSubcomExe
#define hRexxStart             RexxStart

#endif

RexxSubcomHandler *hSubCmd( PRXSTRING command, PUSHORT flags, PRXSTRING retval ) 
{
SHORT rc;

    if( (rc = ProcessPanelCommand(command[0].strptr)) )
        *flags = RXSUBCOM_ERROR;
    else
        *flags = RXSUBCOM_OK;

    sprintf(retval->strptr,"%hd",rc);
    retval->strlength = strlen(retval->strptr);

    return 0;
}

int init_rexx()
{
    if( rexx_initialised )
        return 0;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)
    {
        void *addr;

        if(!(addr = dlopen(REGINA_LIBRARY,RTLD_LAZY)))
            return -1;
        if(!(hRexxRegisterSubcomExe = (rRexxRegisterSubcomExe *)dlsym(addr, "RexxRegisterSubcomExe")))
            return -1;
        if(!(hRexxStart = (rRexxStart *)dlsym(addr,"RexxStart")))
            return -1;
        if(
#if defined ( _MSVC_ )
        putenv( "REGINA_OPTIONS=STDOUT_FOR_STDERR" )
#else
        setenv( "REGINA_OPTIONS", "STDOUT_FOR_STDERR", TRUE )
#endif
            )
            return -1;
    }
#endif

    if(hRexxRegisterSubcomExe( hSubcom, (RexxSubcomHandler *)hSubCmd, NULL) != RXSUBCOM_OK)
        return -1;

//  ZZFIXME:  Need to add RXSIO I/O Exit to handle trace and stack reads
//  RexxRegisterExitExe( hSubcom, hSubExit, NULL);
//  

    rexx_initialised = TRUE;

    return 0;
}

/*-------------------------------------------------------------------*/
/* exec command - execute a rexx script                              */
/*-------------------------------------------------------------------*/
int exec_cmd(int argc, char *argv[],char *cmdline)
{
    SHORT rc;
    RXSTRING retval;
    char buffer[250];
    RXSTRING arg;

    UNREFERENCED(cmdline);

    if(argc < 2)
    {
        logmsg("no exec\n");
        return -1;
    }

    if(init_rexx())
    {
        logmsg("REXX(Regina) Initialisation failed\n");
        return -1;
    }

    if ( argc > 2 )
    {
        int i,l=0;

        for ( i = 2; i < argc; i++ )
            l += (int)strlen( (char *)argv[i] );

        arg.strptr = (char *)malloc( l + argc );
        strcpy( arg.strptr, "" );
        for ( i = 2; i < argc; i++ )
        {
            strcat( arg.strptr, argv[i] );
            if ( i != argc - 1 )
                strcat( arg.strptr, " " );
        }
        arg.strlength = argc + l - 3;
    }
    else
        MAKERXSTRING(arg, NULL, 0);
    
    MAKERXSTRING(retval, buffer, sizeof(buffer));

    hRexxStart ((argc > 2) ? 1 : 0, &arg, argv[1], NULL, hSubcom, RXCOMMAND, 0, &rc, &retval );

    if(arg.strptr)
        free(arg.strptr);

    return rc;
}
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/
