/* $Id$ */
/** @file
 * IPRT - some Windows OS type constants.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___internal_r3_win_h
#define ___internal_r3_win_h

#include "internal/iprt.h"
#include <iprt/types.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Windows OS type as determined by rtSystemWinOSType().
 *
 * @note ASSUMPTIONS are made regarding ordering. Win 9x should come first, then
 *       NT. The Win9x and NT versions should internally be ordered in ascending
 *       version/code-base order.
 */
typedef enum RTWINOSTYPE
{
    kRTWinOSType_UNKNOWN    = 0,
    kRTWinOSType_9XFIRST    = 1,
    kRTWinOSType_95         = kRTWinOSType_9XFIRST,
    kRTWinOSType_95SP1,
    kRTWinOSType_95OSR2,
    kRTWinOSType_98,
    kRTWinOSType_98SP1,
    kRTWinOSType_98SE,
    kRTWinOSType_ME,
    kRTWinOSType_9XLAST     = 99,
    kRTWinOSType_NTFIRST    = 100,
    kRTWinOSType_NT310      = kRTWinOSType_NTFIRST,
    kRTWinOSType_NT350,
    kRTWinOSType_NT351,
    kRTWinOSType_NT4,
    kRTWinOSType_2K,                        /* 5.0 */
    kRTWinOSType_XP,                        /* 5.1 */
    kRTWinOSType_XP64,                      /* 5.2, workstation */
    kRTWinOSType_2003,                      /* 5.2 */
    kRTWinOSType_VISTA,                     /* 6.0, workstation */
    kRTWinOSType_2008,                      /* 6.0, server */
    kRTWinOSType_7,                         /* 6.1, workstation */
    kRTWinOSType_2008R2,                    /* 6.1, server */
    kRTWinOSType_8,                         /* 6.2, workstation */
    kRTWinOSType_2012,                      /* 6.2, server */
    kRTWinOSType_81,                        /* 6.3, workstation */
    kRTWinOSType_2012R2,                    /* 6.3, server */
    kRTWinOSType_10,                        /* 10.0, workstation */
    kRTWinOSType_2016,                      /* 10.0, server */
    kRTWinOSType_NT_UNKNOWN = 199,
    kRTWinOSType_NT_LAST    = kRTWinOSType_UNKNOWN
} RTWINOSTYPE;

/**
 * Windows loader protection level.
 */
typedef enum RTR3WINLDRPROT
{
    RTR3WINLDRPROT_INVALID = 0,
    RTR3WINLDRPROT_NONE,
    RTR3WINLDRPROT_NO_CWD,
    RTR3WINLDRPROT_SAFE,
    RTR3WINLDRPROT_SAFER
} RTR3WINLDRPROT;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
extern DECLHIDDEN(RTR3WINLDRPROT)   g_enmWinLdrProt;
extern DECLHIDDEN(RTWINOSTYPE)      g_enmWinVer;
#ifdef _WINDEF_
extern DECLHIDDEN(OSVERSIONINFOEXW) g_WinOsInfoEx;

extern DECLHIDDEN(HMODULE)                         g_hModKernel32;
typedef UINT (WINAPI *PFNGETWINSYSDIR)(LPWSTR,UINT);
extern DECLHIDDEN(PFNGETWINSYSDIR)                 g_pfnGetSystemWindowsDirectoryW;
extern decltype(SystemTimeToTzSpecificLocalTime)  *g_pfnSystemTimeToTzSpecificLocalTime;

extern DECLHIDDEN(HMODULE)          g_hModNtDll;
typedef NTSTATUS (NTAPI *PFNNTQUERYFULLATTRIBUTESFILE)(struct _OBJECT_ATTRIBUTES *, struct _FILE_NETWORK_OPEN_INFORMATION *);
extern DECLHIDDEN(PFNNTQUERYFULLATTRIBUTESFILE) g_pfnNtQueryFullAttributesFile;
typedef NTSTATUS (NTAPI *PFNNTDUPLICATETOKEN)(HANDLE, ACCESS_MASK, struct _OBJECT_ATTRIBUTES *, BOOLEAN, TOKEN_TYPE, PHANDLE);
extern DECLHIDDEN(PFNNTDUPLICATETOKEN)             g_pfnNtDuplicateToken;
#ifdef ___iprt_nt_nt_h___
extern decltype(NtAlertThread)                    *g_pfnNtAlertThread;
#endif

extern DECLHIDDEN(HMODULE)                         g_hModWinSock;

/** WSAStartup */
typedef int             (WINAPI *PFNWSASTARTUP)(WORD, struct WSAData *);
/** WSACleanup */
typedef int             (WINAPI *PFNWSACLEANUP)(void);
/** WSAGetLastError */
typedef int             (WINAPI *PFNWSAGETLASTERROR)(void);
/** WSASetLastError */
typedef int             (WINAPI *PFNWSASETLASTERROR)(int);
/** WSACreateEvent */
typedef HANDLE          (WINAPI *PFNWSACREATEEVENT)(void);
/** WSASetEvent */
typedef BOOL            (WINAPI *PFNWSASETEVENT)(HANDLE);
/** WSACloseEvent */
typedef BOOL            (WINAPI *PFNWSACLOSEEVENT)(HANDLE);
/** WSAEventSelect */
typedef BOOL            (WINAPI *PFNWSAEVENTSELECT)(UINT_PTR, HANDLE, LONG);
/** WSAEnumNetworkEvents */
typedef int             (WINAPI *PFNWSAENUMNETWORKEVENTS)(UINT_PTR, HANDLE, struct _WSANETWORKEVENTS *);
/** WSASend */
typedef int             (WINAPI *PFNWSASend)(UINT_PTR, struct _WSABUF *, DWORD, LPDWORD, DWORD dwFlags, struct _OVERLAPPED *, PFNRT /*LPWSAOVERLAPPED_COMPLETION_ROUTINE*/);

/** socket */
typedef UINT_PTR        (WINAPI *PFNWINSOCKSOCKET)(int, int, int);
/** closesocket */
typedef int             (WINAPI *PFNWINSOCKCLOSESOCKET)(UINT_PTR);
/** recv */
typedef int             (WINAPI *PFNWINSOCKRECV)(UINT_PTR, char *, int, int);
/** send */
typedef int             (WINAPI *PFNWINSOCKSEND)(UINT_PTR, const char *, int, int);
/** recvfrom */
typedef int             (WINAPI *PFNWINSOCKRECVFROM)(UINT_PTR, char *, int, int, struct sockaddr *, int *);
/** sendto */
typedef int             (WINAPI *PFNWINSOCKSENDTO)(UINT_PTR, const char *, int, int, const struct sockaddr *, int);
/** bind */
typedef int             (WINAPI *PFNWINSOCKBIND)(UINT_PTR, const struct sockaddr *, int);
/** listen  */
typedef int             (WINAPI *PFNWINSOCKLISTEN)(UINT_PTR, int);
/** accept */
typedef UINT_PTR        (WINAPI *PFNWINSOCKACCEPT)(UINT_PTR, struct sockaddr *, int *);
/** connect */
typedef int             (WINAPI *PFNWINSOCKCONNECT)(UINT_PTR, const struct sockaddr *, int);
/** shutdown */
typedef int             (WINAPI *PFNWINSOCKSHUTDOWN)(UINT_PTR, int);
/** getsockopt */
typedef int             (WINAPI *PFNWINSOCKGETSOCKOPT)(UINT_PTR, int, int, char *, int *);
/** setsockopt */
typedef int             (WINAPI *PFNWINSOCKSETSOCKOPT)(UINT_PTR, int, int, const char *, int);
/** ioctlsocket */
typedef int             (WINAPI *PFNWINSOCKIOCTLSOCKET)(UINT_PTR, long, unsigned long *);
/** getpeername   */
typedef int             (WINAPI *PFNWINSOCKGETPEERNAME)(UINT_PTR, struct sockaddr *, int *);
/** getsockname */
typedef int             (WINAPI *PFNWINSOCKGETSOCKNAME)(UINT_PTR, struct sockaddr *, int *);
/** __WSAFDIsSet */
typedef int             (WINAPI *PFNWINSOCK__WSAFDISSET)(UINT_PTR, struct fd_set *);
/** select */
typedef int             (WINAPI *PFNWINSOCKSELECT)(int, struct fd_set *, struct fd_set *, struct fd_set *, const struct timeval *);
/** gethostbyname */
typedef struct hostent *(WINAPI *PFNWINSOCKGETHOSTBYNAME)(const char *);

extern DECLHIDDEN(PFNWSASTARTUP)                   g_pfnWSAStartup;
extern DECLHIDDEN(PFNWSACLEANUP)                   g_pfnWSACleanup;
extern PFNWSAGETLASTERROR                          g_pfnWSAGetLastError;
extern PFNWSASETLASTERROR                          g_pfnWSASetLastError;
extern DECLHIDDEN(PFNWSACREATEEVENT)               g_pfnWSACreateEvent;
extern DECLHIDDEN(PFNWSACLOSEEVENT)                g_pfnWSACloseEvent;
extern DECLHIDDEN(PFNWSASETEVENT)                  g_pfnWSASetEvent;
extern DECLHIDDEN(PFNWSAEVENTSELECT)               g_pfnWSAEventSelect;
extern DECLHIDDEN(PFNWSAENUMNETWORKEVENTS)         g_pfnWSAEnumNetworkEvents;
extern DECLHIDDEN(PFNWSASend)                      g_pfnWSASend;
extern DECLHIDDEN(PFNWINSOCKSOCKET)                g_pfnsocket;
extern DECLHIDDEN(PFNWINSOCKCLOSESOCKET)           g_pfnclosesocket;
extern DECLHIDDEN(PFNWINSOCKRECV)                  g_pfnrecv;
extern DECLHIDDEN(PFNWINSOCKSEND)                  g_pfnsend;
extern DECLHIDDEN(PFNWINSOCKRECVFROM)              g_pfnrecvfrom;
extern DECLHIDDEN(PFNWINSOCKSENDTO)                g_pfnsendto;
extern DECLHIDDEN(PFNWINSOCKBIND)                  g_pfnbind;
extern DECLHIDDEN(PFNWINSOCKLISTEN)                g_pfnlisten;
extern DECLHIDDEN(PFNWINSOCKACCEPT)                g_pfnaccept;
extern DECLHIDDEN(PFNWINSOCKCONNECT)               g_pfnconnect;
extern DECLHIDDEN(PFNWINSOCKSHUTDOWN)              g_pfnshutdown;
extern DECLHIDDEN(PFNWINSOCKGETSOCKOPT)            g_pfngetsockopt;
extern DECLHIDDEN(PFNWINSOCKSETSOCKOPT)            g_pfnsetsockopt;
extern DECLHIDDEN(PFNWINSOCKIOCTLSOCKET)           g_pfnioctlsocket;
extern DECLHIDDEN(PFNWINSOCKGETPEERNAME)           g_pfngetpeername;
extern DECLHIDDEN(PFNWINSOCKGETSOCKNAME)           g_pfngetsockname;
extern DECLHIDDEN(PFNWINSOCK__WSAFDISSET)          g_pfn__WSAFDIsSet;
extern DECLHIDDEN(PFNWINSOCKSELECT)                g_pfnselect;
extern DECLHIDDEN(PFNWINSOCKGETHOSTBYNAME)         g_pfngethostbyname;
#endif


#endif

