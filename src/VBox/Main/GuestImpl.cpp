/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>
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

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    ULONG aMemoryBalloonSize;
    HRESULT ret = mParent->machine()->COMGETTER(MemoryBalloonSize)(&aMemoryBalloonSize);
    if (ret == S_OK)
        mMemoryBalloonSize = aMemoryBalloonSize;
    else
        mMemoryBalloonSize = 0;                     /* Default is no ballooning */

    BOOL fPageFusionEnabled;
    ret = mParent->machine()->COMGETTER(PageFusionEnabled)(&fPageFusionEnabled);
    if (ret == S_OK)
        mfPageFusionEnabled = fPageFusionEnabled;
    else
        mfPageFusionEnabled = false;                /* Default is no page fusion*/

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
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_CONTROL
    /* Scope write lock as much as possible. */
    {
        /*
         * Cleanup must be done *before* AutoUninitSpan to cancel all
         * all outstanding waits in API functions (which hold AutoCaller
         * ref counts).
         */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Clean up callback data. */
        CallbackMapIter it;
        for (it = mCallbackMap.begin(); it != mCallbackMap.end(); it++)
            destroyCtrlCallbackContext(it);

        /* Clear process map. */
        mGuestProcessMap.clear();
    }
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

STDMETHODIMP Guest::COMGETTER(AdditionsRunLevel) (AdditionsRunLevelType_T *aRunLevel)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aRunLevel = mData.mAdditionsRunLevel;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsVersion) (BSTR *aAdditionsVersion)
{
    CheckComArgOutPointerValid(aAdditionsVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;
    if (   mData.mAdditionsVersion.isEmpty()
        /* Only try alternative way if GA are active! */
        && mData.mAdditionsRunLevel > AdditionsRunLevelType_None)
    {
        /*
         * If we got back an empty string from GetAdditionsVersion() we either
         * really don't have the Guest Additions version yet or the guest is running
         * older Guest Additions (< 3.2.0) which don't provide VMMDevReq_ReportGuestInfo2,
         * so get the version + revision from the (hopefully) provided guest properties
         * instead.
         */
        Bstr addVersion;
        LONG64 u64Timestamp;
        Bstr flags;
        hr = mParent->machine()->GetGuestProperty(Bstr("/VirtualBox/GuestAdd/Version").raw(),
                                                  addVersion.asOutParam(), &u64Timestamp, flags.asOutParam());
        if (hr == S_OK)
        {
            Bstr addRevision;
            hr = mParent->machine()->GetGuestProperty(Bstr("/VirtualBox/GuestAdd/Revision").raw(),
                                                      addRevision.asOutParam(), &u64Timestamp, flags.asOutParam());
            if (   hr == S_OK
                && !addVersion.isEmpty()
                && !addRevision.isEmpty())
            {
                /* Some Guest Additions versions had interchanged version + revision values,
                 * so check if the version value at least has a dot to identify it and change
                 * both values to reflect the right content. */
                if (!Utf8Str(addVersion).contains("."))
                {
                    Bstr addTemp = addVersion;
                    addVersion = addRevision;
                    addRevision = addTemp;
                }

                Bstr additionsVersion = BstrFmt("%ls r%ls",
                                                addVersion.raw(), addRevision.raw());
                additionsVersion.cloneTo(aAdditionsVersion);
            }
            /** @todo r=bird: else: Should not return failure! */
        }
        else
        {
            /* If getting the version + revision above fails or they simply aren't there
             * because of *really* old Guest Additions we only can report the interface
             * version to at least have something. */
            mData.mInterfaceVersion.cloneTo(aAdditionsVersion);
            /** @todo r=bird: hr is still indicating failure! */
        }
    }
    else
        mData.mAdditionsVersion.cloneTo(aAdditionsVersion);

    return hr;
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

BOOL Guest::isPageFusionEnabled()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return false;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return mfPageFusionEnabled;
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
        VMMDev *pVMMDev = mParent->getVMMDev();
        /* MUST release all locks before calling VMM device as its critsect
         * has higher lock order than anything in Main. */
        alock.release();
        if (pVMMDev)
        {
            PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
            if (pVMMDevPort)
                pVMMDevPort->pfnSetMemoryBalloon(pVMMDevPort, aMemoryBalloonSize);
        }
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
    VMMDev *pVMMDev = mParent->getVMMDev();
    /* MUST release all locks before calling VMM device as its critsect
     * has higher lock order than anything in Main. */
    alock.release();
    if (pVMMDev)
    {
        PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
        if (pVMMDevPort)
            pVMMDevPort->pfnSetStatisticsInterval(pVMMDevPort, aUpdateInterval);
    }

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

    /* MUST release all locks before calling any PGM statistics queries,
     * as they are executed by EMT and that might deadlock us by VMM device
     * activity which waits for the Guest object lock. */
    alock.release();
    Console::SafeVMPtr pVM (mParent);
    if (pVM.isOk())
    {
        uint64_t uFreeTotal, uAllocTotal, uBalloonedTotal, uSharedTotal;
        *aMemFreeTotal = 0;
        int rc = PGMR3QueryVMMMemoryStats(pVM.raw(), &uAllocTotal, &uFreeTotal, &uBalloonedTotal, &uSharedTotal);
        AssertRC(rc);
        if (rc == VINF_SUCCESS)
        {
            *aMemAllocTotal   = (ULONG)(uAllocTotal / _1K);  /* bytes -> KB */
            *aMemFreeTotal    = (ULONG)(uFreeTotal / _1K);
            *aMemBalloonTotal = (ULONG)(uBalloonedTotal / _1K);
            *aMemSharedTotal  = (ULONG)(uSharedTotal / _1K);
        }

        /* Query the missing per-VM memory statistics. */
        *aMemShared  = 0;
        uint64_t uTotalMem, uPrivateMem, uSharedMem, uZeroMem;
        rc = PGMR3QueryMemoryStats(pVM.raw(), &uTotalMem, &uPrivateMem, &uSharedMem, &uZeroMem);
        if (rc == VINF_SUCCESS)
        {
            *aMemShared = (ULONG)(uSharedMem / _1K);
        }
    }
    else
    {
        *aMemFreeTotal = 0;
        *aMemShared  = 0;
    }

    return S_OK;
}

HRESULT Guest::setStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (enmType >= GUESTSTATTYPE_MAX)
        return E_INVALIDARG;

    mCurrentGuestStat[enmType] = aVal;
    return S_OK;
}

STDMETHODIMP Guest::GetAdditionsStatus(AdditionsRunLevelType_T aLevel, BOOL *aActive)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    switch (aLevel)
    {
        case AdditionsRunLevelType_System:
            *aActive = (mData.mAdditionsRunLevel > AdditionsRunLevelType_None);
            break;

        case AdditionsRunLevelType_Userland:
            *aActive = (mData.mAdditionsRunLevel >= AdditionsRunLevelType_Userland);
            break;

        case AdditionsRunLevelType_Desktop:
            *aActive = (mData.mAdditionsRunLevel >= AdditionsRunLevelType_Desktop);
            break;

        default:
            rc = setError(VBOX_E_NOT_SUPPORTED,
                          tr("Invalid status level defined: %u"), aLevel);
            break;
    }

    return rc;
}

STDMETHODIMP Guest::SetCredentials(IN_BSTR aUserName, IN_BSTR aPassword,
                                   IN_BSTR aDomain, BOOL aAllowInteractiveLogon)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* forward the information to the VMM device */
    VMMDev *pVMMDev = mParent->getVMMDev();
    if (pVMMDev)
    {
        PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
        if (pVMMDevPort)
        {
            uint32_t u32Flags = VMMDEV_SETCREDENTIALS_GUESTLOGON;
            if (!aAllowInteractiveLogon)
                u32Flags = VMMDEV_SETCREDENTIALS_NOLOCALLOGON;

            pVMMDevPort->pfnSetCredentials(pVMMDevPort,
                                           Utf8Str(aUserName).c_str(),
                                           Utf8Str(aPassword).c_str(),
                                           Utf8Str(aDomain).c_str(),
                                           u32Flags);
            return S_OK;
        }
    }

    return setError(VBOX_E_VM_ERROR,
                    tr("VMM device is not available (is the VM running?)"));
}

#ifdef VBOX_WITH_GUEST_CONTROL
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
int Guest::prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnvVars)
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
#ifdef DEBUG_andy
    LogFlowFunc(("pvExtension = %p, u32Function = %d, pvParms = %p, cbParms = %d\n",
                 pvExtension, u32Function, pvParms, cbParms));
#endif
    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);

    int rc = VINF_SUCCESS;
    switch (u32Function)
    {
        case GUEST_DISCONNECTED:
        {
            //LogFlowFunc(("GUEST_DISCONNECTED\n"));

            PCALLBACKDATACLIENTDISCONNECTED pCBData = reinterpret_cast<PCALLBACKDATACLIENTDISCONNECTED>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATACLIENTDISCONNECTED) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGICCLIENTDISCONNECTED == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlClientDisconnected(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_STATUS\n"));

            PCALLBACKDATAEXECSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGICEXECSTATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecStatus(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_OUTPUT:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_OUTPUT\n"));

            PCALLBACKDATAEXECOUT pCBData = reinterpret_cast<PCALLBACKDATAEXECOUT>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECOUT) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGICEXECOUT == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecOut(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_INPUT_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_INPUT_STATUS\n"));

            PCALLBACKDATAEXECINSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECINSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECINSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGICEXECINSTATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecInStatus(u32Function, pCBData);
            break;
        }

        default:
            AssertMsgFailed(("Unknown guest control notification received, u32Function=%u", u32Function));
            rc = VERR_INVALID_PARAMETER;
            break;
    }
    return rc;
}

/* Function for handling the execution start/termination notification. */
int Guest::notifyCtrlExecStatus(uint32_t                u32Function,
                                PCALLBACKDATAEXECSTATUS pData)
{
    int vrc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(pData);
    CallbackMapIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);

    /* Callback can be called several times. */
    if (it != mCallbackMap.end())
    {
        PCALLBACKDATAEXECSTATUS pCBData = (PCALLBACKDATAEXECSTATUS)it->second.pvData;
        AssertPtr(pCBData);

        pCBData->u32PID = pData->u32PID;
        pCBData->u32Status = pData->u32Status;
        pCBData->u32Flags = pData->u32Flags;
        /** @todo Copy void* buffer contents! */

        Utf8Str errMsg;

        /* Was progress canceled before? */
        BOOL fCanceled;
        ComAssert(!it->second.pProgress.isNull());
        if (   SUCCEEDED(it->second.pProgress->COMGETTER(Canceled)(&fCanceled))
            && !fCanceled)
        {
            /* Do progress handling. */
            HRESULT hr;
            switch (pData->u32Status)
            {
                case PROC_STS_STARTED:
                    LogRel(("Guest process (PID %u) started\n", pCBData->u32PID)); /** @todo Add process name */
                    hr = it->second.pProgress->SetNextOperation(Bstr(tr("Waiting for process to exit ...")).raw(), 1 /* Weight */);
                    AssertComRC(hr);
                    break;

                case PROC_STS_TEN: /* Terminated normally. */
                    LogRel(("Guest process (PID %u) exited normally\n", pCBData->u32PID)); /** @todo Add process name */
                    if (!it->second.pProgress->getCompleted())
                    {
                        hr = it->second.pProgress->notifyComplete(S_OK);
                        AssertComRC(hr);

                        LogFlowFunc(("Proccess (CID=%u, status=%u) terminated successfully\n",
                                     pData->hdr.u32ContextID, pData->u32Status));
                    }
                    break;

                case PROC_STS_TEA: /* Terminated abnormally. */
                    LogRel(("Guest process (PID %u) terminated abnormally with exit code = %u\n",
                            pCBData->u32PID, pCBData->u32Flags)); /** @todo Add process name */
                    errMsg = Utf8StrFmt(Guest::tr("Process terminated abnormally with status '%u'"),
                                        pCBData->u32Flags);
                    break;

                case PROC_STS_TES: /* Terminated through signal. */
                    LogRel(("Guest process (PID %u) terminated through signal with exit code = %u\n",
                            pCBData->u32PID, pCBData->u32Flags)); /** @todo Add process name */
                    errMsg = Utf8StrFmt(Guest::tr("Process terminated via signal with status '%u'"),
                                        pCBData->u32Flags);
                    break;

                case PROC_STS_TOK:
                    LogRel(("Guest process (PID %u) timed out and was killed\n", pCBData->u32PID)); /** @todo Add process name */
                    errMsg = Utf8StrFmt(Guest::tr("Process timed out and was killed"));
                    break;

                case PROC_STS_TOA:
                    LogRel(("Guest process (PID %u) timed out and could not be killed\n", pCBData->u32PID)); /** @todo Add process name */
                    errMsg = Utf8StrFmt(Guest::tr("Process timed out and could not be killed"));
                    break;

                case PROC_STS_DWN:
                    LogRel(("Guest process (PID %u) killed because system is shutting down\n", pCBData->u32PID)); /** @todo Add process name */
                    /*
                     * If u32Flags has ExecuteProcessFlag_IgnoreOrphanedProcesses set, we don't report an error to
                     * our progress object. This is helpful for waiters which rely on the success of our progress object
                     * even if the executed process was killed because the system/VBoxService is shutting down.
                     *
                     * In this case u32Flags contains the actual execution flags reached in via Guest::ExecuteProcess().
                     */
                    if (pData->u32Flags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
                    {
                        if (!it->second.pProgress->getCompleted())
                        {
                            hr = it->second.pProgress->notifyComplete(S_OK);
                            AssertComRC(hr);
                        }
                    }
                    else
                    {
                        errMsg = Utf8StrFmt(Guest::tr("Process killed because system is shutting down"));
                    }
                    break;

                case PROC_STS_ERROR:
                    LogRel(("Guest process (PID %u) could not be started because of rc=%Rrc\n",
                            pCBData->u32PID, pCBData->u32Flags)); /** @todo Add process name */
                    errMsg = Utf8StrFmt(Guest::tr("Process execution failed with rc=%Rrc"), pCBData->u32Flags);
                    break;

                default:
                    vrc = VERR_INVALID_PARAMETER;
                    break;
            }

            /* Handle process map. */
            /** @todo What happens on/deal with PID reuse? */
            /** @todo How to deal with multiple updates at once? */
            if (pCBData->u32PID > 0)
            {
                GuestProcessMapIter it_proc = getProcessByPID(pCBData->u32PID);
                if (it_proc == mGuestProcessMap.end())
                {
                    /* Not found, add to map. */
                    GuestProcess newProcess;
                    newProcess.mStatus = pCBData->u32Status;
                    newProcess.mExitCode = pCBData->u32Flags; /* Contains exit code. */
                    newProcess.mFlags = 0;

                    mGuestProcessMap[pCBData->u32PID] = newProcess;
                }
                else /* Update map. */
                {
                    it_proc->second.mStatus = pCBData->u32Status;
                    it_proc->second.mExitCode = pCBData->u32Flags; /* Contains exit code. */
                    it_proc->second.mFlags = 0;
                }
            }
        }
        else
            errMsg = Utf8StrFmt(Guest::tr("Process execution canceled"));

        if (!it->second.pProgress->getCompleted())
        {
            if (   errMsg.length()
                || fCanceled) /* If canceled we have to report E_FAIL! */
            {
                /* Destroy all callbacks which are still waiting on something
                 * which is related to the current PID. */
                CallbackMapIter it2;
                for (it2 = mCallbackMap.begin(); it2 != mCallbackMap.end(); it2++)
                {
                    switch (it2->second.mType)
                    {
                        case VBOXGUESTCTRLCALLBACKTYPE_EXEC_START:
                            break;

                        /* When waiting for process output while the process is destroyed,
                         * make sure we also destroy the actual waiting operation (internal progress object)
                         * in order to not block the caller. */
                        case VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT:
                        {
                            PCALLBACKDATAEXECOUT pItData = (CALLBACKDATAEXECOUT*)it2->second.pvData;
                            AssertPtr(pItData);
                            if (pItData->u32PID == pCBData->u32PID)
                                destroyCtrlCallbackContext(it2);
                            break;
                        }

                        default:
                            AssertMsgFailed(("Unknown callback type %d\n", it2->second.mType));
                            break;
                    }
                }

                HRESULT hr2 = it->second.pProgress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                                   COM_IIDOF(IGuest),
                                                                   Guest::getStaticComponentName(),
                                                                   "%s", errMsg.c_str());
                AssertComRC(hr2);
                LogFlowFunc(("Process (CID=%u, status=%u) reported error: %s\n",
                             pData->hdr.u32ContextID, pData->u32Status, errMsg.c_str()));
            }
        }
    }
    else
        LogFlowFunc(("Unexpected callback (magic=%u, CID=%u) arrived\n", pData->hdr.u32Magic, pData->hdr.u32ContextID));
    LogFlowFunc(("Returned with rc=%Rrc\n", vrc));
    return vrc;
}

/* Function for handling the execution output notification. */
int Guest::notifyCtrlExecOut(uint32_t             u32Function,
                             PCALLBACKDATAEXECOUT pData)
{
    int rc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(pData);
    CallbackMapIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);
    if (it != mCallbackMap.end())
    {
        PCALLBACKDATAEXECOUT pCBData = (PCALLBACKDATAEXECOUT)it->second.pvData;
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

        /* Was progress canceled before? */
        BOOL fCanceled;
        ComAssert(!it->second.pProgress.isNull());
        if (SUCCEEDED(it->second.pProgress->COMGETTER(Canceled)(&fCanceled)) && fCanceled)
        {
            it->second.pProgress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                 COM_IIDOF(IGuest),
                                                 Guest::getStaticComponentName(),
                                                 Guest::tr("The output operation was canceled"));
        }
        else
        {
            BOOL fCompleted;
            if (   SUCCEEDED(it->second.pProgress->COMGETTER(Completed)(&fCompleted))
                && !fCompleted)
            {
                    /* If we previously got completed notification, don't trigger again. */
                it->second.pProgress->notifyComplete(S_OK);
            }
        }
    }
    else
        LogFlowFunc(("Unexpected callback (magic=%u, CID=%u) arrived\n", pData->hdr.u32Magic, pData->hdr.u32ContextID));
    return rc;
}

/* Function for handling the execution input status notification. */
int Guest::notifyCtrlExecInStatus(uint32_t                  u32Function,
                                  PCALLBACKDATAEXECINSTATUS pData)
{
    int rc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(pData);
    CallbackMapIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);
    if (it != mCallbackMap.end())
    {
        PCALLBACKDATAEXECINSTATUS pCBData = (PCALLBACKDATAEXECINSTATUS)it->second.pvData;
        AssertPtr(pCBData);

        /* Save bytes processed. */
        pCBData->cbProcessed = pData->cbProcessed;

        /* Was progress canceled before? */
        BOOL fCanceled;
        ComAssert(!it->second.pProgress.isNull());
        if (SUCCEEDED(it->second.pProgress->COMGETTER(Canceled)(&fCanceled)) && fCanceled)
        {
            it->second.pProgress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                 COM_IIDOF(IGuest),
                                                 Guest::getStaticComponentName(),
                                                 Guest::tr("The input operation was canceled"));
        }
        else
        {
            BOOL fCompleted;
            if (   SUCCEEDED(it->second.pProgress->COMGETTER(Completed)(&fCompleted))
                && !fCompleted)
            {
                    /* If we previously got completed notification, don't trigger again. */
                it->second.pProgress->notifyComplete(S_OK);
            }
        }
    }
    else
        LogFlowFunc(("Unexpected callback (magic=%u, CID=%u) arrived\n", pData->hdr.u32Magic, pData->hdr.u32ContextID));
    return rc;
}

int Guest::notifyCtrlClientDisconnected(uint32_t                        u32Function,
                                        PCALLBACKDATACLIENTDISCONNECTED pData)
{
    int rc = VINF_SUCCESS;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    CallbackMapIter it = getCtrlCallbackContextByID(pData->hdr.u32ContextID);
    if (it != mCallbackMap.end())
    {
        LogFlowFunc(("Client with CID=%u disconnected\n", it->first));
        destroyCtrlCallbackContext(it);
    }
    return rc;
}

Guest::CallbackMapIter Guest::getCtrlCallbackContextByID(uint32_t u32ContextID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return mCallbackMap.find(u32ContextID);
}

Guest::GuestProcessMapIter Guest::getProcessByPID(uint32_t u32PID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return mGuestProcessMap.find(u32PID);
}

/* No locking here; */
void Guest::destroyCtrlCallbackContext(Guest::CallbackMapIter it)
{
    LogFlowFunc(("Destroying callback with CID=%u ...\n", it->first));

    if (it->second.pvData)
    {
        RTMemFree(it->second.pvData);
        it->second.pvData = NULL;
        it->second.cbData = 0;
    }

    /* Notify outstanding waits for progress ... */
    if (    it->second.pProgress
         && !it->second.pProgress.isNull())
    {
        LogFlowFunc(("Handling progress for CID=%u ...\n", it->first));

        /*
         * Assume we didn't complete to make sure we clean up even if the
         * following call fails.
         */
        BOOL fCompleted = FALSE;
        it->second.pProgress->COMGETTER(Completed)(&fCompleted);
        if (!fCompleted)
        {
            LogFlowFunc(("Progress of CID=%u *not* completed, cancelling ...\n", it->first));

            /* Only cancel if not canceled before! */
            BOOL fCanceled;
            if (SUCCEEDED(it->second.pProgress->COMGETTER(Canceled)(&fCanceled)) && !fCanceled)
                it->second.pProgress->Cancel();

            /*
             * To get waitForCompletion completed (unblocked) we have to notify it if necessary (only
             * cancle won't work!). This could happen if the client thread (e.g. VBoxService, thread of a spawned process)
             * is disconnecting without having the chance to sending a status message before, so we
             * have to abort here to make sure the host never hangs/gets stuck while waiting for the
             * progress object to become signalled.
             */
            it->second.pProgress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                 COM_IIDOF(IGuest),
                                                 Guest::getStaticComponentName(),
                                                 Guest::tr("The operation was canceled because client is shutting down"));
        }
        /*
         * Do *not* NULL pProgress here, because waiting function like executeProcess()
         * will still rely on this object for checking whether they have to give up!
         */
    }
}

/* Adds a callback with a user provided data block and an optional progress object
 * to the callback map. A callback is identified by a unique context ID which is used
 * to identify a callback from the guest side. */
uint32_t Guest::addCtrlCallbackContext(eVBoxGuestCtrlCallbackType enmType, void *pvData, uint32_t cbData, Progress *pProgress)
{
    AssertPtr(pProgress);

    /** @todo Put this stuff into a constructor! */
    CallbackContext context;
    context.mType = enmType;
    context.pvData = pvData;
    context.cbData = cbData;
    context.pProgress = pProgress;

    /* Create a new context ID and assign it. */
    CallbackMapIter it;
    uint32_t uNewContext = 0;
    do
    {
        /* Create a new context ID ... */
        uNewContext = ASMAtomicIncU32(&mNextContextID);
        if (uNewContext == UINT32_MAX)
            ASMAtomicUoWriteU32(&mNextContextID, 1000);
        /* Is the context ID already used? */
        it = getCtrlCallbackContextByID(uNewContext);
    } while(it != mCallbackMap.end());

    uint32_t nCallbacks = 0;
    if (   it == mCallbackMap.end()
        && uNewContext > 0)
    {
        /* We apparently got an unused context ID, let's use it! */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        mCallbackMap[uNewContext] = context;
        nCallbacks = mCallbackMap.size();
    }
    else
    {
        /* Should never happen ... */
        {
            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
            nCallbacks = mCallbackMap.size();
        }
        AssertReleaseMsg(uNewContext, ("No free context ID found! uNewContext=%u, nCallbacks=%u", uNewContext, nCallbacks));
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
    return uNewContext;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::ExecuteProcess(IN_BSTR aCommand, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aUserName, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aCommand);
    CheckComArgOutPointerValid(aPID);
    CheckComArgOutPointerValid(aProgress);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & ExecuteProcessFlag_IgnoreOrphanedProcesses))
            return E_INVALIDARG;
    }

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.  Note that this is a multi operation
         * object to perform the following steps:
         * - Operation 1 (0): Create/start process.
         * - Operation 2 (1): Wait for process to exit.
         * If this progress completed successfully (S_OK), the process
         * started and exited normally. In any other case an error/exception
         * occured.
         */
        ComObjPtr <Progress> progress;
        rc = progress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = progress->init(static_cast<IGuest*>(this),
                                Bstr(tr("Executing process")).raw(),
                                TRUE,
                                2,                                          /* Number of operations. */
                                Bstr(tr("Starting process ...")).raw());    /* Description of first stage. */
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
                vrc = RTUtf16ToUtf8(args[i], &papszArgv[i]);
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
                        vrc = prepareExecuteEnv(Utf8Str(env[i]).c_str(), &pvEnv, &cbEnv, &uNumEnv);
                        if (RT_FAILURE(vrc))
                            break;
                    }
                }

                LogRel(("Executing guest process \"%s\" as user \"%s\" ...\n",
                        Utf8Command.c_str(), Utf8UserName.c_str()));

                if (RT_SUCCESS(vrc))
                {
                    PCALLBACKDATAEXECSTATUS pData = (PCALLBACKDATAEXECSTATUS)RTMemAlloc(sizeof(CALLBACKDATAEXECSTATUS));
                    AssertReturn(pData, VBOX_E_IPRT_ERROR);
                    RT_ZERO(*pData);
                    uContextID = addCtrlCallbackContext(VBOXGUESTCTRLCALLBACKTYPE_EXEC_START,
                                                        pData, sizeof(CALLBACKDATAEXECSTATUS), progress);
                    Assert(uContextID > 0);

                    VBOXHGCMSVCPARM paParms[15];
                    int i = 0;
                    paParms[i++].setUInt32(uContextID);
                    paParms[i++].setPointer((void*)Utf8Command.c_str(), (uint32_t)Utf8Command.length() + 1);
                    paParms[i++].setUInt32(aFlags);
                    paParms[i++].setUInt32(uNumArgs);
                    paParms[i++].setPointer((void*)pszArgs, cbArgs);
                    paParms[i++].setUInt32(uNumEnv);
                    paParms[i++].setUInt32(cbEnv);
                    paParms[i++].setPointer((void*)pvEnv, cbEnv);
                    paParms[i++].setPointer((void*)Utf8UserName.c_str(), (uint32_t)Utf8UserName.length() + 1);
                    paParms[i++].setPointer((void*)Utf8Password.c_str(), (uint32_t)Utf8Password.length() + 1);
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
                CallbackMapIter it = getCtrlCallbackContextByID(uContextID);
                BOOL fCanceled = FALSE;
                if (it != mCallbackMap.end())
                {
                    ComAssert(!it->second.pProgress.isNull());

                    /*
                     * Wait for the first stage (=0) to complete (that is starting the process).
                     */
                    PCALLBACKDATAEXECSTATUS pData = NULL;
                    rc = it->second.pProgress->WaitForOperationCompletion(0, aTimeoutMS);
                    if (SUCCEEDED(rc))
                    {
                        /* Was the operation canceled by one of the parties? */
                        rc = it->second.pProgress->COMGETTER(Canceled)(&fCanceled);
                        if (FAILED(rc)) throw rc;

                        if (!fCanceled)
                        {
                            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                            pData = (PCALLBACKDATAEXECSTATUS)it->second.pvData;
                            Assert(it->second.cbData == sizeof(CALLBACKDATAEXECSTATUS));
                            AssertPtr(pData);

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

                                case PROC_STS_UNDEFINED:
                                    vrc = VERR_TIMEOUT;    /* Operation did not complete within time. */
                                    break;

                                default:
                                    vrc = VERR_INVALID_PARAMETER; /* Unknown status, should never happen! */
                                    break;
                            }
                        }
                        else /* Operation was canceled. */
                            vrc = VERR_CANCELLED;
                    }
                    else /* Operation did not complete within time. */
                        vrc = VERR_TIMEOUT;

                    /*
                     * Do *not* remove the callback yet - we might wait with the IProgress object on something
                     * else (like end of process) ...
                     */
                    if (RT_FAILURE(vrc))
                    {
                        if (vrc == VERR_FILE_NOT_FOUND) /* This is the most likely error. */
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The file '%s' was not found on guest"), Utf8Command.c_str());
                        else if (vrc == VERR_PATH_NOT_FOUND)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The path to file '%s' was not found on guest"), Utf8Command.c_str());
                        else if (vrc == VERR_BAD_EXE_FORMAT)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The file '%s' is not an executable format on guest"), Utf8Command.c_str());
                        else if (vrc == VERR_AUTHENTICATION_FAILURE)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The specified user '%s' was not able to logon on guest"), Utf8UserName.c_str());
                        else if (vrc == VERR_TIMEOUT)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The guest did not respond within time (%ums)"), aTimeoutMS);
                        else if (vrc == VERR_CANCELLED)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The execution operation was canceled"));
                        else if (vrc == VERR_PERMISSION_DENIED)
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("Invalid user/password credentials"));
                        else
                        {
                            if (pData && pData->u32Status == PROC_STS_ERROR)
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Process could not be started: %Rrc"), pData->u32Flags);
                            else
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
                else /* Callback context not found; should never happen! */
                    AssertMsg(it != mCallbackMap.end(), ("Callback context with ID %u not found!", uContextID));
            }
            else /* HGCM related error codes .*/
            {
                if (vrc == VERR_INVALID_VM_HANDLE)
                    rc = setError(VBOX_E_VM_ERROR,
                                  tr("VMM device is not available (is the VM running?)"));
                else if (vrc == VERR_TIMEOUT)
                    rc = setError(VBOX_E_VM_ERROR,
                                  tr("The guest execution service is not ready"));
                else if (vrc == VERR_HGCM_SERVICE_NOT_FOUND)
                    rc = setError(VBOX_E_VM_ERROR,
                                  tr("The guest execution service is not available"));
                else /* HGCM call went wrong. */
                    rc = setError(E_UNEXPECTED,
                                  tr("The HGCM call failed with error %Rrc"), vrc);
            }

            for (unsigned i = 0; i < uNumArgs; i++)
                RTMemFree(papszArgv[i]);
            RTMemFree(papszArgv);
        }

        if (RT_FAILURE(vrc))
            LogRel(("Executing guest process \"%s\" as user \"%s\" failed with %Rrc\n",
                    Utf8Command.c_str(), Utf8UserName.c_str(), vrc));
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::SetProcessInput(ULONG aPID, ULONG aFlags, ComSafeArrayIn(BYTE, aData), ULONG *aBytesWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    CheckComArgOutPointerValid(aBytesWritten);

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & ProcessInputFlag_EndOfFile))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        /* Init. */
        *aBytesWritten = 0;

        /* Search for existing PID. */
        GuestProcessMapIterConst itProc = getProcessByPID(aPID);
        if (itProc != mGuestProcessMap.end())
        {
            /* PID exists; check if process is still running. */
            if (itProc->second.mStatus != PROC_STS_STARTED)
            {
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Process (PID %u) does not run anymore! Status: %ld, Flags: %u, Exit Code: %u"),
                              aPID, itProc->second.mStatus, itProc->second.mFlags, itProc->second.mExitCode);
            }
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Process (PID %u) not found!"), aPID);

        if (SUCCEEDED(rc))
        {
            /*
             * Create progress object.
             * This progress object, compared to the one in executeProgress() above
             * is only local and is used to determine whether the operation finished
             * or got canceled.
             */
            ComObjPtr <Progress> progress;
            rc = progress.createObject();
            if (SUCCEEDED(rc))
            {
                rc = progress->init(static_cast<IGuest*>(this),
                                    Bstr(tr("Setting input for process")).raw(),
                                    TRUE /* Cancelable */);
            }
            if (FAILED(rc)) return rc;

            PCALLBACKDATAEXECINSTATUS pData = (PCALLBACKDATAEXECINSTATUS)RTMemAlloc(sizeof(CALLBACKDATAEXECINSTATUS));
            AssertReturn(pData, VBOX_E_IPRT_ERROR);
            RT_ZERO(*pData);
            /* Save PID + output flags for later use. */
            pData->u32PID = aPID;
            pData->u32Flags = aFlags;
            /* Add job to callback contexts. */
            uint32_t uContextID = addCtrlCallbackContext(VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS,
                                                         pData, sizeof(CALLBACKDATAEXECINSTATUS), progress);
            Assert(uContextID > 0);

            com::SafeArray<BYTE> sfaData(ComSafeArrayInArg(aData));
            uint32_t cbSize = sfaData.size();

            VBOXHGCMSVCPARM paParms[6];
            int i = 0;
            paParms[i++].setUInt32(uContextID);
            paParms[i++].setUInt32(aPID);
            paParms[i++].setUInt32(aFlags);
            paParms[i++].setPointer(sfaData.raw(), cbSize);
            paParms[i++].setUInt32(cbSize);

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
                    vrc = vmmDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_SET_INPUT,
                                               i, paParms);
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback ...\n"));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */
                CallbackMapIter it = getCtrlCallbackContextByID(uContextID);
                BOOL fCanceled = FALSE;
                if (it != mCallbackMap.end())
                {
                    ComAssert(!it->second.pProgress.isNull());

                    /* Wait until operation completed. */
                    rc = it->second.pProgress->WaitForCompletion(UINT32_MAX /* Wait forever */);
                    if (FAILED(rc)) throw rc;

                    /* Was the operation canceled by one of the parties? */
                    rc = it->second.pProgress->COMGETTER(Canceled)(&fCanceled);
                    if (FAILED(rc)) throw rc;

                    if (!fCanceled)
                    {
                        BOOL fCompleted;
                        if (   SUCCEEDED(it->second.pProgress->COMGETTER(Completed)(&fCompleted))
                            && fCompleted)
                        {
                            PCALLBACKDATAEXECINSTATUS pStatusData = (PCALLBACKDATAEXECINSTATUS)it->second.pvData;
                            Assert(it->second.cbData == sizeof(CALLBACKDATAEXECINSTATUS));
                            AssertPtr(pStatusData);

                            *aBytesWritten = pStatusData->cbProcessed;
                        }
                    }
                    else /* Operation was canceled. */
                        vrc = VERR_CANCELLED;

                    if (RT_FAILURE(vrc))
                    {
                        if (vrc == VERR_CANCELLED)
                        {
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("The input operation was canceled"));
                        }
                        else
                        {
                            rc = setError(E_UNEXPECTED,
                                          tr("The service call failed with error %Rrc"), vrc);
                        }
                    }

                    {
                        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                        /*
                         * Destroy locally used progress object.
                         */
                        destroyCtrlCallbackContext(it);
                    }

                    /* Remove callback context (not used anymore). */
                    mCallbackMap.erase(it);
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
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessOutput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, LONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    if (aSize < 0)
        return setError(E_INVALIDARG, tr("The size argument (%lld) is negative"), aSize);
    if (aFlags != 0) /* Flags are not supported at the moment. */
        return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.
         * This progress object, compared to the one in executeProgress() above
         * is only local and is used to determine whether the operation finished
         * or got canceled.
         */
        ComObjPtr <Progress> progress;
        rc = progress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = progress->init(static_cast<IGuest*>(this),
                                Bstr(tr("Getting output of process")).raw(),
                                TRUE);
        }
        if (FAILED(rc)) return rc;

        /* Adjust timeout */
        if (aTimeoutMS == 0)
            aTimeoutMS = UINT32_MAX;

        /* Search for existing PID. */
        PCALLBACKDATAEXECOUT pData = (CALLBACKDATAEXECOUT*)RTMemAlloc(sizeof(CALLBACKDATAEXECOUT));
        AssertReturn(pData, VBOX_E_IPRT_ERROR);
        RT_ZERO(*pData);
        /* Save PID + output flags for later use. */
        pData->u32PID = aPID;
        pData->u32Flags = aFlags;
        /* Add job to callback contexts. */
        uint32_t uContextID = addCtrlCallbackContext(VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT,
                                                     pData, sizeof(CALLBACKDATAEXECOUT), progress);
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
            CallbackMapIter it = getCtrlCallbackContextByID(uContextID);
            BOOL fCanceled = FALSE;
            if (it != mCallbackMap.end())
            {
                ComAssert(!it->second.pProgress.isNull());

                /* Wait until operation completed. */
                rc = it->second.pProgress->WaitForCompletion(aTimeoutMS);
                if (FAILED(rc)) throw rc;

                /* Was the operation canceled by one of the parties? */
                rc = it->second.pProgress->COMGETTER(Canceled)(&fCanceled);
                if (FAILED(rc)) throw rc;

                if (!fCanceled)
                {
                    BOOL fCompleted;
                    if (   SUCCEEDED(it->second.pProgress->COMGETTER(Completed)(&fCompleted))
                        && fCompleted)
                    {
                        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                        /* Did we get some output? */
                        pData = (PCALLBACKDATAEXECOUT)it->second.pvData;
                        Assert(it->second.cbData == sizeof(CALLBACKDATAEXECOUT));
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
                {
                    vrc = VERR_CANCELLED;
                }

                if (RT_FAILURE(vrc))
                {
                    if (vrc == VERR_NO_DATA)
                    {
                        /* This is not an error we want to report to COM. */
                        rc = S_OK;
                    }
                    else if (vrc == VERR_TIMEOUT)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The guest did not output within time (%ums)"), aTimeoutMS);
                    }
                    else if (vrc == VERR_CANCELLED)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("The output operation was canceled"));
                    }
                    else
                    {
                        rc = setError(E_UNEXPECTED,
                                      tr("The service call failed with error %Rrc"), vrc);
                    }
                }

                {
                    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                    /*
                     * Destroy locally used progress object.
                     */
                    destroyCtrlCallbackContext(it);
                }

                /* Remove callback context (not used anymore). */
                mCallbackMap.erase(it);
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

        GuestProcessMapIterConst it = getProcessByPID(aPID);
        if (it != mGuestProcessMap.end())
        {
            *aExitCode = it->second.mExitCode;
            *aFlags = it->second.mFlags;
            *aStatus = it->second.mStatus;
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

/** @todo For having a progress object which actually reports something,
  *       the actual copy loop (see below) needs to go to some worker thread
  *       so that this routine can return to the caller (and the caller then
  *       can do display a progress). */
STDMETHODIMP Guest::CopyToGuest(IN_BSTR aSource, IN_BSTR aDest,
                                IN_BSTR aUserName, IN_BSTR aPassword,
                                ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != CopyFileFlag_None)
    {
        if (   !(aFlags & CopyFileFlag_Recursive)
            && !(aFlags & CopyFileFlag_Update)
            && !(aFlags & CopyFileFlag_FollowLinks))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    HRESULT rc = S_OK;

    try
    {
        Utf8Str Utf8Source(aSource);
        Utf8Str Utf8Dest(aDest);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);

        /* Does our source file exist? */
        if (!RTFileExists(Utf8Source.c_str()))
        {
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Source file \"%s\" does not exist"), Utf8Source.c_str());
        }
        else
        {
            RTFILE fileSource;
            int vrc = RTFileOpen(&fileSource, Utf8Source.c_str(),
                                 RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
            if (RT_FAILURE(vrc))
            {
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Could not open source file \"%s\" for reading, rc=%Rrc"),
                              Utf8Source.c_str(),  vrc);
            }
            else
            {
                uint64_t cbSize;
                vrc = RTFileGetSize(fileSource, &cbSize);
                if (RT_FAILURE(vrc))
                {
                    rc = setError(VBOX_E_IPRT_ERROR,
                                  tr("Could not query file size of \"%s\", rc=%Rrc"),
                                  Utf8Source.c_str(),  vrc);
                }
                else
                {
                    com::SafeArray<IN_BSTR> args;
                    com::SafeArray<IN_BSTR> env;

                    /*
                     * Prepare tool command line.
                     */
                    char szOutput[RTPATH_MAX];
                    if (RTStrPrintf(szOutput, sizeof(szOutput), "--output=%s", Utf8Dest.c_str()))
                    {
                        args.push_back(Bstr("vbox_cat").raw()); /* The actual (internal) tool to use (as argv[0]). */
                        args.push_back(Bstr(szOutput).raw());   /* We want to write a file ... */
                    }
                    else
                        rc = setError(VBOX_E_IPRT_ERROR, tr("Error preparing command line"));

                    ComPtr<IProgress> execProgress;
                    ULONG uPID;
                    if (SUCCEEDED(rc))
                    {
                        LogRel(("Copying file \"%s\" to guest \"%s\" ...\n",
                                Utf8Source.c_str(), Utf8Dest.c_str()));

                        /*
                         * Okay, since we gathered all stuff we need until now to start the
                         * actual copying, start the guest part now.
                         */
                        rc = ExecuteProcess(Bstr("vbox_cat").raw(), 0 /* Flags */,
                                            ComSafeArrayAsInParam(args),
                                            ComSafeArrayAsInParam(env),
                                            Bstr(Utf8UserName).raw(),
                                            Bstr(Utf8Password).raw(), 0 /* Timeout */,
                                            &uPID, execProgress.asOutParam());
                    }

                    if (SUCCEEDED(rc))
                    {
                        /* Wait for process to exit ... */
                        BOOL fCompleted = FALSE;
                        BOOL fCanceled = FALSE;

                        size_t cbRead;
                        SafeArray<BYTE> aInputData(_64K);
                        while (   SUCCEEDED(execProgress->COMGETTER(Completed(&fCompleted)))
                               && !fCompleted)
                        {
                            vrc = RTFileRead(fileSource, (uint8_t*)aInputData.raw(), _64K, &cbRead);
                            if (   cbRead == 0
                                || vrc == VERR_EOF)
                                break;

                            aInputData.resize(cbRead);

                            /* Did we reach the end of the content
                             * we want to transfer (last chunk)? */
                            ULONG uFlags = ProcessInputFlag_None;
                            if (cbRead < _64K)
                                uFlags |= ProcessInputFlag_EndOfFile;

                            /* Transfer the current chunk ... */
                            ULONG uBytesWritten;
                            rc = SetProcessInput(uPID, uFlags,
                                                 ComSafeArrayAsInParam(aInputData), &uBytesWritten);
                            if (FAILED(rc))
                                break;

                            /* Progress canceled by Main API? */
                            if (   SUCCEEDED(execProgress->COMGETTER(Canceled(&fCanceled)))
                                && fCanceled)
                            {
                                break;
                            }
                        }

                        if (SUCCEEDED(rc))
                        {
                            /* Return the progress to the caller. */
                            execProgress.queryInterfaceTo(aProgress);
                        }
                    }
                }
                RTFileClose(fileSource);
            }
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Sets the general Guest Additions information like
 * API (interface) version and OS type.  Gets called by
 * vmmdevUpdateGuestInfo.
 *
 * @param aInterfaceVersion
 * @param aOsType
 */
void Guest::setAdditionsInfo(Bstr aInterfaceVersion, VBOXOSTYPE aOsType)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Note: The Guest Additions API (interface) version is deprecated
     * and will not be used anymore!  We might need it to at least report
     * something as version number if *really* ancient Guest Additions are
     * installed (without the guest version + revision properties having set).
     */
    mData.mInterfaceVersion = aInterfaceVersion;

    /*
     * Older Additions rely on the Additions API version whether they
     * are assumed to be active or not.  Since newer Additions do report
     * the Additions version *before* calling this function (by calling
     * VMMDevReportGuestInfo2, VMMDevReportGuestStatus, VMMDevReportGuestInfo,
     * in that order) we can tell apart old and new Additions here. Old
     * Additions never would set VMMDevReportGuestInfo2 (which set mData.mAdditionsVersion)
     * so they just rely on the aInterfaceVersion string (which gets set by
     * VMMDevReportGuestInfo).
     *
     * So only mark the Additions as being active (run level = system) when we
     * don't have the Additions version set.
     */
    if (mData.mAdditionsVersion.isEmpty())
    {
        if (aInterfaceVersion.isEmpty())
            mData.mAdditionsRunLevel = AdditionsRunLevelType_None;
        else
            mData.mAdditionsRunLevel = AdditionsRunLevelType_System;
    }

    /*
     * Older Additions didn't have this finer grained capability bit,
     * so enable it by default.  Newer Additions will not enable this here
     * and use the setSupportedFeatures function instead.
     */
    mData.mSupportsGraphics = mData.mAdditionsRunLevel > AdditionsRunLevelType_None;

    /*
     * Note! There is a race going on between setting mAdditionsRunLevel and
     * mSupportsGraphics here and disabling/enabling it later according to
     * its real status when using new(er) Guest Additions.
     */
    mData.mOSTypeId = Global::OSTypeId (aOsType);
}

/**
 * Sets the Guest Additions version information details.
 * Gets called by vmmdevUpdateGuestInfo2.
 *
 * @param aAdditionsVersion
 * @param aVersionName
 */
void Guest::setAdditionsInfo2(Bstr aAdditionsVersion, Bstr aVersionName, Bstr aRevision)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!aVersionName.isEmpty())
        /*
         * aVersionName could be "x.y.z_BETA1_FOOBAR", so append revision manually to
         * become "x.y.z_BETA1_FOOBARr12345".
         */
        mData.mAdditionsVersion = BstrFmt("%ls r%ls", aVersionName.raw(), aRevision.raw());
    else /* aAdditionsVersion is in x.y.zr12345 format. */
        mData.mAdditionsVersion = aAdditionsVersion;
}

/**
 * Sets the status of a certain Guest Additions facility.
 * Gets called by vmmdevUpdateGuestStatus.
 *
 * @param Facility
 * @param Status
 * @param ulFlags
 */
void Guest::setAdditionsStatus(VBoxGuestStatusFacility Facility, VBoxGuestStatusCurrent Status, ULONG ulFlags)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint32_t uCurFacility = Facility + (Status == VBoxGuestStatusCurrent_Active ? 0 : -1);

    /* First check for disabled status. */
    if (   Facility < VBoxGuestStatusFacility_VBoxGuestDriver
        || (   Facility == VBoxGuestStatusFacility_All
            && (   Status   == VBoxGuestStatusCurrent_Inactive
                || Status   == VBoxGuestStatusCurrent_Disabled
               )
           )
       )
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_None;
    }
    else if (uCurFacility >= VBoxGuestStatusFacility_VBoxTray)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_Desktop;
    }
    else if (uCurFacility >= VBoxGuestStatusFacility_VBoxService)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_Userland;
    }
    else if (uCurFacility >= VBoxGuestStatusFacility_VBoxGuestDriver)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_System;
    }
    else /* Should never happen! */
        AssertMsgFailed(("Invalid facility status/run level detected! uCurFacility=%ld\n", uCurFacility));
}

/**
 * Sets the supported features (and whether they are active or not).
 *
 * @param   fCaps       Guest capability bit mask (VMMDEV_GUEST_SUPPORTS_XXX).
 * @param   fActive     No idea what this is supposed to be, it's always 0 and
 *                      not references by this method.
 */
void Guest::setSupportedFeatures(uint32_t fCaps, uint32_t fActive)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mSupportsSeamless = (fCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS);
    /** @todo Add VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING */
    mData.mSupportsGraphics = (fCaps & VMMDEV_GUEST_SUPPORTS_GRAPHICS);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
