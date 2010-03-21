/* GCHAN.C       (c) Copyright Ivan Warren, 2005-2009                */
/* Based on work (c)Roger Bowler, Jan Jaeger & Others 1999-2009      */
/*              Generic channel device handler                       */

// $Id$

/* This code is covered by the QPL Licence                           */
/**CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION****/
/* THIS CODE IS CURRENTLY IN A DEVELOPMENT STAGE AND IS NOT          */
/* OPERATIONAL                                                       */
/* THIS FONCTIONALITY IS NOT YET SUPPORTED                           */
/**CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION*CAUTION****/

/*-------------------------------------------------------------------*/
/* This module contains code to handle a generic protocol to         */
/* communicate with external device handlers.                        */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.11  2007/11/21 22:54:14  fish
// Use new BEGIN_DEVICE_CLASS_QUERY macro
//
// Revision 1.10  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.9  2006/12/08 09:43:21  jj
// Add CVS message log
//

#include "hercules.h"

#include "devtype.h"

#include "hchan.h"

#if defined(OPTION_DYNAMIC_LOAD) && defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
 SYSBLK *psysblk;
 #define sysblk (*psysblk)
#endif

/*
 * Initialisation string for a Generic Subchannel
 *
 * Format :
 *        <method> parms
 *   The 'EXEC' method will attempt to fork/exec the specified
 *       program. If the program dies during device initialisation, the
 *       subchannel will be invalidated (initialisation error). If it fails
 *       afterwards, the subchannel will be put offline and the offload
 *       thread will attempt to restart it at periodic intervals.
 *   The 'CONNECT' method will attempt to establish a TCP connection
 *       to the IP/PORT specified. If the connection fails, the connection
 *       will be attempted at periodic intervals.
 *   The 'ITHREAD' method will spawn a thread using a dynamically loaded module
 */
static int hchan_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
    int rc;
    dev->devtype=0x2880;        /* Temporary until the device is actually initialised */
    while(1)
    {
        if(argc<1)
        {
            WRITEMSG(HHCGCH003E,dev->devnum);
            rc=-1;
            break;
        }

        if(strcasecmp(argv[0],"EXEC")==0)
        {
            rc=hchan_init_exec(dev,argc,argv);
            break;
        }
        if(strcasecmp(argv[0],"CONNECT")==0)
        {
            rc=hchan_init_connect(dev,argc,argv);
            break;
        }
        if(strcasecmp(argv[0],"ITHREAD")==0)
        {
            rc=hchan_init_int(dev,argc,argv);
            break;
        }
        WRITEMSG(HHCGCH001E,dev->devnum,argv[0]);
        rc=-1;
        break;
    }
    if(rc)
    {
        WRITEMSG(HHCGCH002T,dev->devnum);
    }
    WRITEMSG(HHCGCH999W,dev->devnum);
    return(rc);
}

static  int     hchan_init_exec(DEVBLK *dev,int ac,char **av)
{
    UNREFERENCED(dev);
    UNREFERENCED(ac);
    UNREFERENCED(av);
    return(0);
}
static  int     hchan_init_connect(DEVBLK *dev,int ac,char **av)
{
    UNREFERENCED(dev);
    UNREFERENCED(ac);
    UNREFERENCED(av);
    return(0);
}
static  int     hchan_init_int(DEVBLK *dev,int ac,char **av)
{
    UNREFERENCED(dev);
    UNREFERENCED(ac);
    UNREFERENCED(av);
    return(0);
}

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void hchan_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    BEGIN_DEVICE_CLASS_QUERY( "CHAN", dev, class, buflen, buffer );

    snprintf(buffer,buflen,"** CONTROL UNIT OFFLINE **");
}

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int hchan_close_device ( DEVBLK *dev )
{
    UNREFERENCED(dev);
    return 0;
}


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void hchan_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);
    UNREFERENCED(chained);
    UNREFERENCED(count);
    UNREFERENCED(iobuf);
    UNREFERENCED(more);
    UNREFERENCED(residual);

    /* Process depending on CCW opcode */
    switch (code) {
    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    }

}


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND hchan_device_hndinfo = {
        &hchan_init_handler,           /* Device Initialisation      */
        &hchan_execute_ccw,            /* Device CCW execute         */
        &hchan_close_device,           /* Device Close               */
        &hchan_query_device,           /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        NULL,                          /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt0000_LTX_hdl_ddev
#define hdl_depc hdt0000_LTX_hdl_depc
#define hdl_reso hdt0000_LTX_hdl_reso
#define hdl_init hdt0000_LTX_hdl_init
#define hdl_fini hdt0000_LTX_hdl_fini
#endif


#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION


#if defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  #undef sysblk
  HDL_RESOLVER_SECTION;
  {
      HDL_RESOLVE_PTRVAR( psysblk, sysblk );
  }
  END_RESOLVER_SECTION
#endif


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(HCHAN, hchan_device_hndinfo );
    HDL_DEVICE(2860, hchan_device_hndinfo );
    HDL_DEVICE(2870, hchan_device_hndinfo );
    HDL_DEVICE(2880, hchan_device_hndinfo );
    HDL_DEVICE(9032, hchan_device_hndinfo );
}
END_DEVICE_SECTION
#endif
