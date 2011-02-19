/* TAPEDEV.H    (c) Copyright Ivan Warren and others, 2003-2010      */
/*              Tape Device Handler Structure Definitions            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This header file contains tape related structures and defines     */
/* for the Hercules ESA/390 emulator.                                */
/*-------------------------------------------------------------------*/

// $Id$

#ifndef __TAPEDEV_H__
#define __TAPEDEV_H__

#include "scsitape.h"       /* SCSI Tape handling functions          */
#include "htypes.h"         /* Hercules struct typedefs              */
#include "opcode.h"         /* device_attention, SETMODE, etc.       */
#include "parser.h"         /* generic parameter string parser       */

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define MAX_BLKLEN              65535   /* Maximum I/O buffer size   */
#define TAPE_UNLOADED           "*"     /* Name for unloaded drive   */

/*-------------------------------------------------------------------*/
/* Definitions for 3420/3480 sense bytes                             */
/*-------------------------------------------------------------------*/
#define SENSE1_TAPE_NOISE       0x80    /* Noise                     */
#define SENSE1_TAPE_TUA         0x40    /* TU Status A (ready)       */
#define SENSE1_TAPE_TUB         0x20    /* TU Status B (not ready)   */
#define SENSE1_TAPE_7TRK        0x10    /* 7-track feature           */
#define SENSE1_TAPE_RSE         0x10    /* Record sequence error     */
#define SENSE1_TAPE_LOADPT      0x08    /* Tape is at load point     */
#define SENSE1_TAPE_WRT         0x04    /* Tape is in write status   */
#define SENSE1_TAPE_FP          0x02    /* File protect status       */
#define SENSE1_TAPE_NCA         0x01    /* Not capable               */

#define SENSE4_TAPE_EOT         0x20    /* Tape indicate (EOT)       */

#define SENSE5_TAPE_SRDCHK      0x08    /* Start read check          */
#define SENSE5_TAPE_PARTREC     0x04    /* Partial record            */

#define SENSE7_TAPE_LOADFAIL    0x01    /* Load failure              */

/*-------------------------------------------------------------------*/
/* ISW : Internal error types used to build Device Dependent Sense   */
/*-------------------------------------------------------------------*/
#define TAPE_BSENSE_TAPEUNLOADED   0    /* I/O Attempted but no tape loaded */
#define TAPE_BSENSE_TAPELOADFAIL   1    /* I/O and load failed       */
#define TAPE_BSENSE_READFAIL       2    /* Error reading block       */
#define TAPE_BSENSE_WRITEFAIL      3    /* Error writing block       */
#define TAPE_BSENSE_BADCOMMAND     4    /* The CCW code is not known
                                           or sequence error         */
#define TAPE_BSENSE_INCOMPAT       5    /* The CCW code is known
                                           but is not unsupported    */
#define TAPE_BSENSE_WRITEPROTECT   6    /* Write CCW code was issued
                                           to a read-only media      */
#define TAPE_BSENSE_EMPTYTAPE      7    /* A read was issued but the
                                           tape is empty             */
#define TAPE_BSENSE_ENDOFTAPE      8    /* A read was issued past the
                                           end of the tape or a write
                                           was issued and there is no
                                           space left on the tape    */
#define TAPE_BSENSE_LOADPTERR      9    /* BSF/BSR/RdBW attempted
                                           from BOT                  */
#define TAPE_BSENSE_FENCED         10   /* Media damaged - unload
                                           or /reload required       */
#define TAPE_BSENSE_BADALGORITHM   11   /* Bad compression - HET
                                           tape compressed with an
                                           unsuported method         */
#define TAPE_BSENSE_RUN_SUCCESS    12   /* Rewind Unload success     */
#define TAPE_BSENSE_STATUSONLY     13   /* No exception occured      */
#define TAPE_BSENSE_LOCATEERR      14   /* Can't find block or TM    */
#define TAPE_BSENSE_READTM         15   /* A Tape Mark was read      */

#define TAPE_BSENSE_BLOCKSHORT     17   /* Short Tape block */
#define TAPE_BSENSE_ITFERROR       18   /* Interface error (SCSI
                                           driver unexpected err)    */
#define TAPE_BSENSE_REWINDFAILED   19   /* Rewind operation failed   */
#define TAPE_BSENSE_UNSOLICITED    20   /* Sense without UC          */

/*-------------------------------------------------------------------*/
/* Definitions for 3480 and later commands                           */
/*-------------------------------------------------------------------*/
/* Format control byte for Load Display command */
#define FCB_FS                  0xE0    /* Function Select bits...   */
#define FCB_FS_READYGO          0x00    /* Display msg until motion, */
                                        /* or until msg is updated   */
#define FCB_FS_UNMOUNT          0x20    /* Display msg until unloaded*/
#define FCB_FS_MOUNT            0x40    /* Display msg until loaded  */
#define FCB_FS_RESET_DISPLAY    0x80    /* Reset display (clear Host */
                                        /* msg; replace w/Unit msg)  */
#define FCB_FS_NOP              0x60    /* No-op                     */
#define FCB_FS_UMOUNTMOUNT      0xE0    /* Display msg 1 until tape  */
                                        /* is unloaded, then msg 2   */
                                        /* until tape is loaded      */
#define FCB_AM                  0x10    /* Alternate between msg 1/2 */
#define FCB_BM                  0x08    /* Blink message             */
#define FCB_M2                  0x04    /* Display only message 2    */
#define FCB_RESV                0x02    /* (reserved)                */
#define FCB_AL                  0x01    /* Activate AutoLoader on    */
                                        /* mount/unmount messages    */
/* Mode Set commands */
#define MSET_WRITE_IMMED        0x20    /* Tape-Write-Immediate mode */
#define MSET_SUPVR_INHIBIT      0x10    /* Supervisor Inhibit mode   */
#define MSET_IDRC               0x08    /* IDRC mode                 */

/* Path state byte for Sense Path Group ID command */
#define SPG_PATHSTAT            0xC0    /* Pathing status bits...    */
#define SPG_PATHSTAT_RESET      0x00    /* ...reset                  */
#define SPG_PATHSTAT_RESV       0x40    /* ...reserved bit setting   */
#define SPG_PATHSTAT_UNGROUPED  0x80    /* ...ungrouped              */
#define SPG_PATHSTAT_GROUPED    0xC0    /* ...grouped                */
#define SPG_PARTSTAT            0x30    /* Partitioning status bits..*/
#define SPG_PARTSTAT_IENABLED   0x00    /* ...implicitly enabled     */
#define SPG_PARTSTAT_RESV       0x10    /* ...reserved bit setting   */
#define SPG_PARTSTAT_DISABLED   0x20    /* ...disabled               */
#define SPG_PARTSTAT_XENABLED   0x30    /* ...explicitly enabled     */
#define SPG_PATHMODE            0x08    /* Path mode bit...          */
#define SPG_PATHMODE_SINGLE     0x00    /* ...single path mode       */
#define SPG_PATHMODE_RESV       0x08    /* ...reserved bit setting   */
#define SPG_RESERVED            0x07    /* Reserved bits, must be 0  */

/* Function control byte for Set Path Group ID command */
#define SPG_SET_MULTIPATH       0x80    /* Set multipath mode        */
#define SPG_SET_COMMAND         0x60    /* Set path command bits...  */
#define SPG_SET_ESTABLISH       0x00    /* ...establish group        */
#define SPG_SET_DISBAND         0x20    /* ...disband group          */
#define SPG_SET_RESIGN          0x40    /* ...resign from group      */
#define SPG_SET_COMMAND_RESV    0x60    /* ...reserved bit setting   */
#define SPG_SET_RESV            0x1F    /* Reserved bits, must be 0  */

/* Perform Subsystem Function order byte for PSF command             */
#define PSF_ORDER_PRSD          0x18    /* Prep for Read Subsys Data */
#define PSF_ACTION_SSD_ATNMSG   0x03    /* ..Attention Message       */
#define PSF_ORDER_SSIC          0x1B    /* Set Special Intercept Cond*/
#define PSF_ORDER_MNS           0x1C    /* Message Not Supported     */
#define PSF_ORDER_AFEL          0x80    /* Activate Forced Error Log.*/
#define PSF_ORDER_DFEL          0x81    /* Deact. Forced Error Log.  */
#define PSF_ACTION_FEL_IMPLICIT 0x01    /* ..Implicit (De)Activate   */
#define PSF_ACTION_FEL_EXPLICIT 0x02    /* ..Explicit (De)Activate   */
#define PSF_ORDER_AAC           0x82    /* Activate Access Control   */
#define PSF_ORDER_DAC           0x83    /* Deact. Access Control     */
#define PSF_ACTION_AC_LWP       0x80    /* ..Logical Write Protect   */
#define PSF_ACTION_AC_DCD       0x10    /* ..Data Compaction Default */
#define PSF_ACTION_AC_DCR       0x02    /* ..Data Check Recovery     */
#define PSF_ACTION_AC_ER        0x01    /* ..Extended Recovery       */
#define PSF_ORDER_RVF           0x90    /* Reset Volume Fenced       */
#define PSF_ORDER_PIN_DEV       0xA1    /* Pin Device                */
#define PSF_ACTION_PIN_CU0      0x00    /* ..Control unit 0          */
#define PSF_ACTION_PIN_CU1      0x01    /* ..Control unit 1          */
#define PSF_ORDER_UNPIN_DEV     0xA2    /* Unpin Device              */
#define PSF_FLAG_ZERO           0x00    /* Must be zero for all ord. */

/* Control Access Function Control                                   */
#define CAC_FUNCTION            0xC0    /* Function control bits     */
#define CAC_SET_PASSWORD        0x00    /* ..Set Password            */
#define CAC_COND_ENABLE         0x80    /* ..Conditional Enable      */
#define CAC_COND_DISABLE        0x40    /* ..Conditional Disable     */

/*-------------------------------------------------------------------*/
/* Definitions for tape device type field in device block            */
/*-------------------------------------------------------------------*/
#define TAPEDEVT_UNKNOWN        0       /* AWSTAPE format disk file  */
#define TAPEDEVT_AWSTAPE        1       /* AWSTAPE format disk file  */
#define TAPEDEVT_OMATAPE        2       /* OMATAPE format disk files */
#define TAPEDEVT_SCSITAPE       3       /* Physical SCSI tape        */
#define TAPEDEVT_HETTAPE        4       /* HET format disk file      */
#define TAPEDEVT_FAKETAPE       5       /* Flex FakeTape disk format */
#define TAPEDEVT_DWTVF          6       /* DWTVF disk format         */

#define TTYPSTR(i) (i==1?"aws":i==2?"oma":i==3?"scsi":i==4?"het":i==5?"fake":i==6?"dwtvf":"unknown")

/*-------------------------------------------------------------------*/
/* Fish - macros for checking SCSI tape device-independent status    */
/*-------------------------------------------------------------------*/
#if defined(OPTION_SCSI_TAPE)
#define STS_TAPEMARK(dev)       GMT_SM      ( (dev)->sstat )
#define STS_EOF(dev)            GMT_EOF     ( (dev)->sstat )
#define STS_BOT(dev)            GMT_BOT     ( (dev)->sstat )
#define STS_EOT(dev)            GMT_EOT     ( (dev)->sstat )
#define STS_EOD(dev)            GMT_EOD     ( (dev)->sstat )
#define STS_WR_PROT(dev)        GMT_WR_PROT ( (dev)->sstat )
#define STS_ONLINE(dev)         GMT_ONLINE  ( (dev)->sstat )
#define STS_NOT_MOUNTED(dev)   (GMT_DR_OPEN ( (dev)->sstat ) || (dev)->fd < 0)
#endif

#define  AUTOLOAD_WAIT_FOR_TAPEMOUNT_INTERVAL_SECS  (5) /* (default) */

/*-------------------------------------------------------------------*/
/* Structure definition for HET/AWS/OMA tape block headers           */
/*-------------------------------------------------------------------*/
/*
 * The integer fields in the HET, AWSTAPE and OMATAPE headers are
 * encoded in the Intel format (i.e. the bytes of the integer are held
 * in reverse order).  For this reason the integers are defined as byte
 * arrays, and the bytes are fetched individually in order to make
 * the code portable across architectures which use either the Intel
 * format or the S/370 format.
 *
 * Block length fields contain the length of the emulated tape block
 * and do not include the length of the header.
 *
 * For the AWSTAPE and HET formats:
 * - the first block has a previous block length of zero
 * - a tapemark is indicated by a header with a block length of zero
 *   and a flag byte of X'40'
 *
 * For the OMATAPE format:
 * - the first block has a previous header offset of X'FFFFFFFF'
 * - a tapemark is indicated by a header with a block length of
 *   X'FFFFFFFF'
 * - each block is followed by padding bytes if necessary to ensure
 *   that the next header starts on a 16-byte boundary
 *
 */
typedef struct _AWSTAPE_BLKHDR
{  /*
    * PROGRAMMING NOTE: note that for AWS tape files, the "current
    * chunk size" comes FIRST and the "previous chunk size" comes
    * second. This is the complete opposite from the way it is for
    * Flex FakeTape. Also note that for AWS the size fields are in
    * LITTLE endian binary whereas for Flex FakeTape they're a BIG
    * endian ASCII hex-string.
    */
    HWORD   curblkl;                    /* Length of this block      */
    HWORD   prvblkl;                    /* Length of previous block  */
    BYTE    flags1;                     /* Flags byte 1 (see below)  */
    BYTE    flags2;                     /* Flags byte 2              */

    /* Definitions for AWSTAPE_BLKHDR flags byte 1 */
#define  AWSTAPE_FLAG1_NEWREC     0x80  /* Start of new record       */
#define  AWSTAPE_FLAG1_TAPEMARK   0x40  /* Tape mark                 */
#define  AWSTAPE_FLAG1_ENDREC     0x20  /* End of record             */
}
AWSTAPE_BLKHDR;

/*-------------------------------------------------------------------*/
/* Structure definition for OMA block header                         */
/*-------------------------------------------------------------------*/
typedef struct _OMATAPE_BLKHDR
{
    FWORD   curblkl;                    /* Length of this block      */
    FWORD   prvhdro;                    /* Offset of previous block
                                           header from start of file */
    FWORD   omaid;                      /* OMA identifier (contains
                                           ASCII characters "@HDF")  */
    FWORD   resv;                       /* Reserved                  */
}
OMATAPE_BLKHDR;

/*-------------------------------------------------------------------*/
/* Structure definition for OMA tape descriptor array                */
/*-------------------------------------------------------------------*/
typedef struct _OMATAPE_DESC
{
    int     fd;                         /* File Descriptor for file  */
    char    filename[256];              /* Filename of data file     */
    char    format;                     /* H=HEADERS,T=TEXT,F=FIXED,X=Tape Mark */
    BYTE    resv;                       /* Reserved for alignment    */
    U16     blklen;                     /* Fixed block length        */
}
OMATAPE_DESC;

/*-------------------------------------------------------------------*/
/* Structure definition for Flex FakeTape block headers              */
/*-------------------------------------------------------------------*/
/*
 * The character length fields in a Flex FakeTape header are in BIG
 * endian ASCII hex. That is to say, when the length field is ASCII
 * "0123" (i.e. 0x30, 0x31, 0x32, 0x33), the length of the block is
 * decimal 291 bytes (0x0123 == 291).
 *
 * The two block length fields are followed by an XOR "check" field
 * calculated as the XOR of the two preceding length fields and is
 * used to verify the integrity of the header.
 * 
 * The Flex FakeTape tape format does not support any flag fields
 * in its header and thus does not support any type of compression.
 */
typedef struct _FAKETAPE_BLKHDR
{  /*
    * PROGRAMMING NOTE: note that for Flex FakeTapes, the "previous
    * chunk size" comes FIRST, followed by the "current chunk size"
    * second. This is the complete opposite from the way it is for
    * AWS tape files. Also note that for Flex FakeTape the size fields
    * are in BIG endian ASCII hex-string whereas for AWS tapes
    * they're LITTLE endian binary.
    */
    char  sprvblkl[4];                  /* length of previous block  */
    char  scurblkl[4];                  /* length of this block      */
    char  sxorblkl[4];                  /* XOR both lengths together */
}
FAKETAPE_BLKHDR;

/*-------------------------------------------------------------------*/
/* Tape Auto-Loader table entry                                      */
/*-------------------------------------------------------------------*/
struct TAPEAUTOLOADENTRY
{
    char  *filename;
    int    argc;                /* max entries == AUTOLOADER_MAX     */
    char **argv;                /* max entries == AUTOLOADER_MAX     */
};

/*-------------------------------------------------------------------*/
/* Tape AUTOMOUNT CCWS directory control                             */
/*-------------------------------------------------------------------*/
struct TAMDIR
{
    TAMDIR    *next;            /* ptr to next entry or NULL         */
    char      *dir;             /* resolved directory value          */
    int        len;             /* strlen(dir)                       */
    int        rej;             /* 1 == reject, 0 == accept          */
};

/*-------------------------------------------------------------------*/
/* Generic media-handler-call parameters block                       */
/*-------------------------------------------------------------------*/
typedef struct _GENTMH_PARMS
{
    int      action;        // action code  (i.e. "what to do")
    DEVBLK*  dev;           // -> device block
    BYTE*    unitstat;      // -> unit status
    BYTE     code;          // CCW opcode
    // TODO: define whatever additional arguments may be needed...
}
GENTMH_PARMS;

/*-------------------------------------------------------------------*/
/* Generic media-handler-call action codes                           */
/*-------------------------------------------------------------------*/
#define  GENTMH_SCSI_ACTION_UPDATE_STATUS       (0)
//efine  GENTMH_AWS_ACTION_xxxxx...             (x)
//efine  GENTMH_HET_ACTION_xxxxx...             (x)
//efine  GENTMH_OMA_ACTION_xxxxx...             (x)

/*-------------------------------------------------------------------*/
/* Tape media I/O function vector table layout                       */
/*-------------------------------------------------------------------*/
struct TAPEMEDIA_HANDLER
{
    int  (*generic)    (GENTMH_PARMS*);                 // (generic call)
    int  (*open)       (DEVBLK*,                        BYTE *unitstat, BYTE code);
    void (*close)      (DEVBLK*);
    int  (*read)       (DEVBLK*, BYTE *buf,             BYTE *unitstat, BYTE code);
    int  (*write)      (DEVBLK*, BYTE *buf, U16 blklen, BYTE *unitstat, BYTE code);
    int  (*rewind)     (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*bsb)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*fsb)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*bsf)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*fsf)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*wtm)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*sync)       (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*dse)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*erg)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*tapeloaded) (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*passedeot)  (DEVBLK*);

    /* readblkid o/p values are returned in BIG-ENDIAN guest format  */
    int  (*readblkid)  (DEVBLK*, BYTE* logical, BYTE* physical);

    /* locateblk i/p value is passed in little-endian host format  */
    int  (*locateblk)  (DEVBLK*, U32 blockid,           BYTE *unitstat, BYTE code);
};

/*-------------------------------------------------------------------*/
/* Functions defined in TAPEDEV.C                                    */
/*-------------------------------------------------------------------*/
extern int   tapedev_init_handler   (DEVBLK *dev, int argc, char *argv[]);
extern int   tapedev_close_device   (DEVBLK *dev );
extern void  tapedev_query_device   (DEVBLK *dev, char **class, int buflen, char *buffer);

extern void  autoload_init          (DEVBLK *dev, int ac,   char **av);
extern int   autoload_mount_first   (DEVBLK *dev);
extern int   autoload_mount_next    (DEVBLK *dev);
extern void  autoload_close         (DEVBLK *dev);
extern void  autoload_global_parms  (DEVBLK *dev, int argc, char *argv[]);
extern void  autoload_clean_entry   (DEVBLK *dev, int ix);
extern void  autoload_tape_entry    (DEVBLK *dev, int argc, char *argv[]);
extern int   autoload_mount_tape    (DEVBLK *dev, int alix);

extern void* autoload_wait_for_tapemount_thread (void *db);

extern int   gettapetype            (DEVBLK *dev, char **short_descr);
extern int   gettapetype_byname     (DEVBLK *dev);
extern int   gettapetype_bydata     (DEVBLK *dev);

extern int   mountnewtape           (DEVBLK *dev, int argc, char **argv);
extern void  GetDisplayMsg          (DEVBLK *dev, char *msgbfr, size_t  lenbfr);
extern int   IsAtLoadPoint          (DEVBLK *dev);
extern void  ReqAutoMount           (DEVBLK *dev);
extern void  UpdateDisplay          (DEVBLK *dev);
extern int   return_false1          (DEVBLK *dev);
extern int   write_READONLY5        (DEVBLK *dev, BYTE *bfr, U16 blklen, BYTE *unitstat, BYTE code);
extern int   is_tapeloaded_filename (DEVBLK *dev,             BYTE *unitstat, BYTE code);
extern int   write_READONLY         (DEVBLK *dev,             BYTE *unitstat, BYTE code);
extern int   no_operation           (DEVBLK *dev,             BYTE *unitstat, BYTE code);
extern int   readblkid_virtual      (DEVBLK*, BYTE* logical,  BYTE* physical);
extern int   locateblk_virtual      (DEVBLK*, U32 blockid,    BYTE *unitstat, BYTE code);
extern int   generic_tmhcall        (GENTMH_PARMS*);

/*-------------------------------------------------------------------*/
/* Functions (and data areas) defined in TAPECCWS.C                  */
/*-------------------------------------------------------------------*/
typedef void TapeSenseFunc( int, DEVBLK*, BYTE*, BYTE );    // (sense handling function)

#define  TAPEDEVTYPELIST_ENTRYSIZE  (5)    // #of int's per 'TapeDevtypeList' table entry

extern int             TapeDevtypeList[];
extern BYTE*           TapeCommandTable[];
extern TapeSenseFunc*  TapeSenseTable[];
//tern BYTE            TapeCommandsXXXX[256]...
extern BYTE            TapeImmedCommands[];

extern int   TapeCommandIsValid     (BYTE code, U16 devtype, BYTE *rustat);
extern void  tapedev_execute_ccw    (DEVBLK *dev, BYTE code, BYTE flags,
                                     BYTE chained, U16 count, BYTE prevcode, int ccwseq,
                                     BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual);
extern void  load_display           (DEVBLK *dev, BYTE *buf, U16 count);

extern void  build_senseX           (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3410       (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3420       (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3410_3420  (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3480_etal  (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3490       (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_3590       (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);
extern void  build_sense_Streaming  (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode);

/*-------------------------------------------------------------------*/
/* Calculate I/O Residual                                            */
/*-------------------------------------------------------------------*/
#define RESIDUAL_CALC(_data_len)         \
    len = (_data_len);                   \
    num = (count < len) ? count : len;   \
    *residual = count - num;             \
    if (count < len) *more = 1

/*-------------------------------------------------------------------*/
/* Assign a unique Message Id for this asynchronous I/O if needed    */
/*-------------------------------------------------------------------*/
#if defined(OPTION_SCSI_TAPE)
  #define INCREMENT_MESSAGEID(_dev)   \
    if ((_dev)->SIC_active)           \
        (_dev)->msgid++
#else
  #define INCREMENT_MESSAGEID(_dev)
#endif // defined(OPTION_SCSI_TAPE)

/*-------------------------------------------------------------------*/
/* Functions defined in AWSTAPE.C                                    */
/*-------------------------------------------------------------------*/
extern int  open_awstape      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern void close_awstape     (DEVBLK *dev);
extern int  passedeot_awstape (DEVBLK *dev);
extern int  rewind_awstape    (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  write_awsmark     (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  sync_awstape      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsb_awstape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsb_awstape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsf_awstape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsf_awstape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  readhdr_awstape   (DEVBLK *dev, off_t blkpos, AWSTAPE_BLKHDR *buf,
                                            BYTE *unitstat, BYTE code);
extern int  read_awstape      (DEVBLK *dev, BYTE *buf,
                                            BYTE *unitstat, BYTE code);
extern int  write_awstape     (DEVBLK *dev, BYTE *buf, U16 blklen,
                                            BYTE *unitstat, BYTE code);

/*-------------------------------------------------------------------*/
/* Functions defined in FAKETAPE.C                                   */
/*-------------------------------------------------------------------*/
extern int  open_faketape      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern void close_faketape     (DEVBLK *dev);
extern int  passedeot_faketape (DEVBLK *dev);
extern int  rewind_faketape    (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  write_fakemark     (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  sync_faketape      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsb_faketape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsb_faketape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsf_faketape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsf_faketape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  readhdr_faketape   (DEVBLK *dev, off_t blkpos,
                                             U16* pprvblkl, U16* pcurblkl,
                                             BYTE *unitstat, BYTE code);
extern int  writehdr_faketape  (DEVBLK *dev, off_t blkpos,
                                             U16   prvblkl, U16   curblkl,
                                             BYTE *unitstat, BYTE code);
extern int  read_faketape      (DEVBLK *dev, BYTE *buf,
                                             BYTE *unitstat, BYTE code);
extern int  write_faketape     (DEVBLK *dev, BYTE *buf, U16 blklen,
                                             BYTE *unitstat, BYTE code);

/*-------------------------------------------------------------------*/
/* Functions defined in HETTAPE.C                                    */
/*-------------------------------------------------------------------*/
extern int  open_het      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern void close_het     (DEVBLK *dev);
extern int  passedeot_het (DEVBLK *dev);
extern int  rewind_het    (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  write_hetmark (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  sync_het      (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsb_het       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsb_het       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsf_het       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsf_het       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  read_het      (DEVBLK *dev, BYTE *buf,
                                        BYTE *unitstat, BYTE code);
extern int  write_het     (DEVBLK *dev, BYTE *buf, U16 blklen,
                                        BYTE *unitstat, BYTE code);

/*-------------------------------------------------------------------*/
/* Functions defined in OMATAPE.C                                    */
/*-------------------------------------------------------------------*/
extern int  open_omatape       (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern void close_omatape      (DEVBLK *dev);
extern void close_omatape2     (DEVBLK *dev);
extern int  rewind_omatape     (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsb_omatape        (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsb_omatape        (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  fsf_omatape        (DEVBLK *dev, BYTE *unitstat, BYTE code);
extern int  bsf_omatape        (DEVBLK *dev, BYTE *unitstat, BYTE code);

extern int  read_omadesc       (DEVBLK *dev);
extern int  fsb_omaheaders     (DEVBLK *dev, OMATAPE_DESC *omadesc,            BYTE *unitstat, BYTE code);
extern int  fsb_omafixed       (DEVBLK *dev, OMATAPE_DESC *omadesc,            BYTE *unitstat, BYTE code);
extern int  read_omaheaders    (DEVBLK *dev, OMATAPE_DESC *omadesc, BYTE *buf, BYTE *unitstat, BYTE code);
extern int  read_omafixed      (DEVBLK *dev, OMATAPE_DESC *omadesc, BYTE *buf, BYTE *unitstat, BYTE code);
extern int  read_omatext       (DEVBLK *dev, OMATAPE_DESC *omadesc, BYTE *buf, BYTE *unitstat, BYTE code);
extern int  read_omatape       (DEVBLK *dev,                        BYTE *buf, BYTE *unitstat, BYTE code);
extern int  readhdr_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                                             long blkpos, S32 *pcurblkl,
                                             S32 *pprvhdro, S32 *pnxthdro,     BYTE *unitstat, BYTE code);

/*-------------------------------------------------------------------*/
/* Functions defined in SCSITAPE.C                                   */
/*-------------------------------------------------------------------*/
// (see SCSITAPE.H)

#endif // __TAPEDEV_H__
