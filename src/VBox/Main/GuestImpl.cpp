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
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/HostServices/GuestControlSvc.h>
# include <VBox/com/array.h>
#endif
#include <iprt/cpp/utils.h>

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

    ULONG aStatUpdateInterval;
    ret = mParent->machine()->COMGETTER(StatisticsUpdateInterval)(&aStatUpdateInterval);
    if (ret == S_OK)
        mStatUpdateInterval = aStatUpdateInterval;
    else
        mStatUpdateInterval = 0;                    /* Default is not to report guest statistics at all */

    /* invalidate all stats */
    for (int i=0;i<GuestStatisticType_MaxVal;i++)
        mCurrentGuestStat[i] = GUEST_STAT_INVALID;

    /* start with sample 0 */
    mCurrentGuestStat[GuestStatisticType_SampleNumber] = 0;
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowThisFunc(("\n"));

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

STDMETHODIMP Guest::COMGETTER(StatisticsUpdateInterval) (ULONG *aUpdateInterval)
{
    CheckComArgOutPointerValid(aUpdateInterval);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aUpdateInterval = mStatUpdateInterval;

    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(StatisticsUpdateInterval) (ULONG aUpdateInterval)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT ret = mParent->machine()->COMSETTER(StatisticsUpdateInterval)(aUpdateInterval);
    if (ret == S_OK)
    {
        mStatUpdateInterval = aUpdateInterval;
        /* forward the information to the VMM device */
        VMMDev *vmmDev = mParent->getVMMDev();
        if (vmmDev)
            vmmDev->getVMMDevPort()->pfnSetStatisticsInterval(vmmDev->getVMMDevPort(), aUpdateInterval);
    }

    return ret;
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

STDMETHODIMP Guest::GetStatistic(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG *aStatVal)
{
    CheckComArgExpr(aCpuId, aCpuId == 0);
    CheckComArgExpr(aStatistic, aStatistic < GuestStatisticType_MaxVal);
    CheckComArgOutPointerValid(aStatVal);

    /* Not available or not yet reported? In that case, just return with a proper error
     * but don't use setError(). */
    if (mCurrentGuestStat[aStatistic] == GUEST_STAT_INVALID)
        return E_INVALIDARG;

    *aStatVal = mCurrentGuestStat[aStatistic];
    return S_OK;
}

STDMETHODIMP Guest::SetStatistic(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG aStatVal)
{
    CheckComArgExpr(aCpuId, aCpuId == 0);
    CheckComArgExpr(aStatistic, aStatistic < GuestStatisticType_MaxVal);

    /* internal method assumes that the caller knows what he's doing (no boundary checks) */
    mCurrentGuestStat[aStatistic] = aStatVal;
    return S_OK;
}

HRESULT Guest::prepareExecuteArgs(ComSafeArrayIn(IN_BSTR, aArguments),
                                  char **ppszArgv, uint32_t *pcbList, uint32_t *pcArgs)
{
    com::SafeArray<IN_BSTR> args(ComSafeArrayInArg(aArguments));
    char *pszArgs;
    const char *pszCurArg;

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    for (unsigned i = 0; i < args.size(); ++i)
    {
        if (i > 0) /* Insert space as delimiter. */
            vrc = RTStrAAppendN(&pszArgs, " ", 1);
        if (RT_SUCCESS(vrc))
        {
            pszCurArg = Utf8Str(args[i]).raw();
            vrc = RTStrAAppendN(&pszArgs, pszCurArg, strlen(pszCurArg));
        }
        if (RT_FAILURE(vrc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        *ppszArgv = pszArgs;
        *pcArgs = args.size();
        *pcbList = strlen(pszArgs) + 1; /* Include zero termination. */
    }
    else
    {
        rc = setError(E_UNEXPECTED,
                              tr("The service call failed with the error %Rrc"),
                              vrc);
    }
    return rc;
}

HRESULT Guest::prepareExecuteEnv(ComSafeArrayIn(IN_BSTR, aEnvironment),
                                 void **ppvList, uint32_t *pcbList, uint32_t *pcEnv)
{
    com::SafeArray<IN_BSTR> env(ComSafeArrayInArg(aEnvironment));
    const char *pszCurEnv;
    for (unsigned i = 0; i < env.size(); ++i)
    {
    }

    return S_OK;
}

STDMETHODIMP Guest::ExecuteProgram(IN_BSTR aExecName, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aStdIn, IN_BSTR aStdOut, IN_BSTR aStdErr,
                                   IN_BSTR aUserName, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG* aPID)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aExecName);
    CheckComArgOutPointerValid(aPID);
    /* Flags are not supported at the moment. */
    if (aFlags != 0)
        return E_INVALIDARG;

    HRESULT rc = S_OK;

    try
    {
        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
       
        /* Just be on the safe side when calling another process. */
        alock.leave();
     
        HRESULT rc = E_UNEXPECTED;
        using namespace guestControl;
    
        int vrc = VINF_SUCCESS; 
        Utf8Str Utf8ExecName(aExecName);

        /* Prepare arguments. */
        char *pszArgs;
        uint32_t cNumArgs;
        uint32_t cbArgs;
        rc = prepareExecuteArgs(aArguments,
                                &pszArgs, &cbArgs, &cNumArgs);
        if (SUCCEEDED(rc))
        {
            /* Prepare environment. */
            /** @todo */
            if (SUCCEEDED(rc))
            {
                Utf8Str Utf8StdIn(aStdIn);
                Utf8Str Utf8StdOut(aStdOut);
                Utf8Str Utf8StdErr(aStdErr);
                Utf8Str Utf8UserName(aUserName);
                Utf8Str Utf8Password(aPassword);
            
                /* Call the stub which does the actual work. */
                if (RT_SUCCESS(vrc))
                {
                    VBOXHGCMSVCPARM paParms[13];
                    uint32_t cParms = 13;

                    /* Forward the information to the VMM device. */
                    AssertPtr(mParent);
                    VMMDev *vmmDev = mParent->getVMMDev();
                    if (vmmDev)
                    {
                        vrc = vmmDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_CMD,
                                                   cParms, &paParms[0]);
                    }
                }
                //RTMemFree(pszEnv);
            }
            RTStrFree(pszArgs);
        }
        if (RT_SUCCESS(vrc))
            rc = S_OK;
        else
            rc = setError(E_UNEXPECTED,
                          tr("The service call failed with the error %Rrc"),
                          vrc);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    };

    return rc;    
#endif /* VBOX_WITH_GUEST_CONTROL */
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
