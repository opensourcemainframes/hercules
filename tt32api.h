/**********************************************************************************\
 *
 *                        T U N T A P 3 2 . D L L
 *
 *                     EXPORTED FUNCTION DEFINITIONS
 *
 *    These functions provide a 'C' language interface and can be called from
 *    any type of app that can access a DLL: VB, C/C++, PowerBuilder, etc.
 *
 **********************************************************************************

  Copyright (c) 2002-2004,
  Software Development Laboratories (aka "Fish" (David B. Trout)).

  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

   o  Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

   o  Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   o  Neither the name of Software Development Laboratories nor the name "Fish"
      nor the name David B. Trout nor the names of any of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGE.

 **********************************************************************************
 *
 *                            CHANGE HISTORY
 *
 *  MM/DD/YY   Version      Description...
 *  --------  ---------    -------------------------------------------------------
 *
 *  12/22/01    1.0.0       Created.
 *  07/20/02    2.0.0       (JAP) LCS modifications/enhancements.
 *  07/02/03    2.0.2       (Fish) use std 'uint32_t' type instead of Win32 DWORD
 *
\**********************************************************************************/

#ifndef _TT32API_H_
#define _TT32API_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define TT32SDEVBUFF    _IOW('T', 220, int)
#define TT32GDEVBUFF    _IOR('T', 220, int)
#define TT32SIOBUFF     _IOW('T', 221, int)
#define TT32GIOBUFF     _IOR('T', 221, int)
#define TT32STIMEOUT    _IOW('T', 222, int)
#define TT32GTIMEOUT    _IOR('T', 222, int)
#define TT32GSTATS      _IOR('T', 223, int)

struct tt32ctl
{
    union
    {
        char    ctln_name[IFNAMSIZ];    // Interface name, e.g. "en0".
    } tt32_ctln;

    union
    {
        int     ctlu_devbuffsize;       // Kernel buffer size
        int     ctlu_iobuffsize;        // Read buffer size
        int     ctlu_readtimeout;       // Read timeout value
    } tt32_ctlu;
};

#define tt32ctl_name         tt32_ctln.ctln_name
#define tt32ctl_devbuffsize  tt32_ctlu.ctlu_devbuffsize
#define tt32ctl_iobuffsize   tt32_ctlu.ctlu_iobuffsize
#define tt32ctl_readtimeout  tt32_ctlu.ctlu_readtimeout

typedef struct _tagTT32STATS
{
    uint32_t  dwStructSize;         // size of this structure
    uint32_t  dwKernelBuffSize;     // size of kernel capture buffer
    uint32_t  dwReadBuffSize;       // size of dll I/O buffer
    uint32_t  dwMaxBytesReceived;   // max dll I/O bytes received

    int64_t  n64WriteCalls;         // total #of write requests
    int64_t  n64WriteIOs;           // total #of write I/Os

    int64_t  n64ReadCalls;          // total #of read requests
    int64_t  n64ReadIOs;            // total #of read I/Os

    int64_t  n64PacketsRead;        // total #of packets read
    int64_t  n64PacketsWritten;     // total #of packets written

    int64_t  n64BytesRead;          // total #of bytes read
    int64_t  n64BytesWritten;       // total #of bytes written

    int64_t  n64InternalPackets;    // total #of packets handled internally
    int64_t  n64IgnoredPackets;     // total #of packets ignored
}
TT32STATS, *PTT32STATS;


#ifndef EXPORT
#define EXPORT /*(we must be importing instead of exporting)*/
#endif

/**********************************************************************************\
                     TunTap32.dll exported functions...
\**********************************************************************************/

typedef void (__cdecl *ptr_to_print_debug_string_func)(const char* debug_string);

extern const char* WINAPI EXPORT tuntap32_copyright_string  ();
extern const char* WINAPI EXPORT tuntap32_version_string    ();
extern void        WINAPI EXPORT tuntap32_version_numbers   (int* major, int* inter, int* minor, int* build);
extern int         WINAPI EXPORT tuntap32_set_debug_output_func (ptr_to_print_debug_string_func pfn);

extern int         WINAPI EXPORT tuntap32_open              (char* gatewaydev, int flags);
extern int         WINAPI EXPORT tuntap32_write             (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read              (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_close             (int fd);
extern int         WINAPI EXPORT tuntap32_ioctl             (int fd, int request, char* argp);
extern int         WINAPI EXPORT tuntap32_get_stats         (int fd, TT32STATS* stats);
extern const char* WINAPI EXPORT tuntap32_get_default_iface ();

// For compatability with previous releases (do not use if at all possible!)
extern int         WINAPI EXPORT tuntap32_open_ip_tuntap    (char* virtualipaddr, char* gatewayipaddr,
                                                             u_long capturebuffersize, u_long iobuffersize);
extern int         WINAPI EXPORT tuntap32_write_ip_tun      (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read_ip_tap       (int fd, u_char* buffer, u_long len, u_long timeout);
extern int         WINAPI EXPORT tuntap32_close_ip_tuntap   (int fd);
extern int         WINAPI EXPORT tuntap32_get_ip_stats      (int fd, TT32STATS* stats);

/* (in case they want to use LoadLibrary and GetProcAddress instead) */

typedef const char* (WINAPI *ptuntap32_copyright_string) ();
typedef const char* (WINAPI *ptuntap32_version_string)   ();
typedef void        (WINAPI *ptuntap32_version_numbers)  (int*,int*,int*,int*);
typedef int         (WINAPI *ptuntap32_set_debug_output_func) (ptr_to_print_debug_string_func);

typedef int         (WINAPI *ptuntap32_open)             (char*,int);
typedef int         (WINAPI *ptuntap32_write)            (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read)             (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_close)            (int);
typedef int         (WINAPI *ptuntap32_ioctl)            (int,int,char*);
typedef int         (WINAPI *ptuntap32_get_stats)        (int fd, TT32STATS* stats);
typedef const char* (WINAPI *ptuntap32_get_default_iface)();

// For compatability with previous releases (do not use if at all possible!)
typedef int         (WINAPI *ptuntap32_open_ip_tuntap)   (char*,char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_write_ip_tun)     (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read_ip_tap)      (int,u_char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_close_ip_tuntap)  (int);
typedef int         (WINAPI *ptuntap32_get_ip_stats)     (int, TT32STATS*);

/**********************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _TT32API_H_ */

/**********************************************************************************/
