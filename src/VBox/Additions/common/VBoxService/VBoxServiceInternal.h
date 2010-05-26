/* $Id$ */
/** @file
 * VBoxService - Guest Additions Services.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxServiceInternal_h
#define ___VBoxServiceInternal_h

#include <stdio.h>
#ifdef RT_OS_WINDOWS
# include <Windows.h>
# include <process.h> /* Needed for file version information. */
#endif

#include <iprt/list.h>
#include <iprt/critsect.h>

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

/** @todo Move these Windows stuff into VBoxServiceVMInfo-win.cpp and hide all
 *        the windows details using behind function calls!
 * @{  */
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
/** @} */

#endif /* RT_OS_WINDOWS */
#ifdef VBOX_WITH_GUEST_CONTROL

enum VBOXSERVICECTRLTHREADDATATYPE
{
    VBoxServiceCtrlThreadDataUnknown = 0,
    VBoxServiceCtrlThreadDataExec = 1
};

typedef struct
{
    uint8_t    *pbData;
    uint32_t    cbSize;
    uint32_t    cbOffset;
    uint32_t    cbRead;
    RTCRITSECT  CritSect;
} VBOXSERVICECTRLEXECPIPEBUF;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLEXECPIPEBUF *PVBOXSERVICECTRLEXECPIPEBUF;

/* Structure for holding guest exection relevant data. */
typedef struct
{
    uint32_t  uPID;
    char     *pszCmd;
    uint32_t  uFlags;
    char    **papszArgs;
    uint32_t  uNumArgs;
    char    **papszEnv;
    uint32_t  uNumEnvVars;
    char     *pszUser;
    char     *pszPassword;
    uint32_t  uTimeLimitMS;

    VBOXSERVICECTRLEXECPIPEBUF stdOut;
    VBOXSERVICECTRLEXECPIPEBUF stdErr;

} VBOXSERVICECTRLTHREADDATAEXEC;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLTHREADDATAEXEC *PVBOXSERVICECTRLTHREADDATAEXEC;

/* Structure for holding thread relevant data. */
typedef struct VBOXSERVICECTRLTHREAD
{
    /** Node. */
    RTLISTNODE                      Node;
    /** The worker thread. */
    RTTHREAD                        Thread;
    /** Shutdown indicator. */
    bool volatile                   fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile                   fStopped;
    /** Whether the service was started or not. */
    bool                            fStarted;
    /** Client ID. */
    uint32_t                        uClientID;
    /** Context ID. */
    uint32_t                        uContextID;
    /** Type of thread.  See VBOXSERVICECTRLTHREADDATATYPE for more info. */
    VBOXSERVICECTRLTHREADDATATYPE   enmType;
    /** Pointer to actual thread data, depending on enmType. */
    void                           *pvData;
} VBOXSERVICECTRLTHREAD;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLTHREAD *PVBOXSERVICECTRLTHREAD;

/**
 * For buffering process input supplied by the client.
 */
typedef struct VBOXSERVICECTRLSTDINBUF
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

#endif /* VBOX_WITH_GUEST_CONTROL */
#ifdef VBOX_WITH_GUEST_PROPS

/**
 * A guest property cache.
 */
typedef struct VBOXSERVICEVEPROPCACHE
{
    /** The client ID for HGCM communication. */
    uint32_t    uClientID;
    /** List of VBOXSERVICEVEPROPCACHEENTRY nodes. */
    RTLISTNODE  ListEntries;
    /** Critical section for thread-safe use. */
    RTCRITSECT  CritSect;
} VBOXSERVICEVEPROPCACHE;
/** Pointer to a guest property cache. */
typedef VBOXSERVICEVEPROPCACHE *PVBOXSERVICEVEPROPCACHE;

/**
 * An entry in the property cache (VBOXSERVICEVEPROPCACHE).
 */
typedef struct VBOXSERVICEVEPROPCACHEENTRY
{
    /** Node. */
    RTLISTNODE  Node;
    /** Name (and full path) of guest property. */
    char       *pszName;
    /** The last value stored (for reference). */
    char       *pszValue;
    /** Reset value to write if property is temporary.  If NULL, it will be
     *  deleted. */
    char       *pszValueReset;
    /** Flags. */
    uint32_t    fFlags;
} VBOXSERVICEVEPROPCACHEENTRY;
/** Pointer to a cached guest property. */
typedef VBOXSERVICEVEPROPCACHEENTRY *PVBOXSERVICEVEPROPCACHEENTRY;

#endif /* VBOX_WITH_GUEST_PROPS */

RT_C_DECLS_BEGIN

extern char        *g_pszProgName;
extern int          g_cVerbosity;
extern uint32_t     g_DefaultInterval;
extern VBOXSERVICE  g_TimeSync;
extern VBOXSERVICE  g_Clipboard;
extern VBOXSERVICE  g_Control;
extern VBOXSERVICE  g_VMInfo;
extern VBOXSERVICE  g_CpuHotPlug;
#ifdef VBOXSERVICE_MANAGEMENT
extern VBOXSERVICE  g_MemBalloon;
extern VBOXSERVICE  g_VMStatistics;
#endif
#ifdef VBOX_WITH_PAGE_SHARING
extern VBOXSERVICE  g_PageSharing;
#endif

extern RTEXITCODE   VBoxServiceSyntax(const char *pszFormat, ...);
extern RTEXITCODE   VBoxServiceError(const char *pszFormat, ...);
extern void         VBoxServiceVerbose(int iLevel, const char *pszFormat, ...);
extern int          VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32,
                                         uint32_t u32Min, uint32_t u32Max);
extern int          VBoxServiceStartServices(void);
extern int          VBoxServiceStopServices(void);
extern void         VBoxServiceMainWait(void);
#ifdef RT_OS_WINDOWS
extern RTEXITCODE   VBoxServiceWinInstall(void);
extern RTEXITCODE   VBoxServiceWinUninstall(void);
extern RTEXITCODE   VBoxServiceWinEnterCtrlDispatcher(void);
extern void         VBoxServiceWinSetStopPendingStatus(uint32_t uCheckPoint);
#endif

#ifdef RT_OS_WINDOWS
extern PFNWTSGETACTIVECONSOLESESSIONID g_pfnWTSGetActiveConsoleSessionId; /* VBoxServiceVMInfo-win.cpp */
# ifdef VBOX_WITH_GUEST_PROPS
extern bool VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession, VBOXSERVICEVMINFOPROC const *paProcs, DWORD cProcs);
extern bool VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
extern int  VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
extern void VBoxServiceVMInfoWinProcessesFree(PVBOXSERVICEVMINFOPROC paProcs);
extern int  VBoxServiceWinGetComponentVersions(uint32_t uiClientID);
# endif /* VBOX_WITH_GUEST_PROPS */
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_CONTROL
extern int  VBoxServiceControlExecProcess(uint32_t uContext, const char *pszCmd, uint32_t uFlags,
                                          const char *pszArgs, uint32_t uNumArgs,
                                          const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                          const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS);
extern void VBoxServiceControlExecDestroyThreadData(PVBOXSERVICECTRLTHREADDATAEXEC pThread);
extern int  VBoxServiceControlExecReadPipeBufferContent(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                                        uint8_t *pbBuffer, uint32_t cbBuffer, uint32_t *pcbToRead);
extern int  VBoxServiceControlExecWritePipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                                  uint8_t *pbData, uint32_t cbData);
#endif

#ifdef VBOXSERVICE_MANAGEMENT
extern uint32_t VBoxServiceBalloonQueryPages(uint32_t cbPage);
#endif

RT_C_DECLS_END

#endif

