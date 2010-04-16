/* $Id$ */
/** @file
 * VBoxService - Guest Additions Services.
 */

/*
 * Copyright (C) 2007-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBoxServiceInternal_h
#define ___VBoxServiceInternal_h

#include <stdio.h>
#ifdef RT_OS_WINDOWS
# include <Windows.h>
# include <process.h> /* Needed for file version information. */
#endif
#ifdef VBOX_WITH_GUEST_CONTROL
# include <list>
#endif

/**
 * A service descriptor.
 */
typedef struct
{
    /** The short service name. */
    const char *pszName;
    /** The longer service name. */
    const char *pszDescription;
    /** The usage options stuff for the --help screen. */
    const char *pszUsage;
    /** The option descriptions for the --help screen. */
    const char *pszOptions;

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnPreInit)(void);

    /**
     * Tries to parse the given command line option.
     *
     * @returns 0 if we parsed, -1 if it didn't and anything else means exit.
     * @param   ppszShort   If not NULL it points to the short option iterator. a short argument.
     *                      If NULL examine argv[*pi].
     * @param   argc        The argument count.
     * @param   argv        The argument vector.
     * @param   pi          The argument vector index. Update if any value(s) are eaten.
     */
    DECLCALLBACKMEMBER(int, pfnOption)(const char **ppszShort, int argc, char **argv, int *pi);

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(void);

    /** Called from the worker thread.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS if exitting because *pfTerminate was set.
     * @param   pfTerminate     Pointer to a per service termination flag to check
     *                          before and after blocking.
     */
    DECLCALLBACKMEMBER(int, pfnWorker)(bool volatile *pfTerminate);

    /**
     * Stop an service.
     */
    DECLCALLBACKMEMBER(void, pfnStop)(void);

    /**
     * Does termination cleanups.
     *
     * @remarks This may be called even if pfnInit hasn't been called!
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(void);
} VBOXSERVICE;
/** Pointer to a VBOXSERVICE. */
typedef VBOXSERVICE *PVBOXSERVICE;
/** Pointer to a const VBOXSERVICE. */
typedef VBOXSERVICE const *PCVBOXSERVICE;

#ifdef RT_OS_WINDOWS
/** The service name (needed for mutex creation on Windows). */
# define VBOXSERVICE_NAME           "VBoxService"
/** The friendly service name. */
# define VBOXSERVICE_FRIENDLY_NAME  "VirtualBox Guest Additions Service"
/** The service description (only W2K+ atm) */
# define VBOXSERVICE_DESCRIPTION    "Manages VM runtime information, time synchronization, remote sysprep execution and miscellaneous utilities for guest operating systems."
/** The following constant may be defined by including NtStatus.h. */
# define STATUS_SUCCESS             ((NTSTATUS)0x00000000L)
/** Structure for storing the looked up user information. */
typedef struct
{
    WCHAR wszUser[_MAX_PATH];
    WCHAR wszAuthenticationPackage[_MAX_PATH];
    WCHAR wszLogonDomain[_MAX_PATH];
} VBOXSERVICEVMINFOUSER, *PVBOXSERVICEVMINFOUSER;
/** Structure for the file information lookup. */
typedef struct
{
    char *pszFilePath;
    char *pszFileName;
} VBOXSERVICEVMINFOFILE, *PVBOXSERVICEVMINFOFILE;
/** Structure for process information lookup. */
typedef struct
{
    DWORD id;
    LUID luid;
} VBOXSERVICEVMINFOPROC, *PVBOXSERVICEVMINFOPROC;
/** Function prototypes for dynamic loading. */
typedef DWORD (WINAPI *PFNWTSGETACTIVECONSOLESESSIONID)(void);
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_CONTROL
/* Structure for holding thread relevant data. */
typedef struct
{
    /** The worker thread. */
    RTTHREAD                   Thread;
    /** Shutdown indicator. */
    bool volatile              fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile              fStopped;
    /** Whether the service was started or not. */
    bool                       fStarted;
    /** @todo */
    uint32_t  uClientID;
    uint32_t  uContextID;
    uint32_t  uPID;
    char     *pszCmd;
    uint32_t  uFlags;
    char    **papszArgs;
    uint32_t  uNumArgs;
    char    **papszEnv;
    uint32_t  uNumEnvVars;
    char     *pszStdIn;
    char     *pszStdOut;
    char     *pszStdErr;
    char     *pszUser;
    char     *pszPassword;
    uint32_t  uTimeLimitMS;
} VBOXSERVICECTRLTHREAD;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLTHREAD *PVBOXSERVICECTRLTHREAD;

/**
 * For buffering process input supplied by the client.
 */
typedef struct
{
    /** The mount of buffered data. */
    size_t  cb;
    /** The current data offset. */
    size_t  off;
    /** The data buffer. */
    char   *pch;
    /** The amount of allocated buffer space. */
    size_t  cbAllocated;
    /** Send further input into the bit bucket (stdin is dead). */
    bool    fBitBucket;
    /** The CRC-32 for standard input (received part). */
    uint32_t uCrc32;
} VBOXSERVICECTRLSTDINBUF;
/** Pointer to a standard input buffer. */
typedef VBOXSERVICECTRLSTDINBUF *PVBOXSERVICECTRLSTDINBUF;
#endif

RT_C_DECLS_BEGIN

extern char *g_pszProgName;
extern int g_cVerbosity;
extern uint32_t g_DefaultInterval;

extern int VBoxServiceSyntax(const char *pszFormat, ...);
extern int VBoxServiceError(const char *pszFormat, ...);
extern void VBoxServiceVerbose(int iLevel, const char *pszFormat, ...);
extern int VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32, uint32_t u32Min, uint32_t u32Max);
extern unsigned VBoxServiceGetStartedServices(void);
extern int VBoxServiceStartServices(unsigned iMain);
extern int VBoxServiceStopServices(void);

extern VBOXSERVICE g_TimeSync;
extern VBOXSERVICE g_Clipboard;
extern VBOXSERVICE g_Control;
extern VBOXSERVICE g_VMInfo;
extern VBOXSERVICE g_Exec;
extern VBOXSERVICE g_CpuHotPlug;
#ifdef VBOXSERVICE_MANAGEMENT
extern VBOXSERVICE g_MemBalloon;
extern VBOXSERVICE g_VMStatistics;
#endif
#ifdef VBOX_WITH_PAGE_SHARING
extern VBOXSERVICE g_PageSharing;
#endif

#ifdef RT_OS_WINDOWS
extern DWORD g_rcWinService;
extern SERVICE_TABLE_ENTRY const g_aServiceTable[];     /** @todo generate on the fly, see comment in main() from the enabled sub services. */
extern PFNWTSGETACTIVECONSOLESESSIONID g_pfnWTSGetActiveConsoleSessionId; /* VBoxServiceVMInfo-win.cpp */

extern int  VBoxServiceWinInstall(void);
extern int  VBoxServiceWinUninstall(void);
extern BOOL VBoxServiceWinSetStatus(DWORD dwStatus, DWORD dwCheckPoint);
# ifdef VBOX_WITH_GUEST_PROPS
extern bool VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession, VBOXSERVICEVMINFOPROC const *paProcs, DWORD cProcs);
extern bool VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
extern int  VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
extern void VBoxServiceVMInfoWinProcessesFree(PVBOXSERVICEVMINFOPROC paProcs);
extern int  VBoxServiceWinGetComponentVersions(uint32_t uiClientID);
# endif /* VBOX_WITH_GUEST_PROPS */
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_CONTROL
using namespace std;

typedef std::list< PVBOXSERVICECTRLTHREAD > GuestCtrlExecThreads;
typedef std::list< PVBOXSERVICECTRLTHREAD >::iterator GuestCtrlExecListIter;
typedef std::list< PVBOXSERVICECTRLTHREAD >::const_iterator GuestCtrlExecListIterConst;

extern int VBoxServiceControlExecProcess(uint32_t uContext, const char *pszCmd, uint32_t uFlags, 
                                         const char *pszArgs, uint32_t uNumArgs,                                           
                                         const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                         const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
                                         const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS);
extern void VBoxServiceControlExecDestroyThreadData(PVBOXSERVICECTRLTHREAD pThread);
#endif

#ifdef VBOXSERVICE_MANAGEMENT
extern uint32_t VBoxServiceBalloonQueryPages(uint32_t cbPage);
#endif

RT_C_DECLS_END

#endif

