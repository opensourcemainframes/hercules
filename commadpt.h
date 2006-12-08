// $Id$
//
// $Log$

#ifndef __COMMADPT_H__
#define __COMMADPT_H__

#include "hercules.h"

typedef struct _COMMADPT_RING
{
    BYTE *bfr;
    size_t sz;
    size_t hi;
    size_t lo;
    u_int havedata:1;
    u_int overflow:1;
} COMMADPT_RING;

struct COMMADPT
{
    DEVBLK *dev;                /* the devblk to which this CA is attched   */
    BYTE lnctl;                 /* Line control used                        */
    int  rto;                   /* Read Time-Out                            */
    int  pto;                   /* Poll Time-Out                            */
    int  eto;                   /* Enable Time-Out                          */
    TID  cthread;               /* Thread used to control the socket        */
    BYTE curpending;            /* Current pending operation                */
    U16  lport;                 /* Local listening port                     */
    in_addr_t lhost;            /* Local listening address                  */
    U16 rport;                  /* Remote TCP Port                          */
    in_addr_t rhost;            /* Remote connection IP address             */
    int sfd;                    /* Communication socket FD                  */
    int lfd;                    /* Listen socket for DIAL=IN, INOUT & NO    */
    COND ipc;                   /* I/O <-> thread IPC condition EVB         */
    COND ipc_halt;              /* I/O <-> thread IPC HALT special EVB      */
    LOCK lock;                  /* COMMADPT lock                            */
    int pipe[2];                /* pipe used for I/O to thread signaling    */
    COMMADPT_RING inbfr;        /* Input buffer ring                        */
    COMMADPT_RING outbfr;       /* Output buffer ring                       */
    COMMADPT_RING pollbfr;      /* Ring used for POLL data                  */
    COMMADPT_RING rdwrk;        /* Inbound data flow work ring              */
    U16  devnum;                /* devnum copy from DEVBLK                  */
    BYTE dialdata[32];          /* Dial data information                    */
    U16  dialcount;             /* data count for dial                      */
    BYTE pollix;                /* Next POLL Index                          */
    U16  pollused;              /* Count of Poll data used during Poll      */
    u_int enabled:1;            /* An ENABLE CCW has been sucesfully issued */
    u_int connect:1;            /* A connection exists with the remote peer */
    u_int eibmode:1;            /* EIB Setmode issued                       */
    u_int dialin:1;             /* This is a SWITCHED DIALIN line           */
    u_int dialout:1;            /* This is a SWITCHED DIALOUT line          */
    u_int have_cthread:1;       /* the comm thread is running               */
    u_int dolisten:1;           /* Start a listen                           */
    u_int listening:1;          /* Listening                                */
    u_int haltpending:1;        /* A request has been issued to halt current*/
                                /* CCW                                      */
    u_int xparwwait:1;          /* Transparent Write Wait state : a Write   */
                                /* was previously issued that turned the    */
                                /* line into transparent mode. Anything     */
                                /* else than another write, Sense or NO-OP  */
                                /* is rejected with SENSE_CR                */
                                /* This condition is reset upon receipt of  */
                                /* DLE/ETX or DLE/ETB on a subsequent write */
    u_int input_overrun:1;      /* The input ring buffer has overwritten    */
                                /* itself                                   */
    u_int in_textmode:1;        /* Input buffer processing : text mode      */
    u_int in_xparmode:1;        /* Input buffer processing : transparent    */
    u_int gotdle:1;             /* DLE Received in inbound flow             */
    u_int pollsm:1;             /* Issue Status Modifier on POLL Exit       */
    u_int badpoll:1;            /* Bad poll data (>7 Bytes before ENQ)      */
    u_int callissued:1;         /* The connect out for the DIAL/ENABLE      */
                                /* has already been issued                  */
    u_int readcomp:1;           /* Data in the read buffer completes a read */
    u_int datalostcond:1;       /* Data Lost Condition Raised               */
};

enum {
    COMMADPT_LNCTL_BSC=1,       /* BSC Line Control                         */
    COMMADPT_LNCTL_ASYNC        /* ASYNC Line Control                       */
} commadpt_lnctl;

enum {
    COMMADPT_PEND_IDLE=0,       /* NO CCW currently executing               */
    COMMADPT_PEND_READ,         /* A READ CCW is running                    */
    COMMADPT_PEND_WRITE,        /* A WRITE CCW is running                   */
    COMMADPT_PEND_ENABLE,       /* A ENABLE CCW is running                  */
    COMMADPT_PEND_DIAL,         /* A DIAL CCW is running                    */
    COMMADPT_PEND_DISABLE,      /* A DISABLE CCW is running                 */
    COMMADPT_PEND_PREPARE,      /* A PREPARE CCW is running                 */
    COMMADPT_PEND_POLL,         /* A POLL CCW Is Running                    */
    COMMADPT_PEND_TINIT,        /*                                          */
    COMMADPT_PEND_CLOSED,       /*                                          */
    COMMADPT_PEND_SHUTDOWN      /*                                          */
} commadpt_pendccw;

#define COMMADPT_PEND_TEXT static char *commadpt_pendccw_text[]={\
    "IDLE",\
    "READ",\
    "WRITE",\
    "ENABLE",\
    "DIAL",\
    "DISABLE",\
    "PREPARE",\
    "POLL",\
    "TINIT",\
    "TCLOSED",\
    "SHUTDOWN"}

#endif
