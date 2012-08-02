
/* $Id$ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
 * TODO
 */
class ATL_NO_VTABLE GuestProcess :
    public VirtualBoxBase,
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

    int     init(Console *aConsole, GuestSession *aSession, ULONG aProcessID, const GuestProcessInfo &aProcInfo);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IProcess interface.
     * @{ */
    STDMETHOD(COMGETTER(Arguments))(ComSafeArrayOut(BSTR, aArguments));
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(BSTR, aEnvironment));
    STDMETHOD(COMGETTER(ExecutablePath))(BSTR *aExecutablePath);
    STDMETHOD(COMGETTER(ExitCode))(LONG *aExitCode);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(PID))(ULONG *aPID);
    STDMETHOD(COMGETTER(Status))(ProcessStatus_T *aStatus);

    STDMETHOD(Read)(ULONG aHandle, ULONG aSize, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Terminate)(void);
    STDMETHOD(WaitFor)(ULONG aWaitFlags, ULONG aTimeoutMS, ProcessWaitResult_T *aReason);
    STDMETHOD(WaitForArray)(ComSafeArrayIn(ProcessWaitForFlag_T, aFlags), ULONG aTimeoutMS, ProcessWaitResult_T *aReason);
    STDMETHOD(Write)(ULONG aHandle, ULONG aFlags, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    STDMETHOD(WriteArray)(ULONG aHandle, ComSafeArrayIn(ProcessInputFlag_T, aFlags), ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int callbackDispatcher(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData);
    inline bool callbackExists(uint32_t uContextID);
    void close(void);
    bool isReady(void);
    ULONG getPID(void) { return mData.mPID; }
    int readData(uint32_t uHandle, uint32_t uSize, uint32_t uTimeoutMS, void *pvData, size_t cbData, size_t *pcbRead);
    int startProcess(void);
    int startProcessAsync(void);
    int terminateProcess(void);
    int waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, GuestProcessWaitResult &guestResult);
    int writeData(uint32_t uHandle, uint32_t uFlags, void *pvData, size_t cbData, uint32_t uTimeoutMS, uint32_t *puWritten);
    /** @}  */

protected:
    /** @name Protected internal methods.
     * @{ */
    inline int callbackAdd(GuestCtrlCallback *pCallback, uint32_t *puContextID);
    inline int callbackRemove(uint32_t uContextID);
    inline bool isAlive(void);
    int onGuestDisconnected(GuestCtrlCallback *pCallback, PCALLBACKDATACLIENTDISCONNECTED pData);
    int onProcessInputStatus(GuestCtrlCallback *pCallback, PCALLBACKDATAEXECINSTATUS pData);
    int onProcessNotifyIO(GuestCtrlCallback *pCallback, PCALLBACKDATAEXECSTATUS pData);
    int onProcessStatusChange(GuestCtrlCallback *pCallback, PCALLBACKDATAEXECSTATUS pData);
    int onProcessOutput(GuestCtrlCallback *pCallback, PCALLBACKDATAEXECOUT pData);
    int prepareExecuteEnv(const char *pszEnv, void **ppvList, ULONG *pcbList, ULONG *pcEnvVars);
    int sendCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms);
    int setErrorInternal(int rc, const Utf8Str &strMessage);
    int setErrorExternal(void);
    int signalWaiters(ProcessWaitResult_T enmWaitResult);
    static DECLCALLBACK(int) startProcessThread(RTTHREAD Thread, void *pvUser);
    HRESULT waitResultToErrorEx(const GuestProcessWaitResult &waitResult, bool fLog);
    /** @}  */

private:

    struct Data
    {
        /** Pointer to parent session. */
        GuestSession            *mParent;
        /** Pointer to the console object. Needed
         *  for HGCM (VMMDev) communication. */
        Console                 *mConsole;
        /** All related callbacks to this process. */
        GuestCtrlCallbacks       mCallbacks;
        /** The process start information. */
        GuestProcessInfo         mProcess;
        /** Exit code if process has been terminated. */
        LONG                     mExitCode;
        /** PID reported from the guest. */
        ULONG                    mPID;
        /** Internal, host-side process ID. */
        ULONG                    mProcessID;
        /** The current process status. */
        ProcessStatus_T          mStatus;
        /** The overall rc of the process execution. */
        int                      mRC;
        /** The overall error message of the
         *  process execution. */
        Utf8Str                  mErrorMsg;
        /** The next upcoming context ID. */
        ULONG                    mNextContextID;
        /** The mutex for protecting the waiter(s). */
        RTSEMMUTEX               mWaitMutex;
        /** How many waiters? At the moment there can only
         *  be one. */
        uint32_t                 mWaitCount;
        /** The actual process event for doing the waits.
         *  At the moment we only support one wait a time. */
        GuestProcessEvent       *mWaitEvent;
    } mData;
};

#endif /* !____H_GUESTPROCESSIMPL */

