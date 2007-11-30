////////////////////////////////////////////////////////////////////////////////////
//         w32chan.h           Fish's new i/o scheduling logic
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001-2007. Released under the Q Public License
// (http://www.hercules-390.org/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

// $Id$
//
// $Log$
// Revision 1.14  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.13  2006/12/08 09:43:31  jj
// Add CVS message log
//

#ifndef _W32CHANN_H_
#define _W32CHANN_H_

/////////////////////////////////////////////////////////////////////////////
// I/O Scheduler functions...

extern void  InitIOScheduler    // initialize i/o scheduler vars

//      Only call this function ONCE -- at startup! From
//      then on, just set the variables directly as needed.

(
    int    arch_mode,       // (for calling execute_ccw_chain)
    int*   devt_prio,       // (ptr to device thread priority)
    int    devt_timeout,    // (maximum device thread wait time)
    long   devt_max         // (maximum #of device threads allowed)
);

extern int   ScheduleIORequest(void* pDevBlk, unsigned short wDevNum, int* pnDevPrio);
extern void  TrimDeviceThreads();
extern void  KillAllDeviceThreads();

/////////////////////////////////////////////////////////////////////////////
// Debugging...   (called by panel.c "FishHangReport" command...)

#if defined(FISH_HANG)
extern void  PrintAllDEVTHREADPARMSs();
#endif // defined(FISH_HANG)

/////////////////////////////////////////////////////////////////////////////
// I/O Scheduler variables...

extern long   ios_devtwait;     // #of threads currently idle
extern int    ios_devtnbr;      // #of threads currently active
extern int    ios_devthwm;      // max #of threads that WERE active
extern int    ios_devtmax;      // max #of threads there can be
extern int    ios_devtunavail;  // #of times 'idle' thread unavailable
extern int    ios_arch_mode;    // architectural mode

/////////////////////////////////////////////////////////////////////////////

#endif // _W32CHANN_H_
