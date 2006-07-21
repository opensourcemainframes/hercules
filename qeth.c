/* QETH.C       (c) Copyright Jan Jaeger,   1999-2006                */
/*              OSA Express                                          */

/* This module contains device handling functions for the            */
/* OSA Express emulated card                                         */

/* Device module hdtqeth.dll devtype QETH (config)                   */
/* hercules.cnf:                                                     */
/* 0A00-0A02 QETH <optional parameters>                              */

#include "hstdinc.h"

#include "hercules.h"

#include "devtype.h"


#if defined(WIN32) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  SYSBLK *psysblk;
  #define sysblk (*psysblk)
#endif


static BYTE sense_id_bytes[] = { 0xff,
                                 0x17, 0x31, 0x01,            /* D/T */
                                 0x17, 0x32, 0x01,           /* CU/T */
                                 0x00,
                                 0x40, 0xfa, 0x01, 0x00,  /* RCD CIW */
                                 0x03, 0xfc, 0x01, 0x00, /*Ena Q CIW */
                                 0x04, 0xfd, 0x01, 0x00 /* Act Q CIW */
                               };


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int qeth_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
UNREFERENCED(argc);
UNREFERENCED(argv);

logmsg(D_("dev(%4.4x) experimental driver\n"),dev->devnum);

    dev->numdevid = sizeof(sense_id_bytes);
logmsg(D_("senseidnum=%d\n"),dev->numdevid);
    memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));
    dev->devtype = dev->devid[1] << 8 | dev->devid[2];
    
    dev->pmcw.flag4 |= PMCW4_Q;

    if(!group_device(dev,3))
    {
        logmsg(D_("group device(%4.4x) pending\n"),dev->devnum);
        return 0;
    }
    else
    {
    int i;
        logmsg(D_("group = ( "));
        for(i = 0; i < dev->group->acount; i++)
            logmsg("%4.4x ",dev->group->memdev[i]->devnum);
        logmsg(") complete\n");
    }

    return 0;
} /* end function qeth_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void qeth_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    UNREFERENCED(dev);

    *class = "QETH";

    snprintf (buffer, buflen, "\n");

} /* end function qeth_query_device */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int qeth_close_device ( DEVBLK *dev )
{
    UNREFERENCED(dev);

    /* Close the device file */

    return 0;
} /* end function qeth_close_device */


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void qeth_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc = 0;                         /* Return code               */
int     num;                            /* Number of bytes to move   */
int     blocksize = 1024;
#define CONFIG_DATA_SIZE 1024

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);
    UNREFERENCED(chained);
    UNREFERENCED(rc);
    UNREFERENCED(blocksize);

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
logmsg(D_("Write dev(%4.4x)\n"),dev->devnum);
#define WR_SIZE 0x22

        /* Calculate number of bytes to read and set residual count */
        num = (count < WR_SIZE) ? count : WR_SIZE;
        *residual = count - num;
        if (count < WR_SIZE) *more = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
logmsg(D_("Read dev(%4.4x)\n"),dev->devnum);

#define RD_SIZE 0x22
        /* Calculate number of bytes to read and set residual count */
        num = (count < RD_SIZE) ? count : RD_SIZE;
        *residual = count - num;
        if (count < RD_SIZE) *more = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
logmsg(D_("NOP dev(%4.4x)\n"),dev->devnum);
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
logmsg(D_("Sense dev(%4.4x)\n"),dev->devnum);
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
logmsg(D_("Sense ID dev(%4.4x)\n"),dev->devnum);
        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xFA:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/
logmsg(D_("Read Configuration Data dev(%4.4x)\n"),dev->devnum);
        /* Calculate residual byte count */
        num = (count < CONFIG_DATA_SIZE) ? count : CONFIG_DATA_SIZE;
        *residual = count - num;
        if (count < CONFIG_DATA_SIZE) *more = 1;

        /* Clear the configuration data area */
        memset (iobuf, 0x00, CONFIG_DATA_SIZE);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

        
    case 0xFC:
    /*---------------------------------------------------------------*/
    /* ESTABLISH QUEUES                                              */
    /*---------------------------------------------------------------*/
logmsg(D_("Establish Queues dev(%4.4x)\n"),dev->devnum);
        /* Calculate residual byte count */
        num = (count < CONFIG_DATA_SIZE) ? count : CONFIG_DATA_SIZE;
        *residual = count - num;
        if (count < CONFIG_DATA_SIZE) *more = 1;

        memset (iobuf, 0x00, CONFIG_DATA_SIZE);

        /* INCOMPLETE ZZ
         * QUEUES MUST BE SETUP HERE
         */

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xFD:
    /*---------------------------------------------------------------*/
    /* ACTIVATE QUEUES                                               */
    /*---------------------------------------------------------------*/
logmsg(D_("Activate Queues dev(%4.4x)\n"),dev->devnum);
        /* Calculate residual byte count */
        num = (count < CONFIG_DATA_SIZE) ? count : CONFIG_DATA_SIZE;
        *residual = count - num;
        if (count < CONFIG_DATA_SIZE) *more = 1;

        memset (iobuf, 0x00, CONFIG_DATA_SIZE);

        /* INCOMPLETE ZZ
         * QUEUES MUST BE HANDLED HERE, THIS CCW WILL ONLY EXIT
         * IN CASE OF AN ERROR OR A HALT, CLEAR OR CANCEL SIGNAL
         */

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
logmsg(D_("Unkown CCW dev(%4.4x) code(%2.2x)\n"),dev->devnum,code);
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function qeth_execute_ccw */


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Input                                     */
/*-------------------------------------------------------------------*/
static int qeth_initiate_input(DEVBLK *dev, U32 qmask)
{
    UNREFERENCED(qmask);

logmsg(D_("SIGA-r dev(%4.4x) qmask(%8.8x)\n"),dev->devnum);
    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output                                    */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output(DEVBLK *dev, U32 qmask)
{
    UNREFERENCED(qmask);

logmsg(D_("SIGA-w dev(%4.4x) qmask(%8.8x)\n"),dev->devnum);
    return 0;
}


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND qeth_device_hndinfo =
{
        &qeth_init_handler,     /* Device Initialisation      */
        &qeth_execute_ccw,      /* Device CCW execute         */
        &qeth_close_device,     /* Device Close               */
        &qeth_query_device,     /* Device Query               */
        NULL,                   /* Device Start channel pgm   */
        NULL,                   /* Device End channel pgm     */
        NULL,                   /* Device Resume channel pgm  */
        NULL,                   /* Device Suspend channel pgm */
        NULL,                   /* Device Read                */
        NULL,                   /* Device Write               */
        NULL,                   /* Device Query used          */
        NULL,                   /* Device Reserve             */
        NULL,                   /* Device Release             */
        NULL,                   /* Device Attention           */
        NULL,                   /* Immediate CCW Codes        */
        &qeth_initiate_input,   /* Signal Adapter Input       */
        &qeth_initiate_output,  /* Signal Adapter Output      */
        NULL,                   /* Hercules suspend           */
        NULL                    /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdtqeth_LTX_hdl_ddev
#define hdl_depc hdtqeth_LTX_hdl_depc
#define hdl_reso hdtqeth_LTX_hdl_reso
#define hdl_init hdtqeth_LTX_hdl_init
#define hdl_fini hdtqeth_LTX_hdl_fini
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
    HDL_DEVICE(QETH, qeth_device_hndinfo );
}
END_DEVICE_SECTION
#endif
