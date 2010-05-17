/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    /*
     * Cleanup must be done *before* AutoUninitSpan to cancel all
     * all outstanding waits in API functions (which hold AutoCaller
     * ref counts).
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Clean up callback data. */
    CallbackListIter it;
    for (it = mCallbackList.begin(); it != mCallbackList.end(); it++)
        destroyCtrlCallbackContext(it);

    /* Clear process list. */
    mGuestProcessList.clear();
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
                                          ULONG *aMemTotal, ULONG *aMemFree, ULONG *aMemBalloon, ULONG *aMemShared,
                                          ULONG *aMemCache, ULONG *aPageTotal,
                                          ULONG *aMemAllocTotal, ULONG *aMemFreeTotal, ULONG *aMemBalloonTotal, ULONG *aMemSharedTotal)
{
    CheckComArgOutPointerValid(aCpuUser);
    CheckComArgOutPointerValid(aCpuKernel);
    CheckComArgOutPointerValid(aCpuIdle);
    CheckComArgOutPointerValid(aMemTotal);
    CheckComArgOutPointerValid(aMemFree);
    CheckComArgOutPointerValid(aMemBalloon);
    CheckComArgOutPointerValid(aMemShared);
    CheckComArgOutPointerValid(aMemCache);
    CheckComArgOutPointerValid(aPageTotal);
    CheckComArgOutPointerValid(aMemAllocTotal);
    CheckComArgOutPointerValid(aMemFreeTotal);
    CheckComArgOutPointerValid(aMemBalloonTotal);
    CheckComArgOutPointerValid(aMemSharedTotal);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCpuUser    = mCurrentGuestStat[GUESTSTATTYPE_CPUUSER];
    *aCpuKernel  = mCurrentGuestStat[GUESTSTATTYPE_CPUKERNEL];
    *aCpuIdle    = mCurrentGuestStat[GUESTSTATTYPE_CPUIDLE];
    *aMemTotal   = mCurrentGuestStat[GUESTSTATTYPE_MEMTOTAL] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aMemFree    = mCurrentGuestStat[GUESTSTATTYPE_MEMFREE] * (_4K/_1K);       /* page (4K) -> 1KB units */
    *aMemBalloon = mCurrentGuestStat[GUESTSTATTYPE_MEMBALLOON] * (_4K/_1K); /* page (4K) -> 1KB units */
    *aMemCache   = mCurrentGuestStat[GUESTSTATTYPE_MEMCACHE] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aPageTotal  = mCurrentGuestStat[GUESTSTATTYPE_PAGETOTAL] * (_4K/_1K);   /* page (4K) -> 1KB units */
    *aMemShared  = 0; /** todo */

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
            *aMemSharedTotal  = 0; /** todo */
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

    /* Callback can be called several times. */
    if (it != mCallbackList.end())
    {
        PHOSTEXECCALLBACKDATA pCBData = (HOSTEXECCALLBACKDATA*)it->pvData;
        AssertPtr(pCBData);

        pCBData->u32PID = pData->u32PID;
        pCBData->u32Status = pData->u32Status;
        pCBData->u32Flags = pData->u32Flags;
        /** @todo Copy void* buffer contents! */

        /* Was progress canceled before? */
        BOOL fCancelled;
        it->pProgress->COMGETTER(Canceled)(&fCancelled);

        /* Do progress handling. */
        Utf8Str errMsg;
        HRESULT rc2 = S_OK;
        switch (pData->u32Status)
        {
            case PROC_STS_STARTED:
                break;

            case PROC_STS_TEN: /* Terminated normally. */
                if (   !it->pProgress->getCompleted()
                    && !fCancelled)
                {
                    it->pProgress->notifyComplete(S_OK);
                }
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

        /* Handle process list. */
        /** @todo What happens on/deal with PID reuse? */
        /** @todo How to deal with multiple updates at once? */
        GuestProcessIter it_proc = getProcessByPID(pCBData->u32PID);
        if (it_proc == mGuestProcessList.end())
        {
            /* Not found, add to list. */
            GuestProcess p;
            p.mPID = pCBData->u32PID;            
            p.mStatus = pCBData->u32Status;
            p.mExitCode = pCBData->u32Flags; /* Contains exit code. */
            p.mFlags = 0;
            
            mGuestProcessList.push_back(p);
        }
        else /* Update list. */
        {
            it_proc->mStatus = pCBData->u32Status;
            it_proc->mExitCode = pCBData->u32Flags; /* Contains exit code. */
            it_proc->mFlags = 0;
        }

        if (   !it->pProgress.isNull()
            && errMsg.length())
        {
            if (   !it->pProgress->getCompleted()
                && !fCancelled)
            {
                it->pProgress->notifyComplete(E_FAIL /** @todo Find a better rc! */, COM_IIDOF(IGuest),
                                              (CBSTR)Guest::getComponentName(), errMsg.c_str());
                LogFlowFunc(("Callback (context ID=%u, status=%u) progress marked as completed\n",
                             pData->hdr.u32ContextID, pData->u32Status));
            }
            else
                LogFlowFunc(("Callback (context ID=%u, status=%u) progress already marked as completed\n",
                             pData->hdr.u32ContextID, pData->u32Status));
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

        /* Make sure we really got something! */
        if (   pData->cbData
            && pData->pvData)
        {
            /* Allocate data buffer and copy it */
            pCBData->pvData = RTMemAlloc(pData->cbData);
            pCBData->cbData = pData->cbData;

            AssertReturn(pCBData->pvData, VERR_NO_MEMORY);
            memcpy(pCBData->pvData, pData->pvData, pData->cbData);
        }
        else
        {
            pCBData->pvData = NULL;
            pCBData->cbData = 0;
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

Guest::GuestProcessIter Guest::getProcessByPID(uint32_t u32PID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Maybe use a map instead of list for fast context lookup. */
    GuestProcessIter it;
    for (it = mGuestProcessList.begin(); it != mGuestProcessList.end(); it++)
    {
        if (it->mPID == u32PID)
            return (it);
    }
    return it;
}

/* No locking here; */
void Guest::destroyCtrlCallbackContext(Guest::CallbackListIter it)
{
    LogFlowFuncEnter();
    if (it->pvData)
    {
        RTMemFree(it->pvData);
        it->pvData = NULL;
        it->cbData = 0;

        /* Notify outstanding waits for progress ... */
        if (!it->pProgress.isNull())
        {
            /* Only cancel if not canceled before! */
            BOOL fCancelled;
            if (SUCCEEDED(it->pProgress->COMGETTER(Canceled)(&fCancelled)) && !fCancelled)
                it->pProgress->Cancel();
            /* 
             * Do *not NULL pProgress here, because waiting function like executeProcess() 
             * will still rely on this object for checking whether they have to give up! 
             */
        }
    }
    LogFlowFuncLeave();
}

/* Adds a callback with a user provided data block and an optional progress object
 * to the callback list. A callback is identified by a unique context ID which is used
 * to identify a callback from the guest side. */
uint32_t Guest::addCtrlCallbackContext(eVBoxGuestCtrlCallbackType enmType, void *pvData, uint32_t cbData, Progress *pProgress)
{
    LogFlowFuncEnter();
    uint32_t uNewContext = ASMAtomicIncU32(&mNextContextID);
    if (uNewContext == UINT32_MAX)
        ASMAtomicUoWriteU32(&mNextContextID, 1000);

    /** @todo Put this stuff into a constructor! */
    CallbackContext context;
    context.mContextID = uNewContext;
    context.mType = enmType;
    context.bCalled = false;
    context.pvData = pvData;
    context.cbData = cbData;
    context.pProgress = pProgress;

    uint32_t nCallbacks;
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        mCallbackList.push_back(context);
        nCallbacks = mCallbackList.size();
    }    

#if 0
    if (nCallbacks > 256) /* Don't let the container size get too big! */
    {
        Guest::CallbackListIter it = mCallbackList.begin();
        destroyCtrlCallbackContext(it);
        {
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            mCallbackList.erase(it);
        }
    }
#endif

    LogFlowFuncLeave();
    return uNewContext;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::ExecuteProcess(IN_BSTR aCommand, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aUserName, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aCommand);
    CheckComArgOutPointerValid(aPID);
    CheckComArgStrNotEmptyOrNull(aUserName); /* Do not allow anonymous executions (with system rights). */
    CheckComArgStrNotEmptyOrNull(aPassword);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (aFlags != 0) /* Flags are not supported at the moment. */
        return E_INVALIDARG;

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.
         */
        ComObjPtr <Progress> progress;
        rc = progress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = progress->init(static_cast<IGuest*>(this),
                                BstrFmt(tr("Executing process")),
                                TRUE);
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
            AssertReturn(papszArgv, E_OUTOFMEMORY);
            for (unsigned i = 0; RT_SUCCESS(vrc) && i < uNumArgs; i++)
            {
                int cbLen = RTStrAPrintf(&papszArgv[i], "%s", Utf8Str(args[i]).raw());
                if (cbLen < 0)
                    vrc = VERR_NO_MEMORY;

            }
            papszArgv[uNumArgs] = NULL;
        }

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
                    AssertReturn(pData, VBOX_E_IPRT_ERROR);
                    uContextID = addCtrlCallbackContext(VBOXGUESTCTRLCALLBACKTYPE_EXEC_START, 
                                                        pData, sizeof(HOSTEXECCALLBACKDATA), progress);
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
                    paParms[i++].setPointer((void*)Utf8UserName.raw(), (uint32_t)strlen(Utf8UserName.raw()) + 1);
                    paParms[i++].setPointer((void*)Utf8Password.raw(), (uint32_t)strlen(Utf8Password.raw()) + 1);
                    paParms[i++].setUInt32(aTimeoutMS);

                    VMMDev *vmmDev;
                    {
                        /* Make sure mParent is valid, so set the read lock while using. 
                         * Do not keep this lock while doing the actual call, because in the meanwhile
                         * another thread could request a write lock which would be a bad idea ... */
                        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    
                        /* Forward the information to the VMM device. */
                        AssertPtr(mParent);
                        vmmDev = mParent->getVMMDev();
                    }

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
                BOOL fCanceled = FALSE;
                if (it != mCallbackList.end())
                {
                    uint64_t u64Started = RTTimeMilliTS();
                    while (!it->bCalled)
                    {
                        /* Check for timeout. */
                        unsigned cMsWait;
                        if (aTimeoutMS == RT_INDEFINITE_WAIT)
                            cMsWait = 10;
                        else
                        {
                            uint64_t cMsElapsed = RTTimeMilliTS() - u64Started;
                            if (cMsElapsed >= aTimeoutMS)
                                break; /* Timed out. */
                            cMsWait = RT_MIN(10, aTimeoutMS - (uint32_t)cMsElapsed);
                        }

                        /* Check for manual stop. */
                        if (!it->pProgress.isNull())
                        {                            
                            rc = it->pProgress->COMGETTER(Canceled)(&fCanceled);
                            if (FAILED(rc)) throw rc;
                            if (fCanceled)
                                break; /* Client wants to abort. */
                        }
                        RTThreadSleep(cMsWait);
                    }
                }

                /* Was the whole thing canceled? */
                if (!fCanceled)
                {
                    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS); 

                    PHOSTEXECCALLBACKDATA pData = (HOSTEXECCALLBACKDATA*)it->pvData;
                    Assert(it->cbData == sizeof(HOSTEXECCALLBACKDATA));
                    AssertPtr(pData);

                    if (it->bCalled)
                    {
                        /* Did we get some status? */
                        switch (pData->u32Status)
                        {
                            case PROC_STS_STARTED:
                                /* Process is (still) running; get PID. */
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
                                /* 
                                 * Process (already) ended, but we want to get the
                                 * PID anyway to retrieve the output in a later call. 
                                 */
                                *aPID = pData->u32PID;
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
                    else /* Execution went fine. */
                    {
                        /* Return the progress to the caller. */
                        progress.queryInterfaceTo(aProgress);
                    }
                }
                else /* Operation was canceled. */
                {
                    rc = setError(VBOX_E_IPRT_ERROR,
                                  tr("The operation was canceled."));
                }
            }
            else /* HGCM related error codes .*/
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

    CheckComArgExpr(aPID, aPID > 0);

    if (aFlags != 0) /* Flags are not supported at the moment. */
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc(); 

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.
         * Note that we need at least a local progress object here in order
         * to get notified when someone cancels the operation.
         */
        ComObjPtr <Progress> progress;
        rc = progress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = progress->init(static_cast<IGuest*>(this),
                                BstrFmt(tr("Getting output of process")),
                                TRUE);
        }
        if (FAILED(rc)) return rc;

        /* Adjust timeout */
        if (aTimeoutMS == 0)
            aTimeoutMS = UINT32_MAX;

        /* Search for existing PID. */
        PHOSTEXECOUTCALLBACKDATA pData = (HOSTEXECOUTCALLBACKDATA*)RTMemAlloc(sizeof(HOSTEXECOUTCALLBACKDATA));
        AssertReturn(pData, VBOX_E_IPRT_ERROR);
        uint32_t uContextID = addCtrlCallbackContext(VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT,
                                                     pData, sizeof(HOSTEXECOUTCALLBACKDATA), progress);
        Assert(uContextID > 0);
    
        size_t cbData = (size_t)RT_MIN(aSize, _64K);
        com::SafeArray<BYTE> outputData(cbData);
    
        VBOXHGCMSVCPARM paParms[5];
        int i = 0;
        paParms[i++].setUInt32(uContextID);
        paParms[i++].setUInt32(aPID);
        paParms[i++].setUInt32(aFlags); /** @todo Should represent stdout and/or stderr. */
    
        int vrc = VINF_SUCCESS;
    
        {
            VMMDev *vmmDev;
            {
                /* Make sure mParent is valid, so set the read lock while using. 
                 * Do not keep this lock while doing the actual call, because in the meanwhile
                 * another thread could request a write lock which would be a bad idea ... */
                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        
                /* Forward the information to the VMM device. */
                AssertPtr(mParent);
                vmmDev = mParent->getVMMDev();
            }

            if (vmmDev)
            {
                LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                vrc = vmmDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_GET_OUTPUT,
                                           i, paParms);
            }
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
            BOOL fCanceled = FALSE;
            if (it != mCallbackList.end())
            {
                uint64_t u64Started = RTTimeMilliTS();
                while (!it->bCalled)
                {
                    /* Check for timeout. */
                    unsigned cMsWait;
                    if (aTimeoutMS == RT_INDEFINITE_WAIT)
                        cMsWait = 10;
                    else
                    {
                        uint64_t cMsElapsed = RTTimeMilliTS() - u64Started;
                        if (cMsElapsed >= aTimeoutMS)
                            break; /* Timed out. */
                        cMsWait = RT_MIN(10, aTimeoutMS - (uint32_t)cMsElapsed);
                    }

                    /* Check for manual stop. */
                    if (!it->pProgress.isNull())
                    {
                        rc = it->pProgress->COMGETTER(Canceled)(&fCanceled);
                        if (FAILED(rc)) throw rc;
                        if (fCanceled)
                            break; /* Client wants to abort. */
                    }
                    RTThreadSleep(cMsWait);
                } 
    
                /* Was the whole thing canceled? */
                if (!fCanceled)
                {
                    if (it->bCalled)
                    {
                        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
            
                        /* Did we get some output? */
                        pData = (HOSTEXECOUTCALLBACKDATA*)it->pvData;
                        Assert(it->cbData == sizeof(HOSTEXECOUTCALLBACKDATA));
                        AssertPtr(pData);
            
                        if (pData->cbData)
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
                    }
                    else /* If callback not called within time ... well, that's a timeout! */
                        vrc = VERR_TIMEOUT;
                }
                else /* Operation was canceled. */
                    vrc = VERR_CANCELLED;

                if (RT_FAILURE(vrc))
                {
                    if (vrc == VERR_NO_DATA)
                    {
                        /* This is not an error we want to report to COM. */
                    }
                    else if (vrc == VERR_TIMEOUT)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The guest did not output within time (%ums)"), aTimeoutMS);
                    }
                    else if (vrc == VERR_CANCELLED)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The operation was canceled."));
                    }
                    else
                    {
                        rc = setError(E_UNEXPECTED,
                                      tr("The service call failed with error %Rrc"), vrc);
                    }
                }

                {
                    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                    destroyCtrlCallbackContext(it);
                }
            }
            else /* PID lookup failed. */
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Process (PID %u) not found!"), aPID);    
        }
        else /* HGCM operation failed. */
            rc = setError(E_UNEXPECTED,
                          tr("The HGCM call failed with error %Rrc"), vrc);
    
        /* Cleanup. */
        progress->uninit();
        progress.setNull();

        /* If something failed (or there simply was no data, indicated by VERR_NO_DATA,
         * we return an empty array so that the frontend knows when to give up. */
        if (RT_FAILURE(vrc) || FAILED(rc))
            outputData.resize(0);
        outputData.detachTo(ComSafeArrayOutArg(aData));
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessStatus(ULONG aPID, ULONG *aExitCode, ULONG *aFlags, ULONG *aStatus)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc(); 

    HRESULT rc = S_OK;

    try
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    
        GuestProcessIterConst it;
        for (it = mGuestProcessList.begin(); it != mGuestProcessList.end(); it++)
        {
            if (it->mPID == aPID)
                break;
        }
    
        if (it != mGuestProcessList.end())
        {
            *aExitCode = it->mExitCode;
            *aFlags = it->mFlags;
            *aStatus = it->mStatus;
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Process (PID %u) not found!"), aPID);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
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
