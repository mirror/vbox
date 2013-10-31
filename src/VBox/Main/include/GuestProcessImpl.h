
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

#include "VirtualBoxBase.h"
#include "GuestCtrlImplPrivate.h"

class Console;
class GuestSession;

/**
 * Class for handling a guest process.
 */
class ATL_NO_VTABLE GuestProcess :
    public VirtualBoxBase,
    public GuestObject,
    VBOX_SCRIPTABLE_IMPL(IGuestProcess)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestProcess, IGuestProcess)
    DECLARE_NOT_AGGREGATABLE(GuestProcess)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestProcess)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestProcess)
        COM_INTERFACE_ENTRY(IProcess)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestProcess)

    int     init(Console *aConsole, GuestSession *aSession, ULONG aProcessID, const GuestProcessStartupInfo &aProcInfo);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IProcess interface.
     * @{ */
    STDMETHOD(COMGETTER(Arguments))(ComSafeArrayOut(BSTR, aArguments));
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(BSTR, aEnvironment));
    STDMETHOD(COMGETTER(EventSource))(IEventSource ** aEventSource);
    STDMETHOD(COMGETTER(ExecutablePath))(BSTR *aExecutablePath);
    STDMETHOD(COMGETTER(ExitCode))(LONG *aExitCode);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(PID))(ULONG *aPID);
    STDMETHOD(COMGETTER(Status))(ProcessStatus_T *aStatus);

    STDMETHOD(Read)(ULONG aHandle, ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Terminate)(void);
    STDMETHOD(WaitFor)(ULONG aWaitFlags, ULONG aTimeoutMS, ProcessWaitResult_T *aReason);
    STDMETHOD(WaitForArray)(ComSafeArrayIn(ProcessWaitForFlag_T, aFlags), ULONG aTimeoutMS, ProcessWaitResult_T *aReason);
    STDMETHOD(Write)(ULONG aHandle, ULONG aFlags, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    STDMETHOD(WriteArray)(ULONG aHandle, ComSafeArrayIn(ProcessInputFlag_T, aFlags), ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    inline int checkPID(uint32_t uPID);
    static Utf8Str guestErrorToString(int guestRc);
    int readData(uint32_t uHandle, uint32_t uSize, uint32_t uTimeoutMS, void *pvData, size_t cbData, uint32_t *pcbRead, int *pGuestRc);
    static HRESULT setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    int startProcess(uint32_t uTimeoutMS, int *pGuestRc);
    int startProcessAsync(void);
    int terminateProcess(uint32_t uTimeoutMS, int *pGuestRc);
    static ProcessWaitResult_T waitFlagsToResultEx(uint32_t fWaitFlags, ProcessStatus_T oldStatus, ProcessStatus_T newStatus, uint32_t uProcFlags, uint32_t uProtocol);
    ProcessWaitResult_T waitFlagsToResult(uint32_t fWaitFlags);
    int waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, ProcessWaitResult_T &waitResult, int *pGuestRc);
    int waitForInputNotify(GuestWaitEvent *pEvent, uint32_t uHandle, uint32_t uTimeoutMS, ProcessInputStatus_T *pInputStatus, uint32_t *pcbProcessed);
    int waitForOutput(GuestWaitEvent *pEvent, uint32_t uHandle, uint32_t uTimeoutMS, void* pvData, size_t cbData, uint32_t *pcbRead);
    int waitForStatusChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, ProcessStatus_T *pProcessStatus, int *pGuestRc);
    static bool waitResultImpliesEx(ProcessWaitResult_T waitResult, ProcessStatus_T procStatus, uint32_t uProcFlags, uint32_t uProtocol);
    int writeData(uint32_t uHandle, uint32_t uFlags, void *pvData, size_t cbData, uint32_t uTimeoutMS, uint32_t *puWritten, int *pGuestRc);
    /** @}  */

protected:
    /** @name Protected internal methods.
     * @{ */
    inline bool isAlive(void);
    inline bool hasEnded(void);
    int onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int onProcessInputStatus(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int onProcessNotifyIO(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int onProcessStatusChange(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int onProcessOutput(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int prepareExecuteEnv(const char *pszEnv, void **ppvList, ULONG *pcbList, ULONG *pcEnvVars);
    int setProcessStatus(ProcessStatus_T procStatus, int procRc);
    static DECLCALLBACK(int) startProcessThread(RTTHREAD Thread, void *pvUser);
    /** @}  */

private:

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

    GuestProcessStream &GetStdOut(void) { return mStdOut; }

    GuestProcessStream &GetStdErr(void) { return mStdErr; }

    int Wait(uint32_t fFlags, int *pGuestRc);

    int WaitEx(uint32_t fFlags, GuestProcessStreamBlock *pStreamBlock, int *pGuestRc);

    int GetCurrentBlock(uint32_t uHandle, GuestProcessStreamBlock &strmBlock);

    bool IsRunning(void);

    static int Run(GuestSession *pGuestSession, const GuestProcessStartupInfo &startupInfo, int *pGuestRc);

    static int RunEx(GuestSession *pGuestSession, const GuestProcessStartupInfo &startupInfo, GuestCtrlStreamObjects *pStrmOutObjects,
                     uint32_t cStrmOutObjects, int *pGuestRc);

    int TerminatedOk(LONG *pExitCode);

    int Terminate(uint32_t uTimeoutMS, int *pGuestRc);

protected:

    ComObjPtr<GuestSession>     pSession;
    ComObjPtr<GuestProcess>     pProcess;
    GuestProcessStartupInfo     mStartupInfo;
    GuestProcessStream          mStdOut;
    GuestProcessStream          mStdErr;
};

#endif /* !____H_GUESTPROCESSIMPL */

