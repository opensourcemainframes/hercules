/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2002                */
/*              ESA/390 Emulator Header File                         */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2002      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2002      */

/*-------------------------------------------------------------------*/
/* Header file containing Hercules internal data structures          */
/* and function prototypes.                                          */
/*-------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _REENTRANT    /* Ensure that reentrant code is generated *JJ */
#define _THREAD_SAFE            /* Some systems use this instead *JJ */

#include "feature.h"

#if !defined(_GNU_SOURCE)
 #define _GNU_SOURCE                   /* required by strsignal() *JJ */
#endif

#include "cpuint.h"

#if !defined(_HERCULES_H)

#include "machdep.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>
#include <pwd.h>
#if defined(HAVE_BYTESWAP_H)
 #include <byteswap.h>
#else
 #include "hbyteswp.h"
#endif
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <zlib.h>
#if defined(CCKD_BZIP2) || defined(HET_BZIP2)
#include <bzlib.h>
#endif

#ifndef _POSIX_SYNCHRONIZED_IO
/* If fdatasync is not necessarily available, fsync will do fine */
#define fdatasync(fd) fsync(fd)
#endif

#include "hercnls.h"
#include "version.h"
#include "hetlib.h"
#include "codepage.h"

/* definition of CLK_TCK is not part of the SUSE 7.2 definition.  Added (VB) */
#  ifndef CLK_TCK
#   define CLK_TCK      CLOCKS_PER_SEC
#  endif

#if 1
/* Following declares are missing from suse 7.1 */
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
#endif

#endif /*!defined(_HERCULES_H)*/

#include "esa390.h"

#if !defined(_HERCULES_H)

#define _HERCULES_H

#include "dasdtab.h"

/*-------------------------------------------------------------------*/
/* Windows 32-specific definitions                                   */
/*-------------------------------------------------------------------*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef WIN32
#define socklen_t int
#if !defined(OPTION_FTHREADS)
/* fake loading of <windows.h> and <winsock.h> so we can use         */
/* pthreads-win32 instead of the native cygwin pthreads support,     */
/* which doesn't include pthread_cond bits                           */
#define _WINDOWS_
#define _WINSOCKAPI_
#define _WINDOWS_H
#define _WINSOCK_H
#endif // !defined(OPTION_FTHREADS)
#define HANDLE int
#define DWORD int       /* will be undefined later */
#endif

/*-------------------------------------------------------------------*/
/* Macro for issuing panel commands                                  */
/*-------------------------------------------------------------------*/

#define SYNCHRONOUS_PANEL_CMD(cmdline) \
    panel_command((cmdline))

#define ASYNCHRONOUS_PANEL_CMD(cmdline) \
    create_thread(&cmdtid,&sysblk.detattr,panel_command,(cmdline))

/*-------------------------------------------------------------------*/
/* Macro definitions for tracing                                     */
/*-------------------------------------------------------------------*/
#ifndef FLUSHLOG
#define logmsg(a...) \
    do { \
        fprintf(sysblk.msgpipew, a); \
    } while(0)
#define devmsg(a...) \
    do { \
        fprintf(dev->msgpipew, a); \
    } while(0)
#define cckdmsg(a...) \
    do { \
        fprintf(cckdblk.msgpipew, a); \
    } while(0)
#else
#define logmsg(a...) \
    do { \
        fprintf(sysblk.msgpipew, a); \
        fflush(sysblk.msgpipew); \
    } while(0)
#define devmsg(a...) \
    do { \
        fprintf(dev->msgpipew, a); \
        fflush(dev->msgpipew); \
    } while(0)
#define cckdmsg(a...) \
    do { \
        fprintf(cckdblk.msgpipew, a); \
        fflush(cckdblk.msgpipew); \
    } while(0)
#endif
#define DEVTRACE(format, a...) \
    do { \
        if(dev->ccwtrace||dev->ccwstep) \
            fprintf(dev->msgpipew, "%4.4X:" format, dev->devnum, a); \
    } while(0)

/* Debugging */

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
    #define VERIFY(a) ASSERT((a))
#else
    #define TRACE(a...)
    #define ASSERT(a)
    #define VERIFY(a) ((void)(a))
#endif

/*-------------------------------------------------------------------*/
/* Macro definitions for version number                              */
/*-------------------------------------------------------------------*/
#define STRINGMAC(x)    #x
#define MSTRING(x)      STRINGMAC(x)

/*-------------------------------------------------------------------*/
/* Macro definitions for thread functions                            */
/*-------------------------------------------------------------------*/
#ifndef NOTHREAD
#ifdef WIN32
#define HAVE_STRUCT_TIMESPEC
#endif
#if defined(OPTION_FTHREADS)
#include "fthreads.h"
#else // !defined(OPTION_FTHREADS)
#include <pthread.h>
#endif // defined(OPTION_FTHREADS)
#ifdef WIN32
#undef DWORD
#endif
#if defined(OPTION_FTHREADS)
#define NORMAL_THREAD_PRIORITY  0  /* THREAD_PRIORITY_NORMAL */
#define DEVICE_THREAD_PRIORITY  1  /* THREAD_PRIORITY_ABOVE_NORMAL */
typedef fthread_t         TID;
typedef fthread_mutex_t   LOCK;
typedef fthread_cond_t    COND;
typedef fthread_attr_t    ATTR;

#if defined(FISH_HANG)

	#define create_thread(ptid,pat,fn,arg)         fthread_create(__FILE__,__LINE__,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),NORMAL_THREAD_PRIORITY)
	#define create_device_thread(ptid,pat,fn,arg)  fthread_create(__FILE__,__LINE__,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),DEVICE_THREAD_PRIORITY)

	#define initialize_lock(plk)                   fthread_mutex_init(__FILE__,__LINE__,(plk))
	#define obtain_lock(plk)                       fthread_mutex_lock(__FILE__,__LINE__,(plk))
	#define try_obtain_lock(plk)                   fthread_mutex_trylock(__FILE__,__LINE__,(plk))
	#define test_lock(plk) \
			(fthread_mutex_trylock(__FILE__,__LINE__,(plk)) ? 1 : fthread_mutex_unlock(__FILE__,__LINE__,(plk)) )
	#define release_lock(plk)                      fthread_mutex_unlock(__FILE__,__LINE__,(plk))

	#define initialize_condition(pcond)            fthread_cond_init(__FILE__,__LINE__,(pcond))
	#define signal_condition(pcond)                fthread_cond_signal(__FILE__,__LINE__,(pcond))
	#define broadcast_condition(pcond)             fthread_cond_broadcast(__FILE__,__LINE__,(pcond))
	#define wait_condition(pcond,plk)              fthread_cond_wait(__FILE__,__LINE__,(pcond),(plk))
	#define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait(__FILE__,__LINE__,(pcond),(plk),(tm))

#else // !defined(FISH_HANG)

	#define create_thread(ptid,pat,fn,arg)         fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),NORMAL_THREAD_PRIORITY)
	#define create_device_thread(ptid,pat,fn,arg)  fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),DEVICE_THREAD_PRIORITY)

	#define initialize_lock(plk)                   fthread_mutex_init((plk))
	#define obtain_lock(plk)                       fthread_mutex_lock((plk))
	#define try_obtain_lock(plk)                   fthread_mutex_trylock((plk))
	#define test_lock(plk) \
			(fthread_mutex_trylock((plk)) ? 1 : fthread_mutex_unlock((plk)) )
	#define release_lock(plk)                      fthread_mutex_unlock((plk))

	#define initialize_condition(pcond)            fthread_cond_init((pcond))
	#define signal_condition(pcond)                fthread_cond_signal((pcond))
	#define broadcast_condition(pcond)             fthread_cond_broadcast((pcond))
	#define wait_condition(pcond,plk)              fthread_cond_wait((pcond),(plk))
	#define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait((pcond),(plk),(tm))

#endif // defined(FISH_HANG)

#define initialize_detach_attr(pat)            /* unsupported */
#define signal_thread(tid,signo)               fthread_kill((tid),(signo))
#define thread_id()                            fthread_self()
#define exit_thread(exitvar_ptr)               fthread_exit((exitvar_ptr))
#else // !defined(OPTION_FTHREADS)
typedef pthread_t                       TID;
typedef pthread_mutex_t                 LOCK;
typedef pthread_cond_t                  COND;
typedef pthread_attr_t                  ATTR;
#define initialize_lock(plk) \
        pthread_mutex_init((plk),NULL)
#define obtain_lock(plk) \
        pthread_mutex_lock((plk))
#define try_obtain_lock(plk) \
        pthread_mutex_trylock((plk))
#define release_lock(plk) \
        pthread_mutex_unlock((plk))
#define test_lock(plk) \
        (pthread_mutex_trylock((plk)) ? 1 : pthread_mutex_unlock((plk)) )
#define initialize_condition(pcond) \
        pthread_cond_init((pcond),NULL)
#define signal_condition(pcond) \
        pthread_cond_signal((pcond))
#define broadcast_condition(pcond) \
        pthread_cond_broadcast((pcond))
#define wait_condition(pcond,plk) \
        pthread_cond_wait((pcond),(plk))
#define timed_wait_condition(pcond,plk,timeout) \
        pthread_cond_timedwait((pcond),(plk),(timeout))
#define initialize_detach_attr(pat) \
        pthread_attr_init((pat)); \
		pthread_attr_setstacksize((pat),1048576); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_DETACHED)
typedef void*THREAD_FUNC(void*);
#define create_thread(ptid,pat,fn,arg) \
        pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define create_device_thread(ptid,pat,fn,arg) \
        pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define exit_thread(_code) \
        pthread_exit((_code))
#if !defined(WIN32)
#define signal_thread(tid,signo) \
        pthread_kill(tid,signo)
#else // defined(WIN32)
#define signal_thread(tid,signo)
#endif // !defined(WIN32)
#define thread_id() \
        pthread_self()
#define exit_thread(exitvar_ptr) \
        pthread_exit((exitvar_ptr))
#endif // defined(OPTION_FTHREADS)
#else
typedef int                             TID;
typedef int                             LOCK;
typedef int                             COND;
typedef int                             ATTR;
#define initialize_lock(plk)            *(plk)=0
#define obtain_lock(plk)                *(plk)=1
#define try_obtain_lock(plk)            *(plk)=1
#define release_lock(plk)               *(plk)=0
#define test_lock(plk)                  (*(plk)!=0)
#define initialize_condition(pcond)     *(pcond)=0
#define signal_condition(pcond)         *(pcond)=1
#define broadcast_condition(pcond)      *(pcond)=1
#define wait_condition(pcond,plk)       *(pcond)=1
#define timed_wait_condition(pcond,plk,timeout) *(pcond)=1
#define initialize_detach_attr(pat)     *(pat)=1
#define create_thread(ptid,pat,fn,arg)  (*(ptid)=0,fn(arg),0)
#define signal_thread(tid,signo)        raise(signo)
#define thread_id()                     0
#define exit_thread(exitvar_ptr)        (exit(0)) /* (should not ever be used with NOTHREAD!) */
#endif

/* Pattern for displaying the thread_id */
#define TIDPAT "%8.8lX"
#if defined(WIN32) && !defined(OPTION_FTHREADS)
#undef  TIDPAT
#define TIDPAT "%p"
#endif // defined(WIN32) && !defined(OPTION_FTHREADS)

/*-------------------------------------------------------------------*/
/* Prototype definitions for device handler functions                */
/*-------------------------------------------------------------------*/
struct _DEVBLK;
typedef int DEVIF (struct _DEVBLK *dev, int argc, BYTE *argv[]);
typedef void DEVQF (struct _DEVBLK *dev, BYTE **class, int buflen,
        BYTE *buffer);
typedef void DEVXF (struct _DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual);
typedef int DEVCF (struct _DEVBLK *dev);
typedef void DEVSF (struct _DEVBLK *dev);
typedef int CKDRT (struct _DEVBLK *, int, int, BYTE *);
typedef int CKDUT (struct _DEVBLK *, BYTE *, int, BYTE *);
typedef int FBARB (struct _DEVBLK *, BYTE *, int, BYTE *);
typedef int FBAWB (struct _DEVBLK *, BYTE *, int, BYTE *);

/*-------------------------------------------------------------------*/
/* Structure definition for the Vector Facility                      */
/*-------------------------------------------------------------------*/
#if defined(_FEATURE_VECTOR_FACILITY)
typedef struct _VFREGS {                /* Vector Facility Registers*/
        unsigned int
                online:1;               /* 1=VF is online            */
        U64     vsr;                    /* Vector Status Register    */
        U64     vac;                    /* Vector Activity Count     */
        BYTE    vmr[VECTOR_SECTION_SIZE/8];  /* Vector Mask Register */
        U32     vr[16][VECTOR_SECTION_SIZE]; /* Vector Registers     */
    } VFREGS;
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

typedef void    (*SIEFN) ();

/*-------------------------------------------------------------------*/
/* Structure definition for CPU register context                     */
/*-------------------------------------------------------------------*/
typedef struct _REGS {                  /* Processor registers       */
        int     arch_mode;              /* Architectural mode        */
        U64     ptimer;                 /* CPU timer                 */
        U64     clkc;                   /* 0-7=Clock comparator epoch,
                                           8-63=Comparator bits 0-55 */
        S64     todoffset;              /* TOD offset for this CPU   */
        U64     instcount;              /* Instruction counter       */
        U64     prevcount;              /* Previous instruction count*/
        U32     mipsrate;               /* Instructions/millisecond  */
        U32     siocount;               /* SIO/SSCH counter          */
        U32     siosrate;               /* IOs per second            */
        TLBE    tlb[256];               /* Translation lookaside buf */
        TID     cputid;                 /* CPU thread identifier     */
        DW      gr[16];                 /* General registers         */
        DW      cr[16];                 /* Control registers         */
        DW      px;                     /* Prefix register           */
        DW      mc;                     /* Monitor Code              */
        DW      ea;                     /* Exception address         */
        DW      et;                     /* Execute Target address    */
#if defined(OPTION_AIA_BUFFER)
        DW      ai;                     /* Absolute instruction addr */
        DW      vi;                     /* Virtual instruction addr  */
#endif /*defined(OPTION_AIA_BUFFER)*/
#if defined(OPTION_AEA_BUFFER)
#if !defined(OPTION_FAST_LOGICAL)
        DW      ae[16];                 /* Absolute effective addr   */
        DW      ve[16];                 /* Virtual effective addr    */
        BYTE    aekey[16];              /* Storage Key               */
        int     aeacc[16];              /* Access type               */
#else
#if defined(OPTION_REDUCED_INVAL)
        DW      ae[256];                /* Absolute effective addr   */
        DW      ve[256];                /* Virtual effective addr    */
        BYTE    aekey[256];             /* Storage Key               */
        int     aeacc[256];             /* Access type               */
        int     aearn[256];             /* Address room              */
#else
        DW      ae[16];                 /* Absolute effective addr   */
        DW      ve[16];                 /* Virtual effective addr    */
        BYTE    aekey[16];              /* Storage Key               */
        int     aeacc[16];              /* Access type               */
        int     aearn[16];              /* Address room              */
#endif
        int     aenoarn;                /* no access mode           */
#endif
#endif /*defined(OPTION_AEA_BUFFER)*/
#define GR_G(_r) gr[(_r)].D
#define GR_H(_r) gr[(_r)].F.H.F          /* Fullword bits 0-31       */
#define GR_HHH(_r) gr[(_r)].F.H.H.H.H    /* Halfword bits 0-15       */
#define GR_HHL(_r) gr[(_r)].F.H.H.L.H    /* Halfword low, bits 16-31 */
#define GR_L(_r) gr[(_r)].F.L.F          /* Fullword low, bits 32-63 */
#define GR_LHH(_r) gr[(_r)].F.L.H.H.H    /* Halfword bits 32-47      */
#define GR_LHL(_r) gr[(_r)].F.L.H.L.H    /* Halfword low, bits 48-63 */
#define GR_LHHCH(_r) gr[(_r)].F.L.H.H.B.H   /* Character, bits 32-39 */
#define GR_LA24(_r) gr[(_r)].F.L.A.A     /* 24 bit addr, bits 40-63  */
#define GR_LA8(_r) gr[(_r)].F.L.A.B      /* 24 bit addr, unused bits */
#define GR_LHLCL(_r) gr[(_r)].F.L.H.L.B.L   /* Character, bits 56-63 */
#define GR_LHLCH(_r) gr[(_r)].F.L.H.L.B.H   /* Character, bits 48-55 */
#define CR_G(_r)   cr[(_r)].D            /* Bits 0-63                */
#define CR_H(_r)   cr[(_r)].F.H.F        /* Fullword bits 0-31       */
#define CR_HHH(_r) cr[(_r)].F.H.H.H.H    /* Halfword bits 0-15       */
#define CR_HHL(_r) cr[(_r)].F.H.H.L.H    /* Halfword low, bits 16-31 */
#define CR_L(_r)   cr[(_r)].F.L.F        /* Fullword low, bits 32-63 */
#define CR_LHH(_r) cr[(_r)].F.L.H.H.H    /* Halfword bits 32-47      */
#define CR_LHHCH(_r) cr[(_r)].F.L.H.H.B.H   /* Character, bits 32-39 */
#define CR_LHL(_r) cr[(_r)].F.L.H.L.H    /* Halfword low, bits 48-63 */
#define MC_G    mc.D
#define MC_L    mc.F.L.F
#define EA_G    ea.D
#define EA_L    ea.F.L.F
#define ET_G    et.D
#define ET_L    et.F.L.F
#define PX_G    px.D
#define PX_L    px.F.L.F
#if defined(OPTION_AIA_BUFFER)
#define AI_G    ai.D
#define AI_L    ai.F.L.F
#define VI_G    vi.D
#define VI_L    vi.F.L.F
#endif /*defined(OPTION_AIA_BUFFER)*/
#if defined(OPTION_AEA_BUFFER)
#define AE_G(_r)    ae[(_r)].D
#define AE_L(_r)    ae[(_r)].F.L.F
#define VE_G(_r)    ve[(_r)].D
#define VE_L(_r)    ve[(_r)].F.L.F
#endif /*defined(OPTION_AEA_BUFFER)*/
        U32     ar[16];                 /* Access registers          */
#define AR(_r)  ar[(_r)]
        U32     fpr[32];                /* Floating point registers  */
// #if defined(FEATURE_BINARY_FLOATING_POINT)
        U32     fpc;                    /* IEEE Floating Point
                                                    Control Register */
        U32     dxc;                    /* Data exception code       */
// #endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/
        U16     chanset;                /* Connected channel set     */
        U32     todpr;                  /* TOD programmable register */
        U16     monclass;               /* Monitor event class       */
        U16     cpuad;                  /* CPU address for STAP      */
        PSW     psw;                    /* Program status word       */
        BYTE    excarid;                /* Exception access register */
        BYTE    opndrid;                /* Operand access register   */
        DWORD   exinst;                 /* Target of Execute (EX)    */

        RADR    mainsize;               /* Central Storage size or   */
                                        /* guest storage size (SIE)  */
#if defined(_FEATURE_SIE)
        RADR    sie_state;              /* Address of the SIE state
                                           descriptor block or 0 when
                                           not running under SIE     */
        SIEBK  *siebk;                  /* Sie State Desc structure  */
        SIEFN   sie_guestpi;            /* SIE guest pgm int routine */
        SIEFN   sie_hostpi;             /* SIE host pgm int routine  */
        struct _REGS *hostregs;         /* Pointer to the hypervisor
                                           register context          */
        struct _REGS *guestregs;        /* Pointer to the guest
                                           register context          */
        PSA_3XX *sie_psa;               /* PSA of guest CPU          */
        RADR    sie_mso;                /* Main Storage Origin       */
        RADR    sie_xso;                /* eXpanded Storage Origin   */
        RADR    sie_xsl;                /* eXpanded Storage Limit    */
        RADR    sie_rcpo;               /* Ref and Change Preserv.   */
        RADR    sie_scao;               /* System Contol Area        */
        S64     sie_epoch;              /* TOD offset in state desc. */
        unsigned int
                sie_active:1,           /* Sie active (host only)    */
                sie_pref:1;             /* Preferred-storage mode    */
#endif /*defined(_FEATURE_SIE)*/

// #if defined(FEATURE_PER)
        U16     perc;                   /* PER code                  */
        RADR    peradr;                 /* PER address               */
        BYTE    peraid;                 /* PER access id             */
// #endif /*defined(FEATURE_PER)*/

        BYTE    cpustate;               /* CPU stopped/started state */
        unsigned int                    /* Flags                     */
                cpuonline:1,            /* 1=CPU is online           */
                loadstate:1,            /* 1=CPU is in load state    */
                checkstop:1,            /* 1=CPU is checkstop-ed     */
#ifndef INTERRUPTS_FAST_CHECK
                cpuint:1,               /* 1=There is an interrupt
                                             pending for this CPU    */
                itimer_pending:1,       /* 1=Interrupt is pending for
                                             the interval timer      */
                restart:1,              /* 1=Restart interrpt pending*/
                extcall:1,              /* 1=Extcall interrpt pending*/
                emersig:1,              /* 1=Emersig interrpt pending*/
                ptpend:1,               /* 1=CPU timer int pending   */
                ckpend:1,               /* 1=Clock comp int pending  */
                storstat:1,             /* 1=Stop and store status   */
#endif /*INTERRUPTS_FAST_CHECK*/
                sigpreset:1,            /* 1=SIGP cpu reset received */
                sigpireset:1,           /* 1=SIGP initial cpu reset  */
                panelregs:1,            /* 1=Disallow program checks */
                instvalid:1;            /* 1=Inst field is valid     */
#ifdef INTERRUPTS_FAST_CHECK
        U32     ints_state;             /* CPU Interrupts Status     */
        U32     ints_mask;              /* Respective Interrupts Mask*/
#endif /*INTERRUPTS_FAST_CHECK*/
        BYTE    malfcpu                 /* Malfuction alert flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        BYTE    emercpu                 /* Emergency signal flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        U16     extccpu;                /* CPU causing external call */
        BYTE    inst[6];                /* Last-fetched instruction  */
        BYTE    *ip;                    /* Pointer to Last-fetched
                                           instruction (inst might
                                           not be uptodate           */
// #if MAX_CPU_ENGINES > 1
        unsigned int                    /* Flags                     */
                mainlock:1,             /* MAINLOCK held indicator   */
                mainsync:1;             /* MAINLOCK sync wait        */
// #ifdef SMP_SERIALIZATION
                /* Locking and unlocking the serialisation lock causes
                   the processor cache to be flushed this is used to
                   mimic the S/390 serialisation operation.  To avoid
                   contention, each S/390 CPU has its own lock       */
        LOCK    serlock;                /* Serialization lock        */
// #endif /*SMP_SERIALIZATION*/
// #endif /*MAX_CPU_ENGINES > 1*/

#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS *vf;                     /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

        jmp_buf progjmp;                /* longjmp destination for
                                           program check return      */

        jmp_buf archjmp;                /* longjmp destination to
                                           switch architecture mode  */
#if MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND)
        COND    intcond;                /* CPU interrupt condition   */
#endif /* MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND) */
        U32     cpumask;                /* CPU mask                  */
    } REGS;

/* Definitions for CPU state */
#define CPUSTATE_STOPPED        1       /* CPU is stopped            */
#define CPUSTATE_STOPPING       2       /* CPU is stopping           */
#define CPUSTATE_STARTED        4       /* CPU is started            */
#define CPUSTATE_STARTING       8       /* CPU is starting           */
#define CPUSTATE_ALL          255       /* All CPU states            */

#define ALL_CPUS                0xffffffff

/* Macros to signal interrupt condition to a CPU[s] */
#if MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND)
#define WAKEUP_CPU(cpu) signal_condition(&sysblk.regs[(cpu)].intcond)
#define WAKEUP_WAITING_CPU(mask,statemask) \
 { int i; \
   for (i = 0; i < MAX_CPU_ENGINES; i++) \
     if ((sysblk.regs[i].cpustate & (statemask)) \
      && (sysblk.regs[i].cpumask  & (mask)) \
      && (sysblk.regs[i].cpumask  & sysblk.waitmask)) \
      { \
        signal_condition(&sysblk.regs[i].intcond); \
        break; \
      } \
 }
#define WAKEUP_WAITING_CPUS(mask,statemask) \
 { int i; \
   for (i = 0; i < MAX_CPU_ENGINES; i++) \
     if ((sysblk.regs[i].cpustate & (statemask)) \
      && (sysblk.regs[i].cpumask  & (mask)) \
      && (sysblk.regs[i].cpumask  & sysblk.waitmask)) \
        signal_condition(&sysblk.regs[i].intcond); \
 }
#define INTCOND       regs->intcond
#else /* MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND) */
#define WAKEUP_CPU(cpu)               broadcast_condition(&sysblk.intcond)
#define WAKEUP_WAITING_CPU(m,s)       WAKEUP_CPU(0)
#define WAKEUP_WAITING_CPUS(m,s)      WAKEUP_CPU(0)
#define INTCOND       sysblk.intcond
#endif /* MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND) */

/* Macros to queue/dequeue a device on the I/O interrupt queue */
#define QUEUE_IO_INTERRUPT(dev) \
 { \
   if (sysblk.iointq == NULL) \
   { \
     sysblk.iointq = (dev); \
     (dev)->iointq = NULL; \
   } \
   else if (sysblk.iointq != (dev)) \
   { \
     if (sysblk.iointq->priority > (dev)->priority) \
     { \
       (dev)->iointq = sysblk.iointq; \
       sysblk.iointq = (dev); \
     } \
     else \
     { \
     DEVBLK *prev; \
     for (prev = sysblk.iointq; prev->iointq != NULL; prev = prev->iointq) \
       if (prev->iointq == (dev) \
          || prev->iointq->priority > dev->priority) break; \
     if (prev->iointq != (dev)) \
     { \
       (dev)->iointq = prev->iointq; \
       prev->iointq = (dev); \
     } \
   } \
   } \
 }
#define DEQUEUE_IO_INTERRUPT(dev) \
 { \
   if ((dev) == sysblk.iointq) \
     sysblk.iointq = (dev)->iointq; \
   else { \
     DEVBLK *prev; \
     for (prev = sysblk.iointq; prev && prev->iointq != (dev); prev = prev->iointq); \
     if (prev) prev->iointq = (dev)->iointq; \
   } \
 }

/*-------------------------------------------------------------------*/
/* System configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _SYSBLK {
        int     arch_mode;              /* Architecturual mode       */
                                        /* 0 == S/370                */
                                        /* 1 == ESA/390              */
                                        /* 2 == ESAME                */
        int     arch_z900;              /* 1 == ESAME supported      */
        RADR    mainsize;               /* Main storage size (bytes) */
        BYTE   *mainstor;               /* -> Main storage           */
        BYTE   *storkeys;               /* -> Main storage key array */
        U32     xpndsize;               /* Expanded size (4K pages)  */
        BYTE   *xpndstor;               /* -> Expanded storage       */
        U64     cpuid;                  /* CPU identifier for STIDP  */
        U64     todclk;                 /* 0-7=TOD clock epoch,
                                           8-63=TOD clock bits 0-55  */
        S64     todoffset;              /* Difference in microseconds
                                           between TOD and Unix time */
// #ifdef OPTION_TODCLOCK_DRAG_FACTOR
        U64     todclock_init;          /* TOD clock value at start  */
// #endif /* OPTION_TODCLOCK_DRAG_FACTOR */
        U64     todclock_prev;          /* TOD clock previous value  */
        U64     todclock_diff;          /* TOD clock difference      */
        LOCK    todlock;                /* TOD clock update lock     */
        TID     todtid;                 /* Thread-id for TOD update  */
#ifdef WIN32
        struct timeval   lasttod;       /* Last gettimeofday         */
#endif
        TID     wdtid;                  /* Thread-id for watchdog    */
        BYTE    loadparm[8];            /* IPL load parameter        */
        U16     numcpu;                 /* Number of CPUs installed  */
        REGS    regs[MAX_CPU_ENGINES];  /* Registers for each CPU    */
#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS  vf[MAX_CPU_ENGINES];    /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/
#if defined(_FEATURE_SIE)
        REGS    sie_regs[MAX_CPU_ENGINES];  /* SIE copy of regs      */
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
        REGS    footprregs[MAX_CPU_ENGINES][OPTION_FOOTPRINT_BUFFER];
        U32     footprptr[MAX_CPU_ENGINES];
#endif
        LOCK    mainlock;               /* Main storage lock         */
#if MAX_CPU_ENGINES == 1 || !defined(OPTION_FAST_INTCOND)
        COND    intcond;                /* Interrupt condition       */
#endif /* MAX_CPU_ENGINES == 1 || !defined(OPTION_FAST_INTCOND) */
        LOCK    intlock;                /* Interrupt lock            */
        LOCK    sigplock;               /* Signal processor lock     */
        ATTR    detattr;                /* Detached thread attribute */
        TID     cnsltid;                /* Thread-id for console     */
        U16     cnslport;               /* Port number for console   */
        int     cnslcnt;                /* Number of 3270 devices    */
        RADR    mbo;                    /* Measurement block origin  */
        BYTE    mbk;                    /* Measurement block key     */
        int     mbm;                    /* Measurement block mode    */
        int     mbd;                    /* Device connect time mode  */
        int     toddrag;                /* TOD clock drag factor     */
        int     panrate;                /* Panel refresh rate        */
        struct _DEVBLK *firstdev;       /* -> First device block     */
        U16     highsubchan;            /* Highest subchannel + 1    */
        U32     chp_reset[8];           /* Channel path reset masks  */
        struct _DEVBLK *iointq;         /* I/O interrupt queue       */
#if !defined(OPTION_FISHIO)
        struct _DEVBLK *ioq;            /* I/O queue                 */
        LOCK    ioqlock;                /* I/O queue lock            */
        COND    ioqcond;                /* I/O queue condition       */
        int     devtwait;               /* Device threads waiting    */
        int     devtnbr;                /* Number of device threads  */
        int     devtmax;                /* Max device threads        */
        int     devthwm;                /* High water mark           */
        int     devtunavail;            /* Count thread unavailable  */
#endif // !defined(OPTION_FISHIO)
        RADR    addrlimval;             /* Address limit value (SAL) */
        U32     servparm;               /* Service signal parameter  */
        U32     cp_recv_mask;           /* Syscons CP receive mask   */
        U32     cp_send_mask;           /* Syscons CP send mask      */
        U32     sclp_recv_mask;         /* Syscons SCLP receive mask */
        U32     sclp_send_mask;         /* Syscons SCLP send mask    */
        BYTE    scpcmdstr[123+1];       /* Operator command string   */
        int     scpcmdtype;             /* Operator command type     */
        unsigned int                    /* Flags                     */
#ifndef INTERRUPTS_FAST_CHECK
                iopending:1,            /* 1=I/O interrupt pending   */
                mckpending:1,           /* 1=MCK interrupt pending   */
                extpending:1,           /* 1=EXT interrupt pending   */
                intkey:1,               /* 1=Interrupt key pending   */
                servsig:1,              /* 1=Service signal pending  */
                crwpending:1,           /* 1=Channel report pending  */
#endif /*INTERRUPTS_FAST_CHECK*/
                sigintreq:1,            /* 1=SIGINT request pending  */
                insttrace:1,            /* 1=Instruction trace       */
                inststep:1,             /* 1=Instruction step        */
                instbreak:1,            /* 1=Have breakpoint         */
				inststop:1,        		/* 1 = stop on program check */ /*VMA*/
				vmactive:1,       		/* 1 = vma active            */ /*VMA*/
				mschdelay:1;			/* 1 = delay MSCH instruction*/ /*LNX*/
#ifdef INTERRUPTS_FAST_CHECK
        U32     ints_state;             /* Common Interrupts Status  */
#endif /*INTERRUPTS_FAST_CHECK*/
        U32     waitmask;               /* Mask for waiting CPUs     */
        U32     started_mask;           /* Mask for started CPUs     */
        int     broadcast_code;         /* Broadcast code            */
#define BROADCAST_PTLB 1                /* Broadcast purge tlb       */
#define BROADCAST_PALB 2                /* Broadcast purge alb       */
#define BROADCAST_ITLB 4                /* Broadcast invalidate tlb  */
        U64     broadcast_pfra;         /* Broadcast pfra            */
#if MAX_CPU_ENGINES > 1
        U32     broadcast_mask;         /* Broadcast CPU mask        */
        COND    broadcast_cond;         /* Broadcast condition       */
#endif /* MAX_CPU_ENGINES > 1 */
        U64     breakaddr;              /* Breakpoint address        */
        FILE   *msgpipew;               /* Message pipe write handle */
        int     msgpiper;               /* Message pipe read handle  */
        U64     pgminttr;               /* Program int trace mask    */
        int     pcpu;                   /* Tgt CPU panel cmd & displ */

        int     cpuprio;                /* CPU thread priority       */
        int     pgmprdos;               /* Program product OS flag   */
// #if defined(OPTION_HTTP_SERVER)
        TID     httptid;                /* HTTP listener thread id   */
        U16     httpport;               /* HTTP port number or zero  */
        int     httpauth;               /* HTTP auth required flag   */
        char   *httpuser;               /* HTTP userid               */
        char   *httppass;               /* HTTP password             */
// #endif /*defined(OPTION_HTTP_SERVER)*/
        CPCONV *codepage;
#ifdef OPTION_IODELAY_KLUDGE
        int     iodelay;                /* I/O delay kludge for linux*/
#endif /*OPTION_IODELAY_KLUDGE*/
#if !defined(NO_SETUID)
        uid_t   ruid, euid, suid;
        gid_t   rgid, egid, sgid;
#endif /*!defined(NO_SETUID)*/

#if defined(OPTION_INSTRUCTION_COUNTING)
#define IMAP_FIRST sysblk.imap01
        U64 imap01[256];
        U64 imapa4[256];
        U64 imapa5[16];
        U64 imapa6[256];
        U64 imapa7[16];
        U64 imapb2[256];
        U64 imapb3[256];
        U64 imapb9[256];
        U64 imapc0[16];
        U64 imape3[256];
        U64 imape4[256];
        U64 imape5[256];
        U64 imapeb[256];
        U64 imapec[256];
        U64 imaped[256];
        U64 imapxx[256];
#define IMAP_SIZE \
            ( sizeof(sysblk.imap01) \
            + sizeof(sysblk.imapa4) \
            + sizeof(sysblk.imapa5) \
            + sizeof(sysblk.imapa6) \
            + sizeof(sysblk.imapa7) \
            + sizeof(sysblk.imapb2) \
            + sizeof(sysblk.imapb3) \
            + sizeof(sysblk.imapb9) \
            + sizeof(sysblk.imapc0) \
            + sizeof(sysblk.imape3) \
            + sizeof(sysblk.imape4) \
            + sizeof(sysblk.imape5) \
            + sizeof(sysblk.imapeb) \
            + sizeof(sysblk.imapec) \
            + sizeof(sysblk.imaped) \
            + sizeof(sysblk.imapxx) )
#endif

    } SYSBLK;

/* Definitions for OS tailoring - msb eq mon event, lsb eq oper exc. */
#define OS_NONE         0x7FFFFFFFF7DE7FFFULL   /* No spec OS tail.  */
#define OS_OS390        0x7FF673FFF7DE7FFFULL   /* OS/390            */
#define OS_VSE          0x7FF673FFF7DE7FFFULL   /* VSE               */
#define OS_VM           0x7FFFFFFFF7DE7FFCULL   /* VM                */
#if !defined(NO_IEEE_SUPPORT)
#define OS_LINUX        0x78FFFFFFF7DE7FF7ULL   /* Linux             */
#else
#define OS_LINUX        0x78FFFFFFF7DE7FD6ULL   /* Linux             */
#endif

/* Definitions for program product OS restriction flag. This flag is ORed
   with the SCLP READ CPU INFO response code. A 4 here makes the CPU look
   like an IFL (Integrated Facility for Linux) engine, which cannot run
   licensed ESA/390 or z/Architecture OSes. */
#define PGM_PRD_OS_RESTRICTED 4                 /* Restricted        */
#define PGM_PRD_OS_LICENSED   0                 /* Licensed          */

#ifndef WIN32
#define MAX_DEVICE_THREADS 0
#else
#define MAX_DEVICE_THREADS 8
#endif

#ifdef WIN32
#define ADJUST_TOD(tod,lasttod) \
 { \
   if ((tod).tv_sec  == (lasttod).tv_sec \
    && (tod).tv_usec <= (lasttod).tv_usec) \
    { \
     (tod).tv_usec = (lasttod).tv_usec + 1; \
     if ((tod).tv_usec > 999999) \
     { \
      (tod).tv_sec++; \
      (tod).tv_usec = 0; \
     } \
   } \
   (lasttod).tv_sec  = (tod).tv_sec; \
   (lasttod).tv_usec = (tod).tv_usec; \
 }
#else
#define ADJUST_TOD(tod,lasttod)
#endif

/*-------------------------------------------------------------------*/
/* LIST_ENTRY structure     (must manually define here since         */
/*                           <windows.h> hasn't been #included)      */
/*-------------------------------------------------------------------*/

#include "linklist.h"       // (linked list structure and macros)

typedef struct _LIST_ENTRY
{
    struct  _LIST_ENTRY*  Flink;    // ptr to next link in chain
    struct  _LIST_ENTRY*  Blink;    // ptr to previous link in chain
}
LIST_ENTRY, *PLIST_ENTRY;

/*-------------------------------------------------------------------*/
/* Bind structure for "Socket Devices"                               */
/*-------------------------------------------------------------------*/

struct _DEVBLK;             // (forward reference)

typedef struct _bind_struct
{
    LIST_ENTRY bind_link;   // (just a link in the chain)

    struct _DEVBLK* dev;    // ptr to corresponding device block
    char* spec;             // socket_spec for listening socket
    int sd;                 // listening socket to use in select

    // Note: following two fields malloc'ed! remember to free!

    char* clientname;       // connected client's hostname
                            // (Note: could be NULL)
    char* clientip;         // connected client's IP address
                            // (Note: could be NULL)
}
bind_struct;

struct _DEVDATA;                                /* Forward reference */
/*-------------------------------------------------------------------*/
/* Device configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _DEVBLK {
        struct _DEVBLK *nextdev;        /* -> next device block      */
        LOCK    lock;                   /* Device block lock         */
        FILE   *msgpipew;               /* Message pipe write handle */

        /*  device identification                                    */

        U16     subchan;                /* Subchannel number         */
        U16     devnum;                 /* Device number             */
        U16     devtype;                /* Device type               */
        U16     chanset;                /* Channel Set to which this   
                                           device is connected S/370 */
        char    *typname;               /* Device type name          */

#if defined(WIN32)
        BYTE    filename[1024];         /* Windows pathname          */
#else
        BYTE    filename[256];          /* Unix file name            */
#endif

        /*  device i/o fields...                                     */

        int     fd;                     /* File desc / socket number */
        FILE   *fh;                     /* associated File handle    */
        bind_struct* bs;                /* -> bind_struct if socket-
                                           device, NULL otherwise    */

        /*  device buffer management fields                          */
        BYTE   *buf;                    /* -> Device data buffer     */
        int     bufsize;                /* Device data buffer size   */
        int     bufoff;                 /* Offset into data buffer   */
        int     bufoffhi;               /* Highest offset allowed    */
        int     bufupdlo;               /* Lowest offset updated     */
        int     bufupdhi;               /* Highest offset updated    */
        int     bufupd;                 /* 1=Buffer updated          */

        /*  device i/o scheduling fields...                          */

        TID     tid;                    /* Thread-id executing CCW   */
        int     priority;               /* I/O q scehduling priority */
        struct _DEVBLK *nextioq;        /* -> next device in I/O q   */
        struct _DEVBLK *iointq;         /* -> next device in I/O
                                           interrupt queue           */
        int     cpuprio;                /* CPU thread priority       */

        /*  fields used during ccw execution...                      */
        BYTE    chained;                /* Command chain and data chain
                                           bits from previous CCW    */
        BYTE    prev_chained;           /* Chaining flags from CCW
                                           preceding the data chain  */
        BYTE    code;                   /* Current CCW opcode        */
        BYTE    prevcode;               /* Previous CCW opcode       */
        int     ccwseq;                 /* CCW sequence number       */

        /*  device handler function pointers...                      */

        struct _DEVHND *hnd;            /* -> Device handlers        */

        /*  emulated architecture fields...   (MUST be aligned!)     */

        int     reserved1;              /* ---(ensure alignment)---- */
        ORB     orb;                    /* Operation request blk @IWZ*/
        PMCW    pmcw;                   /* Path management ctl word  */
        SCSW    scsw;                   /* Subchannel status word(XA)*/
        SCSW    pciscsw;                /* PCI subchannel status word*/
        BYTE    csw[8];                 /* Channel status word(S/370)*/
        BYTE    pcicsw[8];              /* PCI channel status word   */
        ESW     esw;                    /* Extended status word      */
        BYTE    ecw[32];                /* Extended control word     */
        int     numsense;               /* Number of sense bytes     */
        BYTE    sense[32];              /* Sense bytes               */
        int     numdevid;               /* Number of device id bytes */
        BYTE    devid[256];             /* Device identifier bytes   */
        int     numdevchar;             /* Number of devchar bytes   */
        BYTE    devchar[64];            /* Device characteristics    */
        BYTE    pgid[11];               /* Path Group ID             */
        BYTE    reserved2[5];           /* (pad/align/unused/avail)  */
        COND    resumecond;             /* Resume condition          */
#ifdef WIN32
        struct timeval   lasttod;       /* Last gettimeofday         */
#endif

        /*  control flags...                                         */

        unsigned int                    /* Flags                     */
#ifdef OPTION_CKD_KEY_TRACING
                ckdkeytrace:1,          /* 1=Log CKD_KEY_TRACE       */
#endif /*OPTION_CKD_KEY_TRACING*/
                syncio:1,               /* 1=Synchronous I/Os allowed*/
                console:1,              /* 1=Console device          */
                connected:1,            /* 1=Console client connected*/
                readpending:2,          /* 1=Console read pending    */
                batch:1,                /* 1=Called by dasdutil      */
                ccwtrace:1,             /* 1=CCW trace               */
                ccwstep:1,              /* 1=CCW single step         */
                cdwmerge:1;             /* 1=Channel will merge data
                                             chained write CCWs      */
        int     pending;                /* 1=Interrupt pending       */
        int     busy;                   /* 1=Device busy             */
        int     pcipending;             /* 1=PCI interrupt pending   */
        int     crwpending;             /* 1=CRW pending             */
        int     syncio_active;          /* 1=Synchronous I/O active  */
        int     syncio_retry;           /* 1=Retry I/O asynchronously*/

        /*  Synchronous I/O                                          */

        U32     syncio_addr;            /* Synchronous i/o ccw addr  */
        U64     syncios;                /* Number synchronous I/Os   */
        U64     asyncios;               /* Number asynchronous I/Os  */

        /*  Device dependent fields for console                      */

        struct  in_addr ipaddr;         /* Client IP address         */
        int     rlen3270;               /* Length of data in buffer  */
        int     pos3270;                /* Current screen position   */
        int     keybdrem;               /* Number of bytes remaining
                                           in keyboard read buffer   */
        unsigned int                    /* Flags                     */
                eab3270:1;              /* 1=Extended attributes     */
        BYTE    aid3270;                /* Current input AID value   */
        BYTE    mod3270;                /* 3270 model number         */

        /*  Device dependent fields for cardrdr                      */

        char    **more_files;           /* for more that one file in
                                           reader */
        char    **current_file;         /* counts how many additional
                                           reader files are avail    */
        int     cardpos;                /* Offset of next byte to be
                                           read from data buffer     */
        int     cardrem;                /* Number of bytes remaining
                                           in data buffer            */
        unsigned int                    /* Flags                     */
                multifile:1,            /* 1=auto-open next i/p file */
                rdreof:1,               /* 1=Unit exception at EOF   */
                ebcdic:1,               /* 1=Card deck is EBCDIC     */
                ascii:1,                /* 1=Convert ASCII to EBCDIC */
                trunc:1,                /* Truncate overlength record*/
                autopad:1;              /* 1=Pad incomplete last rec
                                           to 80 bytes if EBCDIC     */

        /*  Device dependent fields for ctcadpt                      */

        struct _DEVBLK *ctcpair;        /* -> Paired device block    */
        int     ctcpos;                 /* next byte offset          */
        int     ctcrem;                 /* bytes remaining in buffer */
        int     ctclastpos;             /* last packet read          */
        int     ctclastrem;             /* last packet read          */
        unsigned int                    /* Flags                     */
                ctcxmode:1,             /* 0=Basic mode, 1=Extended  */
				lcscmd:1, 		        /* lcs cmd response pending  */ /*LCS*/
				readpnd:1,         		/* read pending on thread    */ /*LCS*/
				readstrt:1,     	    /* read thread started       */ /*LCS*/
				readerr:1;        		/* read error                */ /*LCS*/
        BYTE    ctctype;                /* CTC_xxx device type       */
        BYTE    netdevname[IFNAMSIZ];   /* network device name       */
		FWORD	ctcipsrc;    			/* LCS source IP address     */ /*LCS*/
		int lcslen;           			/* LCS read length           */ /*LCS*/
		int lcscmdlen;        			/* LCS command length        */ /*LCS*/
		BYTE   *lcsbuf;   				/* -> to lcs command buffer  */ /*LCS*/

        /*  Device dependent fields for printer                      */

        int     printpos;               /* Number of bytes already
                                           placed in print buffer    */
        int     printrem;               /* Number of bytes remaining
                                           in print buffer           */
        int     stopprt;                /* 1=stopped; 0=started      */
        unsigned int                    /* Flags                     */
                crlf:1,                 /* 1=CRLF delimiters, 0=LF   */
                diaggate:1,             /* 1=Diagnostic gate command */
                fold:1;                 /* 1=Fold to upper case      */

        /*  Device dependent fields for tapedev                      */

        void   *omadesc;                /* -> OMA descriptor array   */
        U16     omafiles;               /* Number of OMA tape files  */
        U16     curfilen;               /* Current file number       */
        long    blockid;                /* Current device block ID   */
        long    nxtblkpos;              /* Offset from start of file
                                           to next block             */
        long    prvblkpos;              /* Offset from start of file
                                           to previous block         */
        U16     curblkrem;              /* Number of bytes unread
                                           from current block        */
        U16     curbufoff;              /* Offset into buffer of data
                                           for next data chained CCW */
        HETB   *hetb;                   /* HET control block         */
        struct                          /* HET device parms          */
        {
            U16 compress:1;             /* 1=Compression enabled     */
            U16 method:3;               /* Compression method        */
            U16 level:4;                /* Compression level         */
            U16 chksize;                /* Chunk size                */
        }       tdparms;                /* HET device parms          */
        unsigned int                    /* Flags                     */
                readonly:1,             /* 1=Tape is write-protected */
                longfmt:1;              /* 1=Long record format (DDR)*/ /*DDR*/
        BYTE    tapedevt;               /* Tape device type          */

        /*  Device dependent fields for dasd (fba and ckd)           */

        int     dasdcur;                /* Current ckd trk or fba blk*/
        BYTE    dasdsfn[256];           /* Shadow file name          */

        /*  Device dependent fields for fbadasd                      */

        FBARB  *fbardblk;               /* -> Read block routine     */
        FBAWB  *fbawrblk;               /* -> Write block  routine   */
        FBADEV *fbatab;                 /* Device table entry        */
        U32     fbaorigin;              /* Device origin block number*/
        U32     fbanumblk;              /* Number of blocks in device*/
        off_t   fbarba;                 /* Relative byte offset      */
        U32     fbaxblkn;               /* Offset from start of device
                                           to first block of extent  */
        U32     fbaxfirst;              /* Block number within dataset
                                           of first block of extent  */
        U32     fbaxlast;               /* Block number within dataset
                                           of last block of extent   */
        U32     fbalcblk;               /* Block number within dataset
                                           of first block for locate */
        U16     fbalcnum;               /* Block count for locate    */
        U16     fbablksiz;              /* Physical block size       */
        unsigned int                    /* Flags                     */
                fbaxtdef:1;             /* 1=Extent defined          */
        BYTE    fbaoper;                /* Locate operation byte     */
        BYTE    fbamask;                /* Define extent file mask   */

        /*  Device dependent fields for ckddasd                      */

        CKDRT  *ckdrdtrk;               /* -> Read track routine     */
        CKDUT  *ckdupdtrk;              /* -> Update track routine   */
        int     ckdnumfd;               /* Number of CKD image files */
        int     ckdfd[CKD_MAXFILES];    /* CKD image file descriptors*/
        int     ckdhitrk[CKD_MAXFILES]; /* Highest track number
                                           in each CKD image file    */
        CKDDEV *ckdtab;                 /* Device table entry        */
        CKDCU  *ckdcu;                  /* Control unit entry        */
        U64     ckdtrkoff;              /* Track image file offset   */
        int     ckdcyls;                /* Number of cylinders       */
        int     ckdtrks;                /* Number of tracks          */
        int     ckdheads;               /* #of heads per cylinder    */
        int     ckdtrksz;               /* Track size                */
        int     ckdcurcyl;              /* Current cylinder          */
        int     ckdcurhead;             /* Current head              */
        int     ckdcurrec;              /* Current record id         */
        int     ckdcurkl;               /* Current record key length */
        int     ckdorient;              /* Current orientation       */
        int     ckdcuroper;             /* Curr op: read=6, write=5  */
        U16     ckdcurdl;               /* Current record data length*/
        U16     ckdrem;                 /* #of bytes from current
                                           position to end of field  */
        U16     ckdpos;                 /* Offset into buffer of data
                                           for next data chained CCW */
        U16     ckdxblksz;              /* Define extent block size  */
        U16     ckdxbcyl;               /* Define extent begin cyl   */
        U16     ckdxbhead;              /* Define extent begin head  */
        U16     ckdxecyl;               /* Define extent end cyl     */
        U16     ckdxehead;              /* Define extent end head    */
        BYTE    ckdfmask;               /* Define extent file mask   */
        BYTE    ckdxgattr;              /* Define extent global attr */
        U16     ckdltranlf;             /* Locate record transfer
                                           length factor             */
        BYTE    ckdloper;               /* Locate record operation   */
        BYTE    ckdlaux;                /* Locate record aux byte    */
        BYTE    ckdlcount;              /* Locate record count       */
        struct _CKDDASD_CACHE *ckdcache;/* Cache table               */
        int     ckdcachenbr;            /* Cache table size          */
        int     ckdcachehits;           /* Cache hits                */
        int     ckdcachemisses;         /* Cache misses              */
        U64     ckdcacheage;            /* Cache aging counter       */
        void   *cckd_ext;               /* -> Compressed ckddasd
                                           extension otherwise NULL  */
        unsigned int                    /* Flags                     */
                ckd3990:1,              /* 1=Control unit is 3990    */
                ckdxtdef:1,             /* 1=Define Extent processed */
                ckdsetfm:1,             /* 1=Set File Mask processed */
                ckdlocat:1,             /* 1=Locate Record processed */
                ckdspcnt:1,             /* 1=Space Count processed   */
                ckdseek:1,              /* 1=Seek command processed  */
                ckdskcyl:1,             /* 1=Seek cylinder processed */
                ckdrecal:1,             /* 1=Recalibrate processed   */
                ckdrdipl:1,             /* 1=Read IPL processed      */
                ckdxmark:1,             /* 1=End of track mark found */
                ckdhaeq:1,              /* 1=Search Home Addr Equal  */
                ckdideq:1,              /* 1=Search ID Equal         */
                ckdkyeq:1,              /* 1=Search Key Equal        */
                ckdwckd:1,              /* 1=Write R0 or Write CKD   */
                ckdtrkof:1,             /* 1=Track ovfl on this blk  */
                ckdnolazywr:1,          /* 1=Perform updates now     */
                ckdrdonly:1,            /* 1=Open read only          */
                ckdfakewr:1;            /* 1=Fake successful write
                                             for read only file      */
    } DEVBLK;

/*-------------------------------------------------------------------*/
/* Definitions for CTC protocol types                                */
/*-------------------------------------------------------------------*/
#define CTC_XCA         1               /* XCA device                */
#define CTC_LCS         2               /* LCS device                */
#define CTC_CETI        3               /* CETI device               */
#define CTC_CLAW        4               /* CLAW device               */
#define CTC_CTCN        5               /* CTC link via NETBIOS      */
#define CTC_CTCT        6               /* CTC link via TCP          */
#define CTC_CTCI        7               /* CTC link to TCP/IP stack  */
#define CTC_CTCI_W32    8               /* (Win32 CTCI)              */
#define CTC_VMNET       9               /* CTC link via wfk's vmnet  */
#define CTC_CFC        10               /* Coupling facility channel */

/*-------------------------------------------------------------------*/
/* Structure definitions for CKD headers                             */
/*-------------------------------------------------------------------*/
typedef struct _CKDDASD_DEVHDR {        /* Device header             */
        BYTE    devid[8];               /* Device identifier         */
        FWORD   heads;                  /* #of heads per cylinder
                                           (bytes in reverse order)  */
        FWORD   trksize;                /* Track size (reverse order)*/
        BYTE    devtype;                /* Last 2 digits of device type
                                           (0x80=3380, 0x90=3390)    */
        BYTE    fileseq;                /* CKD image file sequence no.
                                           (0x00=only file, 0x01=first
                                           file of multiple files)   */
        HWORD   highcyl;                /* Highest cylinder number on
                                           this file, or zero if this
                                           is the last or only file
                                           (bytes in reverse order)  */
        BYTE    resv[492];              /* Reserved                  */
    } CKDDASD_DEVHDR;

typedef struct _CKDDASD_TRKHDR {        /* Track header              */
        BYTE    bin;                    /* Bin number                */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
    } CKDDASD_TRKHDR;

typedef struct _CKDDASD_RECHDR {        /* Record header             */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
        BYTE    rec;                    /* Record number             */
        BYTE    klen;                   /* Key length                */
        HWORD   dlen;                   /* Data length               */
    } CKDDASD_RECHDR;

typedef struct _CKDDASD_CACHE {         /* Cache entry               */
        int     trk;                    /* Track number              */
        BYTE   *buf;                    /* Buffer address            */
        int     fd;                     /* Track fd                  */
        U64     age;                    /* Entry age                 */
        U64     off;                    /* Entry offset              */
    } CKDDASD_CACHE;

#define CKDDASD_DEVHDR_SIZE     sizeof(CKDDASD_DEVHDR)
#define CKDDASD_TRKHDR_SIZE     sizeof(CKDDASD_TRKHDR)
#define CKDDASD_RECHDR_SIZE     sizeof(CKDDASD_RECHDR)
#define CKDDASD_CACHE_SIZE      sizeof(CKDDASD_CACHE)

/*-------------------------------------------------------------------*/
/* Structure definitions for Compressed CKD devices                  */
/*-------------------------------------------------------------------*/
typedef struct _CCKDDASD_DEVHDR {       /* Compress device header    */
/*  0 */BYTE             vrm[3];        /* Version Release Modifier  */
/*  3 */BYTE             options;       /* Options byte              */
/*  4 */U32              numl1tab;      /* Size of lvl 1 table       */
/*  8 */U32              numl2tab;      /* Size of lvl 2 tables      */
/* 12 */U32              size;          /* File size                 */
/* 16 */U32              used;          /* File used                 */
/* 20 */U32              free;          /* Position to free space    */
/* 24 */U32              free_total;    /* Total free space          */
/* 28 */U32              free_largest;  /* Largest free space        */
/* 32 */U32              free_number;   /* Number free spaces        */
/* 36 */U32              free_imbed;    /* [deprecated]              */
/* 40 */FWORD            cyls;          /* Cylinders on device       */
/* 44 */BYTE             resv1;         /* Reserved                  */
/* 45 */BYTE             compress;      /* Compression algorithm     */
/* 46 */S16              compress_parm; /* Compression parameter     */
/* 48 */BYTE             resv2[464];    /* Reserved                  */
    } CCKDDASD_DEVHDR;

#define CCKD_VERSION           0
#define CCKD_RELEASE           2
#define CCKD_MODLVL            1

#define CCKD_NOFUDGE           1         /* [deprecated]             */
#define CCKD_BIGENDIAN         2
#define CCKD_ORDWR             64        /* Opened read/write since
                                            last chkdsk              */
#define CCKD_OPENED            128


#define CCKD_FREEHDR           size
#define CCKD_FREEHDR_SIZE      28
#define CCKD_FREEHDR_POS       CKDDASD_DEVHDR_SIZE+12

#define CCKD_COMPRESS_NONE     0
#define CCKD_COMPRESS_ZLIB     1
#ifndef CCKD_BZIP2
#define CCKD_COMPRESS_MAX      CCKD_COMPRESS_ZLIB
#else
#define CCKD_COMPRESS_BZIP2    2
#define CCKD_COMPRESS_MAX      CCKD_COMPRESS_BZIP2
#endif
#define CCKD_COMPRESS_STRESSED 0x80

#define CCKD_STRESS_MINLEN     4096
#define CCKD_STRESS_COMP       CCKD_COMPRESS_ZLIB
#define CCKD_STRESS_PARM1      4
#define CCKD_STRESS_PARM2      2

typedef struct _CCKD_L2ENT {            /* Level 2 table entry       */
        U32              pos;           /* Track offset              */
        U16              len;           /* Track length              */
        U16              size;          /* Track size [deprecated]   */
    } CCKD_L2ENT;
typedef CCKD_L2ENT     CCKD_L2TAB[256]; /* Level 2 table             */

typedef U32            CCKD_L1ENT;      /* Level 1 table entry       */
typedef CCKD_L1ENT     CCKD_L1TAB[];    /* Level 1 table             */

typedef struct _CCKD_FREEBLK {          /* Free block                */
        U32              pos;           /* Position next free blk    */
        U32              len;           /* Length this free blk      */
        int              prev;          /* Index to prev free blk    */
        int              next;          /* Index to next free blk    */
    } CCKD_FREEBLK;

typedef struct _CCKD_RA {               /* Readahead queue entry     */
        DEVBLK          *dev;           /* Readahead device          */
        int              trk;           /* Readahead track           */
        int              prev;          /* Index to prev entry       */
        int              next;          /* Index to next entry       */
    } CCKD_RA;

typedef struct _CCKD_CACHE {            /* Cache structure           */
        DEVBLK          *dev;           /* Cached device block ptr   */
        int              trk;           /* Cached track number       */
        int              sfx;           /* Cached l2tab file index   */
        int              l1x;           /* Cached l2tab index        */
        BYTE            *buf;           /* Cached buffer address     */
        U64              age;           /* Cache entry age           */
        U32              flags;         /* Cache flags               */
    } CCKD_CACHE;
#define CCKD_CACHE_ACTIVE    0x80000000 /* Cache entry is active     */
#define CCKD_CACHE_UPDATED   0x40000000 /* Cache buf has been updated*/
#define CCKD_CACHE_READING   0x20000000 /* Cache buf being read      */
#define CCKD_CACHE_WRITING   0x10000000 /* Cache buf being written   */
#define CCKD_CACHE_WRITE     0x08000000 /* Cache buf pending write   */
#define CCKD_CACHE_READWAIT  0x04000000 /* Thread waiting for read   */
#define CCKD_CACHE_WRITEWAIT 0x02000000 /* Thread waiting for write  */
#define CCKD_CACHE_INVALID   0x01000000 /* Cache entry invalid       */
#define CCKD_CACHE_USED      0x00800000 /* Cache entry was used      */
#define CCKD_CACHE_BUSY      0xff000000 /* Cache entry is busy       */

#define CCKDDASD_DEVHDR_SIZE   sizeof(CCKDDASD_DEVHDR)
#define CCKD_L1ENT_SIZE        sizeof(CCKD_L1ENT)
#define CCKD_L1TAB_POS         CKDDASD_DEVHDR_SIZE+CCKDDASD_DEVHDR_SIZE
#define CCKD_L2ENT_SIZE        sizeof(CCKD_L2ENT)
#define CCKD_L2TAB_SIZE        sizeof(CCKD_L2TAB)
#define CCKD_DFWQE_SIZE        sizeof(CCKD_DFWQE)
#define CCKD_FREEBLK_SIZE      8
#define CCKD_FREEBLK_ISIZE     sizeof(CCKD_FREEBLK)
#define CCKD_CACHE_SIZE        sizeof(CCKD_CACHE)
#define CCKD_NULLTRK_SIZE1     37       /* ha r0 r1 ffff */
#define CCKD_NULLTRK_SIZE0     29       /* ha r0 ffff */

/* adjustable values */

#define CCKD_COMPRESS_MIN      512      /* Track images smaller than
                                           this won't be compressed  */
#define CCKD_MAX_SF            8        /* Maximum number of shadow
                                           files: 0 to 9 [0 disables
                                           shadow file support]      */
#define CCKD_MAX_READAHEADS    16       /* Max readahead trks        */
#define CCKD_MAX_RA_SIZE       16       /* Readahead queue size      */
#define CCKD_MAX_RA            9        /* Max readahead threads     */
#define CCKD_MAX_WRITER        9        /* Max writer threads        */
#define CCKD_MAX_GCOL          1        /* Max garbage collectors    */
#define CCKD_MAX_CACHE         1024     /* Max number cache entries  */
#define CCKD_MAX_L2CACHE       1024     /* Max nbr l2 cache entries  */
#define CCKD_MAX_TRACE         200000   /* Max nbr trace entries     */

#define CCKD_MIN_READAHEADS    0        /* Min readahead trks        */
#define CCKD_MIN_RA            0        /* Min readahead threads     */
#define CCKD_MIN_WRITER        1        /* Min writer threads        */
#define CCKD_MIN_GCOL          0        /* Min garbage collectors    */
#define CCKD_MIN_CACHE         16       /* Min number cache entries  */
#define CCKD_MIN_L2CACHE       128      /* Min nbr l2 cache entries  */

#define CCKD_DEFAULT_RA_SIZE   4        /* Readahead queue size      */
#define CCKD_DEFAULT_RA        2        /* Default number readaheads */
#define CCKD_DEFAULT_WRITER    2        /* Default number writers    */
#define CCKD_DEFAULT_GCOL      1        /* Default number garbage
                                              collectors             */
#define CCKD_DEFAULT_GCOLWAIT  5        /* Default wait (seconds)    */
#define CCKD_DEFAULT_GCOLPARM  0        /* Default adjustment parm   */
#define CCKD_DEFAULT_CACHE     128      /* Default cache size        */
#define CCKD_DEFAULT_L2CACHE   256      /* Default level 2 cache size*/
#define CCKD_DEFAULT_READAHEADS 2       /* Default nbr to read ahead */

#define CFBA_BLOCK_NUM         120      /* Number fba blocks / group */
#define CFBA_BLOCK_SIZE        61440    /* Size of a block group 60k */
                                        /* Number of bytes in an fba
                                           block group.  Probably
                                           should be a multiple of 512
                                           but has to be < 64K       */

typedef struct _CCKDBLK {               /* Global cckd dasd block    */
        BYTE             id[8];         /* "CCKDBLK "                */
        FILE            *msgpipew;      /* Message pipe write handle */
        DEVBLK          *dev1st;        /* 1st device in cckd queue  */
        LOCK             l2cachelock;   /* L2 cache lock             */
        LOCK             cachelock;     /* Cache lock                */
        COND             cachecond;     /* Wait for cache condition  */
        U64              cacheage;      /* Cache aging value         */
        int              cachewaiting;  /* Threads waiting for cache */
        LOCK             gclock;        /* Garbage collector lock    */
        COND             gccond;        /* Garbage collector cond    */
        int              gcols;         /* Number garbage collectors */
        int              gcolmax;       /* Max garbage collectors    */
        int              gcolwait;      /* Wait time in seconds      */
        int              gcolparm;      /* Adjustment parm           */
        LOCK             ralock;        /* Readahead lock            */
        COND             racond;        /* Readahead condition       */
        int              ras;           /* Number readahead threads  */
        int              ramax;         /* Max readahead threads     */
        int              rawaiting;     /* Number threads waiting    */
        int              ranbr;         /* Readahead queue size      */
        int              readaheads;    /* Nbr tracks to read ahead  */
        CCKD_RA          ra[CCKD_MAX_RA_SIZE]; /* Readahead queue    */
        int              ra1st;         /* First readahead entry     */
        int              ralast;        /* Last readahead entry      */
        int              rafree;        /* Free readahead entry      */
        COND             writercond;    /* Writer condition          */
        int              writepending;  /* Number writes pending     */
        int              writerswaiting;/* Number writers waiting    */
        int              writers;       /* Number writer threads     */
        int              writermax;     /* Max writer threads        */
        int              writerprio;    /* Writer thread priority    */
        int              writewaiting;  /* Threads waiting for writes*/
        COND             writecond;     /* Write wait condition      */
        COND             termcond;      /* Termination condition     */
        int              l2cachenbr;    /* Size of level 2 cache     */
        int              cachenbr;      /* Size of cache             */
        U64              stats_switches;       /* Switches           */
        U64              stats_cachehits;      /* Cache hits         */
        U64              stats_cachemisses;    /* Cache misses       */
        U64              stats_readaheads;     /* Readaheads         */
        U64              stats_readaheadmisses;/* Readahead misses   */
        U64              stats_syncios;        /* Synchronous i/os   */
        U64              stats_synciomisses;   /* Missed syncios     */
        U64              stats_readwaits;      /* Waits for read     */
        U64              stats_writewaits;     /* Waits for write    */
        U64              stats_cachewaits;     /* Waits for cache    */
        U64              stats_stresswrites;   /* Writes under stress*/
        U64              stats_l2cachehits;    /* L2 cache hits      */
        U64              stats_l2cachemisses;  /* L2 cache misses    */
        U64              stats_l2reads;        /* L2 reads           */
        U64              stats_reads;          /* Number reads       */
        U64              stats_readbytes;      /* Bytes read         */
        U64              stats_writes;         /* Number writes      */
        U64              stats_writebytes;     /* Bytes written      */
        U64              stats_gcolmoves;      /* Spaces moved       */
        U64              stats_gcolbytes;      /* Bytes moved        */
        char            *itrace;        /* Internal trace table      */
        int              itracex;       /* Internal trace index      */
        int              itracen;       /* Internal trace size       */
        CCKD_CACHE       cache[CCKD_MAX_CACHE];     /* CCKD cache    */
        CCKD_CACHE       l2cache[CCKD_MAX_L2CACHE]; /* CCKD l2 cache */
      } CCKDBLK;

typedef struct _CCKDDASD_EXT {          /* Ext for compressed ckd    */
        DEVBLK          *devnext;       /* cckd device queue         */
        unsigned int     ckddasd:1,     /* 1=CKD dasd                */
                         fbadasd:1,     /* 1=FBA dasd                */
                         stopping:1;    /* 1=Device is closing       */
        LOCK             filelock;      /* File lock                 */
        CCKDDASD_DEVHDR  cdevhdr[CCKD_MAX_SF+1];/* cckd device hdr   */
        CCKD_L1ENT      *l1[CCKD_MAX_SF+1]; /* Level 1 tables        */
        int              fd[CCKD_MAX_SF+1]; /* File descriptors      */
        BYTE             swapend[CCKD_MAX_SF+1]; /* Swap endian flag */
        BYTE             open[CCKD_MAX_SF+1];    /* Open flag        */
        int              sfn;           /* Number active shadow files*/
        int              sfx;           /* Active level 2 file index */
        int              l1x;           /* Active level 2 table index*/
        CCKD_L2ENT      *l2;            /* Active level 2 table      */
        CCKD_CACHE      *l2active;      /* Active level 2 cache entry*/
        CCKD_CACHE      *active;        /* Active cache entry        */
        COND             readcond;      /* Wait for read condition   */
        COND             writecond;     /* Wait for write condition  */
        CCKD_FREEBLK    *free;          /* Internal free space chain */
        int              freenbr;       /* Number free space entries */
        int              free1st;       /* Index of 1st entry        */
        int              freeavail;     /* Index of available entry  */
        int              reads[CCKD_MAX_SF+1];   /* Nbr track reads  */
        int              l2reads[CCKD_MAX_SF+1]; /* Nbr l2 reads     */
        int              writes[CCKD_MAX_SF+1];  /* Nbr track writes */
        int              totreads;      /* Total nbr trk reads       */
        int              totwrites;     /* Total nbr trk writes      */
        int              totl2reads;    /* Total nbr l2 reads        */
        int              cachehits;     /* Cache hits                */
        int              readaheads;    /* Number trks read ahead    */
        int              switches;      /* Number trk switches       */
        int              misses;        /* Number readahead misses   */
    } CCKDDASD_EXT;

#define CCKD_OPEN_NONE         0
#define CCKD_OPEN_RO           1
#define CCKD_OPEN_RD           2
#define CCKD_OPEN_RW           3

/*-------------------------------------------------------------------*/
/* Global data areas in module config.c                              */
/*-------------------------------------------------------------------*/
extern SYSBLK   sysblk;                 /* System control block      */
extern CCKDBLK  cckdblk;                /* CCKD global block         */
// ZZ extern BYTE     ascii_to_ebcdic[];      /* Translate table           */
// ZZ extern BYTE     ebcdic_to_ascii[];      /* Translate table           */
#ifdef EXTERNALGUI
extern int extgui;              /* external gui present */
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Global data areas and functions in module cpu.c                   */
/*-------------------------------------------------------------------*/
extern const char* arch_name[];
extern const char* get_arch_mode_string(REGS* regs);

/*-------------------------------------------------------------------*/
/* Function prototypes                                               */
/*-------------------------------------------------------------------*/

/* Functions in module config.c */
void build_config (BYTE *fname);
void release_config ();
DEVBLK *find_device_by_devnum (U16 devnum);
DEVBLK *find_device_by_subchan (U16 subchan);
DEVBLK *find_unused_device ();
int  attach_device (U16 devnum, char *devtype, int addargc,
        BYTE *addargv[]);
int  detach_device (U16 devnum);
int  define_device (U16 olddev, U16 newdev);
int  configure_cpu (REGS *regs);
int  deconfigure_cpu (REGS *regs);
//#ifdef EXTERNALGUI
int parse_args (BYTE* p, int maxargc, BYTE** pargv, int* pargc);
//#endif /*EXTERNALGUI*/

/* Global data areas and functions in module panel.c */
extern int volatile initdone;    /* Initialization complete flag */
extern void panel_display (void);
extern LIST_ENTRY  bind_head;
extern LOCK        bind_lock;
extern int bind_device   (DEVBLK* dev, char* spec);
extern int unbind_device (DEVBLK* dev);

/* Access type parameter passed to translate functions in dat.c */
#define ACCTYPE_READ            1       /* Read operand data         */
#define ACCTYPE_WRITE_SKP       2       /* Write but skip change bit */
#define ACCTYPE_WRITE           3       /* Write operand data        */
#define ACCTYPE_INSTFETCH       4       /* Instruction fetch         */
#define ACCTYPE_TAR             5       /* Test Access               */
#define ACCTYPE_LRA             6       /* Load Real Address         */
#define ACCTYPE_TPROT           7       /* Test Protection           */
#define ACCTYPE_IVSK            8       /* Insert Virtual Storage Key*/
#define ACCTYPE_STACK           9       /* Linkage stack operations  */
#define ACCTYPE_BSG             10      /* Branch in Subspace Group  */
#define ACCTYPE_PTE             11      /* Return PTE raddr          */
#define ACCTYPE_SIE             12      /* SIE host translation      */
#define ACCTYPE_STRAG           13      /* Store real address        */

/* Special value for arn parameter for translate functions in dat.c */
#define USE_REAL_ADDR           (-1)    /* Real address              */
#define USE_PRIMARY_SPACE       (-2)    /* Primary space virtual     */
#define USE_SECONDARY_SPACE     (-3)    /* Secondary space virtual   */

/* Interception codes used by longjmp/SIE */
#define SIE_NO_INTERCEPT        (-1)    /* Continue (after pgmint)   */
#define SIE_HOST_INTERRUPT      (-2)    /* Host interrupt pending    */
#define SIE_HOST_PGMINT         (-3)    /* Host program interrupt    */
#define SIE_INTERCEPT_INST      (-4)    /* Instruction interception  */
#define SIE_INTERCEPT_INSTCOMP  (-5)    /* Instr. int TS/CS/CDS      */
#define SIE_INTERCEPT_EXTREQ    (-6)    /* External interrupt        */
#define SIE_INTERCEPT_IOREQ     (-7)    /* I/O interrupt             */
#define SIE_INTERCEPT_WAIT      (-8)    /* Wait state loaded         */
#define SIE_INTERCEPT_STOPREQ   (-9)    /* STOP reqeust              */
#define SIE_INTERCEPT_RESTART  (-10)    /* Restart interrupt         */
#define SIE_INTERCEPT_MCK      (-11)    /* Machine Check interrupt   */
#define SIE_INTERCEPT_EXT      (-12)    /* External interrupt pending*/
#define SIE_INTERCEPT_VALIDITY (-13)    /* SIE validity check        */
#define SIE_INTERCEPT_PER      (-14)    /* SIE guest per event       */


/* Functions in module panel.c */
void *panel_command (void *cmdline);

/* Functions in module timer.c */
void update_TOD_clock (void);
void *timer_update_thread (void *argp);

/* Functions in module service.c */
void scp_command (BYTE *command, int priomsg);

/* Functions in module ckddasd.c */
void ckd_build_sense ( DEVBLK *, BYTE, BYTE, BYTE, BYTE, BYTE);
int ckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[]);
void ckddasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int ckddasd_close_device ( DEVBLK *dev );
void ckddasd_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer);

/* Functions in module fbadasd.c */
void fbadasd_syncblk_io (DEVBLK *dev, BYTE type, U32 blknum,
        U32 blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual);
int fbadasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[]);
void fbadasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int fbadasd_close_device ( DEVBLK *dev );
void fbadasd_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer);


/* Functions in module cckddasd.c */
DEVIF   cckddasd_init_handler;
int     cckddasd_close_device (DEVBLK *);
int     cckd_read_track (DEVBLK *, int, int, BYTE *);
int     cckd_update_track (DEVBLK *, BYTE *, int, BYTE *);
int     cfba_read_block (DEVBLK *, BYTE *, int, BYTE *);
int     cfba_write_block (DEVBLK *, BYTE *, int, BYTE *);
void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, BYTE *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_sf_comp (DEVBLK *);
int     cckd_command(BYTE *, int);
void    cckd_print_itrace ();

/* Functions in module cckdutil.c */
int     cckd_swapend (int, FILE *);
void    cckd_swapend_chdr (CCKDDASD_DEVHDR *);
void    cckd_swapend_l1 (CCKD_L1ENT *, int);
void    cckd_swapend_l2 (CCKD_L2ENT *);
void    cckd_swapend_free (CCKD_FREEBLK *);
void    cckd_swapend4 (char *);
void    cckd_swapend2 (char *);
int     cckd_endian ();
int     cckd_comp (int, FILE *);
int     cckd_chkdsk(int, FILE *, int);

#if defined(OPTION_W32_CTCI)
/* Functions in module w32ctca.c */
#include "w32ctca.h"
#endif // defined(OPTION_W32_CTCI)


#endif /*!defined(_HERCULES_H)*/
