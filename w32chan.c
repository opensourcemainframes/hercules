////////////////////////////////////////////////////////////////////////////////////
//         w32chan.c           Fish's new i/o scheduling logic
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_CONFIG_H)
#include <config.h>		// (needed 1st to set OPTION_FTHREADS flag appropriately)
#endif

#include "featall.h"	// (needed 2nd to set OPTION_FISHIO flag appropriately)

#if !defined(OPTION_FISHIO)
int dummy = 0;
#else // defined(OPTION_FISHIO)

#include <windows.h>	// (standard WIN32)
#include <errno.h>		// (need "errno")
#include <stdarg.h>		// (need "va_list", etc)
#include <stdio.h>		// (need "FILE", "vfprintf", etc)
#include <malloc.h>		// (need "malloc", etc)
#include "fthreads.h"	// (need "fthread_create")
#include "w32chan.h"	// (function prototypes for this module)
#include "linklist.h"	// (linked list macros)

/////////////////////////////////////////////////////////////////////////////
// (helper macros...)

#if defined(FISH_HANG)

	#include "fishhang.h"

	#define MyInitializeCriticalSection(lock)       (FishHang_InitializeCriticalSection(__FILE__,__LINE__,(lock)))
	#define MyDeleteCriticalSection(lock)           (FishHang_DeleteCriticalSection(__FILE__,__LINE__,(lock)))
	#define MyCreateEvent(sec,man,set,name)         (FishHang_CreateEvent(__FILE__,__LINE__,(sec),(man),(set),(name)))
	#define MySetEvent(handle)                      (FishHang_SetEvent(__FILE__,__LINE__,(handle)))
	#define MyResetEvent(handle)                    (FishHang_ResetEvent(__FILE__,__LINE__,(handle)))
	#define MyWaitForSingleObject(handle,millisecs) (FishHang_WaitForSingleObject(__FILE__,__LINE__,(handle),(millisecs)))
	#define MyCloseHandle(handle)                   (FishHang_CloseHandle(__FILE__,__LINE__,(handle)))
	#define LockScheduler()                         (FishHang_EnterCriticalSection(__FILE__,__LINE__,&IOSchedulerLock))
	#define LockThreadParms(pThreadParms)           (FishHang_EnterCriticalSection(__FILE__,__LINE__,&pThreadParms->IORequestListLock))
	#define UnlockScheduler()                       (FishHang_LeaveCriticalSection(&IOSchedulerLock))
	#define UnlockThreadParms(pThreadParms)         (FishHang_LeaveCriticalSection(&pThreadParms->IORequestListLock))

#else // !defined(FISH_HANG)

	#define MyInitializeCriticalSection(lock)       (InitializeCriticalSection((lock)))
	#define MyDeleteCriticalSection(lock)           (DeleteCriticalSection((lock)))
	#define MyCreateEvent(sec,man,set,name)         (CreateEvent((sec),(man),(set),(name)))
	#define MySetEvent(handle)                      (SetEvent((handle)))
	#define MyResetEvent(handle)                    (ResetEvent((handle)))
	#define MyWaitForSingleObject(handle,millisecs) (WaitForSingleObject((handle),(millisecs)))
	#define MyCloseHandle(handle)                   (CloseHandle((handle)))
	#define LockScheduler()                         (EnterCriticalSection(&IOSchedulerLock))
	#define LockThreadParms(pThreadParms)           (EnterCriticalSection(&pThreadParms->IORequestListLock))
	#define UnlockScheduler()                       (LeaveCriticalSection(&IOSchedulerLock))
	#define UnlockThreadParms(pThreadParms)         (LeaveCriticalSection(&pThreadParms->IORequestListLock))

#endif // defined(FISH_HANG)

#define logmsg(fmt...)           \
{                                \
	fprintf(ios_msgpipew, fmt);  \
	fflush(ios_msgpipew);        \
}

#define IsEventSet(hEventHandle) (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

/////////////////////////////////////////////////////////////////////////////
// Debugging

#if defined(DEBUG) || defined(_DEBUG)
    #define TRACE(a...) logmsg(a)
    #define ASSERT(a) \
        do \
        { \
            if (!(a)) \
            { \
                logmsg("** Assertion Failed: %s(%d)\n",__FILE__,__LINE__); \
            } \
        } \
        while(0)
    #define VERIFY(a) ASSERT(a)
#else
    #define TRACE(a...)
    #define ASSERT(a)
    #define VERIFY(a) ((void)(a))
#endif

/////////////////////////////////////////////////////////////////////////////
// i/o scheduler variables...  (some private, some externally visible)

FILE*  ios_msgpipew           = NULL;
int    ios_arch_mode          = 0;
int    ios_devthread_priority = THREAD_PRIORITY_NORMAL;
int    ios_devthread_timeout  = 30;

LIST_ENTRY        ThreadListHeadListEntry;	// anchor for DEVTHREADPARMS linked list
CRITICAL_SECTION  IOSchedulerLock;			// lock for accessing above list
											// and below variables

long ios_devtwait    = 0;	// #of threads currently idle
int  ios_devtnbr     = 0;	// #of threads currently active
int  ios_devthwm     = 0;	// max #of threads that WERE active
int  ios_devtmax     = 0;	// max #of threads there can be
int  ios_devtunavail = 0;	// #of times 'idle' thread unavailable

/////////////////////////////////////////////////////////////////////////////
// initialize i/o scheduler variables and work areas...

void  InitIOScheduler
(
	FILE*  msgpipew,		// (for issuing msgs to Herc console)
	int    arch_mode,		// (for calling execute_ccw_chain)
	int    devt_priority,	// (for calling fthread_create)
	int    devt_timeout,	// (MAX_DEVICE_THREAD_IDLE_SECS)
	long   devt_max			// (maximum #of device threads allowed)
)
{
	ios_msgpipew           = msgpipew;
	ios_arch_mode          = arch_mode;
	ios_devthread_priority = devt_priority;
	ios_devthread_timeout  = devt_timeout;
	ios_devtmax            = devt_max;

	InitializeListHead(&ThreadListHeadListEntry);
	MyInitializeCriticalSection(&IOSchedulerLock);
}

/////////////////////////////////////////////////////////////////////////////
// device_thread parms block...

typedef struct _DevThreadParms
{
	DWORD             dwThreadID;					// device_thread thread id
	LIST_ENTRY        ThreadListLinkingListEntry;	// (just a link in the chain)
	HANDLE            hShutdownEvent;				// tells device thread to exit
	HANDLE            hRequestQueuedEvent;			// tells device thread it has work
	LIST_ENTRY        IORequestListHeadListEntry;	// anchor for DEVIOREQUEST list
	CRITICAL_SECTION  IORequestListLock;			// (for accessing above list)
	BOOL              bThreadIsDead;				// TRUE = thread has exited
}
DEVTHREADPARMS;

/////////////////////////////////////////////////////////////////////////////
// i/o request block...

typedef struct _DevIORequest
{
	LIST_ENTRY      IORequestListLinkingListEntry;	// (just a link in the chain)
	void*           pDevBlk;						// (ptr to device block)
	unsigned short  wDevNum;						// (device number for debugging)
}
DEVIOREQUEST;

/////////////////////////////////////////////////////////////////////////////
// (forward references for some of our functions...)

DEVTHREADPARMS*  SelectDeviceThread();
DEVTHREADPARMS*  CreateDeviceThread(void* pDevBlk, unsigned short wDevNum);

void*  DeviceThread(void* pThreadParms);
void   RemoveDeadThreadsFromList();
void   RemoveThisThreadFromOurList(DEVTHREADPARMS* pThreadParms);

/////////////////////////////////////////////////////////////////////////////
// Schedule a DeviceThread for this i/o request... (called by the 'startio'
// function whenever the startio or start subchannel instruction is executed)

int  ScheduleIORequest(void* pDevBlk, unsigned short wDevNum)
{
	/////////////////////////////////////////////////////////////////
	// PROGRAMMING NOTE: The various errors that can occur in this
	// function should probably be reported by returning cc1 with
	// 'channel control check' set (or something similar), but until
	// I can work out the details, I'm going to be lazy and just
	// return cc2 (subchannel busy) for now to allow the system to
	// retry the i/o request if it so desires. Hopefully the problem
	// was only intermittent and will work the second time around.
	/////////////////////////////////////////////////////////////////

	DEVIOREQUEST*    pIORequest;			// ptr to i/o request
	DEVTHREADPARMS*  pThreadParms;			// ptr to device_thread parameters

	// Create an i/o request queue entry for this i/o request

	pIORequest = (DEVIOREQUEST*) malloc(sizeof(DEVIOREQUEST));

	if (!pIORequest)
	{
		logmsg("HHC762I malloc(DEVIOREQUEST) failed; device=%4.4X, strerror=\"%s\"\n",
			wDevNum,strerror(errno));
		return 2;
	}

	InitializeListLink(&pIORequest->IORequestListLinkingListEntry);
	pIORequest->pDevBlk = pDevBlk;
	pIORequest->wDevNum = wDevNum;

	// Schedule a device_thread to process this i/o request

	LockScheduler();		// (lock scheduler vars)

	if (ios_devtmax < 0)
	{
		// Create new "one time only" thread each time...

		// (Note: the device thread's parms will automatically
		//  be locked upon return if creation was successful.)

		RemoveDeadThreadsFromList();	// (prevent runaway list)

		if (!(pThreadParms = CreateDeviceThread(pDevBlk,wDevNum)))
		{
			UnlockScheduler();			// (unlock scheduler vars)
			free(pIORequest);			// (discard i/o request)
			return 2;					// (return device busy)
		}
	}
	else
	{
		// Select a non-busy device thread to handle this i/o request...

		// (Note: the device thread's parms will automatically
		//  be locked upon return if selection was successful.)

		if (!(pThreadParms = SelectDeviceThread()))
		{
			// All threads are currently busy or no threads exist yet.

			// Since the possibility of a deadlock[1] can easily occur if we schedule
			// an i/o request to a currently busy device thread[2], we have no choice
			// but to create another device thread.

			// [1] Not an actual programmatic deadlock of course, but nonetheless
			//     a deadlock from the guest operating system's point of view.

			// [2] A curently busy device thread could easily have its channel program
			//     suspended and thus would never see the queued request until such time
			//     its suspended channel program was resumed and allowed to complete, and
			//     if the request we queued (that would end up waiting to be processed
			//     by the currently suspended device thread) just so happens to be one
			//     that the operating system needs to have completed before it can resume
			//     the currently suspended channel program, then the operating system will
			//     obviously hang. (It would end up waiting for an i/o to complete that
			//     would never complete until the currently suspended channel program was
			//     resumed, which would never be resumed until the queued i/o completes,
			//     which would never even be processed until the currently suspended
			//     channel program is resumed ..... etc.)

			if (ios_devtmax && ios_devtnbr >= ios_devtmax)	// max threads already created?
			{
				logmsg("HHC765I *WARNING* max device threads exceeded.\n");
				ios_devtunavail++;			// (count occurrences)
			}

			// Create a new device thread for this i/o request...

			// (Note: the device thread's parms will automatically
			//  be locked upon return if creation was successful.)

			if (!(pThreadParms = CreateDeviceThread(pDevBlk,wDevNum)))
			{
				UnlockScheduler();			// (unlock scheduler vars)
				free(pIORequest);			// (discard i/o request)
				return 2;					// (return device busy)
			}
		}
	}

	// (Note: the thread parms lock should still be held at this point)

	// Queue the i/o request to the selected device_thread's i/o request queue...

	InsertListTail(&pThreadParms->IORequestListHeadListEntry,&pIORequest->IORequestListLinkingListEntry);

	// Tell device_thread it has work (must do this while its request list is still
	// locked to prevent it from prematurely exiting in case it's just about to die)

	MySetEvent(pThreadParms->hRequestQueuedEvent);

	// Now unlock its request queue so it can process the request we just gave it

	UnlockThreadParms(pThreadParms);		// (let it proceed)

	// We're done, so unlock the scheduler vars so another cpu thread
	// in the configuration can schedule and i/o request and then exit
	// back the the startio function...

	UnlockScheduler();			// (unlock vars and exit; we're done)

	return 0;					// (success)
}

/////////////////////////////////////////////////////////////////////////////
// Select a non-busy DeviceThread...
// NOTE! the IOSchedulerLock must be acquired before calling this function!
// NOTE! the selected thread's parms will be locked upon return!

DEVTHREADPARMS*  SelectDeviceThread()
{
	DEVTHREADPARMS*  pThreadParms;		// ptr to selected device thread
	LIST_ENTRY*      pListEntry;		// (work)

	pListEntry = ThreadListHeadListEntry.Flink;

	while (pListEntry != &ThreadListHeadListEntry)
	{
		pThreadParms = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

		LockThreadParms(pThreadParms);		// (freeze moving target)

		if (pThreadParms->bThreadIsDead)
		{
			UnlockThreadParms(pThreadParms);
			RemoveThisThreadFromOurList(pThreadParms);
			pListEntry = ThreadListHeadListEntry.Flink;
			continue;
		}

		if (!IsEventSet(pThreadParms->hRequestQueuedEvent))
		{
			RemoveListEntry(pListEntry);
			InsertListTail(&ThreadListHeadListEntry,pListEntry);
			return pThreadParms;
		}

		UnlockThreadParms(pThreadParms);	// (can't use this one)

		pListEntry = pListEntry->Flink;
	}

	return NULL;	// (all of them are busy)
}

/////////////////////////////////////////////////////////////////////////////
// create a new device thread and add it to the list...
// NOTE! the IOSchedulerLock must be acquired before calling this function!
// NOTE! the created thread's parms will be locked upon return!

DEVTHREADPARMS*  CreateDeviceThread(void* pDevBlk, unsigned short wDevNum)
{
	DEVTHREADPARMS*  pThreadParms;		// ptr to returned device_thread parameters
	DWORD            dwThreadID;		// (work)

	pThreadParms = malloc(sizeof(DEVTHREADPARMS));		// (allocate structure)

	if (!pThreadParms)
	{
		logmsg("HHC761I malloc(DEVTHREADPARMS) failed; device=%4.4X, strerror=\"%s\"\n",
			wDevNum,strerror(errno));
		return NULL;	// (error)
	}

	pThreadParms->hShutdownEvent = MyCreateEvent(NULL,TRUE,FALSE,NULL);

	if (!pThreadParms->hShutdownEvent)
	{
		logmsg("HHC763I CreateEvent(hShutdownEvent) failed; device=%4.4X, strerror=\"%s\"\n",
			wDevNum,strerror(errno));
		free(pThreadParms);
		return NULL;	// (error)
	}

	pThreadParms->hRequestQueuedEvent = MyCreateEvent(NULL,TRUE,FALSE,NULL);

	if (!pThreadParms->hRequestQueuedEvent)
	{
		logmsg("HHC764I CreateEvent(hRequestQueuedEvent) failed; device=%4.4X, strerror=\"%s\"\n",
			wDevNum,strerror(errno));
		MyCloseHandle(pThreadParms->hShutdownEvent);
		free(pThreadParms);
		return NULL;	// (error)
	}

	MyInitializeCriticalSection(&pThreadParms->IORequestListLock);

	InitializeListLink(&pThreadParms->ThreadListLinkingListEntry);
	InitializeListHead(&pThreadParms->IORequestListHeadListEntry);

	pThreadParms->bThreadIsDead = FALSE;
	pThreadParms->dwThreadID = 0;

#ifdef FISH_HANG
	if (fthread_create(__FILE__,__LINE__,&dwThreadID,NULL,DeviceThread,pThreadParms,ios_devthread_priority) != 0)
#else
	if (fthread_create(&dwThreadID,NULL,DeviceThread,pThreadParms,ios_devthread_priority) != 0)
#endif
	{
		logmsg("HHC760I fthread_create(DeviceThread) failed; device=%4.4X, strerror=\"%s\"\n",
			wDevNum,strerror(errno));
		MyCloseHandle(pThreadParms->hShutdownEvent);
		MyCloseHandle(pThreadParms->hRequestQueuedEvent);
		MyDeleteCriticalSection(&pThreadParms->IORequestListLock);
		free(pThreadParms);
		return NULL;	// (error)
	}

	// Add the newly created device_thread to the end of our list of managed threads.

	InsertListTail(&ThreadListHeadListEntry,&pThreadParms->ThreadListLinkingListEntry);

	if (++ios_devtnbr > ios_devthwm) ios_devthwm = ios_devtnbr;

	LockThreadParms(pThreadParms);	// (lock thread parms before using)

	return pThreadParms;			// (success)
}

/////////////////////////////////////////////////////////////////////////////
// the device_thread itself...  (processes queued i/o request
// by calling "execute_ccw_chain" function in channel.c...)

extern void  call_execute_ccw_chain(int arch_mode, void* pDevBlk);

void*  DeviceThread (void* pArg)
{
	DEVTHREADPARMS*  pThreadParms;		// ptr to thread parms
	LIST_ENTRY*      pListEntry;		// (work)
	DEVIOREQUEST*    pIORequest;		// ptr to i/o request
	void*            pDevBlk;			// ptr to device block

	pThreadParms = (DEVTHREADPARMS*) pArg;

	pThreadParms->dwThreadID = GetCurrentThreadId();

	for (;;)
	{
		// Wait for an i/o request to be queued...

		InterlockedIncrement(&ios_devtwait);
		MyWaitForSingleObject(pThreadParms->hRequestQueuedEvent,ios_devthread_timeout * 1000);
		InterlockedDecrement(&ios_devtwait);

		if (IsEventSet(pThreadParms->hShutdownEvent)) break;

		// Lock our queue so it doesn't change while we take a look at it...

		LockThreadParms(pThreadParms);		// (freeze moving target)

		// Check to see if we have any work...

		if (IsListEmpty(&pThreadParms->IORequestListHeadListEntry))
		{
			// We've waited long enough...

			pThreadParms->bThreadIsDead = TRUE;		// (keep scheduler informed)
			UnlockThreadParms(pThreadParms);		// (let go of our parms block)
			return NULL;							// (return, NOT break!)
		}

		// Remove the i/o request from our queue...

		// It's important that we remove the request from our queue but
		// NOT reset our flag (if the list is now empty) until AFTER we've
		// processed the request (see further below for details as to why).

		pListEntry = RemoveListHead(&pThreadParms->IORequestListHeadListEntry);

		UnlockThreadParms(pThreadParms);	// (done with thread parms for now)

		pIORequest = CONTAINING_RECORD(pListEntry,DEVIOREQUEST,IORequestListLinkingListEntry);
		pDevBlk = pIORequest->pDevBlk;		// (this is all we need)
		free(pIORequest);					// (not needed anymore)

		// Process the i/o request by calling the proper 'execute_ccw_chain'
		// function (based on architectural mode) in source module channel.c

		call_execute_ccw_chain(ios_arch_mode, pDevBlk);	// (process i/o request)

		////////////////////////////////////////////////////////////////////////////
		//
		//                  * * *   I M P O R T A N T   * * *
		//
		// It's important that we reset our flag AFTER we're done with our request
		// in order to prevent the scheduler from trying to give us more work while
		// we're still busy processing the current i/o request (in case the channel
		// program we're processing gets suspended). If the channel program we're
		// processing gets suspended and the scheduler places another request in our
		// queue, it won't ever get processed until our suspended channel program is
		// resumed, and if the request that was placed in our queue was a request
		// for an i/o to another device that the operating system needs to complete
		// in order to obtain the information needed to resume our suspended channel
		// program, a deadlock will occur! (i.e. if the operating system needs to
		// read data from another device before it can resume our channel program,
		// then the i/o request to read from that device better not be given to us
		// because we're suspended and will never get around to executing it until
		// we're resumed, which will never happen until the i/o request waiting in
		// our queue is completed, which will never happend until we're resumed,
		// etc...)
		//
		////////////////////////////////////////////////////////////////////////////

		// Now that we're done this i/o, reset our flag.

		LockThreadParms(pThreadParms);		// (freeze moving target)

		if (IsListEmpty(&pThreadParms->IORequestListHeadListEntry))
			MyResetEvent(pThreadParms->hRequestQueuedEvent);

		UnlockThreadParms(pThreadParms);	// (thaw moving target)

		if (IsEventSet(pThreadParms->hShutdownEvent)) break;

		// If we're a "one time only" thread, then we're done.

		if (ios_devtmax < 0)
		{
			// Note: no need to lock our thread parms before setting 'dead' flag
			// since the scheduler should never queue us another request anyway
			// because we're a "one-time-only" thread.

			pThreadParms->bThreadIsDead = TRUE;		// (let garbage collector discard us)
			return NULL;							// (return, NOT break!)
		}
	}

	// The only time we reach here is if the shutdown event was signalled (i.e. we
	// were manually "cancelled" or asked to stop processing; i.e. the i/o subsystem
	// was reset). Discard all i/o requests that may still be remaining in our queue.

	TRACE("** DeviceThread %8.8X: shutdown detected\n",pThreadParms->dwThreadID);

	LockThreadParms(pThreadParms);			// (freeze moving target)

	// Discard all queued i/o requests...

	while (!IsListEmpty(&pThreadParms->IORequestListHeadListEntry))
	{
		pListEntry = RemoveListHead(&pThreadParms->IORequestListHeadListEntry);

		pIORequest = CONTAINING_RECORD(pListEntry,DEVIOREQUEST,IORequestListLinkingListEntry);

		TRACE("** DeviceThread %8.8X: discarding i/o request for device %4.4X\n",
			pThreadParms->dwThreadID,pIORequest->wDevNum);

		free(pIORequest);
	}

	pThreadParms->bThreadIsDead = TRUE;		// (tell scheduler we've died)

	TRACE("** DeviceThread %8.8X: shutdown complete\n",pThreadParms->dwThreadID);

	UnlockThreadParms(pThreadParms);		// (thaw moving target)

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// shutdown idle threads (if possible) until number drops below threshold...

void  TrimDeviceThreads()
{
	DEVTHREADPARMS*  pThreadParms;
	BOOL             bThreadIsBusy;
	LIST_ENTRY*      pListEntry;

	LockScheduler();		// (lock scheduler vars)

	pListEntry = ThreadListHeadListEntry.Flink;

	while (pListEntry != &ThreadListHeadListEntry && ios_devtnbr > ios_devtmax)
	{
		pThreadParms = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

		pListEntry = pListEntry->Flink;

		LockThreadParms(pThreadParms);
		bThreadIsBusy = IsEventSet(pThreadParms->hRequestQueuedEvent);
		UnlockThreadParms(pThreadParms);

		if (bThreadIsBusy) continue;

		// Thread is currently idle and awaiting work. Tell it to exit.

		MySetEvent(pThreadParms->hShutdownEvent);

		Sleep(100);	// (give it time to die)
		RemoveDeadThreadsFromList();
	}

	UnlockScheduler();		// (unlock shceduler vars)
}

#if 0	// (the following is not needed yet)
/////////////////////////////////////////////////////////////////////////////
// ask all device_threads to exit... (i.e. i/o subsystem reset)

void  KillAllDeviceThreads()
{
	DEVTHREADPARMS*  pThreadParms;
	LIST_ENTRY*      pListEntry;

	TRACE("** ENTRY KillAllDeviceThreads\n");

	LockScheduler();				// (lock scheduler vars)

	RemoveDeadThreadsFromList();	// (discard dead threads)

	while (!IsListEmpty(&ThreadListHeadListEntry))
	{
		pListEntry = ThreadListHeadListEntry.Flink;		// (get starting link in chain)

		while (pListEntry != &ThreadListHeadListEntry)	// (while not end of chain reached...)
		{
			pThreadParms = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

			// (Note: we need to signal the hRequestQueuedEvent event too so that
			//  it can wake up and notice that the hShutdownEvent event is signaled)

			TRACE("** KillAllDeviceThreads: setting shutdown event for thread %8.8X\n",
				pThreadParms->dwThreadID);

			LockThreadParms(pThreadParms);					// (lock thread parms)
			MySetEvent(pThreadParms->hShutdownEvent);		// (request shutdown)
			MySetEvent(pThreadParms->hRequestQueuedEvent);	// (wakeup thread)
			UnlockThreadParms(pThreadParms);				// (unlock thread parms)

			pListEntry = pListEntry->Flink;		// (go on to next link in chain)
		}

		UnlockScheduler();				// (unlock scheduler vars)
		TRACE("** KillAllDeviceThreads: waiting for thread(s) to shutdown\n");
		Sleep(100);						// (give them time to die)
		LockScheduler();				// (re-lock scheduler vars)
		RemoveDeadThreadsFromList();	// (discard dead threads)
	}

	UnlockScheduler();				// (unlock scheduler vars)

	TRACE("** EXIT KillAllDeviceThreads\n");
}
#endif // (the preceding is not needed yet)

/////////////////////////////////////////////////////////////////////////////
// remove all dead threads from our list of threads...
// NOTE! the IOSchedulerLock must be acquired before calling this function!

void  RemoveDeadThreadsFromList()
{
	DEVTHREADPARMS*  pThreadParms;
	BOOL             bThreadIsDead;
	LIST_ENTRY*      pListEntry = ThreadListHeadListEntry.Flink;

	while (pListEntry != &ThreadListHeadListEntry)
	{
		pThreadParms = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

		pListEntry = pListEntry->Flink;

		LockThreadParms(pThreadParms);

		bThreadIsDead = pThreadParms->bThreadIsDead;

		UnlockThreadParms(pThreadParms);

		if (bThreadIsDead) RemoveThisThreadFromOurList(pThreadParms);
	}
}

/////////////////////////////////////////////////////////////////////////////
// private function to remove an entry from our list and discard it...
// NOTE! the IOSchedulerLock must be acquired before calling this function!

void  RemoveThisThreadFromOurList(DEVTHREADPARMS* pThreadParms)
{
	RemoveListEntry(&pThreadParms->ThreadListLinkingListEntry);
	MyCloseHandle(pThreadParms->hShutdownEvent);
	MyCloseHandle(pThreadParms->hRequestQueuedEvent);
	MyDeleteCriticalSection(&pThreadParms->IORequestListLock);
	free(pThreadParms);
	ios_devtnbr--;			// (track number of active device_thread)
}

/////////////////////////////////////////////////////////////////////////////

#if defined(FISH_HANG)

char PrintDEVIOREQUESTBuffer[2048];

char*  PrintDEVIOREQUEST(DEVIOREQUEST* pIORequest, DEVTHREADPARMS* pDEVTHREADPARMS)
{
	LIST_ENTRY*    pListEntry;
	DEVIOREQUEST*  pNextDEVIOREQUEST;

	pListEntry = pIORequest->IORequestListLinkingListEntry.Flink;

	if (pListEntry != &pDEVTHREADPARMS->IORequestListHeadListEntry)
	{
		pNextDEVIOREQUEST = CONTAINING_RECORD(pListEntry,DEVIOREQUEST,IORequestListLinkingListEntry);
	}
	else pNextDEVIOREQUEST = NULL;

	sprintf(PrintDEVIOREQUESTBuffer,
		"DEVIOREQUEST @ %8.8X\n"
		"               pDevBlk                       = %8.8X\n"
		"               wDevNum                       = %4.4X\n"
		"               IORequestListLinkingListEntry = %8.8X\n",
		(int)pIORequest,
			(int)pIORequest->pDevBlk,
			pIORequest->wDevNum,
			(int)pNextDEVIOREQUEST
		);

	return PrintDEVIOREQUESTBuffer;
}

/////////////////////////////////////////////////////////////////////////////

void  PrintAllDEVIOREQUESTs(DEVTHREADPARMS* pDEVTHREADPARMS)
{
	DEVIOREQUEST*  pDEVIOREQUEST;
	LIST_ENTRY*    pListEntry;

	pListEntry = pDEVTHREADPARMS->IORequestListHeadListEntry.Flink;

	while (pListEntry != &pDEVTHREADPARMS->IORequestListHeadListEntry)
	{
		pDEVIOREQUEST = CONTAINING_RECORD(pListEntry,DEVIOREQUEST,IORequestListLinkingListEntry);
		pListEntry = pListEntry->Flink;
		fprintf(stdout,"%s\n",PrintDEVIOREQUEST(pDEVIOREQUEST,pDEVTHREADPARMS));
	}
}

/////////////////////////////////////////////////////////////////////////////

char PrintDEVTHREADPARMSBuffer[4096];

char*  PrintDEVTHREADPARMS(DEVTHREADPARMS* pDEVTHREADPARMS)
{
	LIST_ENTRY*      pListEntry;
	DEVIOREQUEST*    pDEVIOREQUEST;
	DEVTHREADPARMS*  pNextDEVTHREADPARMS;

	pListEntry = pDEVTHREADPARMS->IORequestListHeadListEntry.Flink;

	if (pListEntry != &pDEVTHREADPARMS->IORequestListHeadListEntry)
	{
		pDEVIOREQUEST = CONTAINING_RECORD(pListEntry,DEVIOREQUEST,IORequestListLinkingListEntry);
	}
	else pDEVIOREQUEST = NULL;

	pListEntry = pDEVTHREADPARMS->ThreadListLinkingListEntry.Flink;

	pNextDEVTHREADPARMS = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

	sprintf(PrintDEVTHREADPARMSBuffer,
		"DEVTHREADPARMS @ %8.8X\n"
		"                 dwThreadID                 = %8.8X\n"
		"                 hShutdownEvent             = %s\n"
		"                 hRequestQueuedEvent        = %s\n"
		"                 bThreadIsDead              = %s\n"
		"                 IORequestListHeadListEntry = %8.8X\n"
		"                 ThreadListLinkingListEntry = %8.8X\n",
		(int)pDEVTHREADPARMS,
			(int)pDEVTHREADPARMS->dwThreadID,
			IsEventSet(pDEVTHREADPARMS->hShutdownEvent)      ? "** SIGNALED **" : "(not signaled)",
			IsEventSet(pDEVTHREADPARMS->hRequestQueuedEvent) ? "** SIGNALED **" : "(not signaled)",
			pDEVTHREADPARMS->bThreadIsDead                   ?      "TRUE"      :      "false",
			(int)pDEVIOREQUEST,
			(int)pNextDEVTHREADPARMS
		);

	return PrintDEVTHREADPARMSBuffer;
}

/////////////////////////////////////////////////////////////////////////////

void  PrintAllDEVTHREADPARMSs()
{
	LIST_ENTRY*      pListEntry;
	DEVTHREADPARMS*  pDEVTHREADPARMS;

	LockScheduler();

	pListEntry = ThreadListHeadListEntry.Flink;

	if (pListEntry != &ThreadListHeadListEntry)
	{
		pDEVTHREADPARMS = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);
	}
	else pDEVTHREADPARMS = (DEVTHREADPARMS*) &ThreadListHeadListEntry;

	fprintf(stdout,"\nDEVTHREADPARMS LIST ANCHOR @ %8.8X --> %8.8X\n\n",
		(int)&ThreadListHeadListEntry,(int)pDEVTHREADPARMS);

	while (pListEntry != &ThreadListHeadListEntry)
	{
		pDEVTHREADPARMS = CONTAINING_RECORD(pListEntry,DEVTHREADPARMS,ThreadListLinkingListEntry);

		LockThreadParms(pDEVTHREADPARMS);

			fprintf(stdout,"%s\n",PrintDEVTHREADPARMS(pDEVTHREADPARMS));
			PrintAllDEVIOREQUESTs(pDEVTHREADPARMS);

		UnlockThreadParms(pDEVTHREADPARMS);

		pListEntry = pListEntry->Flink;
	}

	UnlockScheduler();
}

/////////////////////////////////////////////////////////////////////////////

#include "fishhang.c"

#endif // defined(FISH_HANG)

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(OPTION_FISHIO)
