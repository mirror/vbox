/* $Id$ */
/** @file
 * VBoxServiceControl.h - Internal guest control definitions.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxServiceControl_h
#define ___VBoxServiceControl_h

#include <iprt/list.h>
#include <iprt/critsect.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>


/**
 * Pipe IDs for handling the guest process poll set.
 */
typedef enum VBOXSERVICECTRLPIPEID
{
    VBOXSERVICECTRLPIPEID_UNKNOWN           = 0,
    VBOXSERVICECTRLPIPEID_STDIN             = 10,
    VBOXSERVICECTRLPIPEID_STDIN_WRITABLE    = 11,
    /** Pipe for reading from guest process' stdout. */
    VBOXSERVICECTRLPIPEID_STDOUT            = 40,
    /** Pipe for reading from guest process' stderr. */
    VBOXSERVICECTRLPIPEID_STDERR            = 50,
    /** Notification pipe for waking up the guest process
     *  control thread. */
    VBOXSERVICECTRLPIPEID_IPC_NOTIFY        = 100
} VBOXSERVICECTRLPIPEID;

/**
 * Request types to perform on a started guest process.
 */
typedef enum VBOXSERVICECTRLREQUESTTYPE
{
    /** Unknown request. */
    VBOXSERVICECTRLREQUEST_UNKNOWN          = 0,
    /** Main control thread asked used to quit. */
    VBOXSERVICECTRLREQUEST_QUIT             = 1,
    /** Performs reading from stdout. */
    VBOXSERVICECTRLREQUEST_PROC_STDOUT      = 50,
    /** Performs reading from stderr. */
    VBOXSERVICECTRLREQUEST_PROC_STDERR      = 60,
    /** Performs writing to stdin. */
    VBOXSERVICECTRLREQUEST_PROC_STDIN       = 70,
    /** Same as VBOXSERVICECTRLREQUEST_STDIN_WRITE, but
     *  marks the end of input. */
    VBOXSERVICECTRLREQUEST_PROC_STDIN_EOF   = 71,
    /** Kill/terminate process. */
    VBOXSERVICECTRLREQUEST_PROC_TERM        = 90,
    /** Gently ask process to terminate. */
    /** @todo Implement this! */
    VBOXSERVICECTRLREQUEST_PROC_HUP         = 91,
    /** Wait for a certain event to happen. */
    VBOXSERVICECTRLREQUEST_WAIT_FOR         = 100
} VBOXSERVICECTRLREQUESTTYPE;

/**
 * Thread list types.
 */
typedef enum VBOXSERVICECTRLTHREADLISTTYPE
{
    /** Unknown list -- uncool to use. */
    VBOXSERVICECTRLTHREADLIST_UNKNOWN       = 0,
    /** Stopped list: Here all guest threads end up
     *  when they reached the stopped state and can
     *  be shut down / free'd safely. */
    VBOXSERVICECTRLTHREADLIST_STOPPED       = 1,
    /**
     * Started list: Here all threads are registered
     * when they're up and running (that is, accepting
     * commands).
     */
    VBOXSERVICECTRLTHREADLIST_RUNNING       = 2
} VBOXSERVICECTRLTHREADLISTTYPE;

/**
 * Structure to perform a request on a started guest
 * process. Needed for letting the main guest control thread
 * to communicate (and wait) for a certain operation which
 * will be done in context of the started guest process thread.
 */
typedef struct VBOXSERVICECTRLREQUEST
{
    /** Event semaphore to serialize access. */
    RTSEMEVENTMULTI            Event;
    /** The request type to handle. */
    VBOXSERVICECTRLREQUESTTYPE enmType;
    /** Payload size; on input, this contains the (maximum) amount
     *  of data the caller  wants to write or to read. On output,
     *  this show the actual amount of data read/written. */
    size_t                     cbData;
    /** Payload data; a pre-allocated data buffer for input/output. */
    void                      *pvData;
    /** The context ID which is required to complete the
     *  request. Not used at the moment. */
    uint32_t                   uCID;
    /** The overall result of the operation. */
    int                        rc;
} VBOXSERVICECTRLREQUEST;
/** Pointer to request. */
typedef VBOXSERVICECTRLREQUEST *PVBOXSERVICECTRLREQUEST;

typedef struct VBOXSERVICECTRLREQDATA_WAIT_FOR
{
    /** Waiting flags. */
    uint32_t uWaitFlags;
    /** Timeout in (ms) for the waiting operation. */
    uint32_t uTimeoutMS;
} VBOXSERVICECTRLREQDATA_WAIT_FOR, *PVBOXSERVICECTRLREQDATA_WAIT_FOR;

/**
 * Structure for one (opened) guest file.
 */
typedef struct VBOXSERVICECTRLFILE
{
    /** Pointer to list archor of following
     *  list node.
     *  @todo Would be nice to have a RTListGetAnchor(). */
    PRTLISTANCHOR                   pAnchor;
    /** Node to global guest control file list. */
    /** @todo Use a map later? */
    RTLISTNODE                      Node;
    /** The file name. */
    char                            szName[RTPATH_MAX];
    /** The file handle on the guest. */
    RTFILE                          hFile;
    /** File handle to identify this file. */
    uint32_t                        uHandle;
    /** Context ID. */
    uint32_t                        uContextID;
} VBOXSERVICECTRLFILE;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLFILE *PVBOXSERVICECTRLFILE;

typedef struct VBOXSERVICECTRLSESSIONSTARTUPINFO
{
    /** The session's protocol version to use. */
    uint32_t                        uProtocol;
    /** The session's ID. */
    uint32_t                        uSessionID;
    /** User name (account) to start the guest session under. */
    char                            szUser[GUESTPROCESS_MAX_USER_LEN];
    /** Password of specified user name (account). */
    char                            szPassword[GUESTPROCESS_MAX_PASSWORD_LEN];
    /** Domain of the user account. */
    char                            szDomain[GUESTPROCESS_MAX_DOMAIN_LEN];
    /** Session creation flags.
     *  @sa VBOXSERVICECTRLSESSIONSTARTUPFLAG_* flags. */
    uint32_t                        uFlags;
} VBOXSERVICECTRLSESSIONSTARTUPINFO;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLSESSIONSTARTUPINFO *PVBOXSERVICECTRLSESSIONSTARTUPINFO;

/**
 * Structure for a guest session thread to
 * observe the forked session instance.
 */
typedef struct VBOXSERVICECTRLSESSIONTHREAD
{
    /** Node to global guest control session list. */
    /** @todo Use a map later? */
    RTLISTNODE                      Node;
    /** The sessions's startup info. */
    VBOXSERVICECTRLSESSIONSTARTUPINFO
                                    StartupInfo;
    /** The worker thread. */
    RTTHREAD                        Thread;
    /** Critical section for thread-safe use. */
    RTCRITSECT                      CritSect;
    /** Process handle for forked child. */
    RTPROCESS                       hProcess;
    /** Shutdown indicator; will be set when the thread
      * needs (or is asked) to shutdown. */
    bool volatile                   fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile                   fStopped;
    /** Whether the thread was started or not. */
    bool                            fStarted;
#if 0 /* Pipe IPC not used yet. */
    /** Pollset containing all the pipes. */
    RTPOLLSET                       hPollSet;
    RTPIPE                          hStdInW;
    RTPIPE                          hStdOutR;
    RTPIPE                          hStdErrR;
    struct StdPipe
    {
        RTHANDLE  hChild;
        PRTHANDLE phChild;
    }                               StdIn,
                                    StdOut,
                                    StdErr;
    /** The notification pipe associated with this guest session.
     *  This is NIL_RTPIPE for output pipes. */
    RTPIPE                          hNotificationPipeW;
    /** The other end of hNotificationPipeW. */
    RTPIPE                          hNotificationPipeR;
#endif
} VBOXSERVICECTRLSESSIONTHREAD;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLSESSIONTHREAD *PVBOXSERVICECTRLSESSIONTHREAD;

/** @todo Documentation needed. */
#define VBOXSERVICECTRLSESSION_FLAG_FORK                 RT_BIT(0)
#define VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT           RT_BIT(1)
#define VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR           RT_BIT(2)

/**
 * Strucutre for maintaining a guest session. This also
 * contains all started threads (e.g. for guest processes).
 *
 * This structure can act in two different ways:
 * - For legacy guest control handling (protocol version < 2)
 *   this acts as a per-guest process structure containing all
 *   the information needed to get a guest process up and running.
 * - For newer guest control protocols (>= 2) this structure is
 *   part of the forked session child, maintaining all guest
 *   control objects under it.
 */
typedef struct VBOXSERVICECTRLSESSION
{
    VBOXSERVICECTRLSESSIONSTARTUPINFO
                                    StartupInfo;
    /** List of active guest control threads (VBOXSERVICECTRLTHREAD). */
    RTLISTANCHOR                    lstControlThreadsActive;
    /** List of inactive guest control threads (VBOXSERVICECTRLTHREAD). */
    /** @todo Still needed? */
    RTLISTANCHOR                    lstControlThreadsInactive;
    /** List of guest control files (VBOXSERVICECTRLFILE). */
    RTLISTANCHOR                    lstFiles;
    /** Critical section for protecting the guest process
     *  threading list. */
    RTCRITSECT                      csControlThreads;
    /** Session flags. */
    uint32_t                        uFlags;
    /** How many processes do we allow keeping around at a time? */
    uint32_t                        uProcsMaxKept;
} VBOXSERVICECTRLSESSION;
/** Pointer to guest session. */
typedef VBOXSERVICECTRLSESSION *PVBOXSERVICECTRLSESSION;

/**
 * Structure holding information for starting a guest
 * process.
 */
typedef struct VBOXSERVICECTRLPROCSTARTUPINFO
{
    /** Full qualified path of process to start (without arguments). */
    char szCmd[GUESTPROCESS_MAX_CMD_LEN];
    /** Process execution flags. @sa */
    uint32_t uFlags;
    /** Command line arguments. */
    char szArgs[GUESTPROCESS_MAX_ARGS_LEN];
    /** Number of arguments specified in pszArgs. */
    uint32_t uNumArgs;
    /** String of environment variables ("FOO=BAR") to pass to the process
      * to start. */
    char szEnv[GUESTPROCESS_MAX_ENV_LEN];
    /** Size (in bytes) of environment variables block. */
    uint32_t cbEnv;
    /** Number of environment variables specified in pszEnv. */
    uint32_t uNumEnvVars;
    /** User name (account) to start the process under. */
    char szUser[GUESTPROCESS_MAX_USER_LEN];
    /** Password of specified user name (account). */
    char szPassword[GUESTPROCESS_MAX_PASSWORD_LEN];
    /** Time limit (in ms) of the process' life time. */
    uint32_t uTimeLimitMS;
    /** Process priority. */
    uint32_t uPriority;
    /** Process affinity. At the moment we support
     *  up to 4 * 64 = 256 CPUs. */
    uint64_t uAffinity[4];
    /** Number of used process affinity blocks. */
    uint32_t uNumAffinity;
} VBOXSERVICECTRLPROCSTARTUPINFO;
/** Pointer to a guest process block. */
typedef VBOXSERVICECTRLPROCSTARTUPINFO *PVBOXSERVICECTRLPROCESS;

/**
 * Structure for holding data for one (started) guest process.
 */
typedef struct VBOXSERVICECTRLTHREAD
{
    /** Pointer to list archor of following
     *  list node.
     *  @todo Would be nice to have a RTListGetAnchor(). */
    PRTLISTANCHOR                   pAnchor;
    /** Node. */
    RTLISTNODE                      Node;
    /** The worker thread. */
    RTTHREAD                        Thread;
    /** The session this guest process
     *  is bound to. */
    PVBOXSERVICECTRLSESSION         pSession;
    /** Shutdown indicator; will be set when the thread
      * needs (or is asked) to shutdown. */
    bool volatile                   fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile                   fStopped;
    /** Whether the service was started or not. */
    bool                            fStarted;
    /** Client ID. */
    uint32_t                        uClientID;
    /** Context ID. */
    uint32_t                        uContextID;
    /** Critical section for thread-safe use. */
    RTCRITSECT                      CritSect;
    /** @todo Document me! */
    uint32_t                        uPID;
    char                           *pszCmd;
    uint32_t                        uFlags;
    char                          **papszArgs;
    uint32_t                        uNumArgs;
    char                          **papszEnv;
    uint32_t                        uNumEnvVars;
    /** Name of specified user account to run the
     *  guest process under. */
    char                           *pszUser;
    /** Password of specified user account. */
    char                           *pszPassword;
    /** Overall time limit (in ms) that the guest process
     *  is allowed to run. 0 for indefinite time. */
    uint32_t                        uTimeLimitMS;
    /** Pointer to the current IPC request being
     *  processed. */
    PVBOXSERVICECTRLREQUEST         pRequest;
    /** StdIn pipe for addressing writes to the
     *  guest process' stdin.*/
    RTPIPE                          pipeStdInW;
    /** The notification pipe associated with this guest process.
     *  This is NIL_RTPIPE for output pipes. */
    RTPIPE                          hNotificationPipeW;
    /** The other end of hNotificationPipeW. */
    RTPIPE                          hNotificationPipeR;
} VBOXSERVICECTRLTHREAD;
/** Pointer to thread data. */
typedef VBOXSERVICECTRLTHREAD *PVBOXSERVICECTRLTHREAD;

RT_C_DECLS_BEGIN

/**
 * Note on naming conventions:
 * - VBoxServiceControl* is named everything sub service module related, e.g.
 *   everything which is callable by main() and/or the service dispatcher(s).
 * - GstCntl* is named everything which declared extern and thus can be called
 *   by different guest control modules as needed.
 * - gstcntl (all lowercase) is a purely static function to split up functionality
 *   inside a module.
 */

/* Guest session thread handling. */
extern int                      GstCntlSessionThreadOpen(PRTLISTANCHOR pList, const PVBOXSERVICECTRLSESSIONSTARTUPINFO pSessionStartupInfo, PVBOXSERVICECTRLSESSIONTHREAD *ppSessionThread);
extern int                      GstCntlSessionThreadClose(PVBOXSERVICECTRLSESSIONTHREAD pSession, uint32_t uFlags);
extern int                      GstCntlSessionThreadTerminate(PVBOXSERVICECTRLSESSIONTHREAD pSession);
extern RTEXITCODE               VBoxServiceControlSessionForkInit(int argc, char **argv);

/* asdf */
extern PVBOXSERVICECTRLTHREAD   GstCntlSessionAcquireProcess(PVBOXSERVICECTRLSESSION pSession, uint32_t uPID);
extern int                      GstCntlSessionCloseAll(PVBOXSERVICECTRLSESSION pSession);
extern int                      GstCntlSessionDestroy(PVBOXSERVICECTRLSESSION pSession);
extern int                      GstCntlSessionHandler(PVBOXSERVICECTRLSESSION pSession, uint32_t uMsg, PVBGLR3GUESTCTRLHOSTCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf, volatile bool *pfShutdown);
extern int                      GstCntlSessionListSet(PVBOXSERVICECTRLSESSION pSession, VBOXSERVICECTRLTHREADLISTTYPE enmList, PVBOXSERVICECTRLTHREAD pThread);
extern int                      GstCntlSessionProcessStartAllowed(const PVBOXSERVICECTRLSESSION pSession, bool *pbAllowed);
extern int                      GstCntlSessionReapThreads(PVBOXSERVICECTRLSESSION pSession);

/* Guest control main thread functions. */
extern int                      GstCntlListSet(VBOXSERVICECTRLTHREADLISTTYPE enmList, PVBOXSERVICECTRLTHREAD pThread);
extern int                      GstCntlSetInactive(PVBOXSERVICECTRLTHREAD pThread);
/* Per-thread guest process functions. */
extern int                      GstCntlProcessPerform(PVBOXSERVICECTRLTHREAD pProcess, PVBOXSERVICECTRLREQUEST pRequest);
extern int                      GstCntlProcessStart(uint32_t uContext, PVBOXSERVICECTRLPROCESS pProcess);
extern int                      GstCntlProcessStop(const PVBOXSERVICECTRLTHREAD pThread);
extern void                     GstCntlProcessRelease(const PVBOXSERVICECTRLTHREAD pThread);
extern int                      GstCntlProcessWait(const PVBOXSERVICECTRLTHREAD pThread, RTMSINTERVAL msTimeout, int *prc);
extern int                      GstCntlProcessFree(PVBOXSERVICECTRLTHREAD pThread);
/* Process request handling. */
extern int                      GstCntlProcessRequestAlloc(PVBOXSERVICECTRLREQUEST *ppReq, VBOXSERVICECTRLREQUESTTYPE enmType);
extern int                      GstCntlProcessRequestAllocEx(PVBOXSERVICECTRLREQUEST *ppReq, VBOXSERVICECTRLREQUESTTYPE  enmType, void *pvData, size_t cbData, uint32_t uCID);
extern void                     GstCntlProcessRequestFree(PVBOXSERVICECTRLREQUEST pReq);
/* Per-session functions. */
extern int gstcntlHandleFileOpen(uint32_t idClient, uint32_t cParms);
extern int gstcntlHandleFileClose(uint32_t idClient, uint32_t cParms);
extern int gstcntlHandleFileRead(uint32_t idClient, uint32_t cParms, void *pvScratchBuf, size_t cbScratchBuf);
extern int gstcntlHandleFileReadAt(uint32_t idClient, uint32_t cParms, void *pvScratchBuf, size_t cbScratchBuf);
extern int gstcntlHandleFileWrite(uint32_t idClient, uint32_t cParms, void *pvScratchBuf, size_t cbScratchBuf);
extern int gstcntlHandleFileWriteAt(uint32_t idClient, uint32_t cParms, void *pvScratchBuf, size_t cbScratchBuf);
extern int gstcntlHandleFileSeek(uint32_t idClient, uint32_t cParms);
extern int gstcntlHandleFileTell(uint32_t idClient, uint32_t cParms);
extern int                      GstCntlSessionHandleProcExec(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
extern int                      GstCntlSessionHandleProcInput(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf);
extern int                      GstCntlSessionHandleProcOutput(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
extern int                      GstCntlSessionHandleProcTerminate(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
extern int                      GstCntlSessionHandleProcWaitFor(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);

RT_C_DECLS_END

#endif /* ___VBoxServiceControl_h */

