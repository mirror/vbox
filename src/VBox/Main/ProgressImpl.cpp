/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if defined (VBOX_WITH_XPCOM)
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>
#endif // defined (VBOX_WITH_XPCOM)

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

/** Subclasses must call this method from their FinalConstruct() implementations */
HRESULT ProgressBase::FinalConstruct()
{
    mCancelable = FALSE;
    mCompleted = FALSE;
    mCanceled = FALSE;
    mResultCode = S_OK;
    mOperationCount = 0;
    mOperation = 0;
    mOperationPercent = 0;

    return S_OK;
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the progress base object.
 *
 *  Subclasses should call this or any other #init() method from their
 *  init() implementations.
 *
 *  @param aParent
 *      Parent object (only for server-side Progress objects)
 *  @param aInitiator
 *      Initiator of the task (for server-side objects
 *      can be NULL which means initiator = parent, otherwise
 *      must not be NULL)
 *  @param aDescription
 *      Task description
 *  @param aID
 *      Address of result GUID structure (optional)
 *
 *  @note
 *      This method doesn't do |isReady()| check and doesn't call
 *      |setReady (true)| on success!
 *  @note
 *      This method must be called from under the object's lock!
 */
HRESULT ProgressBase::protectedInit (
#if !defined (VBOX_COM_INPROC)
                            VirtualBox *aParent,
#endif
                            IUnknown *aInitiator,
                            const BSTR aDescription, GUIDPARAMOUT aId /* = NULL */)
{
#if !defined (VBOX_COM_INPROC)
    ComAssertRet (aParent, E_POINTER);
#else
    ComAssertRet (aInitiator, E_POINTER);
#endif

    ComAssertRet (aDescription, E_INVALIDARG);

#if !defined (VBOX_COM_INPROC)
    mParent = aParent;
#endif

#if !defined (VBOX_COM_INPROC)
    // assign (and therefore addref) initiator only if it is not VirtualBox
    // (to avoid cycling); otherwise mInitiator will remain null which means
    // that it is the same as the parent
    if (aInitiator && !mParent.equalsTo (aInitiator))
        mInitiator = aInitiator;
#else
    mInitiator = aInitiator;
#endif

    mDescription = aDescription;

    mId.create();
    if (aId)
        mId.cloneTo (aId);

#if !defined (VBOX_COM_INPROC)
    // add to the global colleciton of progess operations
    mParent->addProgress (this);
    // cause #uninit() to be called automatically upon VirtualBox uninit
    mParent->addDependentChild (this);
#endif

    return S_OK;
}

/**
 *  Initializes the progress base object.
 *  This is a special initializator for progress objects that are combined
 *  within a CombinedProgress instance, so it doesn't require the parent,
 *  initiator, description and doesn't create an ID. Note that calling respective
 *  getter methods on an object initialized with this constructor will hit an
 *  assertion.
 *
 *  Subclasses should call this or any other #init() method from their
 *  init() implementations.
 *
 *  @note
 *      This method doesn't do |isReady()| check and doesn't call
 *      |setReady (true)| on success!
 *  @note
 *      This method must be called from under the object's lock!
 */
HRESULT ProgressBase::protectedInit()
{
    return S_OK;
}

/**
 *  Uninitializes the instance.
 *  Subclasses should call this from their uninit() implementations.
 *  The readiness flag must be true on input and will be set to false
 *  on output.
 *
 *  @param alock this object's autolock (with lock level = 1!)
 *
 *  @note
 *  Using mParent member after this method returns is forbidden.
 */
void ProgressBase::protectedUninit (AutoLock &alock)
{
    LogFlowMember (("ProgressBase::protectedUninit()\n"));

    Assert (alock.belongsTo (this) && alock.level() == 1);
    Assert (isReady());

    /*
     *  release initiator
     *  (effective only if mInitiator has been assigned in init())
     */
    mInitiator.setNull();

    setReady (false);

#if !defined (VBOX_COM_INPROC)
    if (mParent)
    {
        alock.leave();
        mParent->removeDependentChild (this);
        alock.enter();
    }

    mParent.setNull();
#endif
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ProgressBase::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    ComAssertRet (!mId.isEmpty(), E_FAIL);

    mId.cloneTo (aId);
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    ComAssertRet (!mDescription.isNull(), E_FAIL);

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Initiator) (IUnknown **aInitiator)
{
    if (!aInitiator)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

#if !defined (VBOX_COM_INPROC)
    ComAssertRet (!mInitiator.isNull() || !mParent.isNull(), E_FAIL);

    if (mInitiator)
        mInitiator.queryInterfaceTo (aInitiator);
    else
        mParent.queryInterfaceTo (aInitiator);
#else
    ComAssertRet (!mInitiator.isNull(), E_FAIL);

    mInitiator.queryInterfaceTo (aInitiator);
#endif

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Cancelable) (BOOL *aCancelable)
{
    if (!aCancelable)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *aCancelable = mCancelable;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Percent) (LONG *aPercent)
{
    if (!aPercent)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    if (mCompleted && SUCCEEDED (mResultCode))
        *aPercent = 100;
    else
    {
        // global percent = (100 / mOperationCount) * mOperation +
        //                  ((100 / mOperationCount) / 100) * mOperationPercent
        *aPercent = (100 * mOperation + mOperationPercent) / mOperationCount;
    }

    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Completed) (BOOL *aCompleted)
{
    if (!aCompleted)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *aCompleted = mCompleted;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Canceled) (BOOL *aCanceled)
{
    if (!aCanceled)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *aCanceled = mCanceled;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ResultCode) (HRESULT *aResultCode)
{
    if (!aResultCode)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    if (!mCompleted)
        return setError (E_FAIL,
            tr ("Result code is not available, operation is still in progress"));

    *aResultCode = mResultCode;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(ErrorInfo) (IVirtualBoxErrorInfo **aErrorInfo)
{
    if (!aErrorInfo)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    if (!mCompleted)
        return setError (E_FAIL,
            tr ("Error info is not available, operation is still in progress"));

    mErrorInfo.queryInterfaceTo (aErrorInfo);
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationCount) (ULONG *aOperationCount)
{
    if (!aOperationCount)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *aOperationCount = mOperationCount;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(Operation) (ULONG *aOperation)
{
    if (!aOperation)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *aOperation = mOperation;
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationDescription) (BSTR *aOperationDescription)
{
    if (!aOperationDescription)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    mOperationDescription.cloneTo (aOperationDescription);
    return S_OK;
}

STDMETHODIMP ProgressBase::COMGETTER(OperationPercent) (LONG *aOperationPercent)
{
    if (!aOperationPercent)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    if (mCompleted && SUCCEEDED (mResultCode))
        *aOperationPercent = 100;
    else
        *aOperationPercent = mOperationPercent;
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Progress class
////////////////////////////////////////////////////////////////////////////////

HRESULT Progress::FinalConstruct()
{
    HRESULT rc = ProgressBase::FinalConstruct();
    if (FAILED (rc))
        return rc;

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
 *  Initializes the progress object.
 *
 *  @param aParent          see ProgressBase::init()
 *  @param aInitiator       see ProgressBase::init()
 *  @param aDescription     see ProgressBase::init()
 *  @param aCancelable      Flag whether the task maybe canceled
 *  @param aOperationCount  Number of operations within this task (at least 1)
 *  @param aOperationDescription Description of the first operation
 *  @param aId              see ProgressBase::init()
 */
HRESULT Progress::init (
#if !defined (VBOX_COM_INPROC)
                        VirtualBox *aParent,
#endif
                        IUnknown *aInitiator,
                        const BSTR aDescription, BOOL aCancelable,
                        ULONG aOperationCount, const BSTR aOperationDescription,
                        GUIDPARAMOUT aId /* = NULL */)
{
    LogFlowMember(("Progress::init(): aDescription={%ls}\n", aDescription));

    ComAssertRet (aOperationDescription, E_INVALIDARG);
    ComAssertRet (aOperationCount >= 1, E_INVALIDARG);

    AutoLock lock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    HRESULT rc = S_OK;

    do
    {
        rc = ProgressBase::protectedInit (
#if !defined (VBOX_COM_INPROC)
                                         aParent,
#endif
                                         aInitiator, aDescription, aId);
        CheckComRCBreakRC (rc);

        // set ready to let protectedUninit() be called on failure
        setReady (true);

        mCancelable = aCancelable;

        mOperationCount = aOperationCount;
        mOperation = 0; // the first operation
        mOperationDescription = aOperationDescription;

        int vrc = RTSemEventMultiCreate (&mCompletedSem);
        ComAssertRCBreak (vrc, rc = E_FAIL);

        RTSemEventMultiReset (mCompletedSem);
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

HRESULT Progress::init (BOOL aCancelable, ULONG aOperationCount,
                        const BSTR aOperationDescription)
{
    LogFlowMember(("Progress::init(): <undescriptioned>\n"));

    AutoLock lock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    HRESULT rc = S_OK;

    do
    {
        rc = ProgressBase::protectedInit();
        CheckComRCBreakRC (rc);

        // set ready to let protectedUninit() be called on failure
        setReady (true);

        mCancelable = aCancelable;

        mOperationCount = aOperationCount;
        mOperation = 0; // the first operation
        mOperationDescription = aOperationDescription;

        int vrc = RTSemEventMultiCreate (&mCompletedSem);
        ComAssertRCBreak (vrc, rc = E_FAIL);

        RTSemEventMultiReset (mCompletedSem);
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Progress::uninit()
{
    LogFlowMember (("Progress::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("Progress::uninit(): isReady=%d\n", isReady()));

    if (!isReady())
        return;

    // wake up all threads still waiting by occasion
    if (mWaitersCount > 0)
    {
        LogFlow (("WARNING: There are still %d threads waiting for '%ls' completion!\n",
                  mWaitersCount, mDescription.raw()));
        RTSemEventMultiSignal (mCompletedSem);
    }

    RTSemEventMultiDestroy (mCompletedSem);

    ProgressBase::protectedUninit (alock);
}

// IProgress properties
/////////////////////////////////////////////////////////////////////////////

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  @note
 *      XPCOM: when this method is called not on the main XPCOM thread, it
 *      it simply blocks the thread until mCompletedSem is signalled. If the
 *      thread has its own event queue (hmm, what for?) that it must run, then
 *      calling this method will definitey freese event processing.
 */
STDMETHODIMP Progress::WaitForCompletion (LONG aTimeout)
{
    LogFlowMember(("Progress::WaitForCompletion: BEGIN: timeout=%d\n", aTimeout));

    AutoLock lock(this);
    CHECK_READY();

    // if we're already completed, take a shortcut
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
            lock.unlock();
            int vrc = RTSemEventMultiWait (mCompletedSem,
                                           forever ? RT_INDEFINITE_WAIT
                                                   : (unsigned) timeLeft);
            lock.lock();
            mWaitersCount --;

            // the progress might have been uninitialized
            if (!isReady())
                break;

            // the last waiter resets the semaphore
            if (mWaitersCount == 0)
                RTSemEventMultiReset (mCompletedSem);

            if (VBOX_FAILURE (vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (VBOX_FAILURE (vrc) && vrc != VERR_TIMEOUT)
            return setError (E_FAIL,
                tr ("Failed to wait for the task completion (%Vrc)"), vrc);
    }

    LogFlowMember(("Progress::WaitForCompletion: END\n"));
    return S_OK;
}

/**
 *  @note
 *      XPCOM: when this method is called not on the main XPCOM thread, it
 *      it simply blocks the thread until mCompletedSem is signalled. If the
 *      thread has its own event queue (hmm, what for?) that it must run, then
 *      calling this method will definitey freese event processing.
 */
STDMETHODIMP Progress::WaitForOperationCompletion (ULONG aOperation, LONG aTimeout)
{
    LogFlowMember(("Progress::WaitForOperationCompletion: BEGIN: "
                   "operation=%d, timeout=%d\n", aOperation, aTimeout));

    AutoLock lock(this);
    CHECK_READY();

    if (aOperation >= mOperationCount)
        return setError (E_FAIL,
            tr ("Operation number must be in range [0, %d]"), mOperation - 1);

    // if we're already completed or if the given operation is already done,
    // then take a shortcut
    if (!mCompleted && aOperation >= mOperation)
    {
        RTTIMESPEC time;
        RTTimeNow (&time);

        int vrc = VINF_SUCCESS;
        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && aOperation >= mOperation &&
               (forever || timeLeft > 0))
        {
            mWaitersCount ++;
            lock.unlock();
            int vrc = RTSemEventMultiWait (mCompletedSem,
                                           forever ? RT_INDEFINITE_WAIT
                                                   : (unsigned) timeLeft);
            lock.lock();
            mWaitersCount --;

            // the progress might have been uninitialized
            if (!isReady())
                break;

            // the last waiter resets the semaphore
            if (mWaitersCount == 0)
                RTSemEventMultiReset (mCompletedSem);

            if (VBOX_FAILURE (vrc) && vrc != VERR_TIMEOUT)
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (VBOX_FAILURE (vrc) && vrc != VERR_TIMEOUT)
            return setError (E_FAIL,
                tr ("Failed to wait for the operation completion (%Vrc)"), vrc);
    }

    LogFlowMember(("Progress::WaitForOperationCompletion: END\n"));
    return S_OK;
}

STDMETHODIMP Progress::Cancel()
{
    AutoLock lock(this);
    CHECK_READY();

    if (!mCancelable)
        return setError (E_FAIL, tr ("Operation cannot be cancelled"));

/// @todo (dmik): implement operation cancellation!
//    mCompleted = TRUE;
//    mCanceled = TRUE;
//    return S_OK;

    ComAssertMsgFailed (("Not implemented!"));
    return E_NOTIMPL;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Updates the percentage value of the current operation.
 *
 *  @param aPercent New percentage value of the operation in progress
 *                  (in range [0, 100]).
 */
HRESULT Progress::notifyProgress (LONG aPercent)
{
    AutoLock lock (this);
    AssertReturn (isReady(), E_UNEXPECTED);

    AssertReturn (!mCompleted && !mCanceled, E_FAIL);
    AssertReturn (aPercent >= 0 && aPercent <= 100, E_INVALIDARG);

    mOperationPercent = aPercent;
    return S_OK;
}

/**
 *  Signals that the current operation is successfully completed
 *  and advances to the next operation. The operation percentage is reset
 *  to 0.
 *
 *  @param aOperationDescription    Description of the next operation
 *
 *  @note
 *      The current operation must not be the last one.
 */
HRESULT Progress::advanceOperation (const BSTR aOperationDescription)
{
    AssertReturn (aOperationDescription, E_INVALIDARG);

    AutoLock lock (this);
    AssertReturn (isReady(), E_UNEXPECTED);

    AssertReturn (!mCompleted && !mCanceled, E_FAIL);
    AssertReturn (mOperation + 1 < mOperationCount, E_FAIL);

    mOperation ++;
    mOperationDescription = aOperationDescription;
    mOperationPercent = 0;

    // wake up all waiting threads
    if (mWaitersCount > 0)
        RTSemEventMultiSignal (mCompletedSem);

    return S_OK;
}

/**
 *  Marks the whole task as complete and sets the result code.
 *
 *  If the result code indicates a failure (|FAILED (@a aResultCode)|)
 *  then this method will import the error info from the current
 *  thread and assign it to the errorInfo attribute (it will return an
 *  error if no info is available in such case).
 *
 *  If the result code indicates a success (|SUCCEEDED (@a aResultCode)|)
 *  then the current operation is set to the last
 *
 *  @param aResultCode  Operation result code
 */
HRESULT Progress::notifyComplete (HRESULT aResultCode)
{
    AutoLock lock (this);
    AssertReturn (isReady(), E_FAIL);

    mCompleted = TRUE;
    mResultCode = aResultCode;

    HRESULT rc = S_OK;

    if (FAILED (aResultCode))
    {
        /* try to import error info from the current thread */

#if !defined (VBOX_WITH_XPCOM)
#if defined (RT_OS_WINDOWS)

        ComPtr <IErrorInfo> err;
        rc = ::GetErrorInfo (0, err.asOutParam());
        if (rc == S_OK && err)
        {
            rc = err.queryInterfaceTo (mErrorInfo.asOutParam());
            if (SUCCEEDED (rc) && !mErrorInfo)
                rc = E_FAIL;
        }

#endif // !defined (RT_OS_WINDOWS)
#else // !defined (VBOX_WITH_XPCOM)

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
#endif // !defined (VBOX_WITH_XPCOM)

        AssertMsg (rc == S_OK, ("Couldn't get error info (rc=%08X) while trying "
                                "to set a failed result (%08X)!\n", rc, aResultCode));
    }
    else
    {
        mOperation = mOperationCount - 1; /* last operation */
        mOperationPercent = 100;
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
 *  Marks the operation as complete and attaches full error info.
 *  See VirtualBoxSupportErrorInfoImpl::setError(HRESULT, const GUID &, const wchar_t *, const char *, ...)
 *  for more info.
 *
 *  @param  aResultCode operation result (error) code, must not be S_OK
 *  @param  aIID        IID of the intrface that defines the error
 *  @param  aComponent  name of the component that generates the error
 *  @param  aText       error message (must not be null), an RTStrPrintf-like
 *                      format string in UTF-8 encoding
 *  @param  ...         list of arguments for the format string
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
 *  Marks the operation as complete and attaches full error info.
 *  See VirtualBoxSupportErrorInfoImpl::setError(HRESULT, const GUID &, const wchar_t *, const char *, ...)
 *  for more info.
 *
 *  This method is preferred iy you have a ready (translated and formatted)
 *  Bstr string, because it omits an extra conversion Utf8Str -> Bstr.
 * 
 *  @param  aResultCode operation result (error) code, must not be S_OK
 *  @param  aIID        IID of the intrface that defines the error
 *  @param  aComponent  name of the component that generates the error
 *  @param  aText       error message (must not be null)
 */
HRESULT Progress::notifyCompleteBstr (HRESULT aResultCode, const GUID &aIID,
                                      const Bstr &aComponent, const Bstr &aText)
{
    AutoLock lock (this);
    AssertReturn (isReady(), E_UNEXPECTED);

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
    if (FAILED (rc))
        return rc;

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
 *  Initializes this object based on individual combined progresses.
 *  Must be called only from #init()!
 *
 *  @param aParent          see ProgressBase::init()
 *  @param aInitiator       see ProgressBase::init()
 *  @param aDescription     see ProgressBase::init()
 *  @param aId              see ProgressBase::init()
 */
HRESULT CombinedProgress::protectedInit (
#if !defined (VBOX_COM_INPROC)
                                         VirtualBox *aParent,
#endif
                                         IUnknown *aInitiator,
                                         const BSTR aDescription, GUIDPARAMOUT aId)
{
    LogFlowMember (("CombinedProgress::protectedInit(): "
                    "aDescription={%ls} mProgresses.size()=%d\n",
                    aDescription, mProgresses.size()));

    HRESULT rc = S_OK;

    do
    {
        rc = ProgressBase::protectedInit (
#if !defined (VBOX_COM_INPROC)
                                          aParent,
#endif
                                          aInitiator, aDescription, aId);
        CheckComRCBreakRC (rc);

        // set ready to let protectedUninit() be called on failure
        setReady (true);

        mProgress = 0; // the first object
        mCompletedOperations = 0;

        mCompleted = FALSE;
        mCancelable = TRUE; // until any progress returns FALSE
        mCanceled = FALSE;

        mOperationCount = 0; // will be calculated later
        mOperation = 0;
        rc = mProgresses [0]->COMGETTER(OperationDescription) (
            mOperationDescription.asOutParam());
        CheckComRCBreakRC (rc);

        for (size_t i = 0; i < mProgresses.size(); i ++)
        {
            if (mCancelable)
            {
                BOOL cancelable = FALSE;
                rc = mProgresses [i]->COMGETTER(Cancelable) (&cancelable);
                if (FAILED (rc))
                    return rc;
                if (!cancelable)
                    mCancelable = FALSE;
            }

            {
                ULONG opCount = 0;
                rc = mProgresses [i]->COMGETTER(OperationCount) (&opCount);
                if (FAILED (rc))
                    return rc;
                mOperationCount += opCount;
            }
        }

        rc =  checkProgress();
        CheckComRCBreakRC (rc);
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void CombinedProgress::uninit()
{
    LogFlowMember (("CombinedProgress::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("CombinedProgress::uninit(): isReady=%d\n", isReady()));

    if (!isReady())
        return;

    mProgress = 0;
    mProgresses.clear();

    ProgressBase::protectedUninit (alock);
}

// IProgress properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CombinedProgress::COMGETTER(Percent) (LONG *aPercent)
{
    if (!aPercent)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    if (mCompleted && SUCCEEDED (mResultCode))
        *aPercent = 100;
    else
    {
        HRESULT rc = checkProgress();
        if (FAILED (rc))
            return rc;

        // global percent = (100 / mOperationCount) * mOperation +
        //                  ((100 / mOperationCount) / 100) * mOperationPercent
        *aPercent = (100 * mOperation + mOperationPercent) / mOperationCount;
    }

    return S_OK;
}

STDMETHODIMP CombinedProgress::COMGETTER(Completed) (BOOL *aCompleted)
{
    if (!aCompleted)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(Completed) (aCompleted);
}

STDMETHODIMP CombinedProgress::COMGETTER(Canceled) (BOOL *aCanceled)
{
    if (!aCanceled)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(Canceled) (aCanceled);
}

STDMETHODIMP CombinedProgress::COMGETTER(ResultCode) (HRESULT *aResultCode)
{
    if (!aResultCode)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(ResultCode) (aResultCode);
}

STDMETHODIMP CombinedProgress::COMGETTER(ErrorInfo) (IVirtualBoxErrorInfo **aErrorInfo)
{
    if (!aErrorInfo)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(ErrorInfo) (aErrorInfo);
}

STDMETHODIMP CombinedProgress::COMGETTER(Operation) (ULONG *aOperation)
{
    if (!aOperation)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(Operation) (aOperation);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationDescription) (BSTR *aOperationDescription)
{
    if (!aOperationDescription)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(OperationDescription) (aOperationDescription);
}

STDMETHODIMP CombinedProgress::COMGETTER(OperationPercent) (LONG *aOperationPercent)
{
    if (!aOperationPercent)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    HRESULT rc = checkProgress();
    if (FAILED (rc))
        return rc;

    return ProgressBase::COMGETTER(OperationPercent) (aOperationPercent);
}

// IProgress methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  @note
 *      XPCOM: when this method is called not on the main XPCOM thread, it
 *      it simply blocks the thread until mCompletedSem is signalled. If the
 *      thread has its own event queue (hmm, what for?) that it must run, then
 *      calling this method will definitey freese event processing.
 */
STDMETHODIMP CombinedProgress::WaitForCompletion (LONG aTimeout)
{
    LogFlowMember (("CombinedProgress::WaitForCompletion: BEGIN: timeout=%d\n",
                    aTimeout));

    AutoLock lock (this);
    CHECK_READY();

    // if we're already completed, take a shortcut
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
            lock.unlock();
            rc = mProgresses.back()->WaitForCompletion (
                forever ? -1 : (LONG) timeLeft);
            lock.lock();

            // the progress might have been uninitialized
            if (!isReady())
                break;

            if (SUCCEEDED (rc))
                rc = checkProgress();

            if (FAILED (rc))
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (FAILED (rc))
            return rc;
    }

    LogFlowMember(("CombinedProgress::WaitForCompletion: END\n"));
    return S_OK;
}

/**
 *  @note
 *      XPCOM: when this method is called not on the main XPCOM thread, it
 *      it simply blocks the thread until mCompletedSem is signalled. If the
 *      thread has its own event queue (hmm, what for?) that it must run, then
 *      calling this method will definitey freese event processing.
 */
STDMETHODIMP CombinedProgress::WaitForOperationCompletion (ULONG aOperation, LONG aTimeout)
{
    LogFlowMember(("CombinedProgress::WaitForOperationCompletion: BEGIN: "
                   "operation=%d, timeout=%d\n", aOperation, aTimeout));

    AutoLock lock(this);
    CHECK_READY();

    if (aOperation >= mOperationCount)
        return setError (E_FAIL,
            tr ("Operation number must be in range [0, %d]"), mOperation - 1);

    // if we're already completed or if the given operation is already done,
    // then take a shortcut
    if (!mCompleted && aOperation >= mOperation)
    {
        HRESULT rc = S_OK;

        // find the right progress object to wait for
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
                // found the right progress object
                operation = aOperation - completedOps;
                break;
            }

            completedOps += opCount;
            progress ++;
            ComAssertRet (progress < mProgresses.size(), E_FAIL);
        }
        while (1);

        LogFlowMember (("CombinedProgress::WaitForOperationCompletion(): "
                        "will wait for mProgresses [%d] (%d)\n",
                        progress, operation));

        RTTIMESPEC time;
        RTTimeNow (&time);

        bool forever = aTimeout < 0;
        int64_t timeLeft = aTimeout;
        int64_t lastTime = RTTimeSpecGetMilli (&time);

        while (!mCompleted && aOperation >= mOperation &&
               (forever || timeLeft > 0))
        {
            lock.unlock();
            // wait for the appropriate progress operation completion
            rc = mProgresses [progress]-> WaitForOperationCompletion (
                operation, forever ? -1 : (LONG) timeLeft);
            lock.lock();

            // the progress might have been uninitialized
            if (!isReady())
                break;

            if (SUCCEEDED (rc))
                rc = checkProgress();

            if (FAILED (rc))
                break;

            if (!forever)
            {
                RTTimeNow (&time);
                timeLeft -= RTTimeSpecGetMilli (&time) - lastTime;
                lastTime = RTTimeSpecGetMilli (&time);
            }
        }

        if (FAILED (rc))
            return rc;
    }

    LogFlowMember(("CombinedProgress::WaitForOperationCompletion: END\n"));
    return S_OK;
}

STDMETHODIMP CombinedProgress::Cancel()
{
    AutoLock lock(this);
    CHECK_READY();

    if (!mCancelable)
        return setError (E_FAIL, tr ("Operation cannot be cancelled"));

/// @todo (dmik): implement operation cancellation!
//    mCompleted = TRUE;
//    mCanceled = TRUE;
//    return S_OK;

    return setError (E_NOTIMPL, ("Not implemented!"));
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Fetches the properties of the current progress object and, if it is
 *  successfully completed, advances to the next uncompleted or unsucessfully
 *  completed object in the vector of combined progress objects.
 *
 *  @note Must be called from under the object's lock!
 */
HRESULT CombinedProgress::checkProgress()
{
    // do nothing if we're already marked ourselves as completed
    if (mCompleted)
        return S_OK;

    ComAssertRet (mProgress < mProgresses.size(), E_FAIL);

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

    rc = progress->COMGETTER(OperationPercent) (&mOperationPercent);
    if (SUCCEEDED (rc))
    {
        ULONG operation = 0;
        rc = progress->COMGETTER(Operation) (&operation);
        if (SUCCEEDED (rc) && mCompletedOperations + operation > mOperation)
        {
            mOperation = mCompletedOperations + operation;
            rc = progress->COMGETTER(OperationDescription) (
                mOperationDescription.asOutParam());
        }
    }

    return rc;
}

