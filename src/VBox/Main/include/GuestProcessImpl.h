
/* $Id$ */
/** @file
 * VirtualBox Main - Guest process handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTPROCESSIMPL
#define ____H_GUESTPROCESSIMPL

#include "GuestCtrlImplPrivate.h"
#include "GuestProcessWrap.h"

class Console;
class GuestSession;

/**
 * Class for handling a guest process.
 */
class ATL_NO_VTABLE GuestProcess :
    public GuestProcessWrap,
    public GuestObject
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestProcess)

    int     init(Console *aConsole, GuestSession *aSession, ULONG aProcessID, const GuestProcessStartupInfo &aProcInfo);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */


public:
    /** @name Public internal methods.
     * @{ */
    int i_callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    inline int i_checkPID(uint32_t uPID);
    static Utf8Str i_guestErrorToString(int guestRc);
    int i_onRemove(void);
    int i_readData(uint32_t uHandle, uint32_t uSize, uint32_t uTimeoutMS, void *pvData,
                   size_t cbData, uint32_t *pcbRead, int *pGuestRc);
    static HRESULT i_setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    int i_startProcess(uint32_t uTimeoutMS, int *pGuestRc);
    int i_startProcessAsync(void);
    int i_terminateProcess(uint32_t uTimeoutMS, int *pGuestRc);
    static ProcessWaitResult_T i_waitFlagsToResultEx(uint32_t fWaitFlags, ProcessStatus_T oldStatus,
                                                     ProcessStatus_T newStatus, uint32_t uProcFlags,
                                                     uint32_t uProtocol);
    ProcessWaitResult_T i_waitFlagsToResult(uint32_t fWaitFlags);
    int i_waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, ProcessWaitResult_T &waitResult, int *pGuestRc);
    int i_waitForInputNotify(GuestWaitEvent *pEvent, uint32_t uHandle, uint32_t uTimeoutMS,
                             ProcessInputStatus_T *pInputStatus, uint32_t *pcbProcessed);
    int i_waitForOutput(GuestWaitEvent *pEvent, uint32_t uHandle, uint32_t uTimeoutMS,
                        void* pvData, size_t cbData, uint32_t *pcbRead);
    int i_waitForStatusChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS,
                              ProcessStatus_T *pProcessStatus, int *pGuestRc);
    static bool i_waitResultImpliesEx(ProcessWaitResult_T waitResult, ProcessStatus_T procStatus,
                                      uint32_t uProcFlags, uint32_t uProtocol);
    int i_writeData(uint32_t uHandle, uint32_t uFlags, void *pvData, size_t cbData,
                    uint32_t uTimeoutMS, uint32_t *puWritten, int *pGuestRc);
    /** @}  */

protected:
    /** @name Protected internal methods.
     * @{ */
    inline bool i_isAlive(void);
    inline bool i_hasEnded(void);
    int i_onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int i_onProcessInputStatus(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int i_onProcessNotifyIO(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int i_onProcessStatusChange(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int i_onProcessOutput(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int i_prepareExecuteEnv(const char *pszEnv, void **ppvList, ULONG *pcbList, ULONG *pcEnvVars);
    int i_setProcessStatus(ProcessStatus_T procStatus, int procRc);
    static DECLCALLBACK(int) i_startProcessThread(RTTHREAD Thread, void *pvUser);
    /** @}  */

private:
    /** Wrapped @name IProcess data .
     * @{ */
     HRESULT getArguments(std::vector<com::Utf8Str> &aArguments);
     HRESULT getEnvironment(std::vector<com::Utf8Str> &aEnvironment);
     HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);
     HRESULT getExecutablePath(com::Utf8Str &aExecutablePath);
     HRESULT getExitCode(LONG *aExitCode);
     HRESULT getName(com::Utf8Str &aName);
     HRESULT getPID(ULONG *aPID);
     HRESULT getStatus(ProcessStatus_T *aStatus);

    /** Wrapped @name IProcess methods.
     * @{ */
     HRESULT waitFor(ULONG aWaitFor,
                     ULONG aTimeoutMS,
                     ProcessWaitResult_T *aReason);
     HRESULT waitForArray(const std::vector<ProcessWaitForFlag_T> &aWaitFor,
                          ULONG aTimeoutMS,
                          ProcessWaitResult_T *aReason);
     HRESULT read(ULONG aHandle,
                  ULONG aToRead,
                  ULONG aTimeoutMS,
                  std::vector<BYTE> &aData);
     HRESULT write(ULONG aHandle,
                   ULONG aFlags,
                   const std::vector<BYTE> &aData,
                   ULONG aTimeoutMS,
                   ULONG *aWritten);
     HRESULT writeArray(ULONG aHandle,
                        const std::vector<ProcessInputFlag_T> &aFlags,
                        const std::vector<BYTE> &aData,
                        ULONG aTimeoutMS,
                        ULONG *aWritten);
     HRESULT terminate();

    /**
     * This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization.
     */
    const ComObjPtr<EventSource> mEventSource;

    struct Data
    {
        /** The process startup information. */
        GuestProcessStartupInfo  mProcess;
        /** Exit code if process has been terminated. */
        LONG                     mExitCode;
        /** PID reported from the guest. */
        ULONG                    mPID;
        /** The current process status. */
        ProcessStatus_T          mStatus;
        /** The last returned process status
         *  returned from the guest side. */
        int                      mLastError;
    } mData;
};

/**
 * Guest process tool flags.
 */
/** No flags specified; wait until process terminates.
 *  The maximum waiting time is set in the process' startup
 *  info. */
#define GUESTPROCESSTOOL_FLAG_NONE            0
/** Wait until next stream block from stdout has been
 *  read in completely, then return.
 */
#define GUESTPROCESSTOOL_FLAG_STDOUT_BLOCK    RT_BIT(0)

/**
 * Internal class for handling a VBoxService tool ("vbox_ls", vbox_stat", ...).
 */
class GuestProcessTool
{
public:

    GuestProcessTool(void);

    virtual ~GuestProcessTool(void);

public:

    int Init(GuestSession *pGuestSession, const GuestProcessStartupInfo &startupInfo, bool fAsync, int *pGuestRc);

    GuestProcessStream &i_getStdOut(void) { return mStdOut; }

    GuestProcessStream &i_getStdErr(void) { return mStdErr; }

    int i_wait(uint32_t fFlags, int *pGuestRc);

    int i_waitEx(uint32_t fFlags, GuestProcessStreamBlock *pStreamBlock, int *pGuestRc);

    int i_getCurrentBlock(uint32_t uHandle, GuestProcessStreamBlock &strmBlock);

    bool i_isRunning(void);

    static int i_run(GuestSession *pGuestSession, const GuestProcessStartupInfo &startupInfo, int *pGuestRc);

    static int i_runEx(GuestSession *pGuestSession, const GuestProcessStartupInfo &startupInfo,
                       GuestCtrlStreamObjects *pStrmOutObjects, uint32_t cStrmOutObjects, int *pGuestRc);

    int i_terminatedOk(LONG *pExitCode);

    int i_terminate(uint32_t uTimeoutMS, int *pGuestRc);

protected:

    ComObjPtr<GuestSession>     pSession;
    ComObjPtr<GuestProcess>     pProcess;
    GuestProcessStartupInfo     mStartupInfo;
    GuestProcessStream          mStdOut;
    GuestProcessStream          mStdErr;
};

#endif /* !____H_GUESTPROCESSIMPL */

