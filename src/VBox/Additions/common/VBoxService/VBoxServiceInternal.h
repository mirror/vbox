/* $Id$ */
/** @file
 * VBoxService - Guest Additions Services.
 */

/*
 * Copyright (C) 2007-2011 Oracle Corporation
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

#include <VBox/VBoxGuestLib.h>

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

/** The service name.
 * @note Used on windows to name the service as well as the global mutex. */
#define VBOXSERVICE_NAME            "VBoxService"

#ifdef RT_OS_WINDOWS
/** The friendly service name. */
# define VBOXSERVICE_FRIENDLY_NAME  "VirtualBox Guest Additions Service"
/** The service description (only W2K+ atm) */
# define VBOXSERVICE_DESCRIPTION    "Manages VM runtime information, time synchronization, guest control execution and miscellaneous utilities for guest operating systems."
/** The following constant may be defined by including NtStatus.h. */
# define STATUS_SUCCESS             ((NTSTATUS)0x00000000L)
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_CONTROL
typedef enum VBOXSERVICECTRLTHREADDATATYPE
{
    kVBoxServiceCtrlThreadDataUnknown = 0,
    kVBoxServiceCtrlThreadDataExec
} VBOXSERVICECTRLTHREADDATATYPE;

typedef enum VBOXSERVICECTRLPIPEID
{
    VBOXSERVICECTRLPIPEID_STDIN = 0,
    VBOXSERVICECTRLPIPEID_STDIN_ERROR,
    VBOXSERVICECTRLPIPEID_STDIN_WRITABLE,
    VBOXSERVICECTRLPIPEID_STDIN_INPUT_NOTIFY,
    VBOXSERVICECTRLPIPEID_STDOUT,
    VBOXSERVICECTRLPIPEID_STDERR
} VBOXSERVICECTRLPIPEID;

/**
 * Structure for holding buffered pipe data.
 */
typedef struct
{
    /** The PID the pipe is assigned to. */
    uint32_t    uPID;
    /** The pipe's Id of enum VBOXSERVICECTRLPIPEID. */
    uint8_t     uPipeId;
    /** The data buffer. */
    uint8_t    *pbData;
    /** The amount of allocated buffer space. */
    uint32_t    cbAllocated;
    /** The actual used/occupied buffer space. */
    uint32_t    cbSize;
    /** Helper variable for keeping track of what
     *  already was processed and what not. */
    uint32_t    cbOffset;
    /** Critical section protecting this buffer structure. */
    RTCRITSECT  CritSect;
    /** Flag indicating whether this pipe buffer accepts new
     *  data to be written to or not. If not enabled, already
     *  (allocated) buffered data still can be read out. */
    bool        fEnabled;
    /** Set if it's necessary to write to the notification pipe. */
    bool        fNeedNotification;
    /** Set if the pipe needs to be closed after the next read/write. */
    bool        fPendingClose;
    /** The notification pipe associated with this buffer.
     * This is NIL_RTPIPE for output pipes. */
    RTPIPE      hNotificationPipeW;
    /** The other end of hNotificationPipeW. */
    RTPIPE      hNotificationPipeR;
    /** The event semaphore for getting notified whether something
     *  has changed, e.g. written or read from this buffer. */
    RTSEMEVENT  hEventSem;
} VBOXSERVICECTRLEXECPIPEBUF;
/** Pointer to buffered pipe data. */
typedef VBOXSERVICECTRLEXECPIPEBUF *PVBOXSERVICECTRLEXECPIPEBUF;

/**
 * Structure for holding guest exection relevant data.
 */
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

    RTPIPE                     pipeStdInW;
    VBOXSERVICECTRLEXECPIPEBUF stdIn;
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
#endif /* VBOX_WITH_GUEST_CONTROL */
#ifdef VBOX_WITH_GUEST_PROPS

/**
 * A guest property cache.
 */
typedef struct VBOXSERVICEVEPROPCACHE
{
    /** The client ID for HGCM communication. */
    uint32_t    uClientID;
    /** Head in a list of VBOXSERVICEVEPROPCACHEENTRY nodes. */
    RTLISTNODE  NodeHead;
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
    /** Node to successor.
     * @todo r=bird: This is not really the node to the successor, but
     *       rather the OUR node in the list.  If it helps, remember that
     *       its a doubly linked list. */
    RTLISTNODE  NodeSucc;
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
#ifdef VBOX_WITH_SHARED_FOLDERS
extern VBOXSERVICE  g_AutoMount;
#endif

extern RTEXITCODE   VBoxServiceSyntax(const char *pszFormat, ...);
extern RTEXITCODE   VBoxServiceError(const char *pszFormat, ...);
extern void         VBoxServiceVerbose(int iLevel, const char *pszFormat, ...);
extern int          VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32,
                                         uint32_t u32Min, uint32_t u32Max);
extern int          VBoxServiceStartServices(void);
extern int          VBoxServiceStopServices(void);
extern void         VBoxServiceMainWait(void);
extern int          VBoxServiceReportStatus(VBoxGuestFacilityStatus enmStatus);
#ifdef RT_OS_WINDOWS
extern RTEXITCODE   VBoxServiceWinInstall(void);
extern RTEXITCODE   VBoxServiceWinUninstall(void);
extern RTEXITCODE   VBoxServiceWinEnterCtrlDispatcher(void);
extern void         VBoxServiceWinSetStopPendingStatus(uint32_t uCheckPoint);
#endif

#ifdef VBOXSERVICE_TOOLBOX
extern bool         VBoxServiceToolboxMain(int argc, char **argv, RTEXITCODE *prcExit);
#endif

#ifdef RT_OS_WINDOWS
# ifdef VBOX_WITH_GUEST_PROPS
extern int          VBoxServiceVMInfoWinWriteUsers(char **ppszUserList, uint32_t *pcUsersInList);
extern int          VBoxServiceWinGetComponentVersions(uint32_t uiClientID);
# endif /* VBOX_WITH_GUEST_PROPS */
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_CONTROL
extern void         VBoxServiceControlThreadSignalShutdown(const PVBOXSERVICECTRLTHREAD pThread);
extern int          VBoxServiceControlThreadWaitForShutdown(const PVBOXSERVICECTRLTHREAD pThread);

extern int          VBoxServiceControlExecHandleCmdStartProcess(uint32_t u32ClientId, uint32_t uNumParms);
extern int          VBoxServiceControlExecHandleCmdSetInput(uint32_t u32ClientId, uint32_t uNumParms, size_t cbMaxBufSize);
extern int          VBoxServiceControlExecHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms);
extern int          VBoxServiceControlExecProcess(uint32_t uClientID, uint32_t uContext,
                                                  const char *pszCmd, uint32_t uFlags,
                                                  const char *pszArgs, uint32_t uNumArgs,
                                                  const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                                  const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS);
extern void         VBoxServiceControlExecThreadDataDestroy(PVBOXSERVICECTRLTHREADDATAEXEC pThread);
#endif /* VBOX_WITH_GUEST_CONTROL */

#ifdef VBOXSERVICE_MANAGEMENT
extern uint32_t     VBoxServiceBalloonQueryPages(uint32_t cbPage);
#endif
#if defined(VBOX_WITH_PAGE_SHARING) && defined(RT_OS_WINDOWS)
extern RTEXITCODE   VBoxServicePageSharingInitFork(void);
#endif

RT_C_DECLS_END

#endif

