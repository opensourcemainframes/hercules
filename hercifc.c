// ====================================================================
// Hercules Interface Configuration Program
// ====================================================================
//
// Copyright    (C) Copyright Roger Bowler, 2000-2004
//              (C) Copyright James A. Pierson, 2002-2004
//
// Based on code originally written by Roger Bowler
// Modified to communicate via unix sockets.
//
// This module configures the TUN/TAP interface for Hercules.
// It is invoked as a setuid root program by tuntap.c
//
// The are no command line arguments anymore.
//
// Error messages are written to stderr, which is redirected to 
// the Hercules message log by ctcadpt.c
//
// The exit status is zero if successful, non-zero if error.
//

#include "hercules.h"
#include "hercifc.h"

/* ISW 02/20/2003 - Proposed patch ISW20032002-2
   define dummy 'extgui' so that hercules can be defined on
   a build with --enable-external-gui
   extgui is set to 0 regardless of the environment
   (i.e. there is no EXTERNALGUI argument to hercifc
*/
#ifdef EXTERNALGUI
int   extgui=0;
#endif
/* End of ISW20032002-2 */

// --------------------------------------------------------------------
// HERCIFC program entry point
// --------------------------------------------------------------------

int main( int argc, char **argv )
{
    char*       pszProgName  = NULL;    // Name of this program
    char*       pOp          = NULL;    // Operation text
    char*       pIF          = NULL;    // -> interface name
    void*       pArg         = NULL;    // -> ifreq or rtentry 
    CTLREQ      ctlreq;                 // Request Buffer
    int         sockfd;                 // Socket descriptor
    int         rc;                     // Return code
    char        szMsgBuffer[255];

    UNREFERENCED( argc );

    pszProgName = strdup( argv[0] );

    // Must not be run from the commandline
    if( isatty( STDIN_FILENO ) )
    {
        fprintf( stderr,
                 _("HHCIF001E %s: Must be called from within Hercules.\n"),
                 pszProgName );
        exit( 1 );
    }
        
    // Obtain a socket for ioctl operations
    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );

    if( sockfd < 0 )
    {
        fprintf( stderr,
                 _("HHCIF002E %s: Cannot obtain socket: %s\n"),
                 pszProgName, strerror( errno ) );
        exit( 2 );
    }

    // Process ioctl messages from Hercules
    while( 1 )
    {
        rc = read( STDIN_FILENO, 
                   &ctlreq, 
                   CTLREQ_SIZE );
        
        if( rc == -1 )
        {
            fprintf( stderr,
                     _("HHCIF003E %s: I/O error on read: %s.\n"),
                     pszProgName, strerror( errno ) );
            exit( 3 );
        }

        switch( ctlreq.iCtlOp )
        {
        case SIOCSIFADDR:
            pOp  = "SIOCSIFADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFDSTADDR:
            pOp  = "SIOCSIFDSTADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFFLAGS:
            pOp  = "SIOCSIFFLAGS";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFMTU:
            pOp  = "SIOCSIFMTU";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCADDMULTI:
            pOp  = "SIOCADDMULTI";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCDELMULTI:
            pOp  = "SIOCDELMULTI";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

#if !defined(__APPLE__)
        case SIOCSIFNETMASK:
            pOp  = "SIOCSIFNETMASK";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFHWADDR:
            pOp  = "SIOCSIFHWADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCADDRT:
            pOp  = "SIOCADDRT";
            pArg = &ctlreq.iru.rtentry;
            pIF  = ctlreq.szIFName;
            ctlreq.iru.rtentry.rt_dev = ctlreq.szIFName;
            break;

        case SIOCDELRT:
            pOp  = "SIOCDELRT";
            pArg = &ctlreq.iru.rtentry;
            pIF  = ctlreq.szIFName;
            ctlreq.iru.rtentry.rt_dev = ctlreq.szIFName;
            break;
#endif /* !defined(__APPLE__) */

        case CTLREQ_OP_DONE:
            close( STDIN_FILENO  );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );
            exit( 0 );

        default:
            sprintf( szMsgBuffer,
                     _("HHCIF004W %s: Unknown request: %8.8lX.\n"),
                     pszProgName, ctlreq.iCtlOp );
            
            write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );

            continue;
        }
            
        sprintf( szMsgBuffer,
                 _("HHCIF006I %s: Doing %s on %s\n"),
                 pszProgName, pOp, pIF);

        write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );
    
        rc = ioctl( sockfd, ctlreq.iCtlOp, pArg );

        if( rc < 0 )
        {
            sprintf( szMsgBuffer,
                     _("HHCIF005E %s: ioctl error doing %s on %s: %s\n"),
                     pszProgName, pOp, pIF, strerror( errno ) );
            
            write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );
        }
    }

    // Never reached.
    return 0;
}



