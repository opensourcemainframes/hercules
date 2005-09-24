// Hercules Channel-to-Channel Emulation Support
// ====================================================================
//
// Copyright (C) James A. Pierson, 2002-2005
//               Roger Bowler, 2000-2005
//
// vmnet     (C) Copyright Willem Konynenberg, 2000-2005
// CTCT      (C) Copyright Vic Cross, 2001-2005
//
// Notes:
//   This module contains the remaining CTC emulation modes that
//   have not been moved to seperate modules. There is also logic
//   to allow old style 3088 device definitions for compatibility
//   and may be removed in a future release.
//
//   Please read README.NETWORKING for more info.
//
#include "hstdinc.h"

#define _CTCADPT_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "devtype.h"
#include "ctcadpt.h"

#include "opcode.h"
#include "devtype.h"

// ====================================================================
// Declarations
// ====================================================================

static int      CTCT_Init( DEVBLK *dev, int argc, char *argv[] );

static void     CTCT_Read( DEVBLK* pDEVBLK,   U16   sCount,
                           BYTE*   pIOBuf,    BYTE* pUnitStat,
                           U16*    pResidual, BYTE* pMore );

static void     CTCT_Write( DEVBLK* pDEVBLK,   U16   sCount,
                            BYTE*   pIOBuf,    BYTE* pUnitStat,
                            U16*    pResidual );

static void*    CTCT_ListenThread( void* argp );

static int      VMNET_Init( DEVBLK *dev, int argc, char *argv[] );

static int      VMNET_Write( DEVBLK *dev, BYTE *iobuf,
                             U16 count, BYTE *unitstat );

static int      VMNET_Read( DEVBLK *dev, BYTE *iobuf,
                            U16 count, BYTE *unitstat );

// --------------------------------------------------------------------
// Definitions for CTC general data blocks
// --------------------------------------------------------------------

typedef struct _CTCG_PARMBLK
{
    int                 listenfd;
    struct sockaddr_in  addr;
    DEVBLK*             dev;
}
CTCG_PARMBLK;

// --------------------------------------------------------------------
// Device Handler Information Block
// --------------------------------------------------------------------

DEVHND ctcadpt_device_hndinfo =
{
        &CTCX_Init,                    /* Device Initialisation      */
        &CTCX_ExecuteCCW,              /* Device CCW execute         */
        &CTCX_Close,                   /* Device Close               */
        &CTCX_Query,                   /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

DEVHND ctct_device_hndinfo =
{
        &CTCT_Init,                    /* Device Initialisation      */
        &CTCX_ExecuteCCW,              /* Device CCW execute         */
        &CTCX_Close,                   /* Device Close               */
        &CTCX_Query,                   /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

DEVHND vmnet_device_hndinfo =
{
        &VMNET_Init,                   /* Device Initialisation      */
        &CTCX_ExecuteCCW,              /* Device CCW execute         */
        &CTCX_Close,                   /* Device Close               */
        &CTCX_Query,                   /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

extern DEVHND ctci_device_hndinfo;
extern DEVHND lcs_device_hndinfo;

// ====================================================================
// Primary Module Entry Points
// ====================================================================

// --------------------------------------------------------------------
// Device Initialization Handler (Generic)
// --------------------------------------------------------------------

int  CTCX_Init( DEVBLK* pDEVBLK, int argc, char *argv[] )
{
    pDEVBLK->devtype = 0x3088;

    // The first argument is the device emulation type
    if( argc < 1 )
    {
        logmsg( _("HHCCT001E %4.4X: Incorrect number of parameters\n"),
            pDEVBLK->devnum );
        return -1;
    }

    if((pDEVBLK->hnd = hdl_ghnd(argv[0])))
    {
        if(pDEVBLK->hnd->init == &CTCX_Init)
            return -1;
        free(pDEVBLK->typname);
        pDEVBLK->typname = strdup(argv[0]);
        return (pDEVBLK->hnd->init)( pDEVBLK, --argc, ++argv );
    }
    logmsg (_("HHCCT034E %s: Unrecognized/unsupported CTC emulation type\n"),
        argv[0]);
    return -1;
}

// -------------------------------------------------------------------
// Query the device definition (Generic)
// -------------------------------------------------------------------

void  CTCX_Query( DEVBLK* pDEVBLK,
                  char**  ppszClass,
                  int     iBufLen,
                  char*   pBuffer )
{
    *ppszClass = "CTCA";
    snprintf( pBuffer, iBufLen, "%s", pDEVBLK->filename );
}

// -------------------------------------------------------------------
// Close the device (Generic)
// -------------------------------------------------------------------

int  CTCX_Close( DEVBLK* pDEVBLK )
{
    // Close the device file (if not already closed)
    if( pDEVBLK->fd >= 0 )
    {
        close( pDEVBLK->fd );
        pDEVBLK->fd = -1;           // indicate we're now closed
    }

    return 0;
}

// -------------------------------------------------------------------
// Execute a Channel Command Word (Generic)
// -------------------------------------------------------------------

void  CTCX_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                       BYTE    bFlags,  BYTE  bChained,
                       U16     sCount,  BYTE  bPrevCode,
                       int     iCCWSeq, BYTE* pIOBuf,
                       BYTE*   pMore,   BYTE* pUnitStat,
                       U16*    pResidual )
{
    int             iNum;               // Number of bytes to move
    BYTE            bOpCode;            // CCW opcode with modifier
                                        //   bits masked off

    UNREFERENCED( bFlags    );
    UNREFERENCED( bChained  );
    UNREFERENCED( bPrevCode );
    UNREFERENCED( iCCWSeq   );

    // Intervention required if the device file is not open
    if( pDEVBLK->fd < 0 &&
        !IS_CCW_SENSE( bCode ) &&
        !IS_CCW_CONTROL( bCode ) )
    {
        pDEVBLK->sense[0] = SENSE_IR;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    // Mask off the modifier bits in the CCW bOpCode
    if( ( bCode & 0x07 ) == 0x07 )
        bOpCode = 0x07;
    else if( ( bCode & 0x03 ) == 0x02 )
        bOpCode = 0x02;
    else if( ( bCode & 0x0F ) == 0x0C )
        bOpCode = 0x0C;
    else if( ( bCode & 0x03 ) == 0x01 )
        bOpCode = pDEVBLK->ctcxmode ? ( bCode & 0x83 ) : 0x01;
    else if( ( bCode & 0x1F ) == 0x14 )
        bOpCode = 0x14;
    else if( ( bCode & 0x47 ) == 0x03 )
        bOpCode = 0x03;
    else if( ( bCode & 0xC7 ) == 0x43 )
        bOpCode = 0x43;
    else
        bOpCode = bCode;

    // Process depending on CCW bOpCode
    switch (bOpCode)
    {
    case 0x01:  // 0MMMMM01  WRITE
        //------------------------------------------------------------
        // WRITE
        //------------------------------------------------------------

        // Return normal status if CCW count is zero
        if( sCount == 0 )
        {
            *pUnitStat = CSW_CE | CSW_DE;
            break;
        }

        // Write data and set unit status and residual byte count
        switch( pDEVBLK->ctctype )
        {
        case CTC_CTCT:
            CTCT_Write( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual );
            break;
        case CTC_VMNET:
            *pResidual = sCount - VMNET_Write( pDEVBLK, pIOBuf,
                                               sCount,  pUnitStat );
            break;
        }
        break;

    case 0x81:  // 1MMMMM01  WEOF
        //------------------------------------------------------------
        // WRITE EOF
        //------------------------------------------------------------

        // Return normal status
        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x02:  // MMMMMM10  READ
    case 0x0C:  // MMMM1100  RDBACK
        // -----------------------------------------------------------
        // READ & READ BACKWARDS
        // -----------------------------------------------------------

        // Read data and set unit status and residual byte count
        switch( pDEVBLK->ctctype )
        {
        case CTC_CTCT:
            CTCT_Read( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual, pMore );
            break;
        case CTC_VMNET:
            *pResidual = sCount - VMNET_Read( pDEVBLK, pIOBuf,
                                              sCount,  pUnitStat );
            break;
        }
        break;

    case 0x07:  // MMMMM111  CTL
        // -----------------------------------------------------------
        // CONTROL
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x03:  // M0MMM011  NOP
        // -----------------------------------------------------------
        // CONTROL NO-OPERATON
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x43:  // 00XXX011  SBM
        // -----------------------------------------------------------
        // SET BASIC MODE
        // -----------------------------------------------------------

        // Command reject if in basic mode
        if( pDEVBLK->ctcxmode == 0 )
        {
            pDEVBLK->sense[0] = SENSE_CR;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;

            break;
        }

        // Reset extended mode and return normal status
        pDEVBLK->ctcxmode = 0;

        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xC3:  // 11000011  SEM
        // -----------------------------------------------------------
        // SET EXTENDED MODE
        // -----------------------------------------------------------

        pDEVBLK->ctcxmode = 1;

        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xE3:  // 11100011
        // -----------------------------------------------------------
        // PREPARE (PREP)
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0x14:  // XXX10100  SCB
        // -----------------------------------------------------------
        // SENSE COMMAND BYTE
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x04:  // 00000100  SENSE
      // -----------------------------------------------------------
      // SENSE
      // -----------------------------------------------------------

        // Command reject if in basic mode
        if( pDEVBLK->ctcxmode == 0 )
        {
            pDEVBLK->sense[0] = SENSE_CR;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        // Calculate residual byte count
        iNum = ( sCount < pDEVBLK->numsense ) ?
            sCount : pDEVBLK->numsense;

        *pResidual = sCount - iNum;

        if( sCount < pDEVBLK->numsense )
            *pMore = 1;

        // Copy device sense bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->sense, iNum );

        // Clear the device sense bytes
        memset( pDEVBLK->sense, 0, sizeof( pDEVBLK->sense ) );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xE4:  //  11100100  SID
        // -----------------------------------------------------------
        // SENSE ID
        // -----------------------------------------------------------

        // Calculate residual byte count
        iNum = ( sCount < pDEVBLK->numdevid ) ?
            sCount : pDEVBLK->numdevid;

        *pResidual = sCount - iNum;

        if( sCount < pDEVBLK->numdevid )
            *pMore = 1;

        // Copy device identifier bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->devid, iNum );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    default:
        // ------------------------------------------------------------
        // INVALID OPERATION
        // ------------------------------------------------------------

        // Set command reject sense byte, and unit check status
        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
    }
}

// ====================================================================
// CTCT Support
// ====================================================================

//
// CTCT_Init
//

static int  CTCT_Init( DEVBLK *dev, int argc, char *argv[] )
{
    int            rc;                 // Return code
    int            mtu;                // MTU size (binary)
    int            lport;              // Listen port (binary)
    int            rport;              // Destination port (binary)
    char*          listenp;            // Listening port number
    char*          remotep;            // Destination port number
    char*          mtusize;            // MTU size (characters)
    char*          remaddr;            // Remote IP address
    struct in_addr ipaddr;             // Work area for IP address
    BYTE           c;                  // Character work area
    TID            tid;                // Thread ID for server
    CTCG_PARMBLK   parm;               // Parameters for the server
    char           address[20]="";     // temp space for IP address

    dev->devtype = 0x3088;

    dev->ctctype = CTC_CTCT;

    SetSIDInfo( dev, 0x3088, 0x08, 0x3088, 0x01 );

    // Check for correct number of arguments
    if (argc != 4)
    {
        logmsg( _("HHCCT002E %4.4X: Incorrect number of parameters\n"),
                dev->devnum );
        return -1;
    }

    // The first argument is the listening port number
    listenp = *argv++;

    if( strlen( listenp ) > 5 ||
        sscanf( listenp, "%u%c", &lport, &c ) != 1 ||
        lport < 1024 || lport > 65534 )
    {
        logmsg( _("HHCCT003E %4.4X: Invalid port number: %s\n"),
                dev->devnum, listenp );
        return -1;
    }

    // The second argument is the IP address or hostname of the
    // remote side of the point-to-point link
    remaddr = *argv++;

    if( inet_aton( remaddr, &ipaddr ) == 0 )
    {
        struct hostent *hp;

        if( ( hp = gethostbyname( remaddr ) ) != NULL )
        {
            memcpy( &ipaddr, hp->h_addr, hp->h_length );
            strcpy( address, inet_ntoa( ipaddr ) );
            remaddr = address;
        }
        else
        {
            logmsg( _("HHCCT004E %4.4X: Invalid IP address %s\n"),
                    dev->devnum, remaddr );
            return -1;
        }
    }

    // The third argument is the destination port number
    remotep = *argv++;

    if( strlen( remotep ) > 5 ||
        sscanf( remotep, "%u%c", &rport, &c ) != 1 ||
        rport < 1024 || rport > 65534 )
    {
        logmsg( _("HHCCT005E %4.4X: Invalid port number: %s\n"),
                dev->devnum, remotep );
        return -1;
    }

    // The fourth argument is the maximum transmission unit (MTU) size
    mtusize = *argv;

    if( strlen( mtusize ) > 5 ||
        sscanf( mtusize, "%u%c", &mtu, &c ) != 1 ||
        mtu < 46 || mtu > 65536 )
    {
        logmsg( _("HHCCT006E %4.4X: Invalid MTU size %s\n"),
                dev->devnum, mtusize );
        return -1;
    }

    // Set the device buffer size equal to the MTU size
    dev->bufsize = mtu;

    // Initialize the file descriptor for the socket connection

    // It's a little confusing, but we're using a couple of the
    // members of the server paramter structure to initiate the
    // outgoing connection.  Saves a couple of variable declarations,
    // though.  If we feel strongly about it, we can declare separate
    // variables...

    // make a TCP socket
    parm.listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    if( parm.listenfd < 0 )
    {
        logmsg( _("HHCCT007E %4.4X: Error creating socket: %s\n"),
                dev->devnum, strerror( HSO_errno ) );
        CTCX_Close( dev );
        return -1;
    }

    // bind socket to our local port
    // (might seem like overkill, and usually isn't done, but doing this
    // bind() to the local port we configure gives the other end a chance
    // at validating the connection request)
    memset( &(parm.addr), 0, sizeof( parm.addr ) );
    parm.addr.sin_family      = AF_INET;
    parm.addr.sin_port        = htons(lport);
    parm.addr.sin_addr.s_addr = htonl(INADDR_ANY);

    rc = bind( parm.listenfd,
               (struct sockaddr *)&parm.addr,
               sizeof( parm.addr ) );
    if( rc < 0 )
    {
        logmsg( _("HHCCT008E %4.4X: Error binding to socket: %s\n"),
                dev->devnum, strerror( HSO_errno ) );
        CTCX_Close( dev );
        return -1;
    }

    // initiate a connection to the other end
    memset( &(parm.addr), 0, sizeof( parm.addr ) );
    parm.addr.sin_family = AF_INET;
    parm.addr.sin_port   = htons(rport);
    parm.addr.sin_addr   = ipaddr;
    rc = connect( parm.listenfd,
                  (struct sockaddr *)&parm.addr,
                  sizeof( parm.addr ) );

    // if connection was not successful, start a server
    if( rc < 0 )
    {
        // used to pass parameters to the server thread
        CTCG_PARMBLK* arg;

        logmsg( _("HHCCT009I %4.4X: Connect to %s:%s failed, starting server\n"),
                dev->devnum, remaddr, remotep );

        // probably don't need to do this, not sure...
        close( parm.listenfd );

        parm.listenfd = socket( AF_INET, SOCK_STREAM, 0 );

        if( parm.listenfd < 0 )
        {
            logmsg( _("HHCCT010E %4.4X: Error creating socket: %s\n"),
                    dev->devnum, strerror( HSO_errno ) );
            CTCX_Close( dev );
            return -1;
        }

        // set up the listening port
        memset( &(parm.addr), 0, sizeof( parm.addr ) );

        parm.addr.sin_family      = AF_INET;
        parm.addr.sin_port        = htons(lport);
        parm.addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if( bind( parm.listenfd,
                  (struct sockaddr *)&parm.addr,
                  sizeof( parm.addr ) ) < 0 )
        {
            logmsg( _("HHCCT011E %4.4X: Error binding to socket: %s\n"),
                    dev->devnum, strerror( HSO_errno ) );
            CTCX_Close( dev );
            return -1;
        }

        if( listen( parm.listenfd, 1 ) < 0 )
        {
            logmsg( _("HHCCT012E %4.4X: Error on call to listen: %s\n"),
                    dev->devnum, strerror( HSO_errno ) );
            CTCX_Close( dev );
            return -1;
        }

        // we are listening, so create a thread to accept connection
        arg = malloc( sizeof( CTCG_PARMBLK ) );
        memcpy( arg, &parm, sizeof( parm ) );
        arg->dev = dev;
        create_thread( &tid, NULL, CTCT_ListenThread, arg );
    }
    else  // successfully connected (outbound) to the other end
    {
        logmsg( _("HHCCT013I %4.4X: Connected to %s:%s\n"),
                dev->devnum, remaddr, remotep );
        dev->fd = parm.listenfd;
    }

    // for cosmetics, since we are successfully connected or serving,
    // fill in some details for the panel.
    sprintf( dev->filename, "%s:%s", remaddr, remotep );

    return 0;
}

//
// CTCT_Write
//

static void  CTCT_Write( DEVBLK* pDEVBLK,   U16   sCount,
                         BYTE*   pIOBuf,    BYTE* pUnitStat,
                         U16*    pResidual )
{
    PCTCIHDR   pFrame;                  // -> Frame header
    PCTCISEG   pSegment;                // -> Segment in buffer
    U16        sOffset;                 // Offset of next frame
    U16        sSegLen;                 // Current segment length
    U16        sDataLen;                // Length of IP Frame data
    int        iPos;                    // Offset into buffer
    U16        i;                       // Array subscript
    int        rc;                      // Return code
    BYTE       szStackID[33];           // VSE IP stack identity
    U32        iStackCmd;               // VSE IP stack command

    // Check that CCW count is sufficient to contain block header
    if( sCount < sizeof( CTCIHDR ) )
    {
        logmsg( _("HHCCT014E %4.4X: Write CCW count %u is invalid\n"),
                pDEVBLK->devnum, sCount );

        pDEVBLK->sense[0] = SENSE_DC;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    // Fix-up frame pointer
    pFrame = (PCTCIHDR)pIOBuf;

    // Extract the frame length from the header
    FETCH_HW( sOffset, pFrame->hwOffset );


    // Check for special VSE TCP/IP stack command packet
    if( sOffset == 0 && sCount == 40 )
    {
        // Extract the 32-byte stack identity string
        for( i = 0;
             i < sizeof( szStackID ) - 1 && i < sCount - 4;
             i++)
            szStackID[i] = guest_to_host( pIOBuf[i+4] );

        szStackID[i] = '\0';

        // Extract the stack command word
        FETCH_FW( iStackCmd, *((FWORD*)&pIOBuf[36]) );

        // Display stack command and discard the packet
        logmsg( _("HHCCT015I %4.4X: Interface command: %s %8.8X\n"),
                pDEVBLK->devnum, szStackID, iStackCmd );

        *pUnitStat = CSW_CE | CSW_DE;
        *pResidual = 0;
        return;
    }

    // Check for special L/390 initialization packet
    if( sOffset == 0 )
    {
        // Return normal status and discard the packet
        *pUnitStat = CSW_CE | CSW_DE;
        *pResidual = 0;
        return;
    }

#if 0
    // Notes: It appears that TurboLinux has gotten sloppy in their
    //        ways. They are now giving us buffer sizes that are
    //        greater than the CCW count, but the segment size
    //        is within the count.
    // Check that the frame offset is valid
    if( sOffset < sizeof( CTCIHDR ) || sOffset > sCount )
    {
        logmsg( _("CTC101W %4.4X: Write buffer contains invalid "
                  "frame offset %u\n"),
                pDEVBLK->devnum, sOffset );

        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
        return;
    }
#endif

    // Adjust the residual byte count
    *pResidual -= sizeof( CTCIHDR );

    // Process each segment in the buffer
    for( iPos  = sizeof( CTCIHDR );
         iPos  < sOffset;
         iPos += sSegLen )
    {
        // Check that the segment is fully contained within the block
        if( iPos + sizeof( CTCISEG ) > sOffset )
        {
            logmsg( _("HHCCT016E %4.4X: Write buffer contains incomplete "
                      "segment header at offset %4.4X\n"),
                    pDEVBLK->devnum, iPos );

            pDEVBLK->sense[0] = SENSE_DC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Fix-up segment header in the I/O buffer
        pSegment = (PCTCISEG)(pIOBuf + iPos);

        // Extract the segment length from the segment header
        FETCH_HW( sSegLen, pSegment->hwLength );

        // Check that the segment length is valid
        if( ( sSegLen        < sizeof( CTCISEG ) ) ||
            ( iPos + sSegLen > sOffset           ) ||
            ( iPos + sSegLen > sCount            ) )
        {
            logmsg( _("HHCCT017E %4.4X: Write buffer contains invalid "
                    "segment length %u at offset %4.4X\n"),
                    pDEVBLK->devnum, sSegLen, iPos );

            pDEVBLK->sense[0] = SENSE_DC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Calculate length of IP frame data
        sDataLen = sSegLen - sizeof( CTCISEG );

        // Trace the IP packet before sending
        if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
        {
            logmsg( _("HHCCT018I %4.4X: Sending packet to %s:\n"),
                    pDEVBLK->devnum, pDEVBLK->filename );
            if( pDEVBLK->ccwtrace )
                packet_trace( pSegment->bData, sDataLen );
        }

        // Write the IP packet
        rc = write_socket( pDEVBLK->fd, pSegment->bData, sDataLen );

        if( rc < 0 )
        {
            logmsg( _("HHCCT019E %4.4X: Error writing to %s: %s\n"),
                    pDEVBLK->devnum, pDEVBLK->filename,
                    strerror( HSO_errno ) );

            pDEVBLK->sense[0] = SENSE_EC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Adjust the residual byte count
        *pResidual -= sSegLen;

        // We are done if current segment satisfies CCW count
        if( iPos + sSegLen == sCount )
        {
            *pResidual -= sSegLen;
            *pUnitStat = CSW_CE | CSW_DE;
            return;
        }
    }

    // Set unit status and residual byte count
    *pUnitStat = CSW_CE | CSW_DE;
    *pResidual = 0;
}

//
// CTCT_Read
//

static void  CTCT_Read( DEVBLK* pDEVBLK,   U16   sCount,
                        BYTE*   pIOBuf,    BYTE* pUnitStat,
                        U16*    pResidual, BYTE* pMore )
{
    PCTCIHDR    pFrame   = NULL;       // -> Frame header
    PCTCISEG    pSegment = NULL;       // -> Segment in buffer
    fd_set      rfds;                  // Read FD_SET
    int         iRetVal;               // Return code from 'select'
    ssize_t     iLength  = 0;

    static struct timeval tv;          // Timeout time for 'select'


    // Limit how long we should wait for data to come in
    FD_ZERO( &rfds );
    FD_SET( pDEVBLK->fd, &rfds );

    tv.tv_sec  = CTC_READ_TIMEOUT_SECS;
    tv.tv_usec = 0;

    iRetVal = select( pDEVBLK->fd + 1, &rfds, NULL, NULL, &tv );

    switch( iRetVal )
    {
    case 0:
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC | CSW_SM;
        pDEVBLK->sense[0] = 0;
        return;

    case -1:
        if( HSO_errno == HSO_EINTR )
            return;

        logmsg( _("HHCCT020E %4.4X: Error reading from %s: %s\n"),
                pDEVBLK->devnum, pDEVBLK->filename, strerror( HSO_errno ) );

        pDEVBLK->sense[0] = SENSE_EC;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        return;

    default:
        break;
    }

    // Read an IP packet from the TUN device
    iLength = read_socket( pDEVBLK->fd, pDEVBLK->buf, pDEVBLK->bufsize );

    // Check for other error condition
    if( iLength < 0 )
    {
        logmsg( _("HHCCT021E %4.4X: Error reading from %s: %s\n"),
                pDEVBLK->devnum, pDEVBLK->filename, strerror( HSO_errno ) );
        pDEVBLK->sense[0] = SENSE_EC;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    // Trace the packet received from the TUN device
    if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
    {
        logmsg( _("HHCCT022I %4.4X: Received packet from %s (%d bytes):\n"),
                pDEVBLK->devnum, pDEVBLK->filename, iLength );
        packet_trace( pDEVBLK->buf, iLength );
    }

    // Fix-up Frame pointer
    pFrame = (PCTCIHDR)pIOBuf;

    // Fix-up Segment pointer
    pSegment = (PCTCISEG)( pIOBuf + sizeof( CTCIHDR ) );

    // Initialize segment
    memset( pSegment, 0, iLength + sizeof( CTCISEG ) );

    // Update next frame offset
    STORE_HW( pFrame->hwOffset, 
              iLength + sizeof( CTCIHDR ) + sizeof( CTCISEG ) );

    // Store segment length
    STORE_HW( pSegment->hwLength, iLength + sizeof( CTCISEG ) );

    // Store Frame type
    STORE_HW( pSegment->hwType, FRAME_TYPE_IP );

    // Copy data
    memcpy( pSegment->bData, pDEVBLK->buf, iLength );

    // Fix-up frame pointer and terminate block
    pFrame = (PCTCIHDR)( pIOBuf + sizeof( CTCIHDR ) + 
                         sizeof( CTCISEG ) + iLength );
    STORE_HW( pFrame->hwOffset, 0x0000 );

    // Calculate #of bytes returned including two slack bytes
    iLength += sizeof( CTCIHDR ) + sizeof( CTCISEG ) + 2;

    if( sCount < iLength )
    {
        *pMore     = 1;
        *pResidual = 0;

        iLength    = sCount;
    }
    else
    {
        *pMore      = 0;
        *pResidual -= iLength;
    }

    // Set unit status
    *pUnitStat = CSW_CE | CSW_DE;
}

//
// CTCT_ListenThread
//

static void*  CTCT_ListenThread( void* argp )
{
    int          connfd;
    socklen_t    servlen;
    char         str[80];
    CTCG_PARMBLK parm;

    // set up the parameters passed via create_thread
    parm = *((CTCG_PARMBLK*) argp);
    free( argp );

    for( ; ; )
    {
        servlen = sizeof(parm.addr);

        // await a connection
        connfd = accept( parm.listenfd,
                         (struct sockaddr *)&parm.addr,
                         &servlen );

        sprintf( str, "%s:%d",
                 inet_ntoa( parm.addr.sin_addr ),
                 ntohs( parm.addr.sin_port ) );

        if( strcmp( str, parm.dev->filename ) != 0 )
        {
            logmsg( _("HHCCT023E %4.4X: Incorrect client or config error\n"
                      "                 Config=%s, connecting client=%s\n"),
                    parm.dev->devnum,
                    parm.dev->filename, str);
            close_socket( connfd );
        }
        else
        {
            parm.dev->fd = connfd;
        }

        // Ok, so having done that we're going to loop back to the
        // accept().  This was meant to handle the connection failing
        // at the other end; this end will be ready to accept another
        // connection.  Although this will happen, I'm sure you can
        // see the possibility for bad things to occur (eg if another
        // Hercules tries to connect).  This will also be fixed RSN.
    }

    return NULL;    // make compiler happy
}

// ====================================================================
// VMNET Support -- written by Willem Konynenberg
// ====================================================================

/*-------------------------------------------------------------------*/
/* Definitions for SLIP encapsulation                                */
/*-------------------------------------------------------------------*/
#define SLIP_END        0300
#define SLIP_ESC        0333
#define SLIP_ESC_END    0334
#define SLIP_ESC_ESC    0335

/*-------------------------------------------------------------------*/
/* Functions to support vmnet written by Willem Konynenberg          */
/*-------------------------------------------------------------------*/
static int start_vmnet(DEVBLK *dev, DEVBLK *xdev, int argc, char *argv[])
{
int sockfd[2];
int r, i;
char *ipaddress;

    if (argc < 2) {
        logmsg (_("HHCCT024E %4.4X: Not enough arguments to start vmnet\n"),
                        dev->devnum);
        return -1;
    }

    ipaddress = argv[0];
    argc--;
    argv++;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, sockfd) < 0) {
        logmsg (_("HHCCT025E %4.4X: Failed: socketpair: %s\n"),
                        dev->devnum, strerror(errno));
        return -1;
    }

    r = fork ();

    if (r < 0) {
        logmsg (_("HHCCT026E %4.4X: Failed: fork: %s\n"),
                        dev->devnum, strerror(errno));
        return -1;
    } else if (r == 0) {
        /* child */
        close (0);
        close (1);
        dup (sockfd[1]);
        dup (sockfd[1]);
        r = (sockfd[0] > sockfd[1]) ? sockfd[0] : sockfd[1];
        for (i = 3; i <= r; i++) {
            close (i);
        }

        /* the ugly cast is to silence a compiler warning due to const */
        execv (argv[0], (EXECV_ARG2_ARGV_T)argv);

        exit (1);
    }

    close (sockfd[1]);
    dev->fd = sockfd[0];
    xdev->fd = sockfd[0];

    /* We just blindly copy these out in the hope vmnet will pick them
     * up correctly.  I don't feel like implementing a complete login
     * scripting facility here...
     */
    write(dev->fd, ipaddress, strlen(ipaddress));
    write(dev->fd, "\n", 1);
    return 0;
}

static int VMNET_Init(DEVBLK *dev, int argc, char *argv[])
{
U16             xdevnum;                /* Pair device devnum        */
BYTE            c;                      /* tmp for scanf             */
DEVBLK          *xdev;                  /* Pair device               */

    dev->devtype = 0x3088;

    /* parameters for network CTC are:
     *    devnum of the other CTC device of the pair
     *    ipaddress
     *    vmnet command line
     *
     * CTC adapters are used in pairs, one for READ, one for WRITE.
     * The vmnet is only initialised when both are initialised.
     */
    if (argc < 3) {
        logmsg(_("HHCCT027E %4.4X: Not enough parameters\n"), dev->devnum);
        return -1;
    }
    if (strlen(argv[0]) > 4
        || sscanf(argv[0], "%hx%c", &xdevnum, &c) != 1) {
        logmsg(_("HHCCT028E %4.4X: Bad device number '%s'\n"),
                  dev->devnum, argv[0]);
        return -1;
    }
    xdev = find_device_by_devnum(xdevnum);
    if (xdev != NULL) {
        if (start_vmnet(dev, xdev, argc - 1, &argv[1]))
            return -1;
    }
    strcpy(dev->filename, "vmnet");

    /* Set the control unit type */
    /* Linux/390 currently only supports 3088 model 2 CTCA and ESCON */
    dev->ctctype = CTC_VMNET;

    SetSIDInfo( dev, 0x3088, 0x08, 0x3088, 0x01 );

    /* Initialize the device dependent fields */
    dev->ctcpos = 0;
    dev->ctcrem = 0;

    /* Set length of buffer */
    /* This size guarantees we can write a full iobuf of 65536
     * as a SLIP packet in a single write.  Probably overkill... */
    dev->bufsize = 65536 * 2 + 1;
    return 0;
}

static int VMNET_Write(DEVBLK *dev, BYTE *iobuf, U16 count, BYTE *unitstat)
{
int blklen = (iobuf[0]<<8) | iobuf[1];
int pktlen;
BYTE *p = iobuf + 2;
BYTE *buffer = dev->buf;
int len = 0, rem;

    if (count < blklen) {
        logmsg (_("HHCCT029E %4.4X: bad block length: %d < %d\n"),
                dev->devnum, count, blklen);
        blklen = count;
    }
    while (p < iobuf + blklen) {
        pktlen = (p[0]<<8) | p[1];

        rem = iobuf + blklen - p;

        if (rem < pktlen) {
            logmsg (_("HHCCT030E %4.4X: bad packet length: %d < %d\n"),
                    dev->devnum, rem, pktlen);
            pktlen = rem;
        }
        if (pktlen < 6) {
        logmsg (_("HHCCT031E %4.4X: bad packet length: %d < 6\n"),
                    dev->devnum, pktlen);
            pktlen = 6;
        }

        pktlen -= 6;
        p += 6;

        while (pktlen--) {
            switch (*p) {
            case SLIP_END:
                buffer[len++] = SLIP_ESC;
                buffer[len++] = SLIP_ESC_END;
                break;
            case SLIP_ESC:
                buffer[len++] = SLIP_ESC;
                buffer[len++] = SLIP_ESC_ESC;
                break;
            default:
                buffer[len++] = *p;
                break;
            }
            p++;
        }
        buffer[len++] = SLIP_END;
        write(dev->fd, buffer, len);   /* should check error conditions? */
        len = 0;
    }

    *unitstat = CSW_CE | CSW_DE;

    return count;
}

static int bufgetc(DEVBLK *dev, int blocking)
{
BYTE *bufp = dev->buf + dev->ctcpos, *bufend = bufp + dev->ctcrem;
int n;

    if (bufp >= bufend) {
        if (blocking == 0) return -1;
        do {
            n = read(dev->fd, dev->buf, dev->bufsize);
            if (n <= 0) {
                if (n == 0) {
                    /* VMnet died on us. */
                    logmsg (_("HHCCT032E %4.4X: Error: EOF on read, "
                              "CTC network down\n"),
                            dev->devnum);
                    /* -2 will cause an error status to be set */
                    return -2;
                }
                if( n == EINTR )
                    return -3;
                logmsg (_("HHCCT033E %4.4X: Error: read: %s\n"),
                        dev->devnum, strerror(errno));
                SLEEP(2);
            }
        } while (n <= 0);
        dev->ctcrem = n;
        bufend = &dev->buf[n];
        dev->ctclastpos = dev->ctclastrem = dev->ctcpos = 0;
        bufp = dev->buf;
    }

    dev->ctcpos++;
    dev->ctcrem--;

    return *bufp;
}

static void setblkheader(BYTE *iobuf, int buflen)
{
    iobuf[0] = (buflen >> 8) & 0xFF;
    iobuf[1] = buflen & 0xFF;
}

static void setpktheader(BYTE *iobuf, int packetpos, int packetlen)
{
    iobuf[packetpos] = (packetlen >> 8) & 0xFF;
    iobuf[packetpos+1] = packetlen & 0xFF;
    iobuf[packetpos+2] = 0x08;
    iobuf[packetpos+3] = 0;
    iobuf[packetpos+4] = 0;
    iobuf[packetpos+5] = 0;
}

/* read data from the CTC connection.
 * If a packet overflows the iobuf or the read buffer runs out, there are
 * 2 possibilities:
 * - block has single packet: continue reading packet, drop bytes,
 *   then return truncated packet.
 * - block has multiple packets: back up on last packet and return
 *   what we have.  Do this last packet in the next IO.
 */
static int VMNET_Read(DEVBLK *dev, BYTE *iobuf, U16 count, BYTE *unitstat)
{
int             c;                      /* next byte to process      */
int             len = 8;                /* length of block           */
int             lastlen = 2;            /* block length at last pckt */

    dev->ctclastpos = dev->ctcpos;
    dev->ctclastrem = dev->ctcrem;

    while (1) {
        c = bufgetc(dev, lastlen == 2);
        if (c < 0) {
            if(c == -3)
                return 0;
            /* End of input buffer.  Return what we have. */

            setblkheader (iobuf, lastlen);

            dev->ctcpos = dev->ctclastpos;
            dev->ctcrem = dev->ctclastrem;

            *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

            return lastlen;
        }
        switch (c) {
        case SLIP_END:
            if (len > 8) {
                /* End of packet.  Set up for next. */

                setpktheader (iobuf, lastlen, len-lastlen);

                dev->ctclastpos = dev->ctcpos;
                dev->ctclastrem = dev->ctcrem;
                lastlen = len;

                len += 6;
            }
            break;
        case SLIP_ESC:
            c = bufgetc(dev, lastlen == 2);
            if (c < 0) {
                if(c == -3)
                    return 0;
                /* End of input buffer.  Return what we have. */

                setblkheader (iobuf, lastlen);

                dev->ctcpos = dev->ctclastpos;
                dev->ctcrem = dev->ctclastrem;

                *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

                return lastlen;
            }
            switch (c) {
            case SLIP_ESC_END:
                c = SLIP_END;
                break;
            case SLIP_ESC_ESC:
                c = SLIP_ESC;
                break;
            }
            /* FALLTHRU */
        default:
            if (len < count) {
                iobuf[len++] = c;
            } else if (lastlen > 2) {
                /* IO buffer is full and we have data to return */

                setblkheader (iobuf, lastlen);

                dev->ctcpos = dev->ctclastpos;
                dev->ctcrem = dev->ctclastrem;

                *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

                return lastlen;
            } /* else truncate end of very large single packet... */
        }
    }
}
/*-------------------------------------------------------------------*/
/* End of VMNET functions written by Willem Konynenberg              */
/*-------------------------------------------------------------------*/

// ====================================================================
// Support Functions
// ====================================================================

// ---------------------------------------------------------------------
// ParseMAC
// ---------------------------------------------------------------------
//
// Parse a string containing a MAC (hardware) address and return the
// binary equivalent.
//
// Input:
//      pszMACAddr   Pointer to string containing a MAC Address in the
//                   format "xx-xx-xx-xx-xx-xx" or "xx:xx:xx:xx:xx:xx".
//
// Output:
//      pbMACAddr    Pointer to a BYTE array to receive the MAC Address
//                   that MUST be at least LCS_ADDR_LEN bytes long.
//
// Returns:
//      0 on success, -1 otherwise
//

int             ParseMAC( char* pszMACAddr, BYTE* pbMACAddr )
{
    char    work[((LCS_ADDR_LEN*3)-0)];
    BYTE    sep;
    int     x, i;

    if (strlen(pszMACAddr) != ((LCS_ADDR_LEN*3)-1)
        || (LCS_ADDR_LEN > 1 &&
            *(pszMACAddr+2) != '-' &&
            *(pszMACAddr+2) != ':')
    )
    {
        errno = EINVAL;
        return -1;
    }

    strncpy(work,pszMACAddr,((LCS_ADDR_LEN*3)-1));
    work[((LCS_ADDR_LEN*3)-1)] = sep = *(pszMACAddr+2);

    for (i=0; i < LCS_ADDR_LEN; i++)
    {
        if
        (0
            || !isxdigit(work[(i*3)+0])
            || !isxdigit(work[(i*3)+1])
            ||  sep  !=  work[(i*3)+2]
        )
        {
            errno = EINVAL;
            return -1;
        }

        work[(i*3)+2] = 0;
        sscanf(&work[(i*3)+0],"%x",&x);
        *(pbMACAddr+i) = x;
    }

    return 0;
}

// ---------------------------------------------------------------------
// packet_trace
// ---------------------------------------------------------------------
//
// Subroutine to trace the contents of a buffer
//

void  packet_trace( BYTE* pAddr, int iLen )
{
    int           offset;
    unsigned int  i;
    unsigned char c = '\0';
    unsigned char e = '\0';
    unsigned char print_chars[17];

    for( offset = 0; offset < iLen; )
    {
        memset( print_chars, 0, sizeof( print_chars ) );

        logmsg( "+%4.4X  ", offset );

        for( i = 0; i < 16; i++ )
        {
            c = *pAddr++;

            if( offset < iLen )
            {
                logmsg("%2.2X", c);

                print_chars[i] = '.';
                e = guest_to_host( c );

                if( isprint( e ) )
                    print_chars[i] = e;
                if( isprint( c ) )
                    print_chars[i] = c;
            }
            else
            {
                logmsg( "  " );
            }

            offset++;
            if( ( offset & 3 ) == 0 )
            {
                logmsg( " " );
            }
        }

        logmsg( " %s\n", print_chars );
    }
}
