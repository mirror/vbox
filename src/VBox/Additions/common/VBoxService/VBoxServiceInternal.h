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
/* Structure for holding process execution data. */
typedef struct _VBoxServiceControlProcessData
{
    char szCmd[_1K];
    uint32_t uFlags;
    char szArgs[_1K];
    uint32_t uNumArgs;
    char szEnv[_64K];
    uint32_t cbEnv;
    uint32_t uNumEnvVars;
    char szStdIn[_1K];
    char szStdOut[_1K];
    char szStdErr[_1K];
    char szUser[128];
    char szPassword[128];
    uint32_t uTimeLimitMS;
} VBOXSERVICECTRLPROCDATA;
/** Pointer to execution data. */
typedef VBOXSERVICECTRLPROCDATA *PVBOXSERVICECTRLPROCDATA;
/**
 * For buffering process input supplied by the client.
 */
typedef struct _VBoxServiceControlStdInBuf
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
extern int  VBoxServiceControlExecProcess(PVBOXSERVICECTRLPROCDATA pExecData,
                                          const char * const      *papszArgs,
                                          const char * const      *papszEnv);
#endif

#ifdef VBOXSERVICE_MANAGEMENT
extern uint32_t VBoxServiceBalloonQueryPages(uint32_t cbPage);
#endif

RT_C_DECLS_END

#endif

