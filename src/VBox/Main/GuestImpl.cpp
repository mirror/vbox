/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "GuestImpl.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/com/array.h>
#endif
#include <iprt/cpp/utils.h>
#include <iprt/getopt.h>
#include <VBox/pgm.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (Guest)

HRESULT Guest::FinalConstruct()
{
    return S_OK;
}

void Guest::FinalRelease()
{
    uninit ();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest object.
 */
HRESULT Guest::init (Console *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    /* mData.mAdditionsActive is FALSE */

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    ULONG aMemoryBalloonSize;
    HRESULT ret = mParent->machine()->COMGETTER(MemoryBalloonSize)(&aMemoryBalloonSize);
    if (ret == S_OK)
        mMemoryBalloonSize = aMemoryBalloonSize;
    else
        mMemoryBalloonSize = 0;                     /* Default is no ballooning */

    mStatUpdateInterval = 0;                    /* Default is not to report guest statistics at all */

    /* Clear statistics. */
    for (unsigned i = 0 ; i < GUESTSTATTYPE_MAX; i++)
        mCurrentGuestStat[i] = 0;

#ifdef VBOX_WITH_GUEST_CONTROL
    /* Init the context ID counter at 1000. */
    mNextContextID = 1000;
#endif

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_CONTROL
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Clean up callback data. */
    CallbackListIter it;
    for (it = mCallbackList.begin(); it != mCallbackList.end(); it++)
        removeCtrlCallbackContext(it);
#endif

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent) = NULL;
}

// IGuest properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Guest::COMGETTER(OSTypeId) (BSTR *aOSTypeId)
{
    CheckComArgOutPointerValid(aOSTypeId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    // redirect the call to IMachine if no additions are installed
    if (mData.mAdditionsVersion.isEmpty())
        return mParent->machine()->COMGETTER(OSTypeId)(aOSTypeId);

    mData.mOSTypeId.cloneTo(aOSTypeId);

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsActive) (BOOL *aAdditionsActive)
{
    CheckComArgOutPointerValid(aAdditionsActive);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAdditionsActive = mData.mAdditionsActive;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsVersion) (BSTR *aAdditionsVersion)
{
    CheckComArgOutPointerValid(aAdditionsVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mAdditionsVersion.cloneTo(aAdditionsVersion);

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(SupportsSeamless) (BOOL *aSupportsSeamless)
{
    CheckComArgOutPointerValid(aSupportsSeamless);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSupportsSeamless = mData.mSupportsSeamless;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(SupportsGraphics) (BOOL *aSupportsGraphics)
{
    CheckComArgOutPointerValid(aSupportsGraphics);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSupportsGraphics = mData.mSupportsGraphics;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(MemoryBalloonSize) (ULONG *aMemoryBalloonSize)
{
    CheckComArgOutPointerValid(aMemoryBalloonSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMemoryBalloonSize = mMemoryBalloonSize;

    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(MemoryBalloonSize) (ULONG aMemoryBalloonSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* We must be 100% sure that IMachine::COMSETTER(MemoryBalloonSize)
     * does not call us back in any way! */
    HRESULT ret = mParent->machine()->COMSETTER(MemoryBalloonSize)(aMemoryBalloonSize);
    if (ret == S_OK)
    {
        mMemoryBalloonSize = aMemoryBalloonSize;
        /* forward the information to the VMM device */
        VMMDev *vmmDev = mParent->getVMMDev();
        if (vmmDev)
            vmmDev->getVMMDevPort()->pfnSetMemoryBalloon(vmmDev->getVMMDevPort(), aMemoryBalloonSize);
    }

    return ret;
}

STDMETHODIMP Guest::COMGETTER(StatisticsUpdateInterval)(ULONG *aUpdateInterval)
{
    CheckComArgOutPointerValid(aUpdateInterval);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aUpdateInterval = mStatUpdateInterval;
    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(StatisticsUpdateInterval)(ULONG aUpdateInterval)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mStatUpdateInterval = aUpdateInterval;
    /* forward the information to the VMM device */
    VMMDev *vmmDev = mParent->getVMMDev();
    if (vmmDev)
        vmmDev->getVMMDevPort()->pfnSetStatisticsInterval(vmmDev->getVMMDevPort(), aUpdateInterval);

    return S_OK;
}

STDMETHODIMP Guest::InternalGetStatistics(ULONG *aCpuUser, ULONG *aCpuKernel, ULONG *aCpuIdle,
                                          ULONG *aMemTotal, ULONG *aMemFree, ULONG *aMemBalloon,
                                          ULONG *aMemCache, ULONG *aPageTotal,
                                          ULONG *aMemAllocTotal, ULONG *aMemFreeTotal, ULONG *aMemBalloonTotal)
{
    CheckComArgOutPointerValid(aCpuUser);
    CheckComArgOutPointerValid(aCpuKernel);
    CheckComArgOutPointerValid(aCpuIdle);
    CheckComArgOutPointerValid(aMemTotal);
    CheckComArgOutPointerValid(aMemFree);
    CheckComArgOutPointerValid(aMemBalloon);
    CheckComArgOutPointerValid(aMemCache);
    CheckComArgOutPointerValid(aPageTotal);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCpuUser = mCurrentGuestStat[GUESTSTATTYPE_CPUUSER];
    *aCpuKernel = mCurrentGuestStat[GUESTSTATTYPE_CPUKERNEL];
    *aCpuIdle = mCurrentGuestStat[GUESTSTATTYPE_CPUIDLE];
    *aMemTotal = mCurrentGuestStat[GUESTSTATTYPE_MEMTOTAL] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aMemFree = mCurrentGuestStat[GUESTSTATTYPE_MEMFREE] * (_4K/_1K);       /* page (4K) -> 1KB units */
    *aMemBalloon = mCurrentGuestStat[GUESTSTATTYPE_MEMBALLOON] * (_4K/_1K); /* page (4K) -> 1KB units */
    *aMemCache = mCurrentGuestStat[GUESTSTATTYPE_MEMCACHE] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aPageTotal = mCurrentGuestStat[GUESTSTATTYPE_PAGETOTAL] * (_4K/_1K);   /* page (4K) -> 1KB units */

    Console::SafeVMPtr pVM (mParent);
    if (pVM.isOk())
    {
        uint64_t uFreeTotal, uAllocTotal, uBalloonedTotal;
        *aMemFreeTotal = 0;
        int rc = PGMR3QueryVMMMemoryStats(pVM.raw(), &uAllocTotal, &uFreeTotal, &uBalloonedTotal);
        AssertRC(rc);
        if (rc == VINF_SUCCESS)
        {
            *aMemAllocTotal   = (ULONG)(uAllocTotal / _1K);  /* bytes -> KB */
            *aMemFreeTotal    = (ULONG)(uFreeTotal / _1K);
            *aMemBalloonTotal = (ULONG)(uBalloonedTotal / _1K);
        }
    }
    else
        *aMemFreeTotal = 0;

    return S_OK;
}

HRESULT Guest::SetStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (enmType >= GUESTSTATTYPE_MAX)
        return E_INVALIDARG;

    mCurrentGuestStat[enmType] = aVal;
    return S_OK;
}

STDMETHODIMP Guest::SetCredentials(IN_BSTR aUserName, IN_BSTR aPassword,
                                   IN_BSTR aDomain, BOOL aAllowInteractiveLogon)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* forward the information to the VMM device */
    VMMDev *vmmDev = mParent->getVMMDev();
    if (vmmDev)
    {
        uint32_t u32Flags = VMMDEV_SETCREDENTIALS_GUESTLOGON;
        if (!aAllowInteractiveLogon)
            u32Flags = VMMDEV_SETCREDENTIALS_NOLOCALLOGON;

        vmmDev->getVMMDevPort()->pfnSetCredentials(vmmDev->getVMMDevPort(),
            Utf8Str(aUserName).raw(), Utf8Str(aPassword).raw(),
            Utf8Str(aDomain).raw(), u32Flags);
        return S_OK;
    }

    return setError(VBOX_E_VM_ERROR,
                    tr("VMM device is not available (is the VM running?)"));
}

#ifdef VBOX_WITH_GUEST_CONTROL
/**
 * Creates the argument list as an array used for executing a program.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 * @todo Respect spaces when quoting for arguments, e.g. "c:\\program files\\".
 * @todo Handle empty ("") argguments.
 */
int Guest::prepareExecuteArgs(const char *pszArgs, void **ppvList, uint32_t *pcbList, uint32_t *pcArgs)
{
    char **ppaArg;
    int iArgs;
    int rc = RTGetOptArgvFromString(&ppaArg, &iArgs, pszArgs, NULL);
    if (RT_SUCCESS(rc))
    {
        char *pszTemp = NULL;
        *pcbList = 0;
        for (int i=0; i<iArgs; i++)
        {
            if (i > 0) /* Insert space as delimiter. */
                rc = RTStrAAppendN(&pszTemp, " ", 1);

            if (RT_FAILURE(rc))
                break;
            else
            {
                rc = RTStrAAppendN(&pszTemp, ppaArg[i], strlen(ppaArg[i]));
                if (RT_FAILURE(rc))
                    break;
            }
        }
        RTGetOptArgvFree(ppaArg);
        if (RT_SUCCESS(rc))
        {
            *ppvList = pszTemp;
            *pcArgs = iArgs;
            if (pszTemp)
                *pcbList = strlen(pszTemp) + 1; /* Include zero termination. */
        }
        else
            RTStrFree(pszTemp);
    }
    return rc;
}

/**
 * Appends environment variables to the environment block. Each var=value pair is separated
 * by NULL (\0) sequence. The whole block will be stored in one blob and disassembled on the
 * guest side later to fit into the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 */
int Guest::prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnv)
{
    int rc = VINF_SUCCESS;
    uint32_t cbLen = strlen(pszEnv);
    if (*ppvList)
    {
        uint32_t cbNewLen = *pcbList + cbLen + 1; /* Include zero termination. */
        char *pvTmp = (char*)RTMemRealloc(*ppvList, cbNewLen);
        if (NULL == pvTmp)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cbLen);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void**)pvTmp;
        }
    }
    else
    {
        char *pcTmp;
        if (RTStrAPrintf(&pcTmp, "%s", pszEnv) > 0)
        {
            *ppvList = (void**)pcTmp;
            /* Reset counters. */
            *pcEnv = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += cbLen + 1; /* Include zero termination. */
        *pcEnv += 1;           /* Increase env pairs count. */
    }
    return rc;
}

/**
 * Static callback function for receiving updates on guest control commands
 * from the guest. Acts as a dispatcher for the actual class instance.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 */
DECLCALLBACK(int) Guest::doGuestCtrlNotification(void    *pvExtension,
                                                 uint32_t u32Function,
                                                 void    *pvParms,
                                                 uint32_t cbParms)
{
    using namespace guestControl;

    /*
     * No locking, as this is purely a notification which does not make any
     * changes to the object state.
     */
    LogFlowFunc(("pvExtension = %p, u32Function = %d, pvParms = %p, cbParms = %d\n",
                 pvExtension, u32Function, pvParms, cbParms));
    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);

    int rc = VINF_SUCCESS;
    if (u32Function == GUEST_EXEC_SEND_STATUS)
    {
        LogFlowFunc(("GUEST_EXEC_SEND_STATUS\n"));

        PHOSTEXECCALLBACKDATA pCBData = reinterpret_cast<PHOSTEXECCALLBACKDATA>(pvParms);
        AssertPtr(pCBData);
        AssertReturn(sizeof(HOSTEXECCALLBACKDATA) == cbParms, VERR_INVALID_PARAMETER);
        AssertReturn(HOSTEXECCALLBACKDATAMAGIC == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

        rc = pGuest->notifyCtrlExec(u32Function, pCBData);
    }
    else if (u32Function == GUEST_EXEC_SEND_OUTPUT)
    {
        LogFlowFunc(("GUEST_EXEC_SEND_OUTPUT\n"));

        PHOSTEXECOUTCALLBACKDATA pCBData = reinterpret_cast<PHOSTEXECOUTCALLBACKDATA>(pvParms);
        AssertPtr(pCBData);
        AssertReturn(sizeof(HOSTEXECOUTCALLBACKDATA) == cbParms, VERR_INVALID_PARAMETER);
        AssertReturn(HOSTEXECOUTCALLBACKDATAMAGIC == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

        rc = pGuest->notifyCtrlExecOut(u32Function, pCBData);
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}

/* Function for handling the execution start/termination notification. */
int Guest::notifyCtrlExec(uint32_t              u32Function,
                          PHOSTEXECCALLBACKDATA pData)
{
    LogFlowFuncEnter();
    int rc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(pData);
    CallbackListIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);
    if (it != mCallbackList.end())
    {
        Assert(!it->bCalled);
        PHOSTEXECCALLBACKDATA pCBData = (HOSTEXECCALLBACKDATA*)it->pvData;
        AssertPtr(pCBData);

        pCBData->u32PID = pData->u32PID;
        pCBData->u32Status = pData->u32Status;
        pCBData->u32Flags = pData->u32Flags;
        /** @todo Copy void* buffer contents! */

        /* Do progress handling. */
        Utf8Str errMsg;
        HRESULT rc2 = S_OK;
        switch (pData->u32Status)
        {
            case PROC_STS_STARTED:
                break;

            case PROC_STS_TEN: /* Terminated normally. */
                it->pProgress->notifyComplete(S_OK);
                break;

            case PROC_STS_TEA: /* Terminated abnormally. */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated abnormally with status '%u'"),
                                    pCBData->u32Flags);
                break;

            case PROC_STS_TES: /* Terminated through signal. */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated via signal with status '%u'"),
                                    pCBData->u32Flags);
                break;

            case PROC_STS_TOK:
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and was killed"));
                break;

            case PROC_STS_TOA:
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and could not be killed"));
                break;

            case PROC_STS_DWN:
                errMsg = Utf8StrFmt(Guest::tr("Process exited because system is shutting down"));
                break;

            default:
                break;
        }

        if (   !it->pProgress.isNull()
            && errMsg.length())
        {
            it->pProgress->notifyComplete(E_FAIL /** @todo Find a better rc! */, COM_IIDOF(IGuest),
                                          (CBSTR)Guest::getComponentName(), errMsg.c_str());
        }
        ASMAtomicWriteBool(&it->bCalled, true);
    }
    else
        LogFlowFunc(("Unexpected callback (magic=%u, context ID=%u) arrived\n", pData->hdr.u32Magic, pData->hdr.u32ContextID));
    LogFlowFuncLeave();
    return rc;
}

/* Function for handling the execution output notification. */
int Guest::notifyCtrlExecOut(uint32_t                 u32Function,
                             PHOSTEXECOUTCALLBACKDATA pData)
{
    LogFlowFuncEnter();
    int rc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(pData);
    CallbackListIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);
    if (it != mCallbackList.end())
    {
        Assert(!it->bCalled);
        PHOSTEXECOUTCALLBACKDATA pCBData = (HOSTEXECOUTCALLBACKDATA*)it->pvData;
        AssertPtr(pCBData);

        pCBData->u32PID = pData->u32PID;
        pCBData->u32HandleId = pData->u32HandleId;
        pCBData->u32Flags = pData->u32Flags;

        /* Allocate data buffer and copy it */
        if (pData->cbData && pData->pvData)
        {
            pCBData->pvData = RTMemAlloc(pData->cbData);
            AssertPtr(pCBData->pvData);
            memcpy(pCBData->pvData, pData->pvData, pData->cbData);
            pCBData->cbData = pData->cbData;
        }
        ASMAtomicWriteBool(&it->bCalled, true);
    }
    else
        LogFlowFunc(("Unexpected callback (magic=%u, context ID=%u) arrived\n", pData->hdr.u32Magic, pData->hdr.u32ContextID));
    LogFlowFuncLeave();
    return rc;
}

Guest::CallbackListIter Guest::getCtrlCallbackContextByID(uint32_t u32ContextID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Maybe use a map instead of list for fast context lookup. */
    CallbackListIter it;
    for (it = mCallbackList.begin(); it != mCallbackList.end(); it++)
    {
        if (it->mContextID == u32ContextID)
            return (it);
    }
    return it;
}

Guest::CallbackListIter Guest::getCtrlCallbackContextByPID(uint32_t u32PID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Maybe use a map instead of list for fast context lookup. */
    CallbackListIter it;
    for (it = mCallbackList.begin(); it != mCallbackList.end(); it++)
    {
        PHOSTEXECCALLBACKDATA pCBData = (HOSTEXECCALLBACKDATA*)it->pvData;
        if (pCBData && pCBData->u32PID == u32PID)
            return (it);
    }
    return it;
}

void Guest::removeCtrlCallbackContext(Guest::CallbackListIter it)
{
    LogFlowFuncEnter();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (it->cbData)
    {
        RTMemFree(it->pvData);
        it->cbData = 0;
        it->pvData = NULL;

        /* Notify outstanding waits for progress ... */
        if (!it->pProgress.isNull())
        {
            it->pProgress->notifyComplete(S_OK);
            it->pProgress = NULL;
        }
    }
    LogFlowFuncLeave();
}

/* Adds a callback with a user provided data block and an optional progress object
 * to the callback list. A callback is identified by a unique context ID which is used
 * to identify a callback from the guest side. */
uint32_t Guest::addCtrlCallbackContext(void *pvData, uint32_t cbData, Progress *pProgress)
{
    LogFlowFuncEnter();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    uint32_t uNewContext = ASMAtomicIncU32(&mNextContextID);
    if (uNewContext == UINT32_MAX)
        ASMAtomicUoWriteU32(&mNextContextID, 1000);

    CallbackContext context;
    context.mContextID = uNewContext;
    context.bCalled = false;
    context.pvData = pvData;
    context.cbData = cbData;
    context.pProgress = pProgress;

    mCallbackList.push_back(context);
    if (mCallbackList.size() > 256) /* Don't let the container size get too big! */
    {
        Guest::CallbackListIter it = mCallbackList.begin();
        removeCtrlCallbackContext(it);
        mCallbackList.erase(it);
    }

    LogFlowFuncLeave();
    return uNewContext;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::ExecuteProcess(IN_BSTR aCommand, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aStdIn, IN_BSTR aStdOut, IN_BSTR aStdErr,
                                   IN_BSTR aUserName, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aCommand);
    CheckComArgOutPointerValid(aPID);
    CheckComArgOutPointerValid(aProgress);
    if (aFlags != 0) /* Flags are not supported at the moment. */
        return E_INVALIDARG;

    HRESULT rc = S_OK;

    try
    {
        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        /*
         * Create progress object.
         */
        ComObjPtr <Progress> progress;
        rc = progress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = progress->init(static_cast<IGuest*>(this),
                                BstrFmt(tr("Executing process")),
                                FALSE);
        }
        if (FAILED(rc)) return rc;

        /*
         * Prepare process execution.
         */
        int vrc = VINF_SUCCESS;
        Utf8Str Utf8Command(aCommand);

        /* Adjust timeout */
        if (aTimeoutMS == 0)
            aTimeoutMS = UINT32_MAX;

        /* Prepare arguments. */
        char **papszArgv = NULL;
        uint32_t uNumArgs = 0;
        if (aArguments > 0)
        {
            com::SafeArray<IN_BSTR> args(ComSafeArrayInArg(aArguments));
            uNumArgs = args.size();
            papszArgv = (char**)RTMemAlloc(sizeof(char*) * (uNumArgs + 1));
            AssertPtr(papszArgv);
            for (unsigned i = 0; RT_SUCCESS(vrc) && i < uNumArgs; i++)
                vrc = RTStrAPrintf(&papszArgv[i], "%s", Utf8Str(args[i]).raw());
            papszArgv[uNumArgs] = NULL;
        }

        Utf8Str Utf8StdIn(aStdIn);
        Utf8Str Utf8StdOut(aStdOut);
        Utf8Str Utf8StdErr(aStdErr);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);
        if (RT_SUCCESS(vrc))
        {
            uint32_t uContextID = 0;

            char *pszArgs = NULL;
            if (uNumArgs > 0)
                vrc = RTGetOptArgvToString(&pszArgs, papszArgv, 0);
            if (RT_SUCCESS(vrc))
            {
                uint32_t cbArgs = pszArgs ? strlen(pszArgs) + 1 : 0; /* Include terminating zero. */

                /* Prepare environment. */
                void *pvEnv = NULL;
                uint32_t uNumEnv = 0;
                uint32_t cbEnv = 0;
                if (aEnvironment > 0)
                {
                    com::SafeArray<IN_BSTR> env(ComSafeArrayInArg(aEnvironment));

                    for (unsigned i = 0; i < env.size(); i++)
                    {
                        vrc = prepareExecuteEnv(Utf8Str(env[i]).raw(), &pvEnv, &cbEnv, &uNumEnv);
                        if (RT_FAILURE(vrc))
                            break;
                    }
                }

                if (RT_SUCCESS(vrc))
                {
                    PHOSTEXECCALLBACKDATA pData = (HOSTEXECCALLBACKDATA*)RTMemAlloc(sizeof(HOSTEXECCALLBACKDATA));
                    AssertPtr(pData);
                    uContextID = addCtrlCallbackContext(pData, sizeof(HOSTEXECCALLBACKDATA), progress);
                    Assert(uContextID > 0);

                    VBOXHGCMSVCPARM paParms[15];
                    int i = 0;
                    paParms[i++].setUInt32(uContextID);
                    paParms[i++].setPointer((void*)Utf8Command.raw(), (uint32_t)strlen(Utf8Command.raw()) + 1);
                    paParms[i++].setUInt32(aFlags);
                    paParms[i++].setUInt32(uNumArgs);
                    paParms[i++].setPointer((void*)pszArgs, cbArgs);
                    paParms[i++].setUInt32(uNumEnv);
                    paParms[i++].setUInt32(cbEnv);
                    paParms[i++].setPointer((void*)pvEnv, cbEnv);
                    paParms[i++].setPointer((void*)Utf8StdIn.raw(), (uint32_t)strlen(Utf8StdIn.raw()) + 1);
                    paParms[i++].setPointer((void*)Utf8StdOut.raw(), (uint32_t)strlen(Utf8StdOut.raw()) + 1);
                    paParms[i++].setPointer((void*)Utf8StdErr.raw(), (uint32_t)strlen(Utf8StdErr.raw()) + 1);
                    paParms[i++].setPointer((void*)Utf8UserName.raw(), (uint32_t)strlen(Utf8UserName.raw()) + 1);
                    paParms[i++].setPointer((void*)Utf8Password.raw(), (uint32_t)strlen(Utf8Password.raw()) + 1);
                    paParms[i++].setUInt32(aTimeoutMS);

                    /* Make sure mParent is valid, so set a read lock in this scope. */
                    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                    /* Forward the information to the VMM device. */
                    AssertPtr(mParent);
                    VMMDev *vmmDev = mParent->getVMMDev();
                    if (vmmDev)
                    {
                        LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                        vrc = vmmDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_CMD,
                                                   i, paParms);
                    }
                    else
                        vrc = VERR_INVALID_VM_HANDLE;
                    RTMemFree(pvEnv);
                }
                RTStrFree(pszArgs);
            }
            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback (timeout=%ldms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */
                CallbackListIter it = getCtrlCallbackContextByID(uContextID);
                if (it != mCallbackList.end())
                {
                    uint64_t u64Started = RTTimeMilliTS();
                    do
                    {
                        unsigned cMsWait;
                        if (aTimeoutMS == RT_INDEFINITE_WAIT)
                            cMsWait = 1000;
                        else
                        {
                            uint64_t cMsElapsed = RTTimeMilliTS() - u64Started;
                            if (cMsElapsed >= aTimeoutMS)
                                break; /* Timed out. */
                            cMsWait = RT_MIN(1000, aTimeoutMS - (uint32_t)cMsElapsed);
                        }
                        RTThreadSleep(100);
                    } while (!it->bCalled);
                }

                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                /* Did we get some status? */
                PHOSTEXECCALLBACKDATA pData = (HOSTEXECCALLBACKDATA*)it->pvData;
                Assert(it->cbData == sizeof(HOSTEXECCALLBACKDATA));
                AssertPtr(pData);
                if (it->bCalled)
                {
                    switch (pData->u32Status)
                    {
                        case PROC_STS_STARTED:
                            *aPID = pData->u32PID;
                            break;

                        /* In any other case the process either already 
                         * terminated or something else went wrong, so no PID ... */
                        case PROC_STS_TEN: /* Terminated normally. */
                        case PROC_STS_TEA: /* Terminated abnormally. */
                        case PROC_STS_TES: /* Terminated through signal. */
                        case PROC_STS_TOK:
                        case PROC_STS_TOA:
                        case PROC_STS_DWN:
                            *aPID = 0;
                            break;

                        case PROC_STS_ERROR:
                            vrc = pData->u32Flags; /* u32Flags member contains IPRT error code. */
                            break;

                        default:
                            vrc = VERR_INVALID_PARAMETER; /* Unknown status, should never happen! */
                            break;
                    }
                }
                else /* If callback not called within time ... well, that's a timeout! */
                    vrc = VERR_TIMEOUT;

                alock.release();

                /*
                 * Do *not* remove the callback yet - we might wait with the IProgress object on something
                 * else (like end of process) ...
                 */
                if (RT_FAILURE(vrc))
                {
                    if (vrc == VERR_FILE_NOT_FOUND) /* This is the most likely error. */
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The file '%s' was not found on guest"), Utf8Command.raw());
                    }
                    else if (vrc == VERR_BAD_EXE_FORMAT)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The file '%s' is not an executable format on guest"), Utf8Command.raw());
                    }
                    else if (vrc == VERR_LOGON_FAILURE)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The specified user '%s' was not able to logon on guest"), Utf8UserName.raw());
                    }
                    else if (vrc == VERR_TIMEOUT)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The guest did not respond within time (%ums)"), aTimeoutMS);
                    }
                    else if (vrc == VERR_INVALID_PARAMETER)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The guest reported an unknown process status (%u)"), pData->u32Status);
                    }
                    else
                    {
                        rc = setError(E_UNEXPECTED,
                                      tr("The service call failed with error %Rrc"), vrc);
                    }
                }
                else
                {
                    /* Return the progress to the caller. */
                    progress.queryInterfaceTo(aProgress);
                }
            }
            else
            {
                if (vrc == VERR_INVALID_VM_HANDLE)
                {
                    rc = setError(VBOX_E_VM_ERROR,
                                  tr("VMM device is not available (is the VM running?)"));
                }
                else if (vrc == VERR_TIMEOUT)
                {
                    rc = setError(VBOX_E_VM_ERROR,
                                  tr("The guest execution service is not ready"));
                }
                else /* HGCM call went wrong. */
                {
                    rc = setError(E_UNEXPECTED,
                                  tr("The HGCM call failed with error %Rrc"), vrc);
                }
            }

            for (unsigned i = 0; i < uNumArgs; i++)
                RTMemFree(papszArgv[i]);
            RTMemFree(papszArgv);
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::GetProcessOutput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, ULONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    HRESULT rc = S_OK;

    /* Search for existing PID. */
    PHOSTEXECOUTCALLBACKDATA pData = (HOSTEXECOUTCALLBACKDATA*)RTMemAlloc(sizeof(HOSTEXECOUTCALLBACKDATA));
    AssertPtr(pData);
    uint32_t uContextID = addCtrlCallbackContext(pData, sizeof(HOSTEXECOUTCALLBACKDATA), NULL /* pProgress */);
    Assert(uContextID > 0);

    size_t cbData = (size_t)RT_MIN(aSize, _64K);
    com::SafeArray<BYTE> outputData(cbData);

    VBOXHGCMSVCPARM paParms[5];
    int i = 0;
    paParms[i++].setUInt32(uContextID);
    paParms[i++].setUInt32(aPID);
    paParms[i++].setUInt32(aFlags); /** @todo Should represent stdout and/or stderr. */

    int vrc = VINF_SUCCESS;

    /* Make sure mParent is valid, so set the read lock while using. */
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Forward the information to the VMM device. */
    AssertPtr(mParent);
    VMMDev *vmmDev = mParent->getVMMDev();
    if (vmmDev)
    {
        LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
        vrc = vmmDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_GET_OUTPUT,
                                   i, paParms);
    }

    alock.release();

    if (RT_SUCCESS(vrc))
    {
        LogFlowFunc(("Waiting for HGCM callback (timeout=%ldms) ...\n", aTimeoutMS));

        /*
         * Wait for the HGCM low level callback until the process
         * has been started (or something went wrong). This is necessary to
         * get the PID.
         */
        CallbackListIter it = getCtrlCallbackContextByID(uContextID);
        if (it != mCallbackList.end())
        {
            uint64_t u64Started = RTTimeMilliTS();
            do
            {
                unsigned cMsWait;
                if (aTimeoutMS == RT_INDEFINITE_WAIT)
                    cMsWait = 1000;
                else
                {
                    uint64_t cMsElapsed = RTTimeMilliTS() - u64Started;
                    if (cMsElapsed >= aTimeoutMS)
                        break; /* timed out */
                    cMsWait = RT_MIN(1000, aTimeoutMS - (uint32_t)cMsElapsed);
                }
                RTThreadSleep(100);
            } while (!it->bCalled);

            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

            /* Did we get some output? */
            PHOSTEXECOUTCALLBACKDATA pData = (HOSTEXECOUTCALLBACKDATA*)it->pvData;
            Assert(it->cbData == sizeof(HOSTEXECOUTCALLBACKDATA));
            AssertPtr(pData);
    
            if (   it->bCalled
                && pData->cbData)
            {
                /* Do we need to resize the array? */
                if (pData->cbData > cbData)
                    outputData.resize(pData->cbData);
    
                /* Fill output in supplied out buffer. */
                memcpy(outputData.raw(), pData->pvData, pData->cbData);
                outputData.resize(pData->cbData); /* Shrink to fit actual buffer size. */
            }
            else
                vrc = VERR_NO_DATA; /* This is not an error we want to report to COM. */

            alock.release();
        }

        /* If something failed (or there simply was no data, indicated by VERR_NO_DATA,
         * we return an empty array so that the frontend knows when to give up. */
        if (RT_FAILURE(vrc) || FAILED(rc))
            outputData.resize(0);
        outputData.detachTo(ComSafeArrayOutArg(aData));
    }
    return rc;
#endif
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void Guest::setAdditionsVersion(Bstr aVersion, VBOXOSTYPE aOsType)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mAdditionsVersion = aVersion;
    mData.mAdditionsActive = !aVersion.isEmpty();
    /* Older Additions didn't have this finer grained capability bit,
     * so enable it by default.  Newer Additions will disable it immediately
     * if relevant. */
    mData.mSupportsGraphics = mData.mAdditionsActive;

    mData.mOSTypeId = Global::OSTypeId (aOsType);
}

void Guest::setSupportsSeamless (BOOL aSupportsSeamless)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mSupportsSeamless = aSupportsSeamless;
}

void Guest::setSupportsGraphics (BOOL aSupportsGraphics)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mSupportsGraphics = aSupportsGraphics;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
