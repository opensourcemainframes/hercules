/* QETH.C       (c) Copyright Jan Jaeger,   1999-2011                */
/*              OSA Express                                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This module contains device handling functions for the            */
/* OSA Express emulated card                                         */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */

/* Device module hdtqeth.dll devtype QETH (config)                   */
/* hercules.cnf:                                                     */
/* 0A00-0A02 QETH <optional parameters>                              */
/* Default parm:  iface /dev/net/tun                                 */

// #define DEBUG

#include "hstdinc.h"

#include "hercules.h"

#include "devtype.h"

#include "chsc.h"

#include "qeth.h"

#include "tuntap.h"


#if defined(WIN32) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  SYSBLK *psysblk;
  #define sysblk (*psysblk)
#endif


static const BYTE sense_id_bytes[] = {
    0xFF,
    0x17, 0x31, 0x01,                   // Control Unit Type
    0x17, 0x32, 0x01,                   // Device Type
    0x00,
    0x40, OSA_RCD,0x00, 0x80,           // Read Configuration Data CIW
    0x43, OSA_EQ, 0x10, 0x00,           // Establish Queues CIW
    0x44, OSA_AQ, 0x00, 0x00            // Activate Queues CIW
};


static const BYTE read_configuration_data_bytes[128] = {
/*-------------------------------------------------------------------*/
/* Device NED                                                        */
/*-------------------------------------------------------------------*/
    0xD0,                               // 0:      NED code
    0x01,                               // 1:      Type  (X'01' = I/O Device)
    0x06,                               // 2:      Class (X'06' = Comms)
    0x00,                               // 3:      (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF2,      // 4-9:    Type  ('001732')
    0xF0,0xF0,0xF1,                     // 10-12:  Model ('001')
    0xC8,0xD9,0xC3,                     // 13-15:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 16-17:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 18-29:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 30-31: Tag (x'ccua', cc = chpid, ua=unit address)
/*-------------------------------------------------------------------*/
/* Control Unit NED                                                  */
/*-------------------------------------------------------------------*/
    0xD0,                               // 32:     NED code
    0x02,                               // 33:     Type  (X'02' = Control Unit)
    0x00,                               // 34:     Class (X'00' = N/A)
    0x00,                               // 35:     (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF1,      // 36-41:  Type  ('001731')
    0xF0,0xF0,0xF1,                     // 42-44:  Model ('001')
    0xC8,0xD9,0xC3,                     // 45-47:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 48-49:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 50-61:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 62-63:  Tag cuaddr
/*-------------------------------------------------------------------*/
/* Token NED                                                         */
/*-------------------------------------------------------------------*/
    0xF0,                               // 64:     NED code
    0x00,                               // 65:     Type  (X'00' = N/A)   
    0x00,                               // 66:     Class (X'00' = N/A)
    0x00,                               // 67:     (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF0,      // 68-73:  Type  ('001730')
    0xF0,0xF0,0xF1,                     // 74-76:  Model ('001')
    0xC8,0xD9,0xC3,                     // 77-79:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 80-81:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 82-93:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 94-95:  Tag cuaddr
/*-------------------------------------------------------------------*/
/* General NEQ                                                       */
/*-------------------------------------------------------------------*/
    0x80,                               // 96:     NED code
    0x00,                               // 97:     ?
    0x00,0x00,                          // 98-99:  ?
    0x00,                               // 100:    ?
    0x00,0x00,0x00,                     // 101-103:?
    0x00,                               // 104:    ?
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 105-125:?
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, //
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, //
    0x00,0x00                           // 126-127:?
};


static BYTE qeth_immed_commands [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 00 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 10 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* C0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* E0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0 */
};


static const char *osa_devtyp[] = { "Read", "Write", "Data" };


#if defined(DEBUG)
static inline void DUMP(char* name, void* ptr, int len)
{
int i;

    logmsg(_("DATA: %4.4X %s"), len, name);
    for(i = 0; i < len; i++)
    {
        if(!(i & 15))
            logmsg(_("\n%4.4X:"), i);
        logmsg(_(" %2.2X"), ((BYTE*)ptr)[i]);
    }
    if(--i & 15)
        logmsg(_("\n"));
}
#else
 #define DUMP(_name, _ptr, _len)
#endif


/*-------------------------------------------------------------------*/
/* Adapter Command Routine                                           */
/*-------------------------------------------------------------------*/
static void osa_adapter_cmd(DEVBLK *dev, OSA_TH *req_th, DEVBLK *rdev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
OSA_TH  *th = (OSA_TH*)rdev->qrspbf;
OSA_RRH *rrh;
OSA_PH  *ph;
U16 offset;
U16 rqsize;
U32 ackseq;

    /* Copy request to response buffer */
    FETCH_HW(rqsize,req_th->rrlen);
    memcpy(th,req_th,rqsize);

    FETCH_HW(offset,th->rroff);
    DUMP("TH",th,offset);
    rrh = (OSA_RRH*)((BYTE*)th+offset);

    FETCH_HW(offset,rrh->pduhoff);
    DUMP("RRH",rrh,offset);
    ph = (OSA_PH*)((BYTE*)rrh+offset);
    DUMP("PH",ph,sizeof(OSA_PH));

    /* Update ACK Sequence Number */
    FETCH_FW(ackseq,rrh->ackseq);
    ackseq++;
    STORE_FW(rrh->ackseq,ackseq);

    switch(rrh->type) {

    case RRH_TYPE_CM:
        {
        OSA_PDU *pdu = (OSA_PDU*)(ph+1);
            DUMP("PDU CM",pdu,sizeof(OSA_PDU));
        }
        break;

    case RRH_TYPE_ULP:
        {
        OSA_PDU *pdu = (OSA_PDU*)(ph+1);
            DUMP("PDU ULP",pdu,sizeof(OSA_PDU));

            switch(pdu->tgt) {

            case PDU_TGT_OSA:

                switch(pdu->cmd) {

                case PDU_CMD_SETUP:
                    break;

                case PDU_CMD_ENABLE:
                    VERIFY(!TUNTAP_CreateInterface(grp->tuntap,
                      ((pdu->proto != PDU_PROTO_L3) ? IFF_TAP : IFF_TUN) | IFF_NO_PI,
                                           &grp->ttfd,                      
                                           grp->ttdevn));

                    /* Set Non-Blocking mode */
                    socket_set_blocking_mode(grp->ttfd,0);

                    break;

                case PDU_CMD_ACTIVATE:
                    break;

                default:
                    TRACE(_("ULP Target OSA Cmd %2.2x\n"),pdu->cmd);
                }
                break;

            case PDU_TGT_QDIO:
                break;

            default:
                TRACE(_("ULP Target %2.2x\n"),pdu->tgt);
            }

        }
        break;

    case RRH_TYPE_IPA:
        {
        OSA_IPA *ipa = (OSA_IPA*)(ph+1);
            DUMP("IPA",ipa,sizeof(OSA_IPA));
            FETCH_HW(offset,ph->pdulen);
            DUMP("REQ",(ipa+1),offset-sizeof(OSA_IPA));

            STORE_HW(ipa->rc,0x0000);

            switch(ipa->cmd) {

            case IPA_CMD_SETADPPARMS:
                {
                OSA_IPA_SAP *ipa_sap = (OSA_IPA_SAP*)(ipa+1);
                int cmd;

                    FETCH_FW(cmd,ipa_sap->cmd);
                    TRACE("Set Adapter Parameters: %8.8x\n",cmd);

                    switch(cmd) {

                    case 1:
                        STORE_FW(ipa_sap->suppcm,1);
                        STORE_HW(ipa_sap->rc,0x0000);
// ZZ INCOMPLETE NEED TO ADD SUPPORTEN LAN TYPES RESPONSE
                        break;

                    default:
                        STORE_HW(ipa_sap->rc,0xE00E);
                    }

                }
                break;

            case IPA_CMD_STARTLAN:
                {
                    TRACE(_("STARTLAN\n"));

                    if(
#if defined(TUNTAP_IFF_RUNNING_NEEDED)
                       TUNTAP_SetFlags(grp->ttdevn,IFF_UP
                                                     | IFF_RUNNING
                                                     | IFF_BROADCAST )
#else
                       TUNTAP_SetFlags(grp->ttdevn,IFF_UP
                                                     | IFF_BROADCAST )
#endif /*defined(TUNTAP_IFF_RUNNING_NEEDED)*/
                                )
                        STORE_HW(ipa->rc,0xFFFF);

                }
                break;

            case IPA_CMD_STOPLAN:
                {
                    TRACE(_("STOPLAN\n"));

                    if( TUNTAP_SetFlags(grp->ttdevn,0) )
                        STORE_HW(ipa->rc,0xFFFF);
                }
                break;

            case IPA_CMD_SETVMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);
                char macaddr[18];
                    snprintf(macaddr,sizeof(macaddr),"%02x:%02x:%02x:%02x:%02x:%02x",
                      ipa_mac->macaddr[0],ipa_mac->macaddr[1],ipa_mac->macaddr[2],
                      ipa_mac->macaddr[3],ipa_mac->macaddr[4],
// ZZ THE TAP INTERFACE MUST NOT HAVE THE SAME MACADDR AS THE 
// ZZ GUEST INTERFACE DOING SO WILL CAUSE FRAMES TO BE SENT TO
// ZZ THE HOST RATHER THEN TO THE GUEST WHEN USING BRIDGED INTERFACES
                      ipa_mac->macaddr[5]^1);

                    TRACE("Set VMAC: %s\n",macaddr);

#if defined(OPTION_TUNTAP_SETMACADDR)
// ZZ FIXME SetMACAddr may be called when the interface is up
//          This may be an error in linux, however we may need
//          to handle this condition here...
//          ifconfig up/down (ioctl IFUP) should call STARTLAN/STOPLAN                       
                    if( TUNTAP_SetMACAddr(grp->ttdevn,macaddr) )
                        STORE_HW(ipa->rc,0xFFFF);
#endif /*defined(OPTION_TUNTAP_SETMACADDR)*/

                }
                break;

            case IPA_CMD_DELVMAC:
                    TRACE("Del VMAC\n");
                break;

            case IPA_CMD_SETGMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);
                char macaddr[18];
                    snprintf(macaddr,sizeof(macaddr),"%02x:%02x:%02x:%02x:%02x:%02x",
                      ipa_mac->macaddr[0],ipa_mac->macaddr[1],ipa_mac->macaddr[2],
                      ipa_mac->macaddr[3],ipa_mac->macaddr[4],ipa_mac->macaddr[5]);

                    TRACE("Set GMAC: %s\n",macaddr);

// ZZ FIXME SETMULTADDR NOT YET SUPPORTED BY TUNTAP!!!
#if defined(OPTION_TUNTAP_SETMULTADDR)
                    if( TUNTAP_SetMULTAddr(grp->ttdevn,macaddr) )
                        STORE_HW(ipa->rc,0xFFFF);
#endif /*defined(OPTION_TUNTAP_SETMULTADDR)*/

                }
                break;

            case IPA_CMD_DELGMAC:
                    TRACE("Del GMAC\n");
                break;

            default:
                TRACE("Invalid IPA Cmd(%02x)\n",ipa->cmd);
            }
        }
        break;

    default:
        TRACE("Invalid Type=%2.2x\n",rrh->type);
    }

    // Set Response
    rdev->qrspsz = rqsize;
}


/*-------------------------------------------------------------------*/
/* Device Command Routine                                            */
/*-------------------------------------------------------------------*/
static void osa_device_cmd(DEVBLK *dev, OSA_IEA *iea, DEVBLK *rdev)
{
U16 reqtype;
U16 datadev;
OSA_IEAR *iear = (OSA_IEAR*)rdev->qrspbf;

    memset(iear, 0x00, sizeof(OSA_IEAR));

    FETCH_HW(reqtype, iea->type);

    switch(reqtype) {

    case IDX_ACT_TYPE_READ:
        FETCH_HW(datadev, iea->datadev);
        if(!IS_OSA_READ_DEVICE(dev))
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp = IDX_RSP_RESP_OK;
            iear->flags = IDX_RSP_FLAGS_NOPORTREQ;
            STORE_HW(iear->flevel, 0x0201);

            dev->qidxstate = OSA_IDX_STATE_ACTIVE;
        }
        break;

    case IDX_ACT_TYPE_WRITE:
        FETCH_HW(datadev, iea->datadev);
        if(!IS_OSA_WRITE_DEVICE(dev))
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp = IDX_RSP_RESP_OK;
            iear->flags = IDX_RSP_FLAGS_NOPORTREQ;
            STORE_HW(iear->flevel, 0x0201);

            dev->qidxstate = OSA_IDX_STATE_ACTIVE;
        }
        break;

    default:
        TRACE(_("QETH: IDX ACTIVATE Invalid Request %4.4x for %s device %4.4x\n"),reqtype,osa_devtyp[dev->member],dev->devnum); 
        dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        break;
    }

    rdev->qrspsz = sizeof(OSA_IEAR);
}


/*-------------------------------------------------------------------*/
/* Raise Adapter Interrupt                                           */
/*-------------------------------------------------------------------*/
static void raise_adapter_interrupt(DEVBLK *dev)
{
    obtain_lock(&dev->lock);
    dev->pciscsw.flag2 |= SCSW2_Q;
    dev->pciscsw.flag3 |= SCSW3_SC_INTER | SCSW3_SC_PEND;
    dev->pciscsw.chanstat = CSW_PCI;
    QUEUE_IO_INTERRUPT(&dev->pciioint);
    release_lock (&dev->lock);

    /* Update interrupt status */
    OBTAIN_INTLOCK(devregs(dev));
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(devregs(dev));
}


// When must go through the queues and buffers on a round robin basis
// such that buffers are re-used on a least recently used bases.
// When no buffer are available we will must keep our current position
// When a buffer becomes available, then we will advance to that location 
// When we reach the end of the buffer queue, we will advance to the 
// next available queue. 
// When a queue is newly enabled then we will start at the beginning of 
// the queue (this is handled in signal adapter)

/*-------------------------------------------------------------------*/
/* Process Input Queue                                               */
/*-------------------------------------------------------------------*/
static void process_input_queue(DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int iq = grp->i_qpos;
int mq = grp->i_qcnt;
int noread = 1;

TRACE("Input Qpos(%d) Bpos(%d)\n",grp->i_qpos,grp->i_bpos[grp->i_qpos]);

    while (mq--)
        if(grp->i_qmask & (0x80000000 >> iq))
        {
        int ib = grp->i_bpos[iq];
        OSA_SLSB *slsb;
        int mb = 128;
            slsb = (OSA_SLSB*)(dev->mainstor + grp->i_slsbla[iq]);

            while(mb--)
                if(slsb->slsbe[ib] == SLSBE_INPUT_EMPTY)   
                {
                OSA_SL *sl = (OSA_SL*)(dev->mainstor + grp->i_sla[iq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                OSA_SBAL *sbal;
                int olen = 0; int tlen = 0;
                int ns;
TRACE(_("Input Queue(%d) Buffer(%d)\n"),iq,ib);

                    FETCH_DW(sa,sl->sbala[ib]);
                    sbal = (OSA_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        buf = (BYTE*)(dev->mainstor + la);

// ZZ INCOMPLETE PROCESS BUFFER HERE
// ZZ THIS CODE IS NOT QUITE RIGHT YET!!!!
// ZZ IS MUST BE ABLE TO SPLIT FRAMES INTO MULTIPLE SEGMENTS
// ZZ ALL STORAGE ACCESS IS NOT CHECKED FOR VALIDITY (ACCESS LIMITS KEYS...)
// ZZ INCORRECT BUFFER ADDRESSES MAY GENERATE SEGFAULTS!!!!!
                        if(len > sizeof(OSA_HDR2))
                        {
                            olen = TUNTAP_Read(grp->ttfd,buf+sizeof(OSA_HDR2),len-sizeof(OSA_HDR2));
                            noread = 0;
                        }
if(olen > 0)
DUMP("INPUT TAP",buf+sizeof(OSA_HDR2),olen);
                        if(olen > 0)
                        {
                        OSA_HDR2 *hdr2 = (OSA_HDR2*)buf;
                        memset(hdr2,0x00,sizeof(OSA_HDR2));
                            hdr2->id = 0x02;
                            hdr2->flags[2] = 0x02;
                            STORE_HW(hdr2->pktlen,olen);
                            tlen += olen;
                            STORE_FW(sbal->sbale[ns].length,olen+sizeof(OSA_HDR2));
//                          sbal->sbale[ns].flags[0] = 0x40;
if(sa && la && len)
{
TRACE("SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
TRACE("FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
DUMP("INPUT BUF",hdr2,olen+sizeof(OSA_HDR2));
}
                        }
                    }
                
                    if(tlen > 0)
                    {
                        grp->reqpci = 1;
                        slsb->slsbe[ib] = SLSBE_INPUT_COMPLETED;
                        if(++ib >= 128)
                        {
                            ib = 0;
                            grp->i_bpos[iq] = ib;
                            if(++iq >= grp->i_qcnt)
                                iq = 0;
                            grp->i_qpos = iq;
                            mq = grp->o_qcnt;
                        }
                        grp->i_bpos[iq] = ib;
                        mb = 128;
                    }
                    else
                    {
                        if(ns)
                            sbal->sbale[ns-1].flags[0] = 0x40;
                        return;
                    }
                    sbal->sbale[ns-1].flags[0] = 0x40;
                }
                else /* Buffer not empty */
                {
                    if(++ib >= 128)
                    {
                        ib = 0;
                        grp->i_bpos[iq] = ib;
                        if(++iq >= grp->i_qcnt)
                            iq = 0;
                        grp->i_qpos = iq;
                    }
                    grp->i_bpos[iq] = ib;
                }
        }
        else
            if(++iq >= grp->i_qcnt)
                iq = 0;
    
    if(noread)
    {
    char buff[4096];
    int n;
        if((n = TUNTAP_Read(grp->ttfd,buff,4096)) > 0)
        {
            grp->reqpci = 1;
DUMP("TAP DROPPED",buff,n);
        }
    }
}


/*-------------------------------------------------------------------*/
/* Process Output Queue                                              */
/*-------------------------------------------------------------------*/
static void process_output_queue(DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int oq = grp->o_qpos;
int mq = grp->o_qcnt;

    while (mq--)
        if(grp->o_qmask & (0x80000000 >> oq))
        {
        int ob = grp->o_bpos[oq];
        OSA_SLSB *slsb;
        int mb = 128;
            slsb = (OSA_SLSB*)(dev->mainstor + grp->o_slsbla[oq]);

            while(mb--)
                if(slsb->slsbe[ob] == SLSBE_OUTPUT_PRIMED)   
                {
                OSA_SL *sl = (OSA_SL*)(dev->mainstor + grp->o_sla[oq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                OSA_SBAL *sbal;
                int ns;

TRACE(_("Output Queue(%d) Buffer(%d)\n"),oq,ob);

                    FETCH_DW(sa,sl->sbala[ob]);
                    sbal = (OSA_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        buf = (BYTE*)(dev->mainstor + la);

// ZZ INCOMPLETE PROCESS BUFFER HERE
// ZZ THIS CODE IS NOT QUITE RIGHT YET IT MUST BE ABLE TO ASSEMBLE
// ZZ MULTIPLE FRAGMENTS INTO ONE ETHERNET FRAME
// ZZ ALL STORAGE ACCESS IS NOT CHECKED FOR VALIDITY (ACCESS LIMITS KEYS...)
// ZZ INCORRECT BUFFER ADDRESSES MAY GENERATE SEGFAULTS!!!!!

if(sa && la && len)
{
TRACE("SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
TRACE("FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
DUMP("OUTPUT BUF",buf,len);
}
                        if(len > sizeof(OSA_HDR2))
                            TUNTAP_Write(grp->ttfd,buf+sizeof(OSA_HDR2),len-sizeof(OSA_HDR2));

                        if((sbal->sbale[ns].flags[1] & SBAL_FLAGS1_PCI_REQ))
                            grp->reqpci = 1;
                    }
                
                    slsb->slsbe[ob] = SLSBE_OUTPUT_COMPLETED;
                    if(++ob >= 128)
                    {
                        ob = 0;
                        grp->o_bpos[oq] = ob;
                        if(++oq >= grp->o_qcnt)
                            oq = 0;
                        grp->o_qpos = oq;
                        mq = grp->o_qcnt;
                    }
                    grp->o_bpos[oq] = ob;
                    mb = 128;
                }
                else
                    if(++ob >= 128)
                    {
                        ob = 0;
                        if(++oq >= grp->o_qcnt)
                            oq = 0;
                    }

        }
        else
            if(++oq >= grp->o_qcnt)
                oq = 0;
}


/*-------------------------------------------------------------------*/
/* Halt device handler                                               */
/*-------------------------------------------------------------------*/
static void qeth_halt_device ( DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    /* Signal QDIO end if QDIO is active */
    if(dev->scsw.flag2 & SCSW2_Q)
    {
        dev->scsw.flag2 &= ~SCSW2_Q;
        write(grp->ppfd[1],"*",1);
    }
    else
        if(IS_OSA_READ_DEVICE(dev))
            signal_condition(&grp->qcond);

}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int qeth_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
OSA_GRP *grp;
int grouped;
int i;

    dev->numdevid = sizeof(sense_id_bytes);
    memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));
    dev->devtype = dev->devid[1] << 8 | dev->devid[2];
    
    dev->pmcw.flag4 |= PMCW4_Q;

    if(!(grouped = group_device(dev,OSA_GROUP_SIZE)) && !dev->member)
    {
        dev->group->grp_data = grp = malloc(sizeof(OSA_GRP));
        memset (grp, 0, sizeof(OSA_GRP));

        initialize_condition(&grp->qcond);
        initialize_lock(&grp->qlock);
    
        /* Open write signalling pipe */
        create_pipe(grp->ppfd);

        /* Set Non-Blocking mode */
        socket_set_blocking_mode(grp->ppfd[0],0);

        /* Set defaults */
        grp->tuntap = strdup(TUNTAP_NAME);
    }
    else
        grp = dev->group->grp_data;

    /* Allocate reponse buffer */
    dev->qrspbf = malloc(RSP_BUFSZ);
    dev->qrspsz = 0;

    // process all command line options here
    for(i = 0; i < argc; i++)
    {
        if(!strcasecmp("iface",argv[i]) && (i+1) < argc)
        {
            free(grp->tuntap);
            grp->tuntap = strdup(argv[++i]);
        }
        else
            logmsg(_("QETH: Invalid option %s for device %4.4X\n"),argv[i],dev->devnum);

    }

    if(grouped)
    {
        // Perform group initialisation here

    }

    return 0;
} /* end function qeth_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void qeth_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    BEGIN_DEVICE_CLASS_QUERY( "OSA", dev, class, buflen, buffer );

    snprintf (buffer, buflen-1, "%s%s%s",
      (dev->group->acount == OSA_GROUP_SIZE) ? osa_devtyp[dev->member] : "*Incomplete",
      (dev->scsw.flag2 & SCSW2_Q) ? " QDIO" : "",
      (dev->qidxstate == OSA_IDX_STATE_ACTIVE) ? " IDX" : "");

} /* end function qeth_query_device */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int qeth_close_device ( DEVBLK *dev )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(!dev->member && dev->group->grp_data)
    {
        if(grp->ttfd)
            TUNTAP_Close(grp->ttfd);

        if(grp->ppfd[0])
            close(grp->ppfd[0]);
        if(grp->ppfd[1])
            close(grp->ppfd[1]);
        
        if(grp->tuntap)
            free(grp->tuntap);

        destroy_condition(&grp->qcond);
        destroy_lock(&grp->qlock);
    
        free(dev->group->grp_data);
        dev->group->grp_data = NULL;
    }

    if(dev->qrspbf)
    {
        free(dev->qrspbf);
        dev->qrspbf = NULL;
    }

    return 0;
} /* end function qeth_close_device */


/*-------------------------------------------------------------------*/
/* QDIO subsys desc                                                  */
/*-------------------------------------------------------------------*/
static int qeth_ssqd_desc ( DEVBLK *dev, void *desc )
{
    CHSC_RSP24 *chsc_rsp24 = (void *)desc;

    STORE_HW(chsc_rsp24->sch, dev->subchan);

    chsc_rsp24->flags |= ( CHSC_FLAG_QDIO_CAPABILITY | CHSC_FLAG_VALIDITY );

    chsc_rsp24->qdioac1 |= ( AC1_SIGA_INPUT_NEEDED | AC1_SIGA_OUTPUT_NEEDED );

    return 0;
}


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void qeth_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int num;                                /* Number of bytes to move   */

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);
    UNREFERENCED(chained);

    /* Command reject if the device group has not been established */
    if((dev->group->acount != OSA_GROUP_SIZE)
      && !(IS_CCW_SENSE(code) || IS_CCW_NOP(code) || (code == OSA_RCD)))
    {
        /* Set Intervention required sense, and unit check status */
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
    switch (code) {


    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
    {
    OSA_HDR *hdr = (OSA_HDR*)iobuf;
    U16 ddc;

    /* Device block of device to which response is sent */
    DEVBLK *rdev = (IS_OSA_WRITE_DEVICE(dev) 
                  && (dev->qidxstate == OSA_IDX_STATE_ACTIVE)
                  && (dev->group->memdev[OSA_READ_DEVICE]->qidxstate == OSA_IDX_STATE_ACTIVE)) 
                 ? dev->group->memdev[OSA_READ_DEVICE] : dev;

        if(!rdev->qrspsz)
        {
            FETCH_HW(ddc,hdr->ddc);
  
            obtain_lock(&grp->qlock);
            if(ddc == IDX_ACT_DDC)
                osa_device_cmd(dev,(OSA_IEA*)iobuf, rdev);
            else
                osa_adapter_cmd(dev, (OSA_TH*)iobuf, rdev);
            release_lock(&grp->qlock);

            if(dev != rdev)
                signal_condition(&grp->qcond);
       
            /* Calculate number of bytes to write and set residual count */
            num = (count < RSP_BUFSZ) ? count : RSP_BUFSZ;
            *residual = count - num;
            if (count < RSP_BUFSZ) *more = 1;
    
            /* Return normal status */
            *unitstat = CSW_CE | CSW_DE;
        }
        else
        {
            /* Command reject if no response buffer available */
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
        break;
    }


    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
    {
        int rd_size = 0;

        obtain_lock(&grp->qlock);
        if(dev->qrspsz)
        {
            rd_size = dev->qrspsz;
            memcpy(iobuf,dev->qrspbf,rd_size);
            dev->qrspsz = 0;
        }
        else
        {
            if(IS_OSA_READ_DEVICE(dev)
              && (dev->qidxstate == OSA_IDX_STATE_ACTIVE))
            {
                wait_condition(&grp->qcond, &grp->qlock);
                if(dev->qrspsz)
                {
                    rd_size = dev->qrspsz;
                    memcpy(iobuf,dev->qrspbf,rd_size);
                    dev->qrspsz = 0;
                }
            }
        }
        release_lock(&grp->qlock);

        if(rd_size)
        {
            /* Calculate number of bytes to read and set residual count */
            num = (count < rd_size) ? count : rd_size;
            *residual = count - num;
            if (count < rd_size) *more = 1;

            /* Return normal status */
            *unitstat = CSW_CE | CSW_DE;
        }
        else
        {
            /* Return unit check with status modifier */
            dev->sense[0] = 0;
            *unitstat = CSW_CE | CSW_DE | CSW_UC | CSW_SM;
        }
        break;
    }


    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/

        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;


    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/

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

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;


    case OSA_RCD:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/

        /* Copy configuration data from tempate */
        memcpy (iobuf, read_configuration_data_bytes, sizeof(read_configuration_data_bytes));

        /* Insert chpid & unit address in the device ned */
        iobuf[30] = (dev->devnum >> 8) & 0xff;
        iobuf[31] = (dev->devnum) & 0xff;

        /* Use unit address of OSA read device as control unit address */
        iobuf[62] = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xff;
        iobuf[63] = (dev->group->memdev[OSA_READ_DEVICE]->devnum) & 0xff;

        /* Use unit address of OSA read device as control unit address */
        iobuf[94] = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xff;
        iobuf[95] = (dev->group->memdev[OSA_READ_DEVICE]->devnum) & 0xff;

        /* Calculate residual byte count */
        num = (count < sizeof(read_configuration_data_bytes) ? count : sizeof(read_configuration_data_bytes));
        *residual = count - num;
        if (count < sizeof(read_configuration_data_bytes)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

        
    case OSA_EQ:
    /*---------------------------------------------------------------*/
    /* ESTABLISH QUEUES                                              */
    /*---------------------------------------------------------------*/
    {
        OSA_QDR *qdr = (OSA_QDR*)dev->qrspbf;
        OSA_QDES0 *qdes;
        int i;

        /* Copy QDR from I/O buffer */
        memcpy(qdr,iobuf,count);

        grp->i_qcnt = qdr->iqdcnt < QDIO_MAXQ ? qdr->iqdcnt : QDIO_MAXQ;
        grp->o_qcnt = qdr->oqdcnt < QDIO_MAXQ ? qdr->oqdcnt : QDIO_MAXQ;

        FETCH_DW(grp->qiba,qdr->qiba);
        grp->qibk = qdr->qkey;

//      {
//      OSA_QIB *qib = (OSA_QIB*)(dev->mainstor + grp->qiba);
//          qib->ac |= QIB_AC_PCI; // Incidate PCI on output is supported
//      }

        qdes = qdr->qdf0;

        for(i = 0; i < grp->i_qcnt; i++)
        {
            FETCH_DW(grp->i_sliba[i],qdes->sliba);
            FETCH_DW(grp->i_sla[i],qdes->sla);
            FETCH_DW(grp->i_slsbla[i],qdes->slsba);
            grp->i_slibk[i] = qdes->keyp1 & 0xF0;
            grp->i_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            grp->i_sbalk[i] = qdes->keyp2 & 0xF0;
            grp->i_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;
    
            qdes = (OSA_QDES0*)((BYTE*)qdes+(qdr->iqdsz<<2));
        }

        for(i = 0; i < grp->o_qcnt; i++)
        {
            FETCH_DW(grp->o_sliba[i],qdes->sliba);
            FETCH_DW(grp->o_sla[i],qdes->sla);
            FETCH_DW(grp->o_slsbla[i],qdes->slsba);
            grp->o_slibk[i] = qdes->keyp1 & 0xF0;
            grp->o_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            grp->o_sbalk[i] = qdes->keyp2 & 0xF0;
            grp->o_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;

            qdes = (OSA_QDES0*)((BYTE*)qdes+(qdr->oqdsz<<2));
        }

        /* Calculate residual byte count */
        num = (count < sizeof(OSA_QDR)) ? count : sizeof(OSA_QDR);
        *residual = count - num;
        if (count < sizeof(OSA_QDR)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case OSA_AQ:
    /*---------------------------------------------------------------*/
    /* ACTIVATE QUEUES                                               */
    /*---------------------------------------------------------------*/
    {
    fd_set readset;

        grp->i_qmask = grp->o_qmask = 0;

        FD_ZERO( &readset );
        
        dev->scsw.flag2 |= SCSW2_Q;

        do {
            /* Process the Input Queue if data has been received */
            if(grp->i_qmask && FD_ISSET(grp->ttfd,&readset))
            {
                process_input_queue(dev);
            }

            /* Process Output Queue if data needs to be send */
            if(FD_ISSET(grp->ppfd[0],&readset))
            {
            char c;
                read(grp->ppfd[0],&c,1);
               
                if(grp->o_qmask)
                    process_output_queue(dev);
            }

            if(grp->i_qmask)
                FD_SET(grp->ttfd, &readset);
            FD_SET(grp->ppfd[0], &readset);

            if(grp->reqpci)
            {
                grp->reqpci = 0;
                raise_adapter_interrupt(dev);
            }

            select (((grp->ttfd > grp->ppfd[0]) ? grp->ttfd : grp->ppfd[0]) + 1,
              &readset, NULL, NULL, NULL);

        } while(dev->scsw.flag2 & SCSW2_Q);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
    }
        break;


    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
TRACE(_("Unkown CCW dev(%4.4x) code(%2.2x)\n"),dev->devnum,code);
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
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int noselrd;
TRACE(_("SIGA-r dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;
    
    /* Is there a read select */
    noselrd = !grp->i_qmask;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> grp->i_qcnt);

    /* Reset Queue Positions */
    if(qmask != grp->i_qmask)
    {
    int n;
        for(n = 0; n < grp->i_qcnt; n++)
            if(!(grp->i_qmask & (0x80000000 >> n)))
                grp->i_bpos[n] = 0;
        if(!grp->i_qmask)
            grp->i_qpos = 0;
    }

    /* Update Read Queue Mask */
    grp->i_qmask = qmask;

    /* Send signal to QDIO thread */
    if(noselrd && grp->i_qmask)
        write(grp->ppfd[1],"*",1);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output                                    */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output(DEVBLK *dev, U32 qmask)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
TRACE(_("SIGA-w dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> grp->o_qcnt);

    /* Reset Queue Positions */
    if(qmask != grp->o_qmask)
    {
    int n;
        for(n = 0; n < grp->o_qcnt; n++)
            if(!(grp->o_qmask & (0x80000000 >> n)))
                grp->o_bpos[n] = 0;
        if(!grp->o_qmask)
            grp->o_qpos = 0;
    }

    /* Update Write Queue Mask */
    grp->o_qmask = qmask;

    /* Send signal to QDIO thread */
    if(grp->o_qmask)
        write(grp->ppfd[1],"*",1);

    return 0;
}


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND qeth_device_hndinfo =
{
        &qeth_init_handler,            /* Device Initialisation      */
        &qeth_execute_ccw,             /* Device CCW execute         */
        &qeth_close_device,            /* Device Close               */
        &qeth_query_device,            /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        &qeth_halt_device,             /* Device Halt channel pgm    */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        qeth_immed_commands,           /* Immediate CCW Codes        */
        &qeth_initiate_input,          /* Signal Adapter Input       */
        &qeth_initiate_output,         /* Signal Adapter Output      */
        &qeth_ssqd_desc,               /* QDIO subsys desc           */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
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
