/* $Id$ */
/** @file
 *
 * VirtualBox Progress COM class implementation
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

#include <iprt/types.h>

#if defined(VBOX_WITH_XPCOM)
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>
#endif /* defined(VBOX_WITH_XPCOM) */

#include "ProgressCombinedImpl.h"

#include "VirtualBoxImpl.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Logging.h"

#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>

////////////////////////////////////////////////////////////////////////////////
// ProgressBase class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

ProgressBase::ProgressBase()
#if !defined(VBOX_COM_INPROC)
    : mParent(NULL)
#endif
{
}

ProgressBase::~ProgressBase()
{
}


/**
 * Subclasses must call this method from their FinalConstruct() implementations.
 */
HRESULT ProgressBase::FinalConstruct()
{
    mCancelable = FALSE;
    mCompleted = FALSE;
    mCanceled = FALSE;
    mResultCode = S_OK;

    m_cOperations
        = m_ulTotalOperationsWeight
        = m_ulOperationsCompletedWeight
        = m_ulCurrentOperation
        = m_ulCurrentOperationWeight
        = m_ulOperationPercent
        = m_cMsTimeout
        = 0;

    // get creation timestamp
    m_ullTimestamp = RTTimeMilliTS();

    m_pfnCancelCallback = NULL;
    m_pvCancelUserArg = NULL;

    return BaseFinalConstruct();
}

// protected initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the progress base object.
 *
 * Subclasses should call this or any other #protectedInit() method from their
 * init() implementations.
 *
 * @param aAutoInitSpan AutoInitSpan object instantiated by a subclass.
 * @param aParent       Parent object (only for server-side Progress objects).
 * @param aInitiator    Initiator of the task (for server-side objects. Can be
 *                      NULL which means initiator = parent, otherwise must not
 *                      be NULL).
 * @param aDescription  ask description.
 * @param aID           Address of result GUID structure (optional).
 *
 * @return              COM result indicator.
 */
HRESULT ProgressBase::protectedInit(AutoInitSpan &aAutoInitSpan,
#if !defined(VBOX_COM_INPROC)
                                    VirtualBox *aParent,
#endif
                                    IUnknown *aInitiator,
                                    CBSTR aDescription,
                                    OUT_GUID aId /* = NULL */)
{
    /* Guarantees subclasses call this method at the proper time */
    NOREF(aAutoInitSpan);

    AutoCaller autoCaller(this);
    AssertReturn(autoCaller.state() == InInit, E_FAIL);

#if !defined(VBOX_COM_INPROC)
    AssertReturn(aParent, E_INVALIDARG);
#else
    AssertReturn(aInitiator, E_INVALIDARG);
#endif

    AssertReturn(aDescription, E_INVALIDARG);

#if !defined(VBOX_COM_INPROC)
    /* share parent weakly */
    unconst(mParent) = aParent;
#endif

#if !defined(VBOX_COM_INPROC)
    /* assign (and therefore addref) initiator only if it is not VirtualBox
     * (to avoid cycling); otherwise mInitiator will remain null which means
     * that it is the same as the parent */
    if (aInitiator)
    {
        ComObjPtr<VirtualBox> pVirtualBox(mParent);
        if (!(pVirtualBox == aInitiator))
            unconst(mInitiator) = aInitiator;
    }
#else
    unconst(mInitiator) = aInitiator;
#endif

    unconst(mId).create();
    if (aId)
        mId.cloneTo(aId);

#if !defined(VBOX_COM_INPROC)
    /* add to the global collection of progress operations (note: after
     * creating mId) */
    mParent->addProgress(this);
#endif

    unconst(mDescription) = aDescription;

    return S_OK;
}

/**
 * Initializes the progress base object.
 *
 * This is a special initializer that doesn't initialize any field. Used by one
 * of the Progress::init() forms to create sub-progress operations combined
 * together using a CombinedProgress instance, so it doesn't require the parent,
 * initiator, description and doesn't create an ID.
 *
 * Subclasses should call this or any other #protectedInit() method from their
 * init() implementations.
 *
 * @param aAutoInitSpan AutoInitSpan object instantiated by a subclass.
 */
HRESULT ProgressBase::protectedInit(AutoInitSpan &aAutoInitSpan)
{
    /* Guarantees subclasses call this method at the proper time */
    NOREF(aAutoInitSpan);

    return S_OK;
}

/**
 * Uninitializes the instance.
 *
 * Subclasses should call this from their uninit() implementations.
 *
 * @param aAutoUninitSpan   AutoUninitSpan object instantiated by a subclass.
 *
 * @note Using the mParent member after this method returns is forbidden.
 */
void ProgressBase::protectedUninit(AutoUninitSpan &aAutoUninitSpan)
{
    /* release initiator (effective only if mInitiator has been assigned in
     * init()) */
    unconst(mInitiator).setNull();

#if !defined(VBOX_COM_INPROC)
    if (mParent)
    {
        /* remove the added progress on failure to complete the initialization */
        if (aAutoUninitSpan.initFailed() && !mId.isEmpty())
            mParent->removeProgress(mId.ref());

        unconst(mParent) = NULL;
    }
#endif
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ProgressBase::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mId is constant during life time, no need to lock */
    mId.toUtf16().cloneTo(aId);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mDescription is constant during life time, no need to lock */
    mDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Initiator)(IUnknown **aInitiator)
{
    CheckComArgOutPointerValid(aInitiator);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mInitiator/mParent are constant during life time, no need to lock */

#if !defined(VBOX_COM_INPROC)
    if (mInitiator)
        mInitiator.queryInterfaceTo(aInitiator);
    else
    {
        ComObjPtr<VirtualBox> pVirtualBox(mParent);
        pVirtualBox.queryInterfaceTo(aInitiator);
    }
#else
    mInitiator.queryInterfaceTo(aInitiator);
#endif

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Cancelable)(BOOL *aCancelable)
{
    CheckComArgOutPointerValid(aCancelable);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCancelable = mCancelable;

    return S_OK;
}

/**
 * Internal helper to compute the total percent value based on the member values and
 * returns it as a "double". This is used both by GetPercent (which returns it as a
 * rounded ULONG) and GetTimeRemaining().
 *
 * Requires locking by the caller!
 *
 * @return fractional percentage as a double value.
 */
double ProgressBase::calcTotalPercent()
{
    // avoid division by zero
    if (m_ulTotalOperationsWeight == 0)
        return 0;

    double dPercent = (    (double)m_ulOperationsCompletedWeight                                              // weight of operations that have been completed
                         + ((double)m_ulOperationPercent * (double)m_ulCurrentOperationWeight / (double)100)  // plus partial weight of the current operation
                      ) * (double)100 / (double)m_ulTotalOperationsWeight;

    return dPercent;
}

/**
 * Internal helper for automatically timing out the operation.
 *
 * The caller should hold the object write lock.
 */
void ProgressBase::checkForAutomaticTimeout(void)
{
    if (   m_cMsTimeout
        && mCancelable
        && !mCanceled
        && RTTimeMilliTS() - m_ullTimestamp > m_cMsTimeout
       )
        Cancel();
}


STDMETHODIMP ProgressBase::COMGETTER(TimeRemaining)(LONG *aTimeRemaining)
{
    CheckComArgOutPointerValid(aTimeRemaining);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCompleted)
        *aTimeRemaining = 0;
    else
    {
        double dPercentDone = calcTotalPercent();
        if (dPercentDone < 1)
            *aTimeRemaining = -1;       // unreliable, or avoid division by 0 below
        else
        {
            uint64_t ullTimeNow = RTTimeMilliTS();
            uint64_t ullTimeElapsed = ullTimeNow - m_ullTimestamp;
            uint64_t ullTimeTotal = (uint64_t)(ullTimeElapsed * 100 / dPercentDone);
            uint64_t ullTimeRemaining = ullTimeTotal - ullTimeElapsed;

//             Log(("ProgressBase::GetTimeRemaining: dPercentDone %RI32, ullTimeNow = %RI64, ullTimeElapsed = %RI64, ullTimeTotal = %RI64, ullTimeRemaining = %RI64\n",
//                         (uint32_t)dPercentDone, ullTimeNow, ullTimeElapsed, ullTimeTotal, ullTimeRemaining));

            *aTimeRemaining = (LONG)(ullTimeRemaining / 1000);
        }
    }

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Percent)(ULONG *aPercent)
{
    CheckComArgOutPointerValid(aPercent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    checkForAutomaticTimeout();

    /* checkForAutomaticTimeout requires a write lock. */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCompleted && SUCCEEDED(mResultCode))
        *aPercent = 100;
    else
    {
        ULONG ulPercent = (ULONG)calcTotalPercent();
        // do not report 100% until we're really really done with everything as the Qt GUI dismisses progress dialogs in that case
        if (    ulPercent == 100
             && (    m_ulOperationPercent < 100
                  || (m_ulCurrentOperation < m_cOperations -1)
                )
           )
            *aPercent = 99;
        else
            *aPercent = ulPercent;
    }

    checkForAutomaticTimeout();

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Completed)(BOOL *aCompleted)
{
    CheckComArgOutPointerValid(aCompleted);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCompleted = mCompleted;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Canceled)(BOOL *aCanceled)
{
    CheckComArgOutPointerValid(aCanceled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCanceled = mCanceled;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ResultCode)(LONG *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mCompleted)
        return setError(E_FAIL,
                        tr("Result code is not available, operation is still in progress"));

    *aResultCode = mResultCode;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ErrorInfo)(IVirtualBoxErrorInfo **aErrorInfo)
{
    CheckComArgOutPointerValid(aErrorInfo);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mCompleted)
        return setError(E_FAIL,
                        tr("Error info is not available, operation is still in progress"));

    mErrorInfo.queryInterfaceTo(aErrorInfo);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationCount)(ULONG *aOperationCount)
{
    CheckComArgOutPointerValid(aOperationCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOperationCount = m_cOperations;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Operation)(ULONG *aOperation)
{
    CheckComArgOutPointerValid(aOperation);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOperation = m_ulCurrentOperation;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationDescription)(BSTR *aOperationDescription)
{
    CheckComArgOutPointerValid(aOperationDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m_bstrOperationDescription.cloneTo(aOperationDescription);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationPercent)(ULONG *aOperationPercent)
{
    CheckComArgOutPointerValid(aOperationPercent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCompleted && SUCCEEDED(mResultCode))
        *aOperationPercent = 100;
    else
        *aOperationPercent = m_ulOperationPercent;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationWeight)(ULONG *aOperationWeight)
{
    CheckComArgOutPointerValid(aOperationWeight);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOperationWeight = m_ulCurrentOperationWeight;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMSETTER(Timeout)(ULONG aTimeout)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mCancelable)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Operation cannot be canceled"));

    LogThisFunc(("%#x => %#x\n", m_cMsTimeout, aTimeout));
    m_cMsTimeout = aTimeout;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Timeout)(ULONG *aTimeout)
{
    CheckComArgOutPointerValid(aTimeout);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = m_cMsTimeout;
    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the cancelation callback, checking for cancelation first.
 *
 * @returns Success indicator.
 * @retval  true on success.
 * @retval  false if the progress object has already been canceled or is in an
 *          invalid state
 *
 * @param   pfnCallback     The function to be called upon cancelation.
 * @param   pvUser          The callback argument.
 */
bool ProgressBase::setCancelCallback(void (*pfnCallback)(void *), void *pvUser)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    checkForAutomaticTimeout();
    if (mCanceled)
        return false;

    m_pvCancelUserArg   = pvUser;
    m_pfnCancelCallback = pfnCallback;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Progress class
////////////////////////////////////////////////////////////////////////////////

HRESULT Progress::FinalConstruct()
{
    HRESULT rc = ProgressBase::FinalConstruct();
    if (FAILED(rc)) return rc;

    mCompletedSem = NIL_RTSEMEVENTMULTI;
    mWaitersCount = 0;

    return S_OK;
}

void Progress::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the normal progress object. With this variant, one can have
 * an arbitrary number of sub-operation which IProgress can analyze to
 * have a weighted progress computed.
 *
 * For example, say that one IProgress is supposed to track the cloning
 * of two hard disk images, which are 100 MB and 1000 MB in size, respectively,
 * and each of these hard disks should be one sub-operation of the IProgress.
 *
 * Obviously the progress would be misleading if the progress displayed 50%
 * after the smaller image was cloned and would then take much longer for
 * the second half.
 *
 * With weighted progress, one can invoke the following calls:
 *
 * 1) create progress object with cOperations = 2 and ulTotalOperationsWeight =
 *    1100 (100 MB plus 1100, but really the weights can be any ULONG); pass
 *    in ulFirstOperationWeight = 100 for the first sub-operation
 *
 * 2) Then keep calling setCurrentOperationProgress() with a percentage
 *    for the first image; the total progress will increase up to a value
 *    of 9% (100MB / 1100MB * 100%).
 *
 * 3) Then call setNextOperation with the second weight (1000 for the megabytes
 *    of the second disk).
 *
 * 4) Then keep calling setCurrentOperationProgress() with a percentage for
 *    the second image, where 100% of the operation will then yield a 100%
 *    progress of the entire task.
 *
 * Weighting is optional; you can simply assign a weight of 1 to each operation
 * and pass ulTotalOperationsWeight == cOperations to this constructor (but
 * for that variant and for backwards-compatibility a simpler constructor exists
 * in ProgressImpl.h as well).
 *
 * Even simpler, if you need no sub-operations at all, pass in cOperations =
 * ulTotalOperationsWeight = ulFirstOperationWeight = 1.
 *
 * @param aParent           See ProgressBase::init().
 * @param aInitiator        See ProgressBase::init().
 * @param aDescription      See ProgressBase::init().
 * @param aCancelable       Flag whether the task maybe canceled.
 * @param cOperations       Number of operations within this task (at least 1).
 * @param ulTotalOperationsWeight Total weight of operations; must be the sum of ulFirstOperationWeight and
 *                          what is later passed with each subsequent setNextOperation() call.
 * @param bstrFirstOperationDescription Description of the first operation.
 * @param ulFirstOperationWeight Weight of first sub-operation.
 * @param aId               See ProgressBase::init().
 */
HRESULT Progress::init(
#if !defined(VBOX_COM_INPROC)
                       VirtualBox *aParent,
#endif
                       IUnknown *aInitiator,
                       CBSTR aDescription,
                       BOOL aCancelable,
                       ULONG cOperations,
                       ULONG ulTotalOperationsWeight,
                       CBSTR bstrFirstOperationDescription,
                       ULONG ulFirstOperationWeight,
                       OUT_GUID aId /* = NULL */)
{
    LogFlowThisFunc(("aDescription=\"%ls\", cOperations=%d, ulTotalOperationsWeight=%d, bstrFirstOperationDescription=\"%ls\", ulFirstOperationWeight=%d\n",
                     aDescription,
                     cOperations,
                     ulTotalOperationsWeight,
                     bstrFirstOperationDescription,
                     ulFirstOperationWeight));

    AssertReturn(bstrFirstOperationDescription, E_INVALIDARG);
    AssertReturn(ulTotalOperationsWeight >= 1, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit(autoInitSpan,
#if !defined(VBOX_COM_INPROC)
                                     aParent,
#endif
                                     aInitiator, aDescription, aId);
    if (FAILED(rc)) return rc;

    mCancelable = aCancelable;

    m_cOperations = cOperations;
    m_ulTotalOperationsWeight = ulTotalOperationsWeight;
    m_ulOperationsCompletedWeight = 0;
    m_ulCurrentOperation = 0;
    m_bstrOperationDescription = bstrFirstOperationDescription;
    m_ulCurrentOperationWeight = ulFirstOperationWeight;
    m_ulOperationPercent = 0;

    int vrc = RTSemEventMultiCreate(&mCompletedSem);
    ComAssertRCRet(vrc, E_FAIL);

    RTSemEventMultiReset(mCompletedSem);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the sub-progress object that represents a specific operation of
 * the whole task.
 *
 * Objects initialized with this method are then combined together into the
 * single task using a CombinedProgress instance, so it doesn't require the
 * parent, initiator, description and doesn't create an ID. Note that calling
 * respective getter methods on an object initialized with this method is
 * useless. Such objects are used only to provide a separate wait semaphore and
 * store individual operation descriptions.
 *
 * @param aCancelable       Flag whether the task maybe canceled.
 * @param aOperationCount   Number of sub-operations within this task (at least 1).
 * @param aOperationDescription Description of the individual operation.
 */
HRESULT Progress::init(BOOL aCancelable,
                       ULONG aOperationCount,
                       CBSTR aOperationDescription)
{
    LogFlowThisFunc(("aOperationDescription=\"%ls\"\n", aOperationDescription));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit(autoInitSpan);
    if (FAILED(rc)) return rc;

    mCancelable = aCancelable;

    // for this variant we assume for now that all operations are weighed "1"
    // and equal total weight = operation count
    m_cOperations = aOperationCount;
    m_ulTotalOperationsWeight = aOperationCount;
    m_ulOperationsCompletedWeight = 0;
    m_ulCurrentOperation = 0;
    m_bstrOperationDescription = aOperationDescription;
    m_ulCurrentOperationWeight = 1;
    m_ulOperationPercent = 0;

    int vrc = RTSemEventMultiCreate(&mCompletedSem);
    ComAssertRCRet(vrc, E_FAIL);

    RTSemEventMultiReset(mCompletedSem);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Progress::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    /* wake up all threads still waiting on occasion */
    if (mWaitersCount > 0)
    {
        LogFlow(("WARNING: There are still %d threads waiting for '%ls' completion!\n",
                 mWaitersCount, mDescription.raw()));
        RTSemEventMultiSignal(mCompletedSem);
    }

    RTSemEventMultiDestroy(mCompletedSem);

    ProgressBase::protectedUninit(autoUninitSpan);
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 * @note XPCOM: when this method is not called on the main XPCOM thread, it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitely freeze event processing.
 */
STDMETHODIMP Progress::WaitForCompletion(LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aTimeout=%d\n", aTimeout));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* if we're already completed, take a shortcut */
    if (!mCompleted)
    {
        int vrc = VINF_SUCCESS;
        bool fForever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeMilliTS();

        while (!mCompleted && (fForever || timeLeft > 0))
        {
            mWaitersCount++;
            alock.leave();
            vrc = RTSemEventMultiWait(mCompletedSem,
                                      fForever ? RT_INDEFINITE_WAIT : (RTMSINTERVAL)timeLeft);
            alock.enter();
            mWaitersCount--;

            /* the last waiter resets the semaphore */
            if (mWaitersCount == 0)
                RTSemEventMultiReset(mCompletedSem);

            if (RT_FAILURE(vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!fForever)
            {
                int64_t now = RTTimeMilliTS();
                timeLeft -= now - lastTime;
                lastTime = now;
            }
        }

        if (RT_FAILURE(vrc) && vrc != VERR_TIMEOUT)
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Failed to wait for the task completion (%Rrc)"),
                            vrc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * @note XPCOM: when this method is not called on the main XPCOM thread, it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitely freeze event processing.
 */
STDMETHODIMP Progress::WaitForOperationCompletion(ULONG aOperation, LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aOperation=%d, aTimeout=%d\n", aOperation, aTimeout));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    CheckComArgExpr(aOperation, aOperation < m_cOperations);

    /* if we're already completed or if the given operation is already done,
     * then take a shortcut */
    if (    !mCompleted
         && aOperation >= m_ulCurrentOperation)
    {
        int vrc = VINF_SUCCESS;
        bool fForever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeMilliTS();

        while (    !mCompleted && aOperation >= m_ulCurrentOperation
                && (fForever || timeLeft > 0))
        {
            mWaitersCount ++;
            alock.leave();
            vrc = RTSemEventMultiWait(mCompletedSem,
                                      fForever ? RT_INDEFINITE_WAIT : (unsigned) timeLeft);
            alock.enter();
            mWaitersCount--;

            /* the last waiter resets the semaphore */
            if (mWaitersCount == 0)
                RTSemEventMultiReset(mCompletedSem);

            if (RT_FAILURE(vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!fForever)
            {
                int64_t now = RTTimeMilliTS();
                timeLeft -= now - lastTime;
                lastTime = now;
            }
        }

        if (RT_FAILURE(vrc) && vrc != VERR_TIMEOUT)
            return setError(E_FAIL,
                            tr("Failed to wait for the operation completion (%Rrc)"),
                            vrc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP Progress::WaitForAsyncProgressCompletion(IProgress *pProgressAsync)
{
    LogFlowThisFuncEnter();

    CheckComArgNotNull(pProgressAsync);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Note: we don't lock here, cause we just using public methods. */

    HRESULT rc           = S_OK;
    BOOL fCancelable     = FALSE;
    BOOL fCompleted      = FALSE;
    BOOL fCanceled       = FALSE;
    ULONG prevPercent    = UINT32_MAX;
    ULONG currentPercent = 0;
    ULONG cOp            = 0;
    /* Is the async process cancelable? */
    rc = pProgressAsync->COMGETTER(Cancelable)(&fCancelable);
    if (FAILED(rc)) return rc;
    /* Loop as long as the sync process isn't completed. */
    while (SUCCEEDED(pProgressAsync->COMGETTER(Completed(&fCompleted))))
    {
        /* We can forward any cancel request to the async process only when
         * it is cancelable. */
        if (fCancelable)
        {
            rc = COMGETTER(Canceled)(&fCanceled);
            if (FAILED(rc)) return rc;
            if (fCanceled)
            {
                rc = pProgressAsync->Cancel();
                if (FAILED(rc)) return rc;
            }
        }
        /* Even if the user canceled the process, we have to wait until the
           async task has finished his work (cleanup and such). Otherwise there
           will be sync trouble (still wrong state, dead locks, ...) on the
           used objects. So just do nothing, but wait for the complete
           notification. */
        if (!fCanceled)
        {
            /* Check if the current operation has changed. It is also possible that
             * in the meantime more than one async operation was finished. So we
             * have to loop as long as we reached the same operation count. */
            ULONG curOp;
            for(;;)
            {
                rc = pProgressAsync->COMGETTER(Operation(&curOp));
                if (FAILED(rc)) return rc;
                if (cOp != curOp)
                {
                    Bstr bstr;
                    ULONG currentWeight;
                    rc = pProgressAsync->COMGETTER(OperationDescription(bstr.asOutParam()));
                    if (FAILED(rc)) return rc;
                    rc = pProgressAsync->COMGETTER(OperationWeight(&currentWeight));
                    if (FAILED(rc)) return rc;
                    rc = SetNextOperation(bstr.raw(), currentWeight);
                    if (FAILED(rc)) return rc;
                    ++cOp;
                }
                else
                    break;
            }

            rc = pProgressAsync->COMGETTER(OperationPercent(&currentPercent));
            if (FAILED(rc)) return rc;
            if (currentPercent != prevPercent)
            {
                prevPercent = currentPercent;
                rc = SetCurrentOperationProgress(currentPercent);
                if (FAILED(rc)) return rc;
            }
        }
        if (fCompleted)
            break;

        /* Make sure the loop is not too tight */
        rc = pProgressAsync->WaitForCompletion(100);
        if (FAILED(rc)) return rc;
    }

    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Progress::Cancel()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mCancelable)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Operation cannot be canceled"));

    if (!mCanceled)
    {
        LogThisFunc(("Canceling\n"));
        mCanceled = TRUE;
        if (m_pfnCancelCallback)
            m_pfnCancelCallback(m_pvCancelUserArg);

    }
    else
        LogThisFunc(("Already canceled\n"));

    return S_OK;
}

/**
 * Updates the percentage value of the current operation.
 *
 * @param aPercent  New percentage value of the operation in progress
 *                  (in range [0, 100]).
 */
STDMETHODIMP Progress::SetCurrentOperationProgress(ULONG aPercent)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertMsgReturn(aPercent <= 100, ("%u\n", aPercent), E_INVALIDARG);

    checkForAutomaticTimeout();
    if (mCancelable && mCanceled)
    {
        Assert(!mCompleted);
        return E_FAIL;
    }
    AssertReturn(!mCompleted && !mCanceled, E_FAIL);

    m_ulOperationPercent = aPercent;

    return S_OK;
}

/**
 * Signals that the current operation is successfully completed and advances to
 * the next operation. The operation percentage is reset to 0.
 *
 * @param aOperationDescription     Description of the next operation.
 *
 * @note The current operation must not be the last one.
 */
STDMETHODIMP Progress::SetNextOperation(IN_BSTR bstrNextOperationDescription, ULONG ulNextOperationsWeight)
{
    AssertReturn(bstrNextOperationDescription, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCanceled)
        return E_FAIL;
    AssertReturn(!mCompleted, E_FAIL);
    AssertReturn(m_ulCurrentOperation + 1 < m_cOperations, E_FAIL);

    ++m_ulCurrentOperation;
    m_ulOperationsCompletedWeight += m_ulCurrentOperationWeight;

    m_bstrOperationDescription = bstrNextOperationDescription;
    m_ulCurrentOperationWeight = ulNextOperationsWeight;
    m_ulOperationPercent = 0;

    Log(("Progress::setNextOperation(%ls): ulNextOperationsWeight = %d; m_ulCurrentOperation is now %d, m_ulOperationsCompletedWeight is now %d\n",
         m_bstrOperationDescription.raw(), ulNextOperationsWeight, m_ulCurrentOperation, m_ulOperationsCompletedWeight));

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal(mCompletedSem);

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Sets the internal result code and attempts to retrieve additional error
 * info from the current thread. Gets called from Progress::notifyComplete(),
 * but can be called again to override a previous result set with
 * notifyComplete().
 *
 * @param aResultCode
 */
HRESULT Progress::setResultCode(HRESULT aResultCode)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mResultCode = aResultCode;

    HRESULT rc = S_OK;

    if (FAILED(aResultCode))
    {
        /* try to import error info from the current thread */

#if !defined(VBOX_WITH_XPCOM)

        ComPtr<IErrorInfo> err;
        rc = ::GetErrorInfo(0, err.asOutParam());
        if (rc == S_OK && err)
        {
            rc = err.queryInterfaceTo(mErrorInfo.asOutParam());
            if (SUCCEEDED(rc) && !mErrorInfo)
                rc = E_FAIL;
        }

#else /* !defined(VBOX_WITH_XPCOM) */

        nsCOMPtr<nsIExceptionService> es;
        es = do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED(rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager(getter_AddRefs(em));
            if (NS_SUCCEEDED(rc))
            {
                ComPtr<nsIException> ex;
                rc = em->GetCurrentException(ex.asOutParam());
                if (NS_SUCCEEDED(rc) && ex)
                {
                    rc = ex.queryInterfaceTo(mErrorInfo.asOutParam());
                    if (NS_SUCCEEDED(rc) && !mErrorInfo)
                        rc = E_FAIL;
                }
            }
        }
#endif /* !defined(VBOX_WITH_XPCOM) */

        AssertMsg(rc == S_OK, ("Couldn't get error info (rc=%08X) while trying to set a failed result (%08X)!\n",
                               rc, aResultCode));
    }

    return rc;
}

/**
 * Marks the whole task as complete and sets the result code.
 *
 * If the result code indicates a failure (|FAILED(@a aResultCode)|) then this
 * method will import the error info from the current thread and assign it to
 * the errorInfo attribute (it will return an error if no info is available in
 * such case).
 *
 * If the result code indicates a success (|SUCCEEDED(@a aResultCode)|) then
 * the current operation is set to the last.
 *
 * Note that this method may be called only once for the given Progress object.
 * Subsequent calls will assert.
 *
 * @param aResultCode   Operation result code.
 */
HRESULT Progress::notifyComplete(HRESULT aResultCode)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mCompleted == FALSE, E_FAIL);

    LogFunc(("aResultCode=%d\n", aResultCode));

    if (mCanceled && SUCCEEDED(aResultCode))
        aResultCode = E_FAIL;

    HRESULT rc = setResultCode(aResultCode);

    mCompleted = TRUE;

    if (!FAILED(aResultCode))
    {
        m_ulCurrentOperation = m_cOperations - 1; /* last operation */
        m_ulOperationPercent = 100;
    }

#if !defined VBOX_COM_INPROC
    /* remove from the global collection of pending progress operations */
    if (mParent)
        mParent->removeProgress(mId.ref());
#endif

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal(mCompletedSem);

    return rc;
}

/**
 * Wrapper around Progress:notifyCompleteV.
 */
HRESULT Progress::notifyComplete(HRESULT aResultCode,
                                 const GUID &aIID,
                                 const char *pcszComponent,
                                 const char *aText,
                                 ...)
{
    va_list va;
    va_start(va, aText);
    HRESULT hrc = notifyCompleteV(aResultCode, aIID, pcszComponent, aText, va);
    va_end(va);
    return hrc;
}

/**
 * Marks the operation as complete and attaches full error info.
 *
 * See com::SupportErrorInfoImpl::setError(HRESULT, const GUID &, const wchar_t
 * *, const char *, ...) for more info.
 *
 * @param aResultCode   Operation result (error) code, must not be S_OK.
 * @param aIID          IID of the interface that defines the error.
 * @param aComponent    Name of the component that generates the error.
 * @param aText         Error message (must not be null), an RTStrPrintf-like
 *                      format string in UTF-8 encoding.
 * @param va            List of arguments for the format string.
 */
HRESULT Progress::notifyCompleteV(HRESULT aResultCode,
                                  const GUID &aIID,
                                  const char *pcszComponent,
                                  const char *aText,
                                  va_list va)
{
    Utf8Str text(aText, va);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mCompleted == FALSE, E_FAIL);

    if (mCanceled && SUCCEEDED(aResultCode))
        aResultCode = E_FAIL;

    mCompleted = TRUE;
    mResultCode = aResultCode;

    AssertReturn(FAILED(aResultCode), E_FAIL);

    ComObjPtr<VirtualBoxErrorInfo> errorInfo;
    HRESULT rc = errorInfo.createObject();
    AssertComRC(rc);
    if (SUCCEEDED(rc))
    {
        errorInfo->init(aResultCode, aIID, pcszComponent, text);
        errorInfo.queryInterfaceTo(mErrorInfo.asOutParam());
    }

#if !defined VBOX_COM_INPROC
    /* remove from the global collection of pending progress operations */
    if (mParent)
        mParent->removeProgress(mId.ref());
#endif

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal(mCompletedSem);

    return rc;
}

/**
 * Notify the progress object that we're almost at the point of no return.
 *
 * This atomically checks for and disables cancelation.  Calls to
 * IProgress::Cancel() made after a successful call to this method will fail
 * and the user can be told.  While this isn't entirely clean behavior, it
 * prevents issues with an irreversible actually operation succeeding while the
 * user believe it was rolled back.
 *
 * @returns Success indicator.
 * @retval  true on success.
 * @retval  false if the progress object has already been canceled or is in an
 *          invalid state
 */
bool Progress::notifyPointOfNoReturn(void)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCanceled)
    {
        LogThisFunc(("returns false\n"));
        return false;
    }

    mCancelable = FALSE;
    LogThisFunc(("returns true\n"));
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// CombinedProgress class
////////////////////////////////////////////////////////////////////////////////

HRESULT CombinedProgress::FinalConstruct()
{
    HRESULT rc = ProgressBase::FinalConstruct();
    if (FAILED(rc)) return rc;

    mProgress = 0;
    mCompletedOperations = 0;

    return BaseFinalConstruct();
}

void CombinedProgress::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes this object based on individual combined progresses.
 * Must be called only from #init()!
 *
 * @param aAutoInitSpan AutoInitSpan object instantiated by a subclass.
 * @param aParent       See ProgressBase::init().
 * @param aInitiator    See ProgressBase::init().
 * @param aDescription  See ProgressBase::init().
 * @param aId           See ProgressBase::init().
 */
HRESULT CombinedProgress::protectedInit(AutoInitSpan &aAutoInitSpan,
#if !defined(VBOX_COM_INPROC)
                                        VirtualBox *aParent,
#endif
                                        IUnknown *aInitiator,
                                        CBSTR aDescription, OUT_GUID aId)
{
    LogFlowThisFunc(("aDescription={%ls} mProgresses.size()=%d\n",
                      aDescription, mProgresses.size()));

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit(aAutoInitSpan,
#if !defined(VBOX_COM_INPROC)
                                     aParent,
#endif
                                     aInitiator, aDescription, aId);
    if (FAILED(rc)) return rc;

    mProgress = 0; /* the first object */
    mCompletedOperations = 0;

    mCompleted = FALSE;
    mCancelable = TRUE; /* until any progress returns FALSE */
    mCanceled = FALSE;

    m_cOperations = 0; /* will be calculated later */

    m_ulCurrentOperation = 0;
    rc = mProgresses[0]->COMGETTER(OperationDescription)(m_bstrOperationDescription.asOutParam());
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < mProgresses.size(); i ++)
    {
        if (mCancelable)
        {
            BOOL cancelable = FALSE;
            rc = mProgresses[i]->COMGETTER(Cancelable)(&cancelable);
            if (FAILED(rc)) return rc;

            if (!cancelable)
                mCancelable = FALSE;
        }

        {
            ULONG opCount = 0;
            rc = mProgresses[i]->COMGETTER(OperationCount)(&opCount);
            if (FAILED(rc)) return rc;

            m_cOperations += opCount;
        }
    }

    rc =  checkProgress();
    if (FAILED(rc)) return rc;

    return rc;
}

/**
 * Initializes the combined progress object given two normal progress
 * objects.
 *
 * @param aParent       See ProgressBase::init().
 * @param aInitiator    See ProgressBase::init().
 * @param aDescription  See ProgressBase::init().
 * @param aProgress1    First normal progress object.
 * @param aProgress2    Second normal progress object.
 * @param aId           See ProgressBase::init().
 */
HRESULT CombinedProgress::init(
#if !defined(VBOX_COM_INPROC)
                               VirtualBox *aParent,
#endif
                               IUnknown *aInitiator,
                               CBSTR aDescription,
                               IProgress *aProgress1,
                               IProgress *aProgress2,
                               OUT_GUID aId /* = NULL */)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    mProgresses.resize(2);
    mProgresses[0] = aProgress1;
    mProgresses[1] = aProgress2;

    HRESULT rc =  protectedInit(autoInitSpan,
#if !defined(VBOX_COM_INPROC)
                                aParent,
#endif
                                aInitiator,
                                aDescription,
                                aId);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void CombinedProgress::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mProgress = 0;
    mProgresses.clear();

    ProgressBase::protectedUninit(autoUninitSpan);
}

// IProgress properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CombinedProgress::COMGETTER(Percent)(ULONG *aPercent)
{
    CheckComArgOutPointerValid(aPercent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mCompleted && SUCCEEDED(mResultCode))
        *aPercent = 100;
    else
    {
        HRESULT rc = checkProgress();
        if (FAILED(rc)) return rc;

        /* global percent =
         *      (100 / m_cOperations) * mOperation +
         *      ((100 / m_cOperations) / 100) * m_ulOperationPercent */
        *aPercent = (100 * m_ulCurrentOperation + m_ulOperationPercent) / m_cOperations;
    }

    return S_OK;
}

STDMETHODIMP CombinedProgress::COMGETTER(Completed)(BOOL *aCompleted)
{
    CheckComArgOutPointerValid(aCompleted);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(Completed)(aCompleted);
}

STDMETHODIMP CombinedProgress::COMGETTER(Canceled)(BOOL *aCanceled)
{
    CheckComArgOutPointerValid(aCanceled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(Canceled)(aCanceled);
}

STDMETHODIMP CombinedProgress::COMGETTER(ResultCode)(LONG *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(ResultCode)(aResultCode);
}

STDMETHODIMP CombinedProgress::COMGETTER(ErrorInfo)(IVirtualBoxErrorInfo **aErrorInfo)
{
    CheckComArgOutPointerValid(aErrorInfo);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(ErrorInfo)(aErrorInfo);
}

STDMETHODIMP CombinedProgress::COMGETTER(Operation)(ULONG *aOperation)
{
    CheckComArgOutPointerValid(aOperation);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(Operation)(aOperation);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationDescription)(BSTR *aOperationDescription)
{
    CheckComArgOutPointerValid(aOperationDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(OperationDescription)(aOperationDescription);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationPercent)(ULONG *aOperationPercent)
{
    CheckComArgOutPointerValid(aOperationPercent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkProgress();
    if (FAILED(rc)) return rc;

    return ProgressBase::COMGETTER(OperationPercent)(aOperationPercent);
}

STDMETHODIMP CombinedProgress::COMSETTER(Timeout)(ULONG aTimeout)
{
    NOREF(aTimeout);
    AssertFailed();
    return E_NOTIMPL;
}

STDMETHODIMP CombinedProgress::COMGETTER(Timeout)(ULONG *aTimeout)
{
    CheckComArgOutPointerValid(aTimeout);

    AssertFailed();
    return E_NOTIMPL;
}

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitely freeze event processing.
 */
STDMETHODIMP CombinedProgress::WaitForCompletion(LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aTtimeout=%d\n", aTimeout));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* if we're already completed, take a shortcut */
    if (!mCompleted)
    {
        HRESULT rc = S_OK;
        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeMilliTS();

        while (!mCompleted && (forever || timeLeft > 0))
        {
            alock.leave();
            rc = mProgresses.back()->WaitForCompletion(forever ? -1 : (LONG) timeLeft);
            alock.enter();

            if (SUCCEEDED(rc))
                rc = checkProgress();

            if (FAILED(rc)) break;

            if (!forever)
            {
                int64_t now = RTTimeMilliTS();
                timeLeft -= now - lastTime;
                lastTime = now;
            }
        }

        if (FAILED(rc)) return rc;
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitely freeze event processing.
 */
STDMETHODIMP CombinedProgress::WaitForOperationCompletion(ULONG aOperation, LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aOperation=%d, aTimeout=%d\n", aOperation, aTimeout));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aOperation >= m_cOperations)
        return setError(E_FAIL,
                        tr("Operation number must be in range [0, %d]"), m_ulCurrentOperation - 1);

    /* if we're already completed or if the given operation is already done,
     * then take a shortcut */
    if (!mCompleted && aOperation >= m_ulCurrentOperation)
    {
        HRESULT rc = S_OK;

        /* find the right progress object to wait for */
        size_t progress = mProgress;
        ULONG operation = 0, completedOps = mCompletedOperations;
        do
        {
            ULONG opCount = 0;
            rc = mProgresses[progress]->COMGETTER(OperationCount)(&opCount);
            if (FAILED(rc))
                return rc;

            if (completedOps + opCount > aOperation)
            {
                /* found the right progress object */
                operation = aOperation - completedOps;
                break;
            }

            completedOps += opCount;
            progress ++;
            ComAssertRet(progress < mProgresses.size(), E_FAIL);
        }
        while (1);

        LogFlowThisFunc(("will wait for mProgresses [%d] (%d)\n",
                          progress, operation));

        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeMilliTS();

        while (!mCompleted && aOperation >= m_ulCurrentOperation &&
               (forever || timeLeft > 0))
        {
            alock.leave();
            /* wait for the appropriate progress operation completion */
            rc = mProgresses[progress]-> WaitForOperationCompletion(operation,
                                                                    forever ? -1 : (LONG) timeLeft);
            alock.enter();

            if (SUCCEEDED(rc))
                rc = checkProgress();

            if (FAILED(rc)) break;

            if (!forever)
            {
                int64_t now = RTTimeMilliTS();
                timeLeft -= now - lastTime;
                lastTime = now;
            }
        }

        if (FAILED(rc)) return rc;
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP CombinedProgress::Cancel()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mCancelable)
        return setError(E_FAIL, tr("Operation cannot be canceled"));

    if (!mCanceled)
    {
        LogThisFunc(("Canceling\n"));
        mCanceled = TRUE;
/** @todo Teleportation: Shouldn't this be propagated to mProgresses? If
 *        powerUp creates passes a combined progress object to the client, I
 *        won't get called back since I'm only getting the powerupProgress ...
 *        Or what? */
        if (m_pfnCancelCallback)
            m_pfnCancelCallback(m_pvCancelUserArg);

    }
    else
        LogThisFunc(("Already canceled\n"));

    return S_OK;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Fetches the properties of the current progress object and, if it is
 * successfully completed, advances to the next uncompleted or unsuccessfully
 * completed object in the vector of combined progress objects.
 *
 * @note Must be called from under this object's write lock!
 */
HRESULT CombinedProgress::checkProgress()
{
    /* do nothing if we're already marked ourselves as completed */
    if (mCompleted)
        return S_OK;

    AssertReturn(mProgress < mProgresses.size(), E_FAIL);

    ComPtr<IProgress> progress = mProgresses[mProgress];
    ComAssertRet(!progress.isNull(), E_FAIL);

    HRESULT rc = S_OK;
    BOOL fCompleted = FALSE;

    do
    {
        rc = progress->COMGETTER(Completed)(&fCompleted);
        if (FAILED(rc))
            return rc;

        if (fCompleted)
        {
            rc = progress->COMGETTER(Canceled)(&mCanceled);
            if (FAILED(rc))
                return rc;

            LONG iRc;
            rc = progress->COMGETTER(ResultCode)(&iRc);
            if (FAILED(rc))
                return rc;
            mResultCode = iRc;

            if (FAILED(mResultCode))
            {
                rc = progress->COMGETTER(ErrorInfo)(mErrorInfo.asOutParam());
                if (FAILED(rc))
                    return rc;
            }

            if (FAILED(mResultCode) || mCanceled)
            {
                mCompleted = TRUE;
            }
            else
            {
                ULONG opCount = 0;
                rc = progress->COMGETTER(OperationCount)(&opCount);
                if (FAILED(rc))
                    return rc;

                mCompletedOperations += opCount;
                mProgress ++;

                if (mProgress < mProgresses.size())
                    progress = mProgresses[mProgress];
                else
                    mCompleted = TRUE;
            }
        }
    }
    while (fCompleted && !mCompleted);

    rc = progress->COMGETTER(OperationPercent)(&m_ulOperationPercent);
    if (SUCCEEDED(rc))
    {
        ULONG operation = 0;
        rc = progress->COMGETTER(Operation)(&operation);
        if (SUCCEEDED(rc) && mCompletedOperations + operation > m_ulCurrentOperation)
        {
            m_ulCurrentOperation = mCompletedOperations + operation;
            rc = progress->COMGETTER(OperationDescription)(m_bstrOperationDescription.asOutParam());
        }
    }

    return rc;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
