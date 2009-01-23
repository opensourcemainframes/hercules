/* QDIO.C       (c) Copyright Jan Jaeger, 2003-2009                  */
/*              Queued Direct Input Output                           */

// $Id$

/*      This module contains the Signal Adapter instruction          */

// $Log$
// Revision 1.23  2007/06/23 00:04:15  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.22  2007/01/13 07:24:14  bernard
// backout ccmask
//
// Revision 1.21  2007/01/12 15:24:33  bernard
// ccmask phase 1
//
// Revision 1.20  2006/12/08 09:43:29  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_QDIO_C_)
#define _QDIO_C_
#endif

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_QUEUED_DIRECT_IO)

/*-------------------------------------------------------------------*/
/* B274 SIGA  - Signal Adapter                                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(signal_adapter)
{
int     b2;
RADR    effective_addr2;
DEVBLK *dev;                            /* -> device block           */

    S(inst, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Specification exception if invalid function code */
    if(regs->GR_L(0) > SIGA_FC_MAX)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Program check if the ssid including lcss is invalid */
    SSID_CHECK(regs);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_L(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled or is not a QDIO subchannel */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0
        || (dev->pmcw.flag4 & PMCW4_Q) == 0)
    {
#if defined(_FEATURE_QUEUED_DIRECT_IO_ASSIST)
        SIE_INTERCEPT(regs);
#endif
        regs->psw.cc = 3;
        return;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Check that device is QDIO active */
    if ((dev->scsw.flag2 & SCSW2_Q) == 0)
    {
        release_lock (&dev->lock);
        regs->psw.cc = 1;
        return;
    }

    switch(regs->GR_L(0)) {

    case SIGA_FC_R:
    if(dev->hnd->siga_r)
            regs->psw.cc = (dev->hnd->siga_r) (dev, regs->GR_L(2) );
        else
            regs->psw.cc = 3;
        break;

    case SIGA_FC_W:
    if(dev->hnd->siga_w)
            regs->psw.cc = (dev->hnd->siga_w) (dev, regs->GR_L(2) );
        else
            regs->psw.cc = 3;
        break;

    case SIGA_FC_S:
        /* No signalling required for sync requests as we emulate
           a real machine */
        regs->psw.cc = 0;
        break;

    }

    release_lock (&dev->lock);

}
#endif /*defined(FEATURE_QUEUED_DIRECT_IO)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "qdio.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "qdio.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
