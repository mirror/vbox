/* $Id$ */
/** @file
 * IProgress implementation for Machine::openRemoteSession in VBoxSVC.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/types.h>

#if defined (VBOX_WITH_XPCOM)
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>
#endif /* defined (VBOX_WITH_XPCOM) */

#include "ProgressProxyImpl.h"

#include "VirtualBoxImpl.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Logging.h"

#include <iprt/time.h>
#include <iprt/semaphore.h>

#include <VBox/err.h>

////////////////////////////////////////////////////////////////////////////////
// ProgressProxy class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor / uninitializer
////////////////////////////////////////////////////////////////////////////////


HRESULT ProgressProxy::FinalConstruct()
{
    mcOtherProgressObjects = 1;
    miCurOtherProgressObject = 0;

    HRESULT rc = Progress::FinalConstruct();
    return rc;
}

/**
 * Initalize it as a one operation Progress object.
 *
 * This is used by SessionMachine::OnSessionEnd.
 */
HRESULT ProgressProxy::init(
#if !defined (VBOX_COM_INPROC)
                            VirtualBox *pParent,
#endif
                            IUnknown *pInitiator,
                            CBSTR bstrDescription,
                            BOOL fCancelable)
{
    miCurOtherProgressObject = 0;
    mcOtherProgressObjects = 0;

    return Progress::init(
#if !defined (VBOX_COM_INPROC)
                          pParent,
#endif
                          pInitiator,
                          bstrDescription,
                          fCancelable,
                          1 /* cOperations */,
                          1 /* ulTotalOperationsWeight */,
                          bstrDescription /* bstrFirstOperationDescription */,
                          1 /* ulFirstOperationWeight */,
                          NULL /* pId */);
}

/**
 * Initialize for proxying one or more other objects, assuming we start out and
 * end without proxying anyone.
 *
 * The user must call clearOtherProgressObject when there are no more progress
 * objects to be proxied or we'll leave threads waiting forever.
 */
HRESULT ProgressProxy::init(
#if !defined (VBOX_COM_INPROC)
                            VirtualBox *pParent,
#endif
                            IUnknown *pInitiator,
                            CBSTR bstrDescription,
                            BOOL fCancelable,
                            ULONG cOtherProgressObjects,
                            ULONG uTotalOperationsWeight,
                            CBSTR bstrFirstOperationDescription,
                            ULONG uFirstOperationWeight,
                            OUT_GUID pId)
{
    miCurOtherProgressObject = 0;
    mcOtherProgressObjects = cOtherProgressObjects;

    return Progress::init(
#if !defined (VBOX_COM_INPROC)
                          pParent,
#endif
                          pInitiator,
                          bstrDescription,
                          fCancelable,
                          1 + cOtherProgressObjects + 1 /* cOperations */,
                          uTotalOperationsWeight,
                          bstrFirstOperationDescription,
                          uFirstOperationWeight,
                          pId);
}

void ProgressProxy::FinalRelease()
{
    uninit();
    miCurOtherProgressObject = 0;
}

void ProgressProxy::uninit()
{
    LogFlowThisFunc(("\n"));

    mptrOtherProgress.setNull();
    Progress::uninit();
}

// Public methods
////////////////////////////////////////////////////////////////////////////////

/** Just a wrapper so we can automatically do the handover before setting
 *  the result locally. */
HRESULT ProgressProxy::setResultCode(HRESULT aResultCode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    clearOtherProgressObjectInternal(true /* fEarly */);
    return Progress::setResultCode(aResultCode);
}

/** Just a wrapper so we can automatically do the handover before setting
 *  the result locally. */
HRESULT ProgressProxy::notifyComplete(HRESULT aResultCode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    clearOtherProgressObjectInternal(true /* fEarly */);
    return Progress::notifyComplete(aResultCode);
}

/** Just a wrapper so we can automatically do the handover before setting
 *  the result locally. */
HRESULT ProgressProxy::notifyComplete(HRESULT aResultCode,
                                      const GUID &aIID,
                                      const Bstr &aComponent,
                                      const char *aText,
                                      ...)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    clearOtherProgressObjectInternal(true /* fEarly */);

    va_list va;
    va_start(va, aText);
    HRESULT hrc = Progress::notifyCompleteV(aResultCode, aIID, aComponent, aText, va);
    va_end(va);
    return hrc;
}

/** Just a wrapper so we can automatically do the handover before setting
 *  the result locally. */
bool    ProgressProxy::notifyPointOfNoReturn(void)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    clearOtherProgressObjectInternal(true /* fEarly */);
    return Progress::notifyPointOfNoReturn();
}

/**
 * Sets the other progress object unless the operation has been completed /
 * canceled already.
 *
 * @returns false if failed/canceled, true if not.
 * @param   pOtherProgress      The other progress object. Must not be NULL.
 * @param   uOperationWeight    The weight of this operation.  (The description
 *                              is taken from the other progress object.)
 */
bool ProgressProxy::setOtherProgressObject(IProgress *pOtherProgress, ULONG uOperationWeight)
{
    LogFlowThisFunc(("setOtherProgressObject: %p %u\n", pOtherProgress, uOperationWeight));
    ComPtr<IProgress> ptrOtherProgress = pOtherProgress;

    /* Get the description first. */
    Bstr    bstrOperationDescription;
    HRESULT hrc = pOtherProgress->COMGETTER(Description)(bstrOperationDescription.asOutParam());
    if (FAILED(hrc))
        bstrOperationDescription = "oops";

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Do the hand over from any previous progress object. */
    clearOtherProgressObjectInternal(false /*fEarly*/);
    BOOL fCompletedOrCanceled = mCompleted || mCanceled;
    if (!fCompletedOrCanceled)
    {
        /* Advance to the next object and operation, checking for cancelation
           and completion right away to be on the safe side. */
        Assert(miCurOtherProgressObject < mcOtherProgressObjects);
        mptrOtherProgress = ptrOtherProgress;

        Progress::SetNextOperation(bstrOperationDescription, uOperationWeight);

        BOOL f;
        hrc = ptrOtherProgress->COMGETTER(Completed)(&f);
        fCompletedOrCanceled = FAILED(hrc) || f;

        if (!fCompletedOrCanceled)
        {
            hrc = ptrOtherProgress->COMGETTER(Canceled)(&f);
            fCompletedOrCanceled = SUCCEEDED(hrc) && f;
        }

        if (fCompletedOrCanceled)
        {
            LogFlowThisFunc(("Other object completed or canceled, clearing...\n"));
            clearOtherProgressObjectInternal(false /*fEarly*/);
        }
        else
        {
            /* Mirror the cancelable property. */
            if (mCancelable)
            {
                hrc = ptrOtherProgress->COMGETTER(Cancelable)(&f);
                if (SUCCEEDED(hrc) && !f)
                {
                    LogFlowThisFunc(("The other progress object is not cancelable\n"));
                    mCancelable = FALSE;
                }
            }
        }
    }
    else
    {
        LogFlowThisFunc(("mCompleted=%RTbool mCanceled=%RTbool - Canceling the other progress object!\n",
                         mCompleted, mCanceled));
        hrc = ptrOtherProgress->Cancel();
        LogFlowThisFunc(("Cancel -> %Rhrc", hrc));
    }

    LogFlowThisFunc(("Returns %RTbool\n", !fCompletedOrCanceled));
    return !fCompletedOrCanceled;
}

/**
 * Clears the last other progress objects.
 *
 * @returns false if failed/canceled, true if not.
 * @param   pszLastOperationDescription     The description of the final bit.
 * @param   uLastOperationWeight            The weight of the final bit.
 */
bool ProgressProxy::clearOtherProgressObject(const char *pszLastOperationDescription, ULONG uLastOperationWeight)
{
    LogFlowThisFunc(("%p %u\n", pszLastOperationDescription, uLastOperationWeight));
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    clearOtherProgressObjectInternal(false /* fEarly */);

    /* Advance to the next operation if applicable. */
    bool fCompletedOrCanceled = mCompleted || mCanceled;
    if (!fCompletedOrCanceled)
        Progress::SetNextOperation(Bstr(pszLastOperationDescription), uLastOperationWeight);

    LogFlowThisFunc(("Returns %RTbool\n", !fCompletedOrCanceled));
    return !fCompletedOrCanceled;
}

// Internal methods.
////////////////////////////////////////////////////////////////////////////////


/**
 * Internal version of clearOtherProgressObject that doesn't advance to the next
 * operation.
 *
 * This is used both by clearOtherProgressObject as well as a number of places
 * where we automatically do the hand over because of failure/completion.
 *
 * @param   fEarly          Early clearing or not.
 */
void ProgressProxy::clearOtherProgressObjectInternal(bool fEarly)
{
    if (!mptrOtherProgress.isNull())
    {
        ComPtr<IProgress> ptrOtherProgress = mptrOtherProgress;
        mptrOtherProgress.setNull();
        copyProgressInfo(ptrOtherProgress, fEarly);

        miCurOtherProgressObject++;
        Assert(miCurOtherProgressObject <= mcOtherProgressObjects);
    }
}

/**
 * Called to copy over the progress information from @a pOtherProgress.
 *
 * @param   pOtherProgress  The source of the information.
 * @param   fEarly          Early copy.
 *
 * @note    The caller owns the write lock and as cleared mptrOtherProgress
 *          already (or we might recurse forever)!
 */
void ProgressProxy::copyProgressInfo(IProgress *pOtherProgress, bool fEarly)
{
    HRESULT hrc;
    LogFlowThisFunc(("\n"));

    /*
     * No point in doing this if the progress object was canceled already.
     */
    if (!mCanceled)
    {
        /* Detect if the other progress object was canceled. */
        BOOL fCanceled;
        hrc = pOtherProgress->COMGETTER(Canceled)(&fCanceled); AssertComRC(hrc);
        if (FAILED(hrc))
            fCanceled = FALSE;
        if (fCanceled)
        {
            LogFlowThisFunc(("Canceled\n"));
            mCanceled = TRUE;
            if (m_pfnCancelCallback)
                m_pfnCancelCallback(m_pvCancelUserArg);
        }
        else
        {
            /* Has it completed? */
            BOOL fCompleted;
            hrc = pOtherProgress->COMGETTER(Completed)(&fCompleted); AssertComRC(hrc);
            if (FAILED(hrc))
                fCompleted = TRUE;
            Assert(fCompleted || fEarly);
            if (fCompleted)
            {
                /* Check the result. */
                LONG hrcResult;
                hrc = pOtherProgress->COMGETTER(ResultCode)(&hrcResult); AssertComRC(hrc);
                if (FAILED(hrc))
                    hrcResult = hrc;
                if (SUCCEEDED((HRESULT)hrcResult))
                    LogFlowThisFunc(("Succeeded\n"));
                else
                {
                    /* Get the error information. */
                    ComPtr<IVirtualBoxErrorInfo> ptrErrorInfo;
                    hrc = pOtherProgress->COMGETTER(ErrorInfo)(ptrErrorInfo.asOutParam());
                    if (SUCCEEDED(hrc))
                    {
                        Bstr bstrIID;
                        hrc = ptrErrorInfo->COMGETTER(InterfaceID)(bstrIID.asOutParam());
                        if (FAILED(hrc))
                            bstrIID.setNull();

                        Bstr bstrComponent;
                        hrc = ptrErrorInfo->COMGETTER(Component)(bstrComponent.asOutParam());
                        if (FAILED(hrc))
                            bstrComponent = "failed";

                        Bstr bstrText;
                        hrc = ptrErrorInfo->COMGETTER(Text)(bstrText.asOutParam());
                        if (FAILED(hrc))
                            bstrText = "<failed>";

                        Utf8Str strText(bstrText);
                        LogFlowThisFunc(("Got ErrorInfo(%s); hrcResult=%Rhrc\n", strText.c_str(), hrcResult));
                        Progress::notifyComplete((HRESULT)hrcResult, Guid(bstrIID), bstrComponent, "%s", strText.c_str());
                    }
                    else
                    {
                        LogFlowThisFunc(("ErrorInfo failed with hrc=%Rhrc; hrcResult=%Rhrc\n", hrc, hrcResult));
                        Progress::notifyComplete((HRESULT)hrcResult);
                    }
                }
            }
            else
                LogFlowThisFunc(("Not completed\n"));
        }
    }
    else
        LogFlowThisFunc(("Already canceled\n"));

    /*
     * Did cancelable state change (point of no return)?
     */
    if (mCancelable)
    {
        BOOL fCancelable;
        hrc = pOtherProgress->COMGETTER(Cancelable)(&fCancelable); AssertComRC(hrc);
        if (SUCCEEDED(hrc) && !fCancelable)
        {
            LogFlowThisFunc(("point-of-no-return reached\n"));
            mCancelable = FALSE;
        }
    }
}


// IProgress properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ProgressProxy::COMGETTER(Percent)(ULONG *aPercent)
{
#if 0
    CheckComArgOutPointerValid(aPercent);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(rc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (mptrOtherProgress.isNull())
            hrc = Progress::COMGETTER(Percent)(aPercent);
        else
        {
            ULONG uPct;
            hrc = mptrOtherProgress->COMGETTER(Percent)(&uPct);
            ....
        }
    }
    return hrc;
#else
    return Progress::COMGETTER(Percent)(aPercent);
#endif
}

STDMETHODIMP ProgressProxy::COMGETTER(Completed)(BOOL *aCompleted)
{
    /* Not proxied since we EXPECT a hand back call. */
    return Progress::COMGETTER(Completed)(aCompleted);
}

STDMETHODIMP ProgressProxy::COMGETTER(Canceled)(BOOL *aCanceled)
{
    CheckComArgOutPointerValid(aCanceled);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        /* Check the local data first, then the other object. */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = Progress::COMGETTER(Canceled)(aCanceled);
        if (   SUCCEEDED(hrc)
            && !*aCanceled
            && !mptrOtherProgress.isNull())
        {
            hrc = mptrOtherProgress->COMGETTER(Canceled)(aCanceled);
            if (SUCCEEDED(hrc) && *aCanceled)
                clearOtherProgressObjectInternal(true /*fEarly*/);
        }
    }
    return hrc;
}

STDMETHODIMP ProgressProxy::COMGETTER(ResultCode)(LONG *aResultCode)
{
    /* Not proxied yet since we EXPECT a hand back call. */
    return Progress::COMGETTER(ResultCode)(aResultCode);
}

STDMETHODIMP ProgressProxy::COMGETTER(ErrorInfo)(IVirtualBoxErrorInfo **aErrorInfo)
{
    /* Not proxied yet since we EXPECT a hand back call. */
    return Progress::COMGETTER(ErrorInfo)(aErrorInfo);
}

STDMETHODIMP ProgressProxy::COMGETTER(OperationPercent)(ULONG *aOperationPercent)
{
    /* Not proxied, should be proxied later on. */
    return Progress::COMGETTER(OperationPercent)(aOperationPercent);
}

STDMETHODIMP ProgressProxy::COMSETTER(Timeout)(ULONG aTimeout)
{
    /* Not currently supported. */
    NOREF(aTimeout);
    AssertFailed();
    return E_NOTIMPL;
}

STDMETHODIMP ProgressProxy::COMGETTER(Timeout)(ULONG *aTimeout)
{
    /* Not currently supported. */
    CheckComArgOutPointerValid(aTimeout);

    AssertFailed();
    return E_NOTIMPL;
}

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ProgressProxy::WaitForCompletion(LONG aTimeout)
{
    HRESULT hrc;
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aTimeout=%d\n", aTimeout));

    /* For now we'll always block locally. */
    hrc = Progress::WaitForCompletion(aTimeout);

    LogFlowThisFuncLeave();
    return hrc;
}

STDMETHODIMP ProgressProxy::WaitForOperationCompletion(ULONG aOperation, LONG aTimeout)
{
    HRESULT hrc;
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aOperation=%d aTimeout=%d\n", aOperation, aTimeout));

    /* For now we'll always block locally. Later though, we could consider
       blocking remotely when we can. */
    hrc = Progress::WaitForOperationCompletion(aOperation, aTimeout);

    LogFlowThisFuncLeave();
    return hrc;
}

STDMETHODIMP ProgressProxy::Cancel()
{
    LogFlowThisFunc(("\n"));
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (mCancelable)
        {
            if (!mptrOtherProgress.isNull())
            {
                hrc = mptrOtherProgress->Cancel();
                if (SUCCEEDED(hrc))
                {
                    if (m_pfnCancelCallback)
                        m_pfnCancelCallback(m_pvCancelUserArg);
                    clearOtherProgressObjectInternal(true /*fEarly*/);
                }
            }
            else
                hrc = Progress::Cancel();
        }
        else
            hrc = setError(E_FAIL, tr("Operation cannot be canceled"));
    }

    LogFlowThisFunc(("returns %Rhrc\n", hrc));
    return hrc;
}

STDMETHODIMP ProgressProxy::SetCurrentOperationProgress(ULONG aPercent)
{
    /* Not supported - why do we actually expose this? */
    NOREF(aPercent);
    return E_NOTIMPL;
}

STDMETHODIMP ProgressProxy::SetNextOperation(IN_BSTR bstrNextOperationDescription, ULONG ulNextOperationsWeight)
{
    /* Not supported - why do we actually expose this? */
    NOREF(bstrNextOperationDescription);
    NOREF(ulNextOperationsWeight);
    return E_NOTIMPL;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */

