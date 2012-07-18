
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
    STDMETHOD(COMGETTER(Pid))(ULONG *aPID);
    STDMETHOD(COMGETTER(Status))(ProcessStatus_T *aStatus);

    STDMETHOD(Read)(ULONG aHandle, ULONG aSize, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Terminate)(void);
    STDMETHOD(WaitFor)(ComSafeArrayIn(ProcessWaitForFlag_T, aFlags), ULONG aTimeoutMS, ProcessWaitReason_T *aReason);
    STDMETHOD(Write)(ULONG aHandle, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int callbackAdd(GuestCtrlCallback *pCallback, ULONG *puContextID);
    int callbackDispatcher(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData);
    bool callbackExists(ULONG uContextID);
    bool isReady(void);
    ULONG getPID(void) { return mData.mPID; }
    int onGuestDisconnected(void);
    int onProcessInputStatus(uint32_t uStatus, uint32_t uFlags, uint32_t cbDataProcessed);
    int onProcessStatusChange(uint32_t uStatus, uint32_t uFlags, uint32_t uPID);
    int onProcessOutput(uint32_t uHandle, uint32_t uFlags, void *pvData, uint32_t cbData);
    int prepareExecuteEnv(const char *pszEnv, void **ppvList, ULONG *pcbList, ULONG *pcEnvVars);
    int readData(ULONG uHandle, ULONG uSize, ULONG uTimeoutMS, BYTE *pbData, size_t cbData);
    int startProcess(void);
    static DECLCALLBACK(int) startProcessThread(RTTHREAD Thread, void *pvUser);
    int terminateProcess(void);
    int waitFor(uint32_t fFlags, ULONG uTimeoutMS, ProcessWaitReason_T *penmReason);
    int writeData(ULONG uHandle, BYTE const *pbData, size_t cbData, ULONG uTimeoutMS, ULONG *puWritten);
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
        ULONG                 mProcessID;
        /** The current process status. */
        ProcessStatus_T          mStatus;
        /** Flag indicating whether the process has been started. */
        bool                     mStarted;
        /** The next upcoming context ID. */
        ULONG                    mNextContextID;
        /** The waiting event. */
        RTSEMEVENT               mEvent;
    } mData;

    friend GuestSession; /* Let's be friends! */
};

#endif /* !____H_GUESTPROCESSIMPL */

