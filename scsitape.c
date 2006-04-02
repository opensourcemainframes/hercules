////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.C   --   Hercules SCSI tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005-2006. Released under
// the Q Public License (http://www.conmicro.cx/hercules/herclic.html)
// as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////
//
//  This module contains only the support for SCSI tapes. Please see
//  the 'tapedev.c' (and possibly other) source module(s) for infor-
//  mation regarding other supported emulated tape/media formats.
//
////////////////////////////////////////////////////////////////////////////////////

#include "hstdinc.h"

#include "hercules.h"
#include "scsitape.h"

#if defined(OPTION_SCSI_TAPE)

//#define  ENABLE_TRACING_STMTS     // (Fish: DEBUGGING)

#ifdef ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
#else
  #undef  TRACE
  #define TRACE       1 ? ((void)0) : logmsg
  #undef  ASSERT
  #define ASSERT(a)
  #undef  VERIFY
  #define VERIFY(a)   ((void)(a))
#endif

/*-------------------------------------------------------------------*/
/*                 Open a SCSI tape device                           */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*                                                                   */
/* Note that not having a tape mounted is a non-fatal error (rc==0)) */
/*                                                                   */
/* If the status indicates the tape is not mounted or a good status  */
/* cannot otherwise be obtained, the file descriptor is CLOSED but   */
/* THE RETURN CODE IS STILL == 0 !!!!                                */
/*                                                                   */
/*                       ** WARNING! **                              */
/*                                                                   */
/* The caller MUST check for a valid (non-negative) file descriptor  */
/* before trying to use it if this function returns 0 == success!!   */
/*                                                                   */
/* A success == 0 return means the device filename CAN be opened,    */
/* but not necessarily that the file can be used! If the file cannot */
/* be used (i.e. no tape mounted), the file is CLOSED but the return */
/* code is still == 0.                                               */
/*                                                                   */
/* The return code is -1 ONLY when the filename itself is invalid    */
/* and the device file thus cannot even be opened.                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
int open_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int rc;

    /* Is an open for this device already in progress? */
    obtain_lock( &dev->stape_getstat_lock );
    if ( dev->stape_mountmon_tid )
    {
        release_lock( &dev->stape_getstat_lock );
        /* Yes. Device is good but no tape is mounted (yet) */
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (quick exit; in progress == open success)
    }
    release_lock( &dev->stape_getstat_lock );

    ASSERT( dev->fd < 0 );  // (sanity check)
    dev->fd = -1;
    dev->sstat = GMT_DR_OPEN( -1 );

    /* Open the SCSI tape device */
    dev->readonly = 0;
    rc = open_tape (dev->filename, O_RDWR | O_BINARY);
    if (rc < 0 && EROFS == errno )
    {
        dev->readonly = 1;
        rc = open_tape (dev->filename, O_RDONLY | O_BINARY);
    }

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg (_("HHCTA024E Error opening %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; // (FATAL error; device cannot be opened)
    }

    /* Store the file descriptor in the device block */
    dev->fd = rc;

    /* Obtain the initial tape device/media status information */
    /* and start the mount-monitoring thread if option enabled */
    update_status_scsitape( dev );

    /* Asynchronous open now in progress? */
    obtain_lock( &dev->stape_getstat_lock );
    if ( dev->stape_mountmon_tid )
    {
        release_lock( &dev->stape_getstat_lock );
        /* Yes. Device is good but no tape is mounted (yet) */
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (quick exit; in progress == open success)
    }
    release_lock( &dev->stape_getstat_lock );

    /* Finish up the open process... */
    if ( STS_NOT_MOUNTED( dev ) )
    {
        /* Intervention required if no tape is currently mounted.
           Note: we return "success" because the filename is good
           (device CAN be opened) but close the file descriptor
           since there's no tape currently mounted on the drive.*/
        dev->fd = -1;
        close_tape( rc );
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (because device file IS valid and CAN be opened)
    }

    /* Set variable length block processing to complete the open */
    if ( finish_scsitape_open( dev, unitstat, code ) != 0 )
    {
        /* We cannot use this device; fail the open.
           'finish_scsitape_open' has already issued
           the error message and closed the device. */
        return -1;  // (open failure)
    }
    return 0;       // (open success)

} /* end function open_scsitape */

/*-------------------------------------------------------------------*/
/*  Finish SCSI Tape open:  sets variable length block i/o mode...   */
/*  Returns 0 == success, -1 = failure.                              */
/*-------------------------------------------------------------------*/
/*  THIS FUNCTION IS AN INTEGRAL PART OF TAPE OPEN PROCESSING!       */
/*  If this function fails then the overall tape device open fails!  */
/*-------------------------------------------------------------------*/
int finish_scsitape_open( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Since a tape was just mounted, reset the blockid back to zero */

    dev->blockid = 0;
    dev->poserror = 0;

    /* Set the tape device to process variable length blocks */

    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if (rc < 0)
    {
        /* Device cannot be used; fail the open */
        int save_errno = errno;
        rc = dev->fd;
        dev->fd = -1;
        close_tape( rc );
        errno = save_errno;
        logmsg (_("HHCTA030E Error setting attributes for %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; /* (fatal error) */
    }

    return 0;  /* (success) */

} /* end function finish_scsitape_open */

/*-------------------------------------------------------------------*/
/* Close SCSI tape device file                                       */
/*-------------------------------------------------------------------*/
void close_scsitape(DEVBLK *dev)
{
    int fd = dev->fd; dev->fd = -1;

    if (fd < 0) return;

    if (dev->stape_close_rewinds)
    {
        struct mtop opblk;
//      opblk.mt_op    = MTLOAD;    // (not sure which is more correct)
        opblk.mt_op    = MTREW;
        opblk.mt_count = 1;
        ioctl_tape ( fd, MTIOCTOP, (char*)&opblk) ;
    }

    close_tape( fd );
    dev->sstat = GMT_DR_OPEN( -1 );
    dev->blockid = -1;
    dev->curfilen = 0;
    dev->poserror = 1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;

} /* end function close_scsitape */

/*-------------------------------------------------------------------*/
/* Read a block from a SCSI tape device                              */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int read_scsitape (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int  rc;
/* int  save_errno; */

    rc = read_tape (dev->fd, buf, MAX_BLKLEN);

    if (rc >= 0)
    {
        dev->blockid++;

        /* Increment current file number if tapemark was read */
        if (rc == 0)
            dev->curfilen++;

        /* Return block length or zero if tapemark  */
        return rc;
    }

    /* Handle read error condition */

    logmsg (_("HHCTA032E Error reading data block from %u:%4.4X=%s; errno=%d: %s\n"),
            SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror(errno));

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);

    return -1;

} /* end function read_scsitape */

/*-------------------------------------------------------------------*/
/* Write a block to a SCSI tape device                               */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_scsitape (DEVBLK *dev, BYTE *buf, U16 len,
                        BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;

    /* Write data block to SCSI tape device */

    rc = write_tape (dev->fd, buf, len);

    if (rc >= len)
    {
        dev->blockid++;
        return 0;
    }

    /* Handle write error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA033E Error writing data block to %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));

        update_status_scsitape( dev );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function write_scsitape */

/*-------------------------------------------------------------------*/
/* Write a tapemark to a SCSI tape device                            */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_scsimark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Write tape mark to SCSI tape */

    opblk.mt_op    = MTWEOF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if (rc >= 0)
    {
        /* Increment current file number since tapemark was written */
        dev->curfilen++;
        /* (tapemarks count as block identifiers too!) */
        dev->blockid++;
        return 0;
    }

    /* Handle write error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA034E Error writing tapemark to %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));

        update_status_scsitape( dev );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function write_scsimark */

/*-------------------------------------------------------------------*/
/* Forward space over next block of SCSI tape device                 */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Forward space block on SCSI tape */

    opblk.mt_op    = MTFSR;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->blockid++;
        /* Return +1 to indicate forward space successful */
        return +1;
    }

    /* Since the MT driver does not set EOF status
       when forward spacing over a tapemark, the best
       we can do is to assume that an I/O error means
       that a tapemark was detected... (in which case
       we incremnent the file number and return 0).
    */
    // NOTE: I'm suspecting the reason the MT driver
    // wasn't setting EOF when it spaced over a tapemark
    // was possibly because we were inadvertently causing
    // the EOF status to get reset/lost as a result of
    // our calling the 'get status' function. I hope
    // it now works correctly on *nix with the below
    // commented out. -- Fish
#if 0
    save_errno = errno;
    {
        update_status_scsitape( dev );
    }
    errno = save_errno;
#endif

    if ( EIO == errno && STS_EOF(dev) )
    {
        dev->curfilen++;
        dev->blockid++;
        /* Return 0 to indicate tapemark was spaced over */
        return 0;
    }

    /* Bona fide forward space block error ... */

    save_errno = errno;
    {
        logmsg (_("HHCTA035E Forward space block error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function fsb_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of SCSI tape device                   */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block is a tapemark, the return value is zero,             */
/* and the current file number in the device block is decremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* PROGRAMMING NOTE: There is currently no way to distinguish
    ** between a "normal" backspace-block error and a "backspaced-
    ** into-loadpoint" i/o error, since the only error indication
    ** we get [in response to a backspace block attempt] is simply
    ** 'EIO'. (Interrogating the status AFTER the fact (to see if
    ** we're positioned at loadpoint) doesn't tell us whether we
    ** were already positioned at loadpoint *before* the error was
    ** was encountered or whether we're only positioned ar load-
    ** point because we *did* in fact backspace over the very first
    ** block on the tape (and are thus now, after the fact, sitting
    ** at loadpoint because we *did* backspace over a block but it
    ** just got an error for some reason).
    **
    ** Thus, we have absolutely no choice here but to retrieve the
    ** status BEFORE we attempt the i/o to see if we're ALREADY at
    ** loadpoint. If we are, then we immediately return an error
    ** ("backspaced-into-loadpoint") *without* even attemting the
    ** i/o at all. If we're *not* already sitting at loadpoint how-
    ** ever, then we go ahead an attempt the i/o and then check for
    ** an error afterwards.
    */

    /* Obtain tape status before backward space... (no choice!) */
    update_status_scsitape( dev );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace i/o...*/

    opblk.mt_op    = MTBSR;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->blockid--;
        /* Return +1 to indicate backspace successful */
        return +1;
    }

    /* Since the MT driver does not set EOF status
       when backspacing over a tapemark, the best
       we can do is to assume that an I/O error means
       that a tapemark was detected... (in which case
       we decrement the file number and return 0).
    */
    // NOTE: I'm suspecting the reason the MT driver
    // wasn't setting EOF when it spaced over a tapemark
    // was possibly because we were inadvertently causing
    // the EOF status to get reset/lost as a result of
    // our calling the 'get status' function. I hope
    // it now works correctly on *nix with the below
    // commented out. -- Fish
#if 0
    save_errno = errno;
    {
        update_status_scsitape( dev );
    }
    errno = save_errno;
#endif

    if ( EIO == errno && STS_EOF(dev) )
    {
        dev->curfilen--;
        dev->blockid--;
        /* Return 0 to indicate tapemark was spaced over */
        return 0;
    }

    /* Bona fide backspace block error ... */

    save_errno = errno;
    {
        logmsg (_("HHCTA036E Backspace block error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
    {
        if ( EIO == errno && STS_BOT(dev) )
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        else
            build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
    }

    return -1;

} /* end function bsb_scsitape */

/*-------------------------------------------------------------------*/
/* Forward space to next file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is incremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Forward space file on SCSI tape */

    opblk.mt_op    = MTFSF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    /* Since we have no idea how many blocks we've skipped over
       (as a result of doing the forward-space file), we now have
       no clue as to what the proper current blockid should be.
    */
    dev->blockid = -1;      // (actual position now unknown!)
    dev->poserror = 1;      // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen++;
        return 0;
    }

    /* Handle error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA037E Forward space file error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function fsf_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is decremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* PROGRAMMING NOTE: There is currently no way to distinguish
    ** between a "normal" backspace-file error and a "backspaced-
    ** into-loadpoint" i/o error, since the only error indication
    ** we get [in response to a backspace file attempt] is simply
    ** 'EIO'. (Interrogating the status AFTER the fact (to see if
    ** we're positioned at loadpoint) doesn't tell us whether we
    ** were already positioned at loadpoint *before* the error was
    ** was encountered or whether we're only positioned ar load-
    ** point because we *did* in fact backspace over a BOT tape-
    ** mark on the tape (and are thus now, after the fact, sitting
    ** at loadpoint because we *did* backspace over a tape-mark
    ** but it just got an error for some reason).
    **
    ** Thus, we have absolutely no choice here but to retrieve the
    ** status BEFORE we attempt the i/o to see if we're ALREADY at
    ** loadpoint. If we are, then we immediately return an error
    ** ("backspaced-into-loadpoint") *without* even attemting the
    ** i/o at all. If we're *not* already sitting at loadpoint how-
    ** ever, then we go ahead an attempt the i/o and then check for
    ** an error afterwards.
    */

    /* Obtain tape status before backward space... (no choice!) */
    update_status_scsitape( dev );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace i/o...*/

    opblk.mt_op    = MTBSF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    /* Since we have no idea how many blocks we've skipped over
       (as a result of doing the back-space file), we now have
       no clue as to what the proper current blockid should be.
    */
    dev->blockid = -1;      // (actual position now unknown!)
    dev->poserror = 1;      // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen--;
        return 0;
    }

    /* Handle error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA038E Backspace file error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
    {
        if ( EIO == errno && STS_BOT(dev) )
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        else
            build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
    }

    return -1;

} /* end function bsf_scsitape */

/*-------------------------------------------------------------------*/
/* Rewind an SCSI tape device                                        */
/*-------------------------------------------------------------------*/
int rewind_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
int rc;
/* int  save_errno; */
struct mtop opblk;

//  opblk.mt_op    = MTLOAD;    // (not sure which is more correct)
    opblk.mt_op    = MTREW;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->sstat |= GMT_BOT( -1 );  // (forced)
        dev->blockid = 0;
        dev->curfilen = 0;
        dev->poserror = 0;
        return 0;
    }

    dev->poserror = 1;      // (because the rewind failed)
    dev->blockid  = -1;     // (because the rewind failed)
    dev->curfilen = -1;     // (because the rewind failed)

    logmsg (_("HHCTA073E Error rewinding %u:%4.4X=%s; errno=%d:  %s\n"),
            SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror(errno));

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

    return -1;

} /* end function rewind_scsitape */

/*-------------------------------------------------------------------*/
/* Rewind Unload a SCSI tape device (and CLOSE it too!)              */
/*-------------------------------------------------------------------*/
void rewind_unload_scsitape(DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int rc;
struct mtop opblk;

//  opblk.mt_op    = MTUNLOAD;  // (not sure which is more correct)
    opblk.mt_op    = MTOFFL;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        if ( dev->ccwtrace || dev->ccwstep )
            logmsg (_("HHCTA077I Tape %u:%4.4X unloaded\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum);
        close_scsitape( dev ); // (required for REW UNLD)
        return;
    }

    dev->poserror = 1;  // (because the rewind-unload failed)
    dev->curfilen = -1; // (because the rewind-unload failed)
    dev->blockid  = -1; // (because the rewind-unload failed)

    logmsg ( _("HHCTA076E Error unloading %u:%4.4X=%s; errno=%d: %s\n" ),
            SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror( errno ) );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

} /* end function rewind_unload_scsitape */

/*-------------------------------------------------------------------*/
/* Erase Gap                                                         */
/*-------------------------------------------------------------------*/
int erg_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
#if defined( OPTION_SCSI_ERASE_GAP )

    struct mtop opblk;

    opblk.mt_op    = MTERASE;
    opblk.mt_count = 0;         // (zero means "short" erase-gap)

    if ( ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk ) < 0 )
    {
        logmsg (_("HHCTA999E Erase Gap error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }
#endif // defined( OPTION_SCSI_ERASE_GAP )

    build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
    return 0;       // (treat as nop)

} /* end function erg_scsitape */

/*-------------------------------------------------------------------*/
/* Data Security Erase                                               */
/*-------------------------------------------------------------------*/
int dse_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
#if defined( OPTION_SCSI_ERASE_TAPE )

    struct mtop opblk;

    opblk.mt_op    = MTERASE;
    opblk.mt_count = 1;         // (one means "long" erase-tape)

    if ( ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk ) < 0 )
    {
        logmsg (_("HHCTA999E Data Security Erase error on %u:%4.4X=%s; errno=%d: %s\n"),
                SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }
#endif // defined( OPTION_SCSI_ERASE_TAPE )

    build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
    return 0;       // (treat as nop)

} /* end function dse_scsitape */

/*-------------------------------------------------------------------*/
/* Determine if the tape is Ready   (tape drive door status)         */
/* Returns:  true/false:  1 = ready,   0 = NOT ready                 */
/*-------------------------------------------------------------------*/
int driveready_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    /* Update tape status */
    update_status_scsitape( dev );

    return ( !STS_NOT_MOUNTED( dev ) );
} /* end function driveready_scsitape */

/*-------------------------------------------------------------------*/
/* get_stape_status_thread                                           */
/*-------------------------------------------------------------------*/
static
void* get_stape_status_thread( void *db )
{
    DEVBLK* dev = db;                   /* Pointer to device block   */
    struct mtget mtget;                 /* device status work field  */
    int priority;                       /* Thread priority           */
    int rc;                             /* return code               */

    // Set thread priority BELOW that of the cpu and device threads
    // in order to minimize whatever impact we may have on them...

    priority = ((sysblk.cpuprio > sysblk.devprio) ? sysblk.cpuprio : sysblk.devprio ) + 8;
    SETMODE(ROOT); VERIFY(setpriority(PRIO_PROCESS,0,priority)==0); SETMODE(USER);

    obtain_lock( &dev->stape_getstat_lock );
    dev->stape_getstat_busy = 0;
    broadcast_condition( &dev->stape_getstat_cond );

    for (;;)
    {
        // Wait for work...

        wait_condition( &dev->stape_getstat_cond, &dev->stape_getstat_lock );

        // Check if time to exit...

        if (0
            || sysblk.shutdown
            || dev->stape_getstat_exit
        )
            break;

        // Perform work...

        // Since this may take quite a while to do if there's no tape
        // mounted, we release the lock before attempting to retrieve
        // the status and then reacquire it afterwards before moving
        // the results back into the device block...

        dev->stape_getstat_busy = 1;
        release_lock( &dev->stape_getstat_lock );

        // (this is the call that may take a LOOOONG time to complete)

        if ( dev->fd > 0 )
            rc = ioctl_tape( dev->fd, MTIOCGET, (char*)&mtget );
        else
            rc = -1;

        // Since the above may have taken quite a while to complete
        // (up to *10* seconds on Windows!) we need to check again
        // if we're to exit since things could have changed quite
        // dramatically during that time...

        obtain_lock( &dev->stape_getstat_lock );
        dev->stape_getstat_busy = 0;

        if (0
            || sysblk.shutdown
            || dev->stape_getstat_exit
        )
            break;

        // Indicate work completed...

        if ( 0 == rc )
            dev->stape_getstat_sstat = mtget.mt_gstat;

        broadcast_condition( &dev->stape_getstat_cond );
    }

    // Indicate we're going away...

    dev->stape_getstat_tid = 0;
    broadcast_condition( &dev->stape_getstat_cond );
    release_lock( &dev->stape_getstat_lock );

    return NULL;

} /* end function get_stape_status_thread */


/*-------------------------------------------------------------------*/
/* kill_stape_status_thread                                          */
/*-------------------------------------------------------------------*/
void kill_stape_status_thread ( DEVBLK *dev )
{
    // (NOTE: also kills mount-monitoring thread too if active)
    obtain_lock( &dev->stape_getstat_lock );
    dev->stape_getstat_exit = 1;
    broadcast_condition( &dev->stape_getstat_cond );
//  wait_condition( &dev->stape_getstat_cond, &dev->stape_getstat_lock );
    timed_wait_condition_relative_usecs
    (
        &dev->stape_getstat_cond,   // ptr to condition to wait on
        &dev->stape_getstat_lock,   // ptr to controlling lock (must be held!)
        10*1000,                    // max #of microseconds to wait
        NULL                        // [OPTIONAL] ptr to tod value (may be NULL)
    );
    release_lock( &dev->stape_getstat_lock );
//  join_thread(dev->stape_getstat_tid,NULL);

} /* end function kill_stape_status_thread */


/*-------------------------------------------------------------------*/
/* scsi_get_status_fast                                              */
/*-------------------------------------------------------------------*/
static
void scsi_get_status_fast( DEVBLK* dev )
{
    struct timeval wait_tod;

    obtain_lock( &dev->stape_getstat_lock );

    // Create the status retrieval thread if it hasn't been yet...

    if ( !dev->stape_getstat_tid )
    {
        dev->stape_getstat_busy = 0;
        dev->stape_getstat_exit = 0;

        VERIFY
        (
            create_thread
            (
                &dev->stape_getstat_tid,
                &sysblk.joinattr,
                get_stape_status_thread,
                dev,
                "get_stape_status_thread"
            )
            == 0
        );

        wait_condition( &dev->stape_getstat_cond, &dev->stape_getstat_lock );
    }

    // Wake it up...

    if (!dev->stape_getstat_busy)
        broadcast_condition( &dev->stape_getstat_cond );

    // Wait only so long for status to be retrieved...

    gettimeofday( &wait_tod, NULL );

    for (;;)
    {
        if (0 != timed_wait_condition_relative_usecs
        (
            &dev->stape_getstat_cond,   // ptr to condition to wait on
            &dev->stape_getstat_lock,   // ptr to controlling lock (must be held!)
            10*1000,                    // max #of microseconds to wait
            &wait_tod                   // [OPTIONAL] ptr to tod value (may be NULL)
        ))
            break;  // (timeout; keep use existing status)

        // If request was completed, then use the new status...

        if (!dev->stape_getstat_busy)
        {
            // Updated status retrieved; update device block...
            dev->sstat = dev->stape_getstat_sstat;
            break;
        }

        // Else spurious wakeup; keep waiting...
    }

    release_lock( &dev->stape_getstat_lock );

} /* end function scsi_get_status_fast */


/*-------------------------------------------------------------------*/
/* Update SCSI tape status (and display it if CCW tracing is active) */
/*-------------------------------------------------------------------*/
void update_status_scsitape( DEVBLK* dev )
{
    if ( dev->fd < 0 )
    {
        // The device is offline and cannot currently be used.
        dev->sstat = GMT_DR_OPEN(-1);
    }
    else
    {
        scsi_get_status_fast( dev );
    }

    /* Display tape status if tracing is active */
    if ( dev->ccwtrace || dev->ccwstep )
    {
        char  buf[256];

        snprintf
        (
            buf, sizeof(buf),

            "%u:%4.4X filename=%s (%s), sstat=0x%8.8X: %s %s"

            ,SSID_TO_LCSS(dev->ssid)
            ,dev->devnum
            ,( (dev->filename[0]) ? (dev->filename) : ("(undefined)") )
            ,( (dev->fd   <   0 ) ?   ("closed")    : (          "opened"  ) )
            ,dev->sstat
            ,STS_ONLINE(dev)      ? "ON-LINE" : "OFF-LINE"
            ,STS_NOT_MOUNTED(dev) ? "NO-TAPE" : "READY"
        );

        if ( STS_TAPEMARK(dev) ) strlcat ( buf, " TAPE-MARK"    , sizeof(buf) );
        if ( STS_EOF     (dev) ) strlcat ( buf, " END-OF-FILE"  , sizeof(buf) );
        if ( STS_BOT     (dev) ) strlcat ( buf, " LOAD-POINT"   , sizeof(buf) );
        if ( STS_EOT     (dev) ) strlcat ( buf, " END-OF-TAPE"  , sizeof(buf) );
        if ( STS_EOD     (dev) ) strlcat ( buf, " END-OF-DATA"  , sizeof(buf) );
        if ( STS_WR_PROT (dev) ) strlcat ( buf, " WRITE-PROTECT", sizeof(buf) );

        logmsg ( _("HHCTA023I %s\n"), buf );
    }

	//               AUTO-SCSI-MOUNT
	//
    // If no tape is currently mounted on this device,
    // kick off the tape mount monitoring thread that
    // will monitor for tape mounts (if it doesn't al-
    // ready still exist)...

    obtain_lock( &dev->stape_getstat_lock );

    if (1
        &&  STS_NOT_MOUNTED( dev )
        && !dev->stape_mountmon_tid
        &&  sysblk.auto_scsi_mount_secs
    )
    {
        VERIFY
        (
            create_thread
            (
                &dev->stape_mountmon_tid,
                &sysblk.detattr,
                scsi_tapemountmon_thread,
                dev,
                "scsi_tapemountmon_thread"
            )
            == 0
        );
    }

    release_lock( &dev->stape_getstat_lock );

} /* end function update_status_scsitape */

/*-------------------------------------------------------------------*/
/*                  ASYNCHRONOUS TAPE OPEN                           */
/* SCSI tape tape-mount monitoring thread (monitors for tape mounts) */
/* Auto-started by 'update_status_scsitape' when it notices there is */
/* no tape mounted on whatever device it's checking the status of,   */
/* or by the ReqAutoMount function for unsatisfied mount requests.   */
/*-------------------------------------------------------------------*/
void *scsi_tapemountmon_thread( void *db )
{
    struct timeval interval_tod;
    BYTE tape_was_mounted;
    DEVBLK* dev = db;
    int priority;
    int fd, timeout, exit = 0;

    // Set thread priority BELOW that of the cpu and device threads
    // in order to minimize whatever impact we may have on them...

    priority = ((sysblk.cpuprio > sysblk.devprio) ? sysblk.cpuprio : sysblk.devprio ) + 8;
    SETMODE(ROOT); VERIFY(setpriority(PRIO_PROCESS,0,priority)==0); SETMODE(USER);

    logmsg
    (
        _( "HHCTA200I SCSI-Tape mount-monitoring thread started;\n"
           "          dev=%u:%4.4X, tid="TIDPAT", pri=%d, pid=%d\n" )

        ,SSID_TO_LCSS(dev->ssid)
        ,dev->devnum
        ,thread_id()
        ,getpriority(PRIO_PROCESS,0)
        ,getpid()
    );

    for (;;)
    {
        // Wait for someone to signal us to exit
        // or for our timeout interval to expire...

        obtain_lock( &dev->stape_getstat_lock );

        gettimeofday( &interval_tod, NULL );

        for (;;)
        {
            timeout = timed_wait_condition_relative_usecs
            (
                &dev->stape_getstat_cond,               // ptr to condition to wait on
                &dev->stape_getstat_lock,               // ptr to controlling lock (must be held!)
                sysblk.auto_scsi_mount_secs * 1000000,  // max #of microseconds to wait
                &interval_tod                           // [OPTIONAL] ptr to tod value (may be NULL)
            );

            // Check for exit request each time we're woken...
            if (0
                || sysblk.shutdown
                || !sysblk.auto_scsi_mount_secs
                || dev->stape_getstat_exit
            )
            {
                dev->stape_getstat_exit = exit = 1;
                broadcast_condition( &dev->stape_getstat_cond );
                break;  // (time to exit)
            }

            // If timeout then time to get back to work...
            if (timeout)
                break;  // (time to try open again)

            // Else spurious wakeup; go back to sleep...
        }

        release_lock( &dev->stape_getstat_lock );

        if (exit) break;    // (exit when told)

        // Try the open again...

        if ( (fd = dev->fd) < 0 )
        {
            dev->readonly = 0;
            fd = open_tape (dev->filename, O_RDWR | O_BINARY);
            if (fd < 0 && EROFS == errno )
            {
                dev->readonly = 1;
                fd = open_tape (dev->filename, O_RDONLY | O_BINARY);
            }

            if (fd < 0)
            {
                logmsg (_("HHCTA024E Error opening %u:%4.4X=%s; errno=%d: %s\n"),
                        SSID_TO_LCSS(dev->ssid), dev->devnum,
                        dev->filename, errno, strerror(errno));
                break; // (can't open device!)
            }

            dev->fd = fd;   // (so far so good)
        }

        // Retrieve status...
        scsi_get_status_fast( dev );

        // Mounted yet?
        if ( !STS_NOT_MOUNTED( dev ) )
            break; // YEP!

        // Close device and go back to sleep...
        dev->fd = -1;
        close_tape( fd );
    }

    // Finish open processing...

    tape_was_mounted = ( !exit && !STS_NOT_MOUNTED( dev ) );

    if ( tape_was_mounted )
    {
        // Set drive to variable length block processing mode, etc...
        if ( finish_scsitape_open( dev, NULL, 0 ) == 0 )
        {
            // Notify the guest that the tape has now been loaded by
            // presenting an unsolicited device attention interrupt...
            device_attention( dev, CSW_DE );
        }
        else
        {
            /* We cannot use this device; fail the open.
               'finish_scsitape_open' has already issued
               the error message and closed the device. */
        }
    }

    logmsg
    (
        _( "HHCTA299I SCSI-Tape mount-monitoring thread ended;\n"
           "          dev=%u:%4.4X, tid="TIDPAT", pid=%d\n" )

        ,SSID_TO_LCSS(dev->ssid)
        ,dev->devnum
        ,thread_id()
        ,getpid()
    );

    // Notify the interested parties that we're done
    // so they can know to start us back up again
    // if this drive ever has its tape unloaded again...

    obtain_lock( &dev->stape_getstat_lock );
    dev->stape_mountmon_tid = 0;  // (we're going away)
    release_lock( &dev->stape_getstat_lock );

    return NULL;

} /* end function scsi_tapemountmon_thread */

#endif // defined(OPTION_SCSI_TAPE)
