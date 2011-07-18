/* CTCADPT.H    (c) Copyright Roger Bowler, 2000-2011                */
/*              (c) Copyright James A. Pierson, 2002-2009            */
/*              (c) Copyright Willem Konynenberg, 2000-2009          */
/*              (c) Copyright Vic Cross, 2001-2009                   */
/*              (c) Copyright David B. Trout, 2002-2009              */
/*              Hercules Channel-to-Channel Emulation Support        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef __CTCADPT_H_
#define __CTCADPT_H_

// --------------------------------------------------------------------
// Pack all structures to byte boundary...
// --------------------------------------------------------------------

#undef ATTRIBUTE_PACKED
#if defined(_MSVC_)
 #pragma pack(push)
 #pragma pack(1)
 #define ATTRIBUTE_PACKED
#else
 #define ATTRIBUTE_PACKED __attribute__((packed))
#endif

// --------------------------------------------------------------------
// Definitions for 3088 model numbers
// --------------------------------------------------------------------

#define CTC_3088_01     0x308801        // 3172 XCA
#define CTC_3088_04     0x308804        // 3088 model 1 CTCA
#define CTC_3088_08     0x308808        // 3088 model 2 CTCA
#define CTC_3088_1F     0x30881F        // 3172 LCS
#define CTC_3088_60     0x308860        // OSA or 8232 LCS
#define CTC_3088_61     0x308861        // CLAW device

// --------------------------------------------------------------------
// Media Access Control address (MAC address)
// --------------------------------------------------------------------

#ifndef IFHWADDRLEN                     // (only predefined on Linux)
#define IFHWADDRLEN  6                  // Ethernet MAC address length
#endif

typedef uint8_t  MAC[ IFHWADDRLEN ];    // Data Type for MAC Addresses

// --------------------------------------------------------------------
// External Declarations
// --------------------------------------------------------------------

extern int      CTCX_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      CTCX_Close( DEVBLK* pDEVBLK );
extern void     CTCX_Query( DEVBLK* pDEVBLK, char** ppszClass,
                            int     iBufLen, char*  pBuffer );
extern void     CTCX_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                 BYTE    bFlags,  BYTE  bChained,
                                 U16     sCount,  BYTE  bPrevCode,
                                 int     iCCWSeq, BYTE* pIOBuf,
                                 BYTE*   pMore,   BYTE* pUnitStat,
                                 U16*    pResidual );

extern int      CTCI_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      CTCI_Close( DEVBLK* pDEVBLK );
extern void     CTCI_Query( DEVBLK* pDEVBLK, char** ppszClass,
                            int     iBufLen, char*  pBuffer );
extern void     CTCI_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                 BYTE    bFlags,  BYTE  bChained,
                                 U16     sCount,  BYTE  bPrevCode,
                                 int     iCCWSeq, BYTE* pIOBuf,
                                 BYTE*   pMore,   BYTE* pUnitStat,
                                 U16*    pResidual );

extern void     CTCI_Read( DEVBLK* pDEVBLK,   U16   sCount,
                           BYTE*   pIOBuf,    BYTE* UnitStat,
                           U16*    pResidual, BYTE* pMore );
extern void     CTCI_Write( DEVBLK* pDEVBLK,   U16   sCount,
                            BYTE*   pIOBuf,    BYTE* UnitStat,
                            U16*    pResidual );

extern int      LCS_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      LCS_Close( DEVBLK* pDEVBLK );
extern void     LCS_Query( DEVBLK* pDEVBLK, char** ppszClass,
                           int     iBufLen, char*  pBuffer );
extern void     LCS_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                BYTE    bFlags,  BYTE  bChained,
                                U16     sCount,  BYTE  bPrevCode,
                                int     iCCWSeq, BYTE* pIOBuf,
                                BYTE*   pMore,   BYTE* pUnitStat,
                                U16*    pResidual );

extern void     LCS_Read( DEVBLK* pDEVBLK,   U16   sCount,
                          BYTE*   pIOBuf,    BYTE* UnitStat,
                          U16*    pResidual, BYTE* pMore );
extern void     LCS_Write( DEVBLK* pDEVBLK,   U16   sCount,
                           BYTE*   pIOBuf,    BYTE* UnitStat,
                           U16*    pResidual );
extern void     LCS_SDC( DEVBLK* pDEVBLK,   BYTE   bOpCode,
                         U16     sCount,    BYTE*  pIOBuf,
                         BYTE*   UnitStat,  U16*   pResidual,
                         BYTE*   pMore );

extern void     packet_trace( BYTE *addr, int len, BYTE dir );



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **              STANDARD ETHERNET FRAMES LAYOUT                     **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


// --------------------------------------------------------------------
// Ethernet Frame Header                (network byte order)
// --------------------------------------------------------------------

struct _ETHFRM
{
    MAC         bDestMAC;                // 0x00
    MAC         bSrcMAC;                 // 0x06
    HWORD       hwEthernetType;          // 0x0C  (see below #defines)
    BYTE        bData[FLEXIBLE_ARRAY];   // 0x0E
} ATTRIBUTE_PACKED;


typedef struct _ETHFRM ETHFRM, *PETHFRM;


#define  ETH_TYPE_IP        0x0800
#define  ETH_TYPE_ARP       0x0806
#define  ETH_TYPE_RARP      0x0835
#define  ETH_TYPE_SNA       0x80D5


// --------------------------------------------------------------------
// IP Version 4 Frame Header (Type 0x0800)  (network byte order)
// --------------------------------------------------------------------

struct  _IP4FRM
{
    BYTE        bVersIHL;                // 0x00 Vers:4, IHL:4
    BYTE        bTOS;                    // 0x01
    HWORD       hwTotalLength;           // 0x02
    HWORD       hwIdentification;        // 0x04
    U16         bFlagsFragOffset;        // 0x06 Flags:3, FragOffset:13
    BYTE        bTTL;                    // 0x08
    BYTE        bProtocol;               // 0x09
    HWORD       hwChecksum;              // 0x0A
    U32         lSrcIP;                  // 0x0C
    U32         lDstIP;                  // 0x10
    BYTE        bData[FLEXIBLE_ARRAY];   // 0x14
} ATTRIBUTE_PACKED;


typedef struct _IP4FRM IP4FRM, *PIP4FRM;


// --------------------------------------------------------------------
// Address Resolution Protocol Frame (Type 0x0806) (network byte order)
// --------------------------------------------------------------------

struct  _ARPFRM
{
    HWORD       hwHardwareType;          // 0x00
    HWORD       hwProtocolType;          // 0x02
    BYTE        bHardwareSize;           // 0x04
    BYTE        bProtocolSize;           // 0x05
    HWORD       hwOperation;             // 0x06
    MAC         bSendEthAddr;            // 0x08
    U32         lSendIPAddr;             // 0x12
    MAC         bTargEthAddr;            // 0x16
    U32         lTargIPAddr;             // 0x1C
} ATTRIBUTE_PACKED;


typedef struct _ARPFRM ARPFRM, *PARPFRM;


#define  ARP_REQUEST        0x01
#define  ARP_REPLY          0x02
#define  RARP_REQUEST       0x03
#define  RARP_REPLY         0x04



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **                  CTCI DEVICE CONTROL BLOCKS                      **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


#define MAX_CTCI_FRAME_SIZE( pCTCBLK )      \
    (                                       \
        pCTCBLK->iMaxFrameBufferSize  /* (whatever CTCI_Init defined) */  \
        - sizeof( CTCIHDR )                 \
        - sizeof( CTCISEG )                 \
        - sizeof_member(CTCIHDR,hwOffset)   \
    )


#define MAX_LCS_ETH_FRAME_SIZE( pLCSDEV )   \
    (                                       \
        pLCSDEV->iMaxFrameBufferSize  /* (whatever LCS_Startup defined) */  \
        - sizeof( LCSETHFRM )               \
        - sizeof_member(LCSHDR,hwOffset)    \
    )

// PROGRAMMING NOTE: the following frame buffer size should always be
// no smaller than the maximum frame buffer size possible for an LCS
// device (currently hard-coded in S390 Linux to be 0x5000 via the
// #define LCS_IOBUFFERSIZE). Also note that the minimum and maximum
// frame buffer size, according to IBM documentation, is 16K to 64K.

#define CTC_FRAME_BUFFER_SIZE     (0x5000)  // 20K CTCI/LCS frame buffer

#define CTC_MIN_FRAME_BUFFER_SIZE (0x4000)  // Minimum frame buffer size
#define CTC_MAX_FRAME_BUFFER_SIZE (0xFFFF)  // Maximum frame buffer size

#define CTC_READ_TIMEOUT_SECS  (5)          // five seconds

#define CTC_DELAY_USECS        (100)        // 100 microseconds delay; used
                                            // mostly by enqueue frame buffer
                                            // full delay loop...

struct  _CTCBLK;
struct  _CTCIHDR;
struct  _CTCISEG;

typedef struct _CTCBLK  CTCBLK, *PCTCBLK;
typedef struct _CTCIHDR CTCIHDR,*PCTCIHDR;
typedef struct _CTCISEG CTCISEG,*PCTCISEG;


// --------------------------------------------------------------------
// CTCBLK -                                (host byte order)
// --------------------------------------------------------------------

struct  _CTCBLK
{
    int         fd;                       // TUN/TAP fd
    TID         tid;                      // Read Thread ID
    pid_t       pid;                      // Read Thread pid

    DEVBLK*     pDEVBLK[2];               // 0 - Read subchannel
                                          // 1 - Write subchannel

    U16         iMaxFrameBufferSize;      // Device Buffer Size
    BYTE        bFrameBuffer[CTC_FRAME_BUFFER_SIZE]; // (this really SHOULD be dynamically allocated!)
    U16         iFrameOffset;             // Curr Offset into Buffer
    U16         sMTU;                     // Max MTU

    LOCK        Lock;                     // Data LOCK
    LOCK        EventLock;                // Condition LOCK
    COND        Event;                    // Condition signal

    u_int       fDebug:1;                 // Debugging
    u_int       fOldFormat:1;             // Old Config Format
    u_int       fCreated:1;               // Interface Created
    u_int       fStarted:1;               // Startup Received
    u_int       fDataPending:1;           // Data is pending for
                                          //   read device
    u_int       fCloseInProgress:1;       // Close in progress

    int         iKernBuff;                // Kernel buffer in K bytes.
    int         iIOBuff;                  // I/O buffer in K bytes.
    char        szGuestIPAddr[32];        // IP Address (Guest OS)
    char        szDriveIPAddr[32];        // IP Address (Driver)
    char        szNetMask[32];            // Netmask for P2P link
    char        szMTU[32];
    char        szTUNCharName[256];       // TUN/TAP char filename
    char        szTUNDevName[IFNAMSIZ];   // Network Device Name
    char        szMACAddress[32];         // MAC Address
};



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **                   CTCI DEVICE FRAMES                             **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


// --------------------------------------------------------------------
// CTCI Block Header                    (host byte order)
// --------------------------------------------------------------------

struct _CTCIHDR                         // CTCI Block Header
{
    HWORD   hwOffset;                   // Offset of next block
    BYTE    bData[FLEXIBLE_ARRAY];      // start of data (CTCISEG)
} ATTRIBUTE_PACKED;


// --------------------------------------------------------------------
// CTCI Segment Header                  (host byte order)
// --------------------------------------------------------------------

struct _CTCISEG                         // CTCI Segment Header
{
    HWORD   hwLength;                   // Segment length including
                                        //   this header
    HWORD   hwType;                     // Ethernet packet type
    HWORD   _reserved;                  // Unused, set to zeroes
    BYTE    bData[FLEXIBLE_ARRAY];      // Start of data (IP pakcet)
} ATTRIBUTE_PACKED;



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **                  LCS DEVICE CONTROL BLOCKS                       **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


#define LCS_MAX_PORTS   4


struct  _LCSBLK;
struct  _LCSDEV;
struct  _LCSPORT;
struct  _LCSRTE;
struct  _LCSHDR;
struct  _LCSCMDHDR;
struct  _LCSSTDFRM;
struct  _LCSSTRTFRM;
struct  _LCSQIPFRM;
struct  _LCSLSTFRM;
struct  _LCSIPMPAIR;
struct  _LCSIPMFRM;
struct  _LCSETHFRM;


typedef struct  _LCSBLK     LCSBLK,     *PLCSBLK;
typedef struct  _LCSDEV     LCSDEV,     *PLCSDEV;
typedef struct  _LCSPORT    LCSPORT,    *PLCSPORT;
typedef struct  _LCSRTE     LCSRTE,     *PLCSRTE;
typedef struct  _LCSHDR     LCSHDR,     *PLCSHDR;
typedef struct  _LCSCMDHDR  LCSCMDHDR,  *PLCSCMDHDR;
typedef struct  _LCSSTDFRM  LCSSTDFRM,  *PLCSSTDFRM;
typedef struct  _LCSSTRTFRM LCSSTRTFRM, *PLCSSTRTFRM;
typedef struct  _LCSQIPFRM  LCSQIPFRM,  *PLCSQIPFRM;
typedef struct  _LCSLSTFRM  LCSLSTFRM,  *PLCSLSTFRM;
typedef struct  _LCSIPMPAIR LCSIPMPAIR, *PLCSIPMPAIR;
typedef struct  _LCSIPMFRM  LCSIPMFRM,  *PLCSIPMFRM;
typedef struct  _LCSETHFRM  LCSETHFRM,  *PLCSETHFRM;

#if       defined ( _MSVC_ )
struct  _RBPKT;
struct  _RINGBUFFER;
typedef struct  _RBPKT      RBPKT,      *PRBPKT;
typedef struct  _RINGBUFFER RINGBUFFER, *PRINGBUFFER;
#endif

// --------------------------------------------------------------------
// LCS Device                              (host byte order)
// --------------------------------------------------------------------

struct  _LCSDEV
{
    U16         sAddr;                  // Device Base Address
    BYTE        bMode;                  // (see below #defines)
    BYTE        bPort;                  // Relative Adapter No.
    BYTE        bType;                  // (see below #defines)
    char*       pszIPAddress;           // IP Address (string)

    U32         lIPAddress;             // IP Address (binary),
                                        // (network byte order)

    PLCSBLK     pLCSBLK;                // -> LCSBLK
    DEVBLK*     pDEVBLK[2];             // 0 - Read subchannel
                                        // 1 - Write cubchannel

    U16         iMaxFrameBufferSize;    // Device Buffer Size
    BYTE        bFrameBuffer[CTC_FRAME_BUFFER_SIZE]; // (this really SHOULD be dynamically allocated!)
    U16         iFrameOffset;           // Curr Offset into Buffer

    LOCK        Lock;                   // Data LOCK
    LOCK        EventLock;              // Condition LOCK
    COND        Event;                  // Condition signal

    u_int       fCreated:1;             // DEVBLK(s) Created
    u_int       fStarted:1;             // Device Started
    u_int       fRouteAdded:1;          // Routing Added
    u_int       fReplyPending:1;        // Cmd Reply is Pending
    u_int       fDataPending:1;         // Data is Pending

    PLCSDEV     pNext;                  // Next device
};



#define LCSDEV_MODE_IP          0x01
#define LCSDEV_MODE_SNA         0x02


#define LCSDEV_TYPE_NONE        0x00
#define LCSDEV_TYPE_PRIMARY     0x01
#define LCSDEV_TYPE_SECONDARY   0x02


// --------------------------------------------------------------------
// LCS Port (or Relative Adapter)         (host byte order)
// --------------------------------------------------------------------

struct  _LCSPORT
{
    BYTE        bPort;                    // Relative Adapter No
    MAC         MAC_Address;              // MAC Address of Adapter
    PLCSRTE     pRoutes;                  // -> Routes chain
    PLCSBLK     pLCSBLK;                  // -> LCSBLK

    U16         sIPAssistsSupported;      // IP Assist Info
    U16         sIPAssistsEnabled;

    LOCK        Lock;                     // Data LOCK
    LOCK        EventLock;                // Condition LOCK
    COND        Event;                    // Condition signal

    u_int       fUsed:1;                  // Port is used
    u_int       fLocalMAC:1;              // MAC is specified in OAT
    u_int       fCreated:1;               // Interface Created
    u_int       fStarted:1;               // Startup Received
    u_int       fRouteAdded:1;            // Routing Added
    u_int       fCloseInProgress:1;       // Close in progress

    int         fd;                       // TUN/TAP fd
#if       defined ( _MSVC_ )
    HANDLE      handle;                   // NDIS handle
    TID         rb_tid;                   // Ring Buffer Thread ID
    pid_t       rb_pid;                   // Ring Buffer pid
    PRINGBUFFER prb_struct;               // -> RINGBUFFER
#endif // defined ( _MSVC_ )
    TID         tid;                      // Read Thread ID
    pid_t       pid;                      // Read Thread pid
    int         icDevices;                // Device count
    char        szNetDevName[IFNAMSIZ];   // Network Device Name
    char        szMACAddress[32];         // MAC Address
    char        szGWAddress[32];          // Gateway for W32
};


// --------------------------------------------------------------------
// LCSRTE - Routing Entries               (host byte order)
// --------------------------------------------------------------------

struct  _LCSRTE
{
    char*       pszNetAddr;
    char*       pszNetMask;
    PLCSRTE     pNext;
};


// --------------------------------------------------------------------
// LCSBLK - Common Storage for LCS Emulation   (host byte order)
// --------------------------------------------------------------------

struct  _LCSBLK
{
    // Config line parameters
    char*       pszTUNDevice;             // TUN/TAP char device
#if defined ( _MSVC_ )
    char*       pszNDISDevice;            // NDIS char device name
    char*       pszNDISDevDesc;           // NDIS common name of device
    char*       pszNDISDevMac;            // MAC Address of the physical card
    char*       pszServiceName;           // Service Name for NDIS
#endif
    char*       pszOATFilename;           // OAT Filename
    char*       pszIPAddress;             // IP Address
    char*       pszMACAddress;            // MAC Address (string)
    MAC         MAC_Address;              // MAC Address (binary)

    u_int       fDebug:1;
#if defined( OPTION_W32_CTCI )
    u_int       fNoMultiWrite:1;          // CTCI-WIN v3.3+ WinPCap v4.1+
#endif
#if defined ( _MSVC_ )
    u_int       passthruStarted:1;        // NDIS passthru services started
#endif
    int         icDevices;                // Number of devices
    int         iKernBuff;                // Kernel buffer in K bytes.
    int         iIOBuff;                  // I/O buffer in K bytes.

    PLCSDEV     pDevices;                 // -> Device chain
    LCSPORT     Port[LCS_MAX_PORTS];      // Port Blocks

    // Self Describing Component Information
    char        szSerialNumber[13];
};



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **                   LCS DEVICE FRAMES                              **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


// --------------------------------------------------------------------
// LCS Frame Header                             (network byte order)
// --------------------------------------------------------------------

struct _LCSHDR      // *ALL* LCS Frames start with the following header
{
    HWORD       hwOffset;               // Offset to next frame or 0
    BYTE        bType;                  // (see below #defines)
    BYTE        bSlot;                  // (i.e. port)
} ATTRIBUTE_PACKED;


#define  LCS_FRMTYP_CMD     0x00        // LCS command mode
#define  LCS_FRMTYP_ENET    0x01        // Ethernet Passthru
#define  LCS_FRMTYP_TR      0x02        // Token Ring
#define  LCS_FRMTYP_FDDI    0x07        // FDDI
#define  LCS_FRMTYP_AUTO    0xFF        // auto-detect


// --------------------------------------------------------------------
// LCS Command Frame Header                     (network byte order)
// --------------------------------------------------------------------

struct _LCSCMDHDR    // All LCS *COMMAND* Frames start with this header
{
    LCSHDR      bLCSHdr;                // LCS Frame header

    BYTE        bCmdCode;               // (see below #defines)
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;               // usually LCS_FRMTYP_ENET
    BYTE        bRelAdapterNo;          // (i.e. port)
} ATTRIBUTE_PACKED;


#define  LCS_CMD_TIMING         0x00        // Timing request
#define  LCS_CMD_STRTLAN        0x01        // Start LAN
#define  LCS_CMD_STOPLAN        0x02        // Stop  LAN
#define  LCS_CMD_GENSTAT        0x03        // Generate Stats
#define  LCS_CMD_LANSTAT        0x04        // LAN Stats
#define  LCS_CMD_LISTLAN        0x06        // List LAN
#define  LCS_CMD_STARTUP        0x07        // Start Host
#define  LCS_CMD_SHUTDOWN       0x08        // Shutdown Host
#define  LCS_CMD_LISTLAN2       0x0B        // List LAN (another version)
#define  LCS_CMD_QIPASSIST      0xB2        // Query IP Assists
#define  LCS_CMD_SETIPM         0xB4        // Set IP Multicast
#define  LCS_CMD_DELIPM         0xB5        // Delete IP Multicast


// --------------------------------------------------------------------
// LCS Standard Command Frame                   (network byte order)
// --------------------------------------------------------------------

struct _LCSSTDFRM
{
    LCSCMDHDR   bLCSCmdHdr;             // LCS Command Frame header

    HWORD       hwParameterCount;
    BYTE        bOperatorFlags[3];
    BYTE        _reserved[3];
    BYTE        bData[FLEXIBLE_ARRAY];
} ATTRIBUTE_PACKED;


// --------------------------------------------------------------------
// LCS Startup Command Frame                    (network byte order)
// --------------------------------------------------------------------

struct _LCSSTRTFRM
{
    LCSCMDHDR   bLCSCmdHdr;             // LCS Command Frame header

    HWORD       hwBufferSize;
    BYTE        _unused2[6];
} ATTRIBUTE_PACKED;


// --------------------------------------------------------------------
// LCS Query IP Assists Command Frame           (network byte order)
// --------------------------------------------------------------------

struct  _LCSQIPFRM
{
    LCSCMDHDR   bLCSCmdHdr;             // LCS Command Frame header

    HWORD       hwNumIPPairs;
    HWORD       hwIPAssistsSupported;
    HWORD       hwIPAssistsEnabled;
    HWORD       hwIPVersion;
} ATTRIBUTE_PACKED;


#define LCS_ARP_PROCESSING            0x0001
#define LCS_INBOUND_CHECKSUM_SUPPORT  0x0002
#define LCS_OUTBOUND_CHECKSUM_SUPPORT 0x0004
#define LCS_IP_FRAG_REASSEMBLY        0x0008
#define LCS_IP_FILTERING              0x0010
#define LCS_IP_V6_SUPPORT             0x0020
#define LCS_MULTICAST_SUPPORT         0x0040


// --------------------------------------------------------------------
// LCS LAN Statistics Command Frame             (network byte order)
// --------------------------------------------------------------------

struct  _LCSLSTFRM
{
    LCSCMDHDR   bLCSCmdHdr;             // LCS Command Frame header

    BYTE        _unused1[10];
    MAC         MAC_Address;            // MAC Address of Adapter
    FWORD       fwPacketsDeblocked;
    FWORD       fwPacketsBlocked;
    FWORD       fwTX_Packets;
    FWORD       fwTX_Errors;
    FWORD       fwTX_PacketsDiscarded;
    FWORD       fwRX_Packets;
    FWORD       fwRX_Errors;
    FWORD       fwRX_DiscardedNoBuffs;
    U32         fwRX_DiscardedTooLarge;
} ATTRIBUTE_PACKED;


// --------------------------------------------------------------------
// LCS Set IP Multicast Command Frame           (network byte order)
// --------------------------------------------------------------------

struct  _LCSIPMPAIR
{
    U32         IP_Addr;
    MAC         MAC_Address;            // MAC Address of Adapter
    BYTE        _reserved[2];
} ATTRIBUTE_PACKED;

#define MAX_IP_MAC_PAIRS      32

struct  _LCSIPMFRM
{
    LCSCMDHDR   bLCSCmdHdr;             // LCS Command Frame header

    HWORD       hwNumIPPairs;
    U16         hwIPAssistsSupported;
    U16         hwIPAssistsEnabled;
    U16         hwIPVersion;
    LCSIPMPAIR  IP_MAC_Pair[MAX_IP_MAC_PAIRS];
    U32         fwResponseData;
} ATTRIBUTE_PACKED;


// --------------------------------------------------------------------
// LCS Ethernet Passthru Frame                  (network byte order)
// --------------------------------------------------------------------

struct  _LCSETHFRM
{
    LCSHDR      bLCSHdr;                // LCS Frame header
    BYTE        bData[FLEXIBLE_ARRAY];  // Ethernet Frame
} ATTRIBUTE_PACKED;



/**********************************************************************\
 **********************************************************************
 **                                                                  **
 **                   INLINE FUNCTIONS                               **
 **                                                                  **
 **********************************************************************
\**********************************************************************/


// --------------------------------------------------------------------
// Set SenseID Information
// --------------------------------------------------------------------

static inline void SetSIDInfo( DEVBLK* pDEVBLK,
                               U16     wCUType,
                               BYTE    bCUMod,
                               U16     wDevType,
                               BYTE    bDevMod )
{
    BYTE* pSIDInfo = pDEVBLK->devid;

    memset( pSIDInfo, 0, 256 );

    *pSIDInfo++ = 0x0FF;
    *pSIDInfo++ = (BYTE)(( wCUType >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wCUType & 0x00FF );
    *pSIDInfo++ = bCUMod;
    *pSIDInfo++ = (BYTE)(( wDevType >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wDevType & 0x00FF );
    *pSIDInfo++ = bDevMod;
    *pSIDInfo++ = 0x00;

    pDEVBLK->numdevid = 7;
}


// --------------------------------------------------------------------
// Set SenseID CIW Information
// --------------------------------------------------------------------

static inline void SetCIWInfo( DEVBLK* pDEVBLK,
                               U16     bOffset,
                               BYTE    bCIWType,
                               BYTE    bCIWOp,
                               U16     wCIWCount )
{
    BYTE* pSIDInfo = pDEVBLK->devid;

    pSIDInfo += 8;
    pSIDInfo += ( bOffset * 4 );

    *pSIDInfo++ = bCIWType | 0x40;
    *pSIDInfo++ = bCIWOp;
    *pSIDInfo++ = (BYTE)(( wCIWCount >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wCIWCount & 0x00FF );

    pDEVBLK->numdevid += pDEVBLK->numdevid == 7 ? 5 : 4;
}


// --------------------------------------------------------------------

#if defined(_MSVC_)
 #pragma pack(pop)
#endif

#endif // __CTCADPT_H_
