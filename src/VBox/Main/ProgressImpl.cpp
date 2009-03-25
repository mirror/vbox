/* $Id$ */
/** @file
 *
 * VirtualBox Progress COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include <iprt/types.h>

#if defined (VBOX_WITH_XPCOM)
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>
#endif /* defined (VBOX_WITH_XPCOM) */

#include "ProgressImpl.h"

#include "VirtualBoxImpl.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Logging.h"

#include <iprt/time.h>
#include <iprt/semaphore.h>

#include <VBox/err.h>

////////////////////////////////////////////////////////////////////////////////
// ProgressBase class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (ProgressBase)

/**
 * Subclasses must call this method from their FinalConstruct() implementations.
 */
HRESULT ProgressBase::FinalConstruct()
{
    mCancelable = FALSE;
    mCompleted = FALSE;
    mCanceled = FALSE;
    mResultCode = S_OK;

    m_cOperations =
    m_ulTotalOperationsWeight =
    m_ulOperationsCompletedWeight =
    m_ulCurrentOperation =
    m_ulCurrentOperationWeight =
    m_ulOperationPercent = 0;

    return S_OK;
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
 * @param aDescription  Task description.
 * @param aID           Address of result GUID structure (optional).
 *
 * @return              COM result indicator.
 */
HRESULT ProgressBase::protectedInit (AutoInitSpan &aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
                            VirtualBox *aParent,
#endif
                            IUnknown *aInitiator,
                            CBSTR aDescription, OUT_GUID aId /* = NULL */)
{
    /* Guarantees subclasses call this method at the proper time */
    NOREF (aAutoInitSpan);

    AutoCaller autoCaller(this);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

#if !defined (VBOX_COM_INPROC)
    AssertReturn (aParent, E_INVALIDARG);
#else
    AssertReturn (aInitiator, E_INVALIDARG);
#endif

    AssertReturn (aDescription, E_INVALIDARG);

#if !defined (VBOX_COM_INPROC)
    /* share parent weakly */
    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);
#endif

#if !defined (VBOX_COM_INPROC)
    /* assign (and therefore addref) initiator only if it is not VirtualBox
     * (to avoid cycling); otherwise mInitiator will remain null which means
     * that it is the same as the parent */
    if (aInitiator && !mParent.equalsTo (aInitiator))
        unconst (mInitiator) = aInitiator;
#else
    unconst (mInitiator) = aInitiator;
#endif

    unconst (mId).create();
    if (aId)
        mId.cloneTo (aId);

#if !defined (VBOX_COM_INPROC)
    /* add to the global colleciton of progess operations (note: after
     * creating mId) */
    mParent->addProgress (this);
#endif

    unconst (mDescription) = aDescription;

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
HRESULT ProgressBase::protectedInit (AutoInitSpan &aAutoInitSpan)
{
    /* Guarantees subclasses call this method at the proper time */
    NOREF (aAutoInitSpan);

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
void ProgressBase::protectedUninit (AutoUninitSpan &aAutoUninitSpan)
{
    /* release initiator (effective only if mInitiator has been assigned in
     * init()) */
    unconst (mInitiator).setNull();

#if !defined (VBOX_COM_INPROC)
    if (mParent)
    {
        /* remove the added progress on failure to complete the initialization */
        if (aAutoUninitSpan.initFailed() && !mId.isEmpty())
            mParent->removeProgress (mId);

        mParent->removeDependentChild (this);

        unconst (mParent).setNull();
    }
#endif
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ProgressBase::COMGETTER(Id) (OUT_GUID aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mId is constant during life time, no need to lock */
    mId.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Description) (BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mDescription is constant during life time, no need to lock */
    mDescription.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Initiator) (IUnknown **aInitiator)
{
    CheckComArgOutPointerValid(aInitiator);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mInitiator/mParent are constant during life time, no need to lock */

#if !defined (VBOX_COM_INPROC)
    if (mInitiator)
        mInitiator.queryInterfaceTo (aInitiator);
    else
        mParent.queryInterfaceTo (aInitiator);
#else
    mInitiator.queryInterfaceTo (aInitiator);
#endif

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Cancelable) (BOOL *aCancelable)
{
    CheckComArgOutPointerValid(aCancelable);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aCancelable = mCancelable;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Percent)(ULONG *aPercent)
{
    CheckComArgOutPointerValid(aPercent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (mCompleted && SUCCEEDED (mResultCode))
        *aPercent = 100;
    else
    {
        /* global percent =
         *      (100 / m_cOperations) * mOperation +
         *      ((100 / m_cOperations) / 100) * m_ulOperationPercent */
//         *aPercent = (100 * mOperation + m_ulOperationPercent) / m_cOperations;

        *aPercent = (ULONG)(    (    (double)m_ulOperationsCompletedWeight                                              // weight of operations that have been completed
                                   + ((double)m_ulOperationPercent * (double)m_ulCurrentOperationWeight / (double)100)  // plus partial weight of the current operation
                                ) * (double)100 / (double)m_ulTotalOperationsWeight);
    }

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Completed) (BOOL *aCompleted)
{
    CheckComArgOutPointerValid(aCompleted);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aCompleted = mCompleted;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Canceled) (BOOL *aCanceled)
{
    CheckComArgOutPointerValid(aCanceled);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aCanceled = mCanceled;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ResultCode) (HRESULT *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    if (!mCompleted)
        return setError (E_FAIL,
            tr ("Result code is not available, operation is still in progress"));

    *aResultCode = mResultCode;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ErrorInfo) (IVirtualBoxErrorInfo **aErrorInfo)
{
    CheckComArgOutPointerValid(aErrorInfo);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    if (!mCompleted)
        return setError (E_FAIL,
            tr ("Error info is not available, operation is still in progress"));

    mErrorInfo.queryInterfaceTo (aErrorInfo);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationCount) (ULONG *aOperationCount)
{
    CheckComArgOutPointerValid(aOperationCount);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aOperationCount = m_cOperations;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Operation) (ULONG *aOperation)
{
    CheckComArgOutPointerValid(aOperation);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aOperation = m_ulCurrentOperation;

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationDescription) (BSTR *aOperationDescription)
{
    CheckComArgOutPointerValid(aOperationDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    m_bstrOperationDescription.cloneTo(aOperationDescription);

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationPercent)(ULONG *aOperationPercent)
{
    CheckComArgOutPointerValid(aOperationPercent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    if (mCompleted && SUCCEEDED (mResultCode))
        *aOperationPercent = 100;
    else
        *aOperationPercent = m_ulOperationPercent;

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the error info stored in the given progress object as the error info on
 * the current thread.
 *
 * This method is useful if some other COM method uses IProgress to wait for
 * something and then wants to return a failed result of the operation it was
 * waiting for as its own result retaining the extended error info.
 *
 * If the operation tracked by this progress object is completed successfully
 * and returned S_OK, this method does nothing but returns S_OK. Otherwise, the
 * failed warning or error result code specified at progress completion is
 * returned and the extended error info object (if any) is set on the current
 * thread.
 *
 * Note that the given progress object must be completed, otherwise this method
 * will assert and fail.
 */
/* static */
HRESULT ProgressBase::setErrorInfoOnThread (IProgress *aProgress)
{
    AssertReturn (aProgress != NULL, E_INVALIDARG);

    HRESULT resultCode;
    HRESULT rc = aProgress->COMGETTER(ResultCode) (&resultCode);
    AssertComRCReturnRC (rc);

    if (resultCode == S_OK)
        return resultCode;

    ComPtr <IVirtualBoxErrorInfo> errorInfo;
    rc = aProgress->COMGETTER(ErrorInfo) (errorInfo.asOutParam());
    AssertComRCReturnRC (rc);

    if (!errorInfo.isNull())
        setErrorInfo (errorInfo);

    return resultCode;
}

////////////////////////////////////////////////////////////////////////////////
// Progress class
////////////////////////////////////////////////////////////////////////////////

HRESULT Progress::FinalConstruct()
{
    HRESULT rc = ProgressBase::FinalConstruct();
    CheckComRCReturnRC (rc);

    mCompletedSem = NIL_RTSEMEVENTMULTI;
    mWaitersCount = 0;

    return S_OK;
}

void Progress::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the normal progress object.
 *
 * @param aParent           See ProgressBase::init().
 * @param aInitiator        See ProgressBase::init().
 * @param aDescription      See ProgressBase::init().
 * @param aCancelable       Flag whether the task maybe canceled.
 * @param aOperationCount   Number of operations within this task (at least 1).
 * @param aOperationDescription Description of the first operation.
 * @param aId               See ProgressBase::init().
 */
HRESULT Progress::init (
#if !defined (VBOX_COM_INPROC)
                        VirtualBox *aParent,
#endif
                        IUnknown *aInitiator,
                        CBSTR aDescription, BOOL aCancelable,
                        ULONG cOperations, ULONG ulTotalOperationsWeight,
                        CBSTR bstrFirstOperationDescription, ULONG ulFirstOperationWeight,
                        OUT_GUID aId /* = NULL */)
{
    LogFlowThisFunc (("aDescription=\"%ls\"\n", aDescription));

    AssertReturn(bstrFirstOperationDescription, E_INVALIDARG);
    AssertReturn(ulTotalOperationsWeight >= 1, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit (autoInitSpan,
#if !defined (VBOX_COM_INPROC)
                                      aParent,
#endif
                                      aInitiator, aDescription, aId);
    CheckComRCReturnRC (rc);

    mCancelable = aCancelable;

    m_cOperations = cOperations;
    m_ulTotalOperationsWeight = ulTotalOperationsWeight;
    m_ulOperationsCompletedWeight = 0;
    m_ulCurrentOperation = 0;
    m_bstrOperationDescription = bstrFirstOperationDescription;
    m_ulCurrentOperationWeight = ulFirstOperationWeight;
    m_ulOperationPercent = 0;

    int vrc = RTSemEventMultiCreate (&mCompletedSem);
    ComAssertRCRet (vrc, E_FAIL);

    RTSemEventMultiReset (mCompletedSem);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
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
    LogFlowThisFunc (("aOperationDescription=\"%ls\"\n", aOperationDescription));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit (autoInitSpan);
    CheckComRCReturnRC (rc);

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

    int vrc = RTSemEventMultiCreate (&mCompletedSem);
    ComAssertRCRet (vrc, E_FAIL);

    RTSemEventMultiReset (mCompletedSem);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
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
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    /* wake up all threads still waiting on occasion */
    if (mWaitersCount > 0)
    {
        LogFlow (("WARNING: There are still %d threads waiting for '%ls' completion!\n",
                  mWaitersCount, mDescription.raw()));
        RTSemEventMultiSignal (mCompletedSem);
    }

    RTSemEventMultiDestroy (mCompletedSem);

    ProgressBase::protectedUninit (autoUninitSpan);
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitey freese event processing.
 */
STDMETHODIMP Progress::WaitForCompletion (LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aTimeout=%d\n", aTimeout));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* if we're already completed, take a shortcut */
    if (!mCompleted)
    {
        RTTIMESPEC time;
        RTTimeNow (&time);

        int vrc = VINF_SUCCESS;
        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && (forever || timeLeft > 0))
        {
            mWaitersCount ++;
            alock.leave();
            int vrc = RTSemEventMultiWait (mCompletedSem,
                forever ? RT_INDEFINITE_WAIT : (unsigned) timeLeft);
            alock.enter();
            mWaitersCount --;

            /* the last waiter resets the semaphore */
            if (mWaitersCount == 0)
                RTSemEventMultiReset (mCompletedSem);

            if (RT_FAILURE (vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (RT_FAILURE (vrc) && vrc != VERR_TIMEOUT)
            return setError (VBOX_E_IPRT_ERROR,
                tr ("Failed to wait for the task completion (%Rrc)"), vrc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitey freese event processing.
 */
STDMETHODIMP Progress::WaitForOperationCompletion(ULONG aOperation, LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aOperation=%d, aTimeout=%d\n", aOperation, aTimeout));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CheckComArgExpr(aOperation, aOperation < m_cOperations);

    /* if we're already completed or if the given operation is already done,
     * then take a shortcut */
    if (    !mCompleted
         && aOperation >= m_ulCurrentOperation)
    {
        RTTIMESPEC time;
        RTTimeNow (&time);

        int vrc = VINF_SUCCESS;
        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && aOperation >= m_ulCurrentOperation &&
               (forever || timeLeft > 0))
        {
            mWaitersCount ++;
            alock.leave();
            int vrc = RTSemEventMultiWait (mCompletedSem,
                                           forever ? RT_INDEFINITE_WAIT
                                                   : (unsigned) timeLeft);
            alock.enter();
            mWaitersCount --;

            /* the last waiter resets the semaphore */
            if (mWaitersCount == 0)
                RTSemEventMultiReset (mCompletedSem);

            if (RT_FAILURE (vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (RT_FAILURE (vrc) && vrc != VERR_TIMEOUT)
            return setError (E_FAIL,
                tr ("Failed to wait for the operation completion (%Rrc)"), vrc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP Progress::Cancel()
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (!mCancelable)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("Operation cannot be canceled"));

    mCanceled = TRUE;
    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Updates the percentage value of the current operation.
 *
 * @param aPercent  New percentage value of the operation in progress
 *                  (in range [0, 100]).
 */
HRESULT Progress::setCurrentOperationProgress(ULONG aPercent)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(aPercent <= 100, E_INVALIDARG);

    if (mCancelable && mCanceled)
    {
        Assert(!mCompleted);
        return E_FAIL;
    }
    else
        AssertReturn (!mCompleted && !mCanceled, E_FAIL);

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
HRESULT Progress::setNextOperation(CBSTR bstrNextOperationDescription, ULONG ulNextOperationsWeight)
{
    AssertReturn(bstrNextOperationDescription, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn (!mCompleted && !mCanceled, E_FAIL);
    AssertReturn (m_ulCurrentOperation + 1 < m_cOperations, E_FAIL);

    ++m_ulCurrentOperation;
    m_ulOperationsCompletedWeight += m_ulCurrentOperationWeight;

    m_bstrOperationDescription = bstrNextOperationDescription;
    m_ulCurrentOperationWeight = ulNextOperationsWeight;
    m_ulOperationPercent = 0;

    Log(("Progress::setNextOperation(%ls): ulNextOperationsWeight = %d; m_ulCurrentOperation is now %d, m_ulOperationsCompletedWeightis now %d\n",
         m_bstrOperationDescription.raw(), ulNextOperationsWeight, m_ulCurrentOperation, m_ulOperationsCompletedWeight));

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal (mCompletedSem);

    return S_OK;
}

/**
 * Marks the whole task as complete and sets the result code.
 *
 * If the result code indicates a failure (|FAILED (@a aResultCode)|) then this
 * method will import the error info from the current thread and assign it to
 * the errorInfo attribute (it will return an error if no info is available in
 * such case).
 *
 * If the result code indicates a success (|SUCCEEDED (@a aResultCode)|) then
 * the current operation is set to the last.
 *
 * Note that this method may be called only once for the given Progress object.
 * Subsequent calls will assert.
 *
 * @param aResultCode   Operation result code.
 */
HRESULT Progress::notifyComplete (HRESULT aResultCode)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn (mCompleted == FALSE, E_FAIL);

    if (mCanceled && SUCCEEDED(aResultCode))
        aResultCode = E_FAIL;

    mCompleted = TRUE;
    mResultCode = aResultCode;

    HRESULT rc = S_OK;

    if (FAILED (aResultCode))
    {
        /* try to import error info from the current thread */

#if !defined (VBOX_WITH_XPCOM)

        ComPtr <IErrorInfo> err;
        rc = ::GetErrorInfo (0, err.asOutParam());
        if (rc == S_OK && err)
        {
            rc = err.queryInterfaceTo (mErrorInfo.asOutParam());
            if (SUCCEEDED (rc) && !mErrorInfo)
                rc = E_FAIL;
        }

#else /* !defined (VBOX_WITH_XPCOM) */

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            if (NS_SUCCEEDED (rc))
            {
                ComPtr <nsIException> ex;
                rc = em->GetCurrentException (ex.asOutParam());
                if (NS_SUCCEEDED (rc) && ex)
                {
                    rc = ex.queryInterfaceTo (mErrorInfo.asOutParam());
                    if (NS_SUCCEEDED (rc) && !mErrorInfo)
                        rc = E_FAIL;
                }
            }
        }
#endif /* !defined (VBOX_WITH_XPCOM) */

        AssertMsg (rc == S_OK, ("Couldn't get error info (rc=%08X) while trying "
                                "to set a failed result (%08X)!\n", rc, aResultCode));
    }
    else
    {
        m_ulCurrentOperation = m_cOperations - 1; /* last operation */
        m_ulOperationPercent = 100;
    }

#if !defined VBOX_COM_INPROC
    /* remove from the global collection of pending progress operations */
    if (mParent)
        mParent->removeProgress (mId);
#endif

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal (mCompletedSem);

    return rc;
}

/**
 * Marks the operation as complete and attaches full error info.
 *
 * See com::SupportErrorInfoImpl::setError(HRESULT, const GUID &, const wchar_t
 * *, const char *, ...) for more info.
 *
 * @param aResultCode   Operation result (error) code, must not be S_OK.
 * @param aIID          IID of the intrface that defines the error.
 * @param aComponent    Name of the component that generates the error.
 * @param aText         Error message (must not be null), an RTStrPrintf-like
 *                      format string in UTF-8 encoding.
 * @param  ...          List of arguments for the format string.
 */
HRESULT Progress::notifyComplete (HRESULT aResultCode, const GUID &aIID,
                                  const Bstr &aComponent,
                                  const char *aText, ...)
{
    va_list args;
    va_start (args, aText);
    Bstr text = Utf8StrFmtVA (aText, args);
    va_end (args);

    return notifyCompleteBstr (aResultCode, aIID, aComponent, text);
}

/**
 * Marks the operation as complete and attaches full error info.
 *
 * See com::SupportErrorInfoImpl::setError(HRESULT, const GUID &, const wchar_t
 * *, const char *, ...) for more info.
 *
 * This method is preferred iy you have a ready (translated and formatted) Bstr
 * string, because it omits an extra conversion Utf8Str -> Bstr.
 *
 * @param aResultCode   Operation result (error) code, must not be S_OK.
 * @param aIID          IID of the intrface that defines the error.
 * @param aComponent    Name of the component that generates the error.
 * @param aText         Error message (must not be null).
 */
HRESULT Progress::notifyCompleteBstr (HRESULT aResultCode, const GUID &aIID,
                                      const Bstr &aComponent, const Bstr &aText)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn (mCompleted == FALSE, E_FAIL);

    if (mCanceled && SUCCEEDED(aResultCode))
        aResultCode = E_FAIL;

    mCompleted = TRUE;
    mResultCode = aResultCode;

    AssertReturn (FAILED (aResultCode), E_FAIL);

    ComObjPtr <VirtualBoxErrorInfo> errorInfo;
    HRESULT rc = errorInfo.createObject();
    AssertComRC (rc);
    if (SUCCEEDED (rc))
    {
        errorInfo->init (aResultCode, aIID, aComponent, aText);
        errorInfo.queryInterfaceTo (mErrorInfo.asOutParam());
    }

#if !defined VBOX_COM_INPROC
    /* remove from the global collection of pending progress operations */
    if (mParent)
        mParent->removeProgress (mId);
#endif

    /* wake up all waiting threads */
    if (mWaitersCount > 0)
        RTSemEventMultiSignal (mCompletedSem);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// CombinedProgress class
////////////////////////////////////////////////////////////////////////////////

HRESULT CombinedProgress::FinalConstruct()
{
    HRESULT rc = ProgressBase::FinalConstruct();
    CheckComRCReturnRC (rc);

    mProgress = 0;
    mCompletedOperations = 0;

    return S_OK;
}

void CombinedProgress::FinalRelease()
{
    uninit();
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
HRESULT CombinedProgress::protectedInit (AutoInitSpan &aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
                                         VirtualBox *aParent,
#endif
                                         IUnknown *aInitiator,
                                         CBSTR aDescription, OUT_GUID aId)
{
    LogFlowThisFunc (("aDescription={%ls} mProgresses.size()=%d\n",
                      aDescription, mProgresses.size()));

    HRESULT rc = S_OK;

    rc = ProgressBase::protectedInit (aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
                                      aParent,
#endif
                                      aInitiator, aDescription, aId);
    CheckComRCReturnRC (rc);

    mProgress = 0; /* the first object */
    mCompletedOperations = 0;

    mCompleted = FALSE;
    mCancelable = TRUE; /* until any progress returns FALSE */
    mCanceled = FALSE;

    m_cOperations = 0; /* will be calculated later */

    m_ulCurrentOperation = 0;
    rc = mProgresses [0]->COMGETTER(OperationDescription) (
        m_bstrOperationDescription.asOutParam());
    CheckComRCReturnRC (rc);

    for (size_t i = 0; i < mProgresses.size(); i ++)
    {
        if (mCancelable)
        {
            BOOL cancelable = FALSE;
            rc = mProgresses [i]->COMGETTER(Cancelable) (&cancelable);
            CheckComRCReturnRC (rc);

            if (!cancelable)
                mCancelable = FALSE;
        }

        {
            ULONG opCount = 0;
            rc = mProgresses [i]->COMGETTER(OperationCount) (&opCount);
            CheckComRCReturnRC (rc);

            m_cOperations += opCount;
        }
    }

    rc =  checkProgress();
    CheckComRCReturnRC (rc);

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
HRESULT CombinedProgress::init (
#if !defined (VBOX_COM_INPROC)
                                VirtualBox *aParent,
#endif
                                IUnknown *aInitiator,
                                CBSTR aDescription,
                                IProgress *aProgress1, IProgress *aProgress2,
                                OUT_GUID aId /* = NULL */)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    mProgresses.resize (2);
    mProgresses [0] = aProgress1;
    mProgresses [1] = aProgress2;

    HRESULT rc =  protectedInit (autoInitSpan,
#if !defined (VBOX_COM_INPROC)
                                 aParent,
#endif
                                 aInitiator, aDescription, aId);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
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
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mProgress = 0;
    mProgresses.clear();

    ProgressBase::protectedUninit (autoUninitSpan);
}

// IProgress properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CombinedProgress::COMGETTER(Percent)(ULONG *aPercent)
{
    CheckComArgOutPointerValid(aPercent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    if (mCompleted && SUCCEEDED (mResultCode))
        *aPercent = 100;
    else
    {
        HRESULT rc = checkProgress();
        CheckComRCReturnRC (rc);

        /* global percent =
         *      (100 / m_cOperations) * mOperation +
         *      ((100 / m_cOperations) / 100) * m_ulOperationPercent */
        *aPercent = (100 * m_ulCurrentOperation + m_ulOperationPercent) / m_cOperations;
    }

    return S_OK;
}

STDMETHODIMP CombinedProgress::COMGETTER(Completed) (BOOL *aCompleted)
{
    CheckComArgOutPointerValid(aCompleted);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(Completed) (aCompleted);
}

STDMETHODIMP CombinedProgress::COMGETTER(Canceled) (BOOL *aCanceled)
{
    CheckComArgOutPointerValid(aCanceled);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(Canceled) (aCanceled);
}

STDMETHODIMP CombinedProgress::COMGETTER(ResultCode) (HRESULT *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(ResultCode) (aResultCode);
}

STDMETHODIMP CombinedProgress::COMGETTER(ErrorInfo) (IVirtualBoxErrorInfo **aErrorInfo)
{
    CheckComArgOutPointerValid(aErrorInfo);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(ErrorInfo) (aErrorInfo);
}

STDMETHODIMP CombinedProgress::COMGETTER(Operation) (ULONG *aOperation)
{
    CheckComArgOutPointerValid(aOperation);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(Operation) (aOperation);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationDescription) (BSTR *aOperationDescription)
{
    CheckComArgOutPointerValid(aOperationDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(OperationDescription) (aOperationDescription);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationPercent)(ULONG *aOperationPercent)
{
    CheckComArgOutPointerValid(aOperationPercent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* checkProgress needs a write lock */
    AutoWriteLock alock(this);

    HRESULT rc = checkProgress();
    CheckComRCReturnRC (rc);

    return ProgressBase::COMGETTER(OperationPercent) (aOperationPercent);
}

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitey freese event processing.
 */
STDMETHODIMP CombinedProgress::WaitForCompletion (LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aTtimeout=%d\n", aTimeout));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* if we're already completed, take a shortcut */
    if (!mCompleted)
    {
        RTTIMESPEC time;
        RTTimeNow (&time);

        HRESULT rc = S_OK;
        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && (forever || timeLeft > 0))
        {
            alock.leave();
            rc = mProgresses.back()->WaitForCompletion (
                forever ? -1 : (LONG) timeLeft);
            alock.enter();

            if (SUCCEEDED (rc))
                rc = checkProgress();

            CheckComRCBreakRC (rc);

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        CheckComRCReturnRC (rc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * @note XPCOM: when this method is called not on the main XPCOM thread, it it
 *       simply blocks the thread until mCompletedSem is signalled. If the
 *       thread has its own event queue (hmm, what for?) that it must run, then
 *       calling this method will definitey freese event processing.
 */
STDMETHODIMP CombinedProgress::WaitForOperationCompletion (ULONG aOperation, LONG aTimeout)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aOperation=%d, aTimeout=%d\n", aOperation, aTimeout));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (aOperation >= m_cOperations)
        return setError (E_FAIL,
            tr ("Operation number must be in range [0, %d]"), m_ulCurrentOperation - 1);

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
            rc = mProgresses [progress]->COMGETTER(OperationCount) (&opCount);
            if (FAILED (rc))
                return rc;

            if (completedOps + opCount > aOperation)
            {
                /* found the right progress object */
                operation = aOperation - completedOps;
                break;
            }

            completedOps += opCount;
            progress ++;
            ComAssertRet (progress < mProgresses.size(), E_FAIL);
        }
        while (1);

        LogFlowThisFunc (("will wait for mProgresses [%d] (%d)\n",
                          progress, operation));

        RTTIMESPEC time;
        RTTimeNow (&time);

        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && aOperation >= m_ulCurrentOperation &&
               (forever || timeLeft > 0))
        {
            alock.leave();
            /* wait for the appropriate progress operation completion */
            rc = mProgresses [progress]-> WaitForOperationCompletion (
                operation, forever ? -1 : (LONG) timeLeft);
            alock.enter();

            if (SUCCEEDED (rc))
                rc = checkProgress();

            CheckComRCBreakRC (rc);

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        CheckComRCReturnRC (rc);
    }

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP CombinedProgress::Cancel()
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (!mCancelable)
        return setError (E_FAIL, tr ("Operation cannot be cancelled"));

    mCanceled = TRUE;
    return S_OK;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Fetches the properties of the current progress object and, if it is
 * successfully completed, advances to the next uncompleted or unsucessfully
 * completed object in the vector of combined progress objects.
 *
 * @note Must be called from under this object's write lock!
 */
HRESULT CombinedProgress::checkProgress()
{
    /* do nothing if we're already marked ourselves as completed */
    if (mCompleted)
        return S_OK;

    AssertReturn (mProgress < mProgresses.size(), E_FAIL);

    ComPtr <IProgress> progress = mProgresses [mProgress];
    ComAssertRet (!progress.isNull(), E_FAIL);

    HRESULT rc = S_OK;
    BOOL completed = FALSE;

    do
    {
        rc = progress->COMGETTER(Completed) (&completed);
        if (FAILED (rc))
            return rc;

        if (completed)
        {
            rc = progress->COMGETTER(Canceled) (&mCanceled);
            if (FAILED (rc))
                return rc;

            rc = progress->COMGETTER(ResultCode) (&mResultCode);
            if (FAILED (rc))
                return rc;

            if (FAILED (mResultCode))
            {
                rc = progress->COMGETTER(ErrorInfo) (mErrorInfo.asOutParam());
                if (FAILED (rc))
                    return rc;
            }

            if (FAILED (mResultCode) || mCanceled)
            {
                mCompleted = TRUE;
            }
            else
            {
                ULONG opCount = 0;
                rc = progress->COMGETTER(OperationCount) (&opCount);
                if (FAILED (rc))
                    return rc;

                mCompletedOperations += opCount;
                mProgress ++;

                if (mProgress < mProgresses.size())
                    progress = mProgresses [mProgress];
                else
                    mCompleted = TRUE;
            }
        }
    }
    while (completed && !mCompleted);

    rc = progress->COMGETTER(OperationPercent) (&m_ulOperationPercent);
    if (SUCCEEDED (rc))
    {
        ULONG operation = 0;
        rc = progress->COMGETTER(Operation) (&operation);
        if (SUCCEEDED (rc) && mCompletedOperations + operation > m_ulCurrentOperation)
        {
            m_ulCurrentOperation = mCompletedOperations + operation;
            rc = progress->COMGETTER(OperationDescription) (
                m_bstrOperationDescription.asOutParam());
        }
    }

    return rc;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
