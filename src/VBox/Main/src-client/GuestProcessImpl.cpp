
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "GuestProcessImpl.h"
#include "GuestSessionImpl.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "VMMDev.h"

#include <iprt/asm.h>
#include <iprt/getopt.h>
#include <VBox/VMMDev.h>
#include <VBox/com/array.h>


struct GuestProcessTask
{
    GuestProcessTask(GuestProcess *pProcess)
        : mProcess(pProcess) { }

    ~GuestProcessTask(void) { }

    int rc() const { return mRC; }
    bool isOk() const { return RT_SUCCESS(rc()); }

    const ComObjPtr<GuestProcess>    mProcess;

private:
    int                              mRC;
};

struct GuestProcessStartTask : public GuestProcessTask
{
    GuestProcessStartTask(GuestProcess *pProcess)
        : GuestProcessTask(pProcess) { }
};


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestProcess)

HRESULT GuestProcess::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));

    mData.mExitCode = 0;
    mData.mNextContextID = 0;
    mData.mPID = 0;
    mData.mProcessID = 0;
    mData.mStatus = ProcessStatus_Undefined;
    mData.mStarted = false;

    return BaseFinalConstruct();
}

void GuestProcess::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestProcess::init(Console *aConsole, GuestSession *aSession, uint32_t aProcessID, const GuestProcessInfo &aProcInfo)
{
    AssertPtrReturn(aSession, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mConsole = aConsole;
    mData.mParent = aSession;
    mData.mProcessID = aProcessID;
    mData.mStatus = ProcessStatus_Starting;
    /* Everything else will be set by the actual starting routine. */

    /* Asynchronously start the process on the guest by kicking off a
     * worker thread. */
    std::auto_ptr<GuestProcessStartTask> pTask(new GuestProcessStartTask(this));
    AssertReturn(pTask->isOk(), pTask->rc());

    int rc = RTThreadCreate(NULL, GuestProcess::startProcessThread,
                            (void *)pTask.get(), 0,
                            RTTHREADTYPE_MAIN_WORKER, 0,
                            "gctlPrcStart");
    if (RT_SUCCESS(rc))
    {
        /* task is now owned by startProcessThread(), so release it. */
        pTask.release();

        /* Confirm a successful initialization when it's the case. */
        autoInitSpan.setSucceeded();
    }

    return rc;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestProcess::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestProcess::COMGETTER(Arguments)(ComSafeArrayOut(BSTR, aArguments))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutSafeArrayPointerValid(aArguments);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> collection(mData.mProcess.mArguments.size());
    size_t s = 0;
    for (ProcessArguments::const_iterator it = mData.mProcess.mArguments.begin();
         it != mData.mProcess.mArguments.end();
         ++it, ++s)
    {
        collection[s] = Bstr((*it)).raw();
    }

    collection.detachTo(ComSafeArrayOutArg(aArguments));

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::COMGETTER(Environment)(ComSafeArrayOut(BSTR, aEnvironment))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutSafeArrayPointerValid(aEnvironment);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> collection(mData.mProcess.mEnvironment.size());
    size_t s = 0;
    for (ProcessEnvironmentMap::const_iterator it = mData.mProcess.mEnvironment.begin();
         it != mData.mProcess.mEnvironment.end();
         ++it, ++s)
    {
        collection[s] = Bstr(it->first + "=" + it->second).raw();
    }

    collection.detachTo(ComSafeArrayOutArg(aEnvironment));

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::COMGETTER(ExecutablePath)(BSTR *aExecutablePath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mProcess.mCommand.cloneTo(aExecutablePath);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::COMGETTER(ExitCode)(LONG *aExitCode)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aExitCode = mData.mExitCode;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::COMGETTER(Pid)(ULONG *aPID)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPID = mData.mPID;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::COMGETTER(Status)(ProcessStatus_T *aStatus)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aStatus = mData.mStatus;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/*

    SYNC TO ASK:
    Everything which involves HGCM communication (start, read/write/status(?)/...)
    either can be called synchronously or asynchronously by running in a Main worker
    thread.

    Rules:
        - Only one async operation per process a time can be around.

*/

int GuestProcess::callbackAdd(const GuestCtrlCallback& theCallback, uint32_t *puContextID)
{
    const ComObjPtr<GuestSession> pSession(mData.mParent);
    Assert(!pSession.isNull());
    ULONG uSessionID = 0;
    HRESULT hr = pSession->COMGETTER(Id)(&uSessionID);
    ComAssertComRC(hr);

    /* Create a new context ID and assign it. */
    int rc = VERR_NOT_FOUND;
    uint32_t uNewContextID = 0;
    uint32_t uTries = 0;
    for (;;)
    {
        /* Create a new context ID ... */
        uNewContextID = VBOX_GUESTCTRL_CONTEXTID_MAKE(uSessionID,
                                                      mData.mProcessID, ASMAtomicIncU32(&mData.mNextContextID));
        if (uNewContextID == UINT32_MAX)
            ASMAtomicUoWriteU32(&mData.mNextContextID, 0);
        /* Is the context ID already used?  Try next ID ... */
        if (!callbackExists(uNewContextID))
        {
            /* Callback with context ID was not found. This means
             * we can use this context ID for our new callback we want
             * to add below. */
            rc = VINF_SUCCESS;
            break;
        }

        if (++uTries == UINT32_MAX)
            break; /* Don't try too hard. */
    }

    if (RT_SUCCESS(rc))
    {
        /* Add callback with new context ID to our callback map. */
        mData.mCallbacks[uNewContextID] = theCallback;
        Assert(mData.mCallbacks.size());

        /* Report back new context ID. */
        if (puContextID)
            *puContextID = uNewContextID;
    }

    return rc;
}

bool GuestProcess::callbackExists(uint32_t uContextID)
{
    AssertReturn(uContextID, false);

    GuestCtrlCallbacks::const_iterator it = mData.mCallbacks.find(uContextID);
    return (it == mData.mCallbacks.end()) ? false : true;
}

bool GuestProcess::isReady(void)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData.mStatus == ProcessStatus_Started)
    {
        Assert(mData.mPID); /* PID must not be 0. */
        return true;
    }

    return false;
}

/**
 * Appends environment variables to the environment block.
 *
 * Each var=value pair is separated by the null character ('\\0').  The whole
 * block will be stored in one blob and disassembled on the guest side later to
 * fit into the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @param   pszEnvVar       The environment variable=value to append to the
 *                          environment block.
 * @param   ppvList         This is actually a pointer to a char pointer
 *                          variable which keeps track of the environment block
 *                          that we're constructing.
 * @param   pcbList         Pointer to the variable holding the current size of
 *                          the environment block.  (List is a misnomer, go
 *                          ahead a be confused.)
 * @param   pcEnvVars       Pointer to the variable holding count of variables
 *                          stored in the environment block.
 */
int GuestProcess::prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnvVars)
{
    int rc = VINF_SUCCESS;
    uint32_t cchEnv = strlen(pszEnv); Assert(cchEnv >= 2);
    if (*ppvList)
    {
        uint32_t cbNewLen = *pcbList + cchEnv + 1; /* Include zero termination. */
        char *pvTmp = (char *)RTMemRealloc(*ppvList, cbNewLen);
        if (pvTmp == NULL)
            rc = VERR_NO_MEMORY;
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cchEnv);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void **)pvTmp;
        }
    }
    else
    {
        char *pszTmp;
        if (RTStrAPrintf(&pszTmp, "%s", pszEnv) >= 0)
        {
            *ppvList = (void **)pszTmp;
            /* Reset counters. */
            *pcEnvVars = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += cchEnv + 1; /* Include zero termination. */
        *pcEnvVars += 1;        /* Increase env variable count. */
    }
    return rc;
}

int GuestProcess::readData(ULONG aHandle, ULONG aSize, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData))
{
    LogFlowFuncEnter();

    LogFlowFuncLeave();
    return 0;
}

int GuestProcess::startProcess(void)
{
    LogFlowFuncEnter();

    AssertReturn(!mData.mStarted, VERR_ALREADY_EXISTS);

    int rc;
    uint32_t uContextID;

    {
        /* Wait until the caller function (if kicked off by a thread)
         * has returned and continue operation. */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Create callback and add it to the map. */
        GuestCtrlCallback callbackStart;
        rc = callbackStart.Init(VBOXGUESTCTRLCALLBACKTYPE_EXEC_START);
        if (RT_FAILURE(rc))
            return rc;

        rc = callbackAdd(callbackStart, &uContextID);
        Assert(uContextID);
    }

    if (RT_SUCCESS(rc))
    {
        ComObjPtr<GuestSession> pSession(mData.mParent);
        Assert(!pSession.isNull());

        const GuestCredentials &sessionCreds = pSession->getCredentials();

        /* Prepare arguments. */
        char *pszArgs = NULL;
        size_t cArgs = mData.mProcess.mArguments.size();
        if (cArgs)
        {
            char **papszArgv = (char**)RTMemAlloc(sizeof(char*) * (cArgs + 1));
            AssertReturn(papszArgv, VERR_NO_MEMORY);
            for (size_t i = 0; RT_SUCCESS(rc) && i < cArgs; i++)
                rc = RTStrDupEx(&papszArgv[i], mData.mProcess.mArguments[i].c_str());
            papszArgv[cArgs] = NULL;

            if (RT_SUCCESS(rc))
                rc = RTGetOptArgvToString(&pszArgs, papszArgv, RTGETOPTARGV_CNV_QUOTE_MS_CRT);
        }
        uint32_t cbArgs = pszArgs ? strlen(pszArgs) + 1 : 0; /* Include terminating zero. */

        /* Prepare environment. */
        void *pvEnv = NULL;
        size_t cEnv = mData.mProcess.mEnvironment.size();
        uint32_t cbEnv = 0;
        if (   RT_SUCCESS(rc)
            && cEnv)
        {
            uint32_t cEnvBuild = 0;
            ProcessEnvironmentMap::const_iterator itEnv = mData.mProcess.mEnvironment.begin();
            for (; itEnv != mData.mProcess.mEnvironment.end() && RT_SUCCESS(rc); itEnv++)
            {
                char *pszEnv;
                if (!RTStrAPrintf(&pszEnv, "%s=%s", itEnv->first, itEnv->second))
                    break;
                AssertPtr(pszEnv);
                rc = prepareExecuteEnv(pszEnv, &pvEnv, &cbEnv, &cEnvBuild);
                RTStrFree(pszEnv);
            }
            Assert(cEnv == cEnvBuild);
        }

        if (RT_SUCCESS(rc))
        {
            /* Prepare HGCM call. */
            VBOXHGCMSVCPARM paParms[15];
            int i = 0;
            paParms[i++].setUInt32(uContextID);
            paParms[i++].setPointer((void*)mData.mProcess.mCommand.c_str(),
                                    (uint32_t)mData.mProcess.mCommand.length() + 1);
            paParms[i++].setUInt32(mData.mProcess.mFlags);
            paParms[i++].setUInt32(mData.mProcess.mArguments.size());
            paParms[i++].setPointer((void*)pszArgs, cbArgs);
            paParms[i++].setUInt32(mData.mProcess.mEnvironment.size());
            paParms[i++].setUInt32(cbEnv);
            paParms[i++].setPointer((void*)pvEnv, cbEnv);
            paParms[i++].setPointer((void*)sessionCreds.mUser.c_str(), (uint32_t)sessionCreds.mUser.length() + 1);
            paParms[i++].setPointer((void*)sessionCreds.mPassword.c_str(), (uint32_t)sessionCreds.mPassword.length() + 1);

            /*
             * If the WaitForProcessStartOnly flag is set, we only want to define and wait for a timeout
             * until the process was started - the process itself then gets an infinite timeout for execution.
             * This is handy when we want to start a process inside a worker thread within a certain timeout
             * but let the started process perform lengthly operations then.
             */
            if (mData.mProcess.mFlags & ProcessCreateFlag_WaitForProcessStartOnly)
                paParms[i++].setUInt32(UINT32_MAX /* Infinite timeout */);
            else
                paParms[i++].setUInt32(mData.mProcess.mTimeoutMS);

            const ComObjPtr<Console> pConsole(mData.mConsole);
            Assert(!pConsole.isNull());

            VMMDev *pVMMDev = NULL;
            {
                /* Make sure mParent is valid, so set the read lock while using.
                 * Do not keep this lock while doing the actual call, because in the meanwhile
                 * another thread could request a write lock which would be a bad idea ... */
                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                /* Forward the information to the VMM device. */
                pVMMDev = pConsole->getVMMDev();
            }

            LogFlowFunc(("hgcmHostCall numParms=%d, CID=%RU32\n", i, uContextID));
            rc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_CMD,
                                       i, paParms);
        }

        if (pvEnv)
            RTMemFree(pvEnv);
        if (pszArgs)
            RTStrFree(pszArgs);
    }

    LogFlowFuncLeave();
    return rc;
}

DECLCALLBACK(int) GuestProcess::startProcessThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    const ComObjPtr<GuestProcess> pProcess = static_cast<GuestProcess*>(pvUser);
    Assert(!pProcess.isNull());

    int rc = pProcess->startProcess();

    LogFlowFuncLeave();
    return rc;
}

int GuestProcess::terminateProcess(void)
{
    LogFlowFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowFuncLeave();
    return 0;
}

int GuestProcess::waitFor(ComSafeArrayIn(ProcessWaitForFlag_T, aFlags), ULONG aTimeoutMS, ProcessWaitReason_T *aReason)
{
    LogFlowFuncEnter();

    LogFlowFuncLeave();
    return 0;
}

int GuestProcess::writeData(ULONG aHandle, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten)
{
    LogFlowFuncEnter();

    LogFlowFuncLeave();
    return 0;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestProcess::Read(ULONG aHandle, ULONG aSize, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = readData(aHandle, aSize, aTimeoutMS, ComSafeArrayOutArg(aData));
    /** @todo Do setError() here. */
    return RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::Terminate(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = terminateProcess();
    /** @todo Do setError() here. */
    return RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::WaitFor(ComSafeArrayIn(ProcessWaitForFlag_T, aFlags), ULONG aTimeoutMS, ProcessWaitReason_T *aReason)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = waitFor(ComSafeArrayInArg(aFlags), aTimeoutMS, aReason);
    /** @todo Do setError() here. */
    return RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestProcess::Write(ULONG aHandle, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = writeData(aHandle, ComSafeArrayInArg(aData), aTimeoutMS, aWritten);
    /** @todo Do setError() here. */
    return RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

