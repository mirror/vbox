/* $Id$ */

/** @file
 *
 * VirtualBox COM base classes implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/semaphore.h>
#include <iprt/asm.h>
#include <iprt/cpp/exception.h>

#include <typeinfo>

#if !defined (VBOX_WITH_XPCOM)
#include <windows.h>
#include <dbghelp.h>
#else /* !defined (VBOX_WITH_XPCOM) */
/// @todo remove when VirtualBoxErrorInfo goes away from here
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#endif /* !defined (VBOX_WITH_XPCOM) */

#include "VirtualBoxBase.h"
#include "AutoCaller.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "Logging.h"

#include "VBox/com/ErrorInfo.h"
#include "VBox/com/MultiResult.h"

////////////////////////////////////////////////////////////////////////////////
//
// VirtualBoxBase
//
////////////////////////////////////////////////////////////////////////////////

VirtualBoxBase::VirtualBoxBase()
    : mStateLock(LOCKCLASS_OBJECTSTATE)
{
    mState = NotReady;
    mStateChangeThread = NIL_RTTHREAD;
    mCallers = 0;
    mZeroCallersSem = NIL_RTSEMEVENT;
    mInitUninitSem = NIL_RTSEMEVENTMULTI;
    mInitUninitWaiters = 0;
    mObjectLock = NULL;
}

VirtualBoxBase::~VirtualBoxBase()
{
    if (mObjectLock)
        delete mObjectLock;
    Assert(mInitUninitWaiters == 0);
    Assert(mInitUninitSem == NIL_RTSEMEVENTMULTI);
    if (mZeroCallersSem != NIL_RTSEMEVENT)
        RTSemEventDestroy (mZeroCallersSem);
    mCallers = 0;
    mStateChangeThread = NIL_RTTHREAD;
    mState = NotReady;
}

/**
 * This virtual method returns an RWLockHandle that can be used to
 * protect instance data. This RWLockHandle is generally referred to
 * as the "object lock"; its locking class (for lock order validation)
 * must be returned by another virtual method, getLockingClass(), which
 * by default returns LOCKCLASS_OTHEROBJECT but is overridden by several
 * subclasses such as VirtualBox, Host, Machine and others.
 *
 * On the first call this method lazily creates the RWLockHandle.
 *
 * @return
 */
/* virtual */
RWLockHandle *VirtualBoxBase::lockHandle() const
{
    /* lazy initialization */
    if (RT_UNLIKELY(!mObjectLock))
    {
        AssertCompile (sizeof (RWLockHandle *) == sizeof (void *));

        // getLockingClass() is overridden by many subclasses to return
        // one of the locking classes listed at the top of AutoLock.h
        RWLockHandle *objLock = new RWLockHandle(getLockingClass());
        if (!ASMAtomicCmpXchgPtr(&mObjectLock, objLock, NULL))
        {
            delete objLock;
            objLock = ASMAtomicReadPtrT(&mObjectLock, RWLockHandle *);
        }
        return objLock;
    }
    return mObjectLock;
}

/**
 * Increments the number of calls to this object by one.
 *
 * After this method succeeds, it is guaranteed that the object will remain
 * in the Ready (or in the Limited) state at least until #releaseCaller() is
 * called.
 *
 * This method is intended to mark the beginning of sections of code within
 * methods of COM objects that depend on the readiness (Ready) state. The
 * Ready state is a primary "ready to serve" state. Usually all code that
 * works with component's data depends on it. On practice, this means that
 * almost every public method, setter or getter of the object should add
 * itself as an object's caller at the very beginning, to protect from an
 * unexpected uninitialization that may happen on a different thread.
 *
 * Besides the Ready state denoting that the object is fully functional,
 * there is a special Limited state. The Limited state means that the object
 * is still functional, but its functionality is limited to some degree, so
 * not all operations are possible. The @a aLimited argument to this method
 * determines whether the caller represents this limited functionality or
 * not.
 *
 * This method succeeds (and increments the number of callers) only if the
 * current object's state is Ready. Otherwise, it will return E_ACCESSDENIED
 * to indicate that the object is not operational. There are two exceptions
 * from this rule:
 * <ol>
 *   <li>If the @a aLimited argument is |true|, then this method will also
 *       succeed if the object's state is Limited (or Ready, of course).
 *   </li>
 *   <li>If this method is called from the same thread that placed
 *       the object to InInit or InUninit state (i.e. either from within the
 *       AutoInitSpan or AutoUninitSpan scope), it will succeed as well (but
 *       will not increase the number of callers).
 *   </li>
 * </ol>
 *
 * Normally, calling addCaller() never blocks. However, if this method is
 * called by a thread created from within the AutoInitSpan scope and this
 * scope is still active (i.e. the object state is InInit), it will block
 * until the AutoInitSpan destructor signals that it has finished
 * initialization.
 *
 * When this method returns a failure, the caller must not use the object
 * and should return the failed result code to its own caller.
 *
 * @param aState        Where to store the current object's state (can be
 *                      used in overridden methods to determine the cause of
 *                      the failure).
 * @param aLimited      |true| to add a limited caller.
 *
 * @return              S_OK on success or E_ACCESSDENIED on failure.
 *
 * @note It is preferable to use the #addLimitedCaller() rather than
 *       calling this method with @a aLimited = |true|, for better
 *       self-descriptiveness.
 *
 * @sa #addLimitedCaller()
 * @sa #releaseCaller()
 */
HRESULT VirtualBoxBase::addCaller(State *aState /* = NULL */,
                                  bool aLimited /* = false */)
{
    AutoWriteLock stateLock(mStateLock COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = E_ACCESSDENIED;

    if (mState == Ready || (aLimited && mState == Limited))
    {
        /* if Ready or allows Limited, increase the number of callers */
        ++ mCallers;
        rc = S_OK;
    }
    else
    if (mState == InInit || mState == InUninit)
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /* Called from the same thread that is doing AutoInitSpan or
             * AutoUninitSpan, just succeed */
            rc = S_OK;
        }
        else if (mState == InInit)
        {
            /* addCaller() is called by a "child" thread while the "parent"
             * thread is still doing AutoInitSpan/AutoReinitSpan, so wait for
             * the state to become either Ready/Limited or InitFailed (in
             * case of init failure).
             *
             * Note that we increase the number of callers anyway -- to
             * prevent AutoUninitSpan from early completion if we are
             * still not scheduled to pick up the posted semaphore when
             * uninit() is called.
             */
            ++ mCallers;

            /* lazy semaphore creation */
            if (mInitUninitSem == NIL_RTSEMEVENTMULTI)
            {
                RTSemEventMultiCreate (&mInitUninitSem);
                Assert(mInitUninitWaiters == 0);
            }

            ++ mInitUninitWaiters;

            LogFlowThisFunc(("Waiting for AutoInitSpan/AutoReinitSpan to finish...\n"));

            stateLock.release();
            RTSemEventMultiWait (mInitUninitSem, RT_INDEFINITE_WAIT);
            stateLock.acquire();

            if (-- mInitUninitWaiters == 0)
            {
                /* destroy the semaphore since no more necessary */
                RTSemEventMultiDestroy (mInitUninitSem);
                mInitUninitSem = NIL_RTSEMEVENTMULTI;
            }

            if (mState == Ready || (aLimited && mState == Limited))
                rc = S_OK;
            else
            {
                Assert(mCallers != 0);
                -- mCallers;
                if (mCallers == 0 && mState == InUninit)
                {
                    /* inform AutoUninitSpan ctor there are no more callers */
                    RTSemEventSignal(mZeroCallersSem);
                }
            }
        }
    }

    if (aState)
        *aState = mState;

    if (FAILED(rc))
    {
        if (mState == VirtualBoxBase::Limited)
            rc = setError(rc, "The object functionality is limited");
        else
            rc = setError(rc, "The object is not ready");
    }

    return rc;
}

/**
 * Decreases the number of calls to this object by one.
 *
 * Must be called after every #addCaller() or #addLimitedCaller() when
 * protecting the object from uninitialization is no more necessary.
 */
void VirtualBoxBase::releaseCaller()
{
    AutoWriteLock stateLock(mStateLock COMMA_LOCKVAL_SRC_POS);

    if (mState == Ready || mState == Limited)
    {
        /* if Ready or Limited, decrease the number of callers */
        AssertMsgReturn(mCallers != 0, ("mCallers is ZERO!"), (void) 0);
        --mCallers;

        return;
    }

    if (mState == InInit || mState == InUninit)
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /* Called from the same thread that is doing AutoInitSpan or
             * AutoUninitSpan: just succeed */
            return;
        }

        if (mState == InUninit)
        {
            /* the caller is being released after AutoUninitSpan has begun */
            AssertMsgReturn(mCallers != 0, ("mCallers is ZERO!"), (void) 0);
            --mCallers;

            if (mCallers == 0)
                /* inform the Auto*UninitSpan ctor there are no more callers */
                RTSemEventSignal(mZeroCallersSem);

            return;
        }
    }

    AssertMsgFailed (("mState = %d!", mState));
}

/**
 * Handles unexpected exceptions by turning them into COM errors in release
 * builds or by hitting a breakpoint in the release builds.
 *
 * Usage pattern:
 * @code
        try
        {
            // ...
        }
        catch (LaLalA)
        {
            // ...
        }
        catch (...)
        {
            rc = VirtualBox::handleUnexpectedExceptions(this, RT_SRC_POS);
        }
 * @endcode
 *
 * @param aThis             object where the exception happened
 * @param RT_SRC_POS_DECL   "RT_SRC_POS" macro instantiation.
 *  */
/* static */
HRESULT VirtualBoxBase::handleUnexpectedExceptions(VirtualBoxBase *const aThis, RT_SRC_POS_DECL)
{
    try
    {
        /* re-throw the current exception */
        throw;
    }
    catch (const RTCError &err)      // includes all XML exceptions
    {
        return setErrorInternal(E_FAIL, aThis->getClassIID(), aThis->getComponentName(),
                                Utf8StrFmt(tr("%s.\n%s[%d] (%s)"),
                                           err.what(),
                                           pszFile, iLine, pszFunction).c_str(),
                                false /* aWarning */,
                                true /* aLogIt */);
    }
    catch (const std::exception &err)
    {
        return setErrorInternal(E_FAIL, aThis->getClassIID(), aThis->getComponentName(),
                                Utf8StrFmt(tr("Unexpected exception: %s [%s]\n%s[%d] (%s)"),
                                           err.what(), typeid(err).name(),
                                           pszFile, iLine, pszFunction).c_str(),
                                false /* aWarning */,
                                true /* aLogIt */);
    }
    catch (...)
    {
        return setErrorInternal(E_FAIL, aThis->getClassIID(), aThis->getComponentName(),
                                Utf8StrFmt(tr("Unknown exception\n%s[%d] (%s)"),
                                           pszFile, iLine, pszFunction).c_str(),
                                false /* aWarning */,
                                true /* aLogIt */);
    }

    /* should not get here */
    AssertFailed();
    return E_FAIL;
}

/**
 *  Sets error info for the current thread. This is an internal function that
 *  gets eventually called by all public variants.  If @a aWarning is
 *  @c true, then the highest (31) bit in the @a aResultCode value which
 *  indicates the error severity is reset to zero to make sure the receiver will
 *  recognize that the created error info object represents a warning rather
 *  than an error.
 */
/* static */
HRESULT VirtualBoxBase::setErrorInternal(HRESULT aResultCode,
                                         const GUID &aIID,
                                         const char *pcszComponent,
                                         Utf8Str aText,
                                         bool aWarning,
                                         bool aLogIt)
{
    /* whether multi-error mode is turned on */
    bool preserve = MultiResult::isMultiEnabled();

    if (aLogIt)
        LogRel(("%s [COM]: aRC=%Rhrc (%#08x) aIID={%RTuuid} aComponent={%s} aText={%s}, preserve=%RTbool\n",
                aWarning ? "WARNING" : "ERROR",
                aResultCode,
                aResultCode,
                &aIID,
                pcszComponent,
                aText.c_str(),
                aWarning,
                preserve));

    /* these are mandatory, others -- not */
    AssertReturn((!aWarning && FAILED(aResultCode)) ||
                  (aWarning && aResultCode != S_OK),
                  E_FAIL);

    /* reset the error severity bit if it's a warning */
    if (aWarning)
        aResultCode &= ~0x80000000;

    HRESULT rc = S_OK;

    if (aText.isEmpty())
    {
        /* Some default info */
        switch (aResultCode)
        {
            case E_INVALIDARG:                 aText = "A parameter has an invalid value"; break;
            case E_POINTER:                    aText = "A parameter is an invalid pointer"; break;
            case E_UNEXPECTED:                 aText = "The result of the operation is unexpected"; break;
            case E_ACCESSDENIED:               aText = "The access to an object is not allowed"; break;
            case E_OUTOFMEMORY:                aText = "The allocation of new memory failed"; break;
            case E_NOTIMPL:                    aText = "The requested operation is not implemented"; break;
            case E_NOINTERFACE:                aText = "The requested interface is not implemented"; break;
            case E_FAIL:                       aText = "A general error occurred"; break;
            case E_ABORT:                      aText = "The operation was canceled"; break;
            case VBOX_E_OBJECT_NOT_FOUND:      aText = "Object corresponding to the supplied arguments does not exist"; break;
            case VBOX_E_INVALID_VM_STATE:      aText = "Current virtual machine state prevents the operation"; break;
            case VBOX_E_VM_ERROR:              aText = "Virtual machine error occurred attempting the operation"; break;
            case VBOX_E_FILE_ERROR:            aText = "File not accessible or erroneous file contents"; break;
            case VBOX_E_IPRT_ERROR:            aText = "Runtime subsystem error"; break;
            case VBOX_E_PDM_ERROR:             aText = "Pluggable Device Manager error"; break;
            case VBOX_E_INVALID_OBJECT_STATE:  aText = "Current object state prohibits operation"; break;
            case VBOX_E_HOST_ERROR:            aText = "Host operating system related error"; break;
            case VBOX_E_NOT_SUPPORTED:         aText = "Requested operation is not supported"; break;
            case VBOX_E_XML_ERROR:             aText = "Invalid XML found"; break;
            case VBOX_E_INVALID_SESSION_STATE: aText = "Current session state prohibits operation"; break;
            case VBOX_E_OBJECT_IN_USE:         aText = "Object being in use prohibits operation"; break;
            default:                           aText = "Unknown error"; break;
        }
    }

    do
    {
        ComObjPtr<VirtualBoxErrorInfo> info;
        rc = info.createObject();
        if (FAILED(rc)) break;

#if !defined (VBOX_WITH_XPCOM)

        ComPtr<IVirtualBoxErrorInfo> curInfo;
        if (preserve)
        {
            /* get the current error info if any */
            ComPtr<IErrorInfo> err;
            rc = ::GetErrorInfo (0, err.asOutParam());
            if (FAILED(rc)) break;
            rc = err.queryInterfaceTo(curInfo.asOutParam());
            if (FAILED(rc))
            {
                /* create a IVirtualBoxErrorInfo wrapper for the native
                 * IErrorInfo object */
                ComObjPtr<VirtualBoxErrorInfo> wrapper;
                rc = wrapper.createObject();
                if (SUCCEEDED(rc))
                {
                    rc = wrapper->init (err);
                    if (SUCCEEDED(rc))
                        curInfo = wrapper;
                }
            }
        }
        /* On failure, curInfo will stay null */
        Assert(SUCCEEDED(rc) || curInfo.isNull());

        /* set the current error info and preserve the previous one if any */
        rc = info->init(aResultCode, aIID, pcszComponent, aText, curInfo);
        if (FAILED(rc)) break;

        ComPtr<IErrorInfo> err;
        rc = info.queryInterfaceTo(err.asOutParam());
        if (SUCCEEDED(rc))
            rc = ::SetErrorInfo (0, err);

#else // !defined (VBOX_WITH_XPCOM)

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED(rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            if (FAILED(rc)) break;

            ComPtr<IVirtualBoxErrorInfo> curInfo;
            if (preserve)
            {
                /* get the current error info if any */
                ComPtr<nsIException> ex;
                rc = em->GetCurrentException (ex.asOutParam());
                if (FAILED(rc)) break;
                rc = ex.queryInterfaceTo(curInfo.asOutParam());
                if (FAILED(rc))
                {
                    /* create a IVirtualBoxErrorInfo wrapper for the native
                     * nsIException object */
                    ComObjPtr<VirtualBoxErrorInfo> wrapper;
                    rc = wrapper.createObject();
                    if (SUCCEEDED(rc))
                    {
                        rc = wrapper->init (ex);
                        if (SUCCEEDED(rc))
                            curInfo = wrapper;
                    }
                }
            }
            /* On failure, curInfo will stay null */
            Assert(SUCCEEDED(rc) || curInfo.isNull());

            /* set the current error info and preserve the previous one if any */
            rc = info->init(aResultCode, aIID, pcszComponent, Bstr(aText), curInfo);
            if (FAILED(rc)) break;

            ComPtr<nsIException> ex;
            rc = info.queryInterfaceTo(ex.asOutParam());
            if (SUCCEEDED(rc))
                rc = em->SetCurrentException (ex);
        }
        else if (rc == NS_ERROR_UNEXPECTED)
        {
            /*
             *  It is possible that setError() is being called by the object
             *  after the XPCOM shutdown sequence has been initiated
             *  (for example, when XPCOM releases all instances it internally
             *  references, which can cause object's FinalConstruct() and then
             *  uninit()). In this case, do_GetService() above will return
             *  NS_ERROR_UNEXPECTED and it doesn't actually make sense to
             *  set the exception (nobody will be able to read it).
             */
            LogWarningFunc(("Will not set an exception because nsIExceptionService is not available "
                            "(NS_ERROR_UNEXPECTED). XPCOM is being shutdown?\n"));
            rc = NS_OK;
        }

#endif // !defined (VBOX_WITH_XPCOM)
    }
    while (0);

    AssertComRC (rc);

    return SUCCEEDED(rc) ? aResultCode : rc;
}

/**
 * Shortcut instance method to calling the static setErrorInternal with the
 * class interface ID and component name inserted correctly. This uses the
 * virtual getClassIID() and getComponentName() methods which are automatically
 * defined by the VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT macro.
 * @param aResultCode
 * @param pcsz
 * @return
 */
HRESULT VirtualBoxBase::setError(HRESULT aResultCode)
{
    return setErrorInternal(aResultCode,
                            this->getClassIID(),
                            this->getComponentName(),
                            "",
                            false /* aWarning */,
                            true /* aLogIt */);
}

/**
 * Shortcut instance method to calling the static setErrorInternal with the
 * class interface ID and component name inserted correctly. This uses the
 * virtual getClassIID() and getComponentName() methods which are automatically
 * defined by the VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT macro.
 * @param aResultCode
 * @return
 */
HRESULT VirtualBoxBase::setError(HRESULT aResultCode, const char *pcsz, ...)
{
    va_list args;
    va_start(args, pcsz);
    HRESULT rc = setErrorInternal(aResultCode,
                                  this->getClassIID(),
                                  this->getComponentName(),
                                  Utf8Str(pcsz, args),
                                  false /* aWarning */,
                                  true /* aLogIt */);
    va_end(args);
    return rc;
}

/**
 * Shortcut instance method to calling the static setErrorInternal with the
 * class interface ID and component name inserted correctly. This uses the
 * virtual getClassIID() and getComponentName() methods which are automatically
 * defined by the VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT macro.
 * @param ei
 * @return
 */
HRESULT VirtualBoxBase::setError(const com::ErrorInfo &ei)
{
    /* whether multi-error mode is turned on */
    bool preserve = MultiResult::isMultiEnabled();

    HRESULT rc = S_OK;

    do
    {
        ComObjPtr<VirtualBoxErrorInfo> info;
        rc = info.createObject();
        if (FAILED(rc)) break;

#if !defined (VBOX_WITH_XPCOM)

        ComPtr<IVirtualBoxErrorInfo> curInfo;
        if (preserve)
        {
            /* get the current error info if any */
            ComPtr<IErrorInfo> err;
            rc = ::GetErrorInfo (0, err.asOutParam());
            if (FAILED(rc)) break;
            rc = err.queryInterfaceTo(curInfo.asOutParam());
            if (FAILED(rc))
            {
                /* create a IVirtualBoxErrorInfo wrapper for the native
                 * IErrorInfo object */
                ComObjPtr<VirtualBoxErrorInfo> wrapper;
                rc = wrapper.createObject();
                if (SUCCEEDED(rc))
                {
                    rc = wrapper->init (err);
                    if (SUCCEEDED(rc))
                        curInfo = wrapper;
                }
            }
        }
        /* On failure, curInfo will stay null */
        Assert(SUCCEEDED(rc) || curInfo.isNull());

        /* set the current error info and preserve the previous one if any */
        rc = info->init(ei, curInfo);
        if (FAILED(rc)) break;

        ComPtr<IErrorInfo> err;
        rc = info.queryInterfaceTo(err.asOutParam());
        if (SUCCEEDED(rc))
            rc = ::SetErrorInfo (0, err);

#else // !defined (VBOX_WITH_XPCOM)

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED(rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            if (FAILED(rc)) break;

            ComPtr<IVirtualBoxErrorInfo> curInfo;
            if (preserve)
            {
                /* get the current error info if any */
                ComPtr<nsIException> ex;
                rc = em->GetCurrentException (ex.asOutParam());
                if (FAILED(rc)) break;
                rc = ex.queryInterfaceTo(curInfo.asOutParam());
                if (FAILED(rc))
                {
                    /* create a IVirtualBoxErrorInfo wrapper for the native
                     * nsIException object */
                    ComObjPtr<VirtualBoxErrorInfo> wrapper;
                    rc = wrapper.createObject();
                    if (SUCCEEDED(rc))
                    {
                        rc = wrapper->init (ex);
                        if (SUCCEEDED(rc))
                            curInfo = wrapper;
                    }
                }
            }
            /* On failure, curInfo will stay null */
            Assert(SUCCEEDED(rc) || curInfo.isNull());

            /* set the current error info and preserve the previous one if any */
            rc = info->init(ei, curInfo);
            if (FAILED(rc)) break;

            ComPtr<nsIException> ex;
            rc = info.queryInterfaceTo(ex.asOutParam());
            if (SUCCEEDED(rc))
                rc = em->SetCurrentException (ex);
        }
        else if (rc == NS_ERROR_UNEXPECTED)
        {
            /*
             *  It is possible that setError() is being called by the object
             *  after the XPCOM shutdown sequence has been initiated
             *  (for example, when XPCOM releases all instances it internally
             *  references, which can cause object's FinalConstruct() and then
             *  uninit()). In this case, do_GetService() above will return
             *  NS_ERROR_UNEXPECTED and it doesn't actually make sense to
             *  set the exception (nobody will be able to read it).
             */
            LogWarningFunc(("Will not set an exception because nsIExceptionService is not available "
                            "(NS_ERROR_UNEXPECTED). XPCOM is being shutdown?\n"));
            rc = NS_OK;
        }

#endif // !defined (VBOX_WITH_XPCOM)
    }
    while (0);

    AssertComRC (rc);

    return SUCCEEDED(rc) ? ei.getResultCode() : rc;
}

/**
 * Like setError(), but sets the "warning" bit in the call to setErrorInternal().
 * @param aResultCode
 * @param pcsz
 * @return
 */
HRESULT VirtualBoxBase::setWarning(HRESULT aResultCode, const char *pcsz, ...)
{
    va_list args;
    va_start(args, pcsz);
    HRESULT rc = setErrorInternal(aResultCode,
                                  this->getClassIID(),
                                  this->getComponentName(),
                                  Utf8Str(pcsz, args),
                                  true /* aWarning */,
                                  true /* aLogIt */);
    va_end(args);
    return rc;
}

/**
 * Like setError(), but disables the "log" flag in the call to setErrorInternal().
 * @param aResultCode
 * @param pcsz
 * @return
 */
HRESULT VirtualBoxBase::setErrorNoLog(HRESULT aResultCode, const char *pcsz, ...)
{
    va_list args;
    va_start(args, pcsz);
    HRESULT rc = setErrorInternal(aResultCode,
                                  this->getClassIID(),
                                  this->getComponentName(),
                                  Utf8Str(pcsz, args),
                                  false /* aWarning */,
                                  false /* aLogIt */);
    va_end(args);
    return rc;
}

/**
 * Clear the current error information.
 */
/*static*/
void VirtualBoxBase::clearError(void)
{
#if !defined(VBOX_WITH_XPCOM)
    ::SetErrorInfo (0, NULL);
#else
    HRESULT rc = S_OK;
    nsCOMPtr <nsIExceptionService> es;
    es = do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
    if (NS_SUCCEEDED(rc))
    {
        nsCOMPtr <nsIExceptionManager> em;
        rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
        if (SUCCEEDED(rc))
            em->SetCurrentException(NULL);
    }
#endif
}


////////////////////////////////////////////////////////////////////////////////
//
// AutoInitSpan methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a smart initialization span object that places the object to
 * InInit state.
 *
 * Please see the AutoInitSpan class description for more info.
 *
 * @param aObj      |this| pointer of the managed VirtualBoxBase object whose
 *                  init() method is being called.
 * @param aResult   Default initialization result.
 */
AutoInitSpan::AutoInitSpan(VirtualBoxBase *aObj,
                           Result aResult /* = Failed */)
    : mObj(aObj),
      mResult(aResult),
      mOk(false)
{
    Assert(aObj);

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    mOk = mObj->mState == VirtualBoxBase::NotReady;
    AssertReturnVoid (mOk);

    mObj->setState(VirtualBoxBase::InInit);
}

/**
 * Places the managed VirtualBoxBase object to Ready/Limited state if the
 * initialization succeeded or partly succeeded, or places it to InitFailed
 * state and calls the object's uninit() method.
 *
 * Please see the AutoInitSpan class description for more info.
 */
AutoInitSpan::~AutoInitSpan()
{
    /* if the state was other than NotReady, do nothing */
    if (!mOk)
        return;

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    Assert(mObj->mState == VirtualBoxBase::InInit);

    if (mObj->mCallers > 0)
    {
        Assert(mObj->mInitUninitWaiters > 0);

        /* We have some pending addCaller() calls on other threads (created
         * during InInit), signal that InInit is finished and they may go on. */
        RTSemEventMultiSignal(mObj->mInitUninitSem);
    }

    if (mResult == Succeeded)
    {
        mObj->setState(VirtualBoxBase::Ready);
    }
    else
    if (mResult == Limited)
    {
        mObj->setState(VirtualBoxBase::Limited);
    }
    else
    {
        mObj->setState(VirtualBoxBase::InitFailed);
        /* release the lock to prevent nesting when uninit() is called */
        stateLock.release();
        /* call uninit() to let the object uninit itself after failed init() */
        mObj->uninit();
        /* Note: the object may no longer exist here (for example, it can call
         * the destructor in uninit()) */
    }
}

// AutoReinitSpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a smart re-initialization span object and places the object to
 * InInit state.
 *
 * Please see the AutoInitSpan class description for more info.
 *
 * @param aObj      |this| pointer of the managed VirtualBoxBase object whose
 *                  re-initialization method is being called.
 */
AutoReinitSpan::AutoReinitSpan(VirtualBoxBase *aObj)
    : mObj(aObj),
      mSucceeded(false),
      mOk(false)
{
    Assert(aObj);

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    mOk = mObj->mState == VirtualBoxBase::Limited;
    AssertReturnVoid (mOk);

    mObj->setState(VirtualBoxBase::InInit);
}

/**
 * Places the managed VirtualBoxBase object to Ready state if the
 * re-initialization succeeded (i.e. #setSucceeded() has been called) or back to
 * Limited state otherwise.
 *
 * Please see the AutoInitSpan class description for more info.
 */
AutoReinitSpan::~AutoReinitSpan()
{
    /* if the state was other than Limited, do nothing */
    if (!mOk)
        return;

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    Assert(mObj->mState == VirtualBoxBase::InInit);

    if (mObj->mCallers > 0 && mObj->mInitUninitWaiters > 0)
    {
        /* We have some pending addCaller() calls on other threads (created
         * during InInit), signal that InInit is finished and they may go on. */
        RTSemEventMultiSignal(mObj->mInitUninitSem);
    }

    if (mSucceeded)
    {
        mObj->setState(VirtualBoxBase::Ready);
    }
    else
    {
        mObj->setState(VirtualBoxBase::Limited);
    }
}

// AutoUninitSpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a smart uninitialization span object and places this object to
 * InUninit state.
 *
 * Please see the AutoInitSpan class description for more info.
 *
 * @note This method blocks the current thread execution until the number of
 *       callers of the managed VirtualBoxBase object drops to zero!
 *
 * @param aObj  |this| pointer of the VirtualBoxBase object whose uninit()
 *              method is being called.
 */
AutoUninitSpan::AutoUninitSpan(VirtualBoxBase *aObj)
    : mObj(aObj),
      mInitFailed(false),
      mUninitDone(false)
{
    Assert(aObj);

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    Assert(mObj->mState != VirtualBoxBase::InInit);

    /* Set mUninitDone to |true| if this object is already uninitialized
     * (NotReady) or if another AutoUninitSpan is currently active on some
     *  other thread (InUninit). */
    mUninitDone =    mObj->mState == VirtualBoxBase::NotReady
                  || mObj->mState == VirtualBoxBase::InUninit;

    if (mObj->mState == VirtualBoxBase::InitFailed)
    {
        /* we've been called by init() on failure */
        mInitFailed = true;
    }
    else
    {
        if (mUninitDone)
        {
            /* do nothing if already uninitialized */
            if (mObj->mState == VirtualBoxBase::NotReady)
                return;

            /* otherwise, wait until another thread finishes uninitialization.
             * This is necessary to make sure that when this method returns, the
             * object is NotReady and therefore can be deleted (for example). */

            /* lazy semaphore creation */
            if (mObj->mInitUninitSem == NIL_RTSEMEVENTMULTI)
            {
                RTSemEventMultiCreate(&mObj->mInitUninitSem);
                Assert(mObj->mInitUninitWaiters == 0);
            }
            ++mObj->mInitUninitWaiters;

            LogFlowFunc(("{%p}: Waiting for AutoUninitSpan to finish...\n",
                         mObj));

            stateLock.release();
            RTSemEventMultiWait(mObj->mInitUninitSem, RT_INDEFINITE_WAIT);
            stateLock.acquire();

            if (--mObj->mInitUninitWaiters == 0)
            {
                /* destroy the semaphore since no more necessary */
                RTSemEventMultiDestroy(mObj->mInitUninitSem);
                mObj->mInitUninitSem = NIL_RTSEMEVENTMULTI;
            }

            return;
        }
    }

    /* go to InUninit to prevent from adding new callers */
    mObj->setState(VirtualBoxBase::InUninit);

    /* wait for already existing callers to drop to zero */
    if (mObj->mCallers > 0)
    {
        /* lazy creation */
        Assert(mObj->mZeroCallersSem == NIL_RTSEMEVENT);
        RTSemEventCreate(&mObj->mZeroCallersSem);

        /* wait until remaining callers release the object */
        LogFlowFunc(("{%p}: Waiting for callers (%d) to drop to zero...\n",
                     mObj, mObj->mCallers));

        stateLock.release();
        RTSemEventWait(mObj->mZeroCallersSem, RT_INDEFINITE_WAIT);
    }
}

/**
 *  Places the managed VirtualBoxBase object to the NotReady state.
 */
AutoUninitSpan::~AutoUninitSpan()
{
    /* do nothing if already uninitialized */
    if (mUninitDone)
        return;

    AutoWriteLock stateLock(mObj->mStateLock COMMA_LOCKVAL_SRC_POS);

    Assert(mObj->mState == VirtualBoxBase::InUninit);

    mObj->setState(VirtualBoxBase::NotReady);
}

////////////////////////////////////////////////////////////////////////////////
//
// MultiResult methods
//
////////////////////////////////////////////////////////////////////////////////

RTTLS MultiResult::sCounter = NIL_RTTLS;

/*static*/
void MultiResult::incCounter()
{
    if (sCounter == NIL_RTTLS)
    {
        sCounter = RTTlsAlloc();
        AssertReturnVoid(sCounter != NIL_RTTLS);
    }

    uintptr_t counter = (uintptr_t)RTTlsGet(sCounter);
    ++counter;
    RTTlsSet(sCounter, (void*)counter);
}

/*static*/
void MultiResult::decCounter()
{
    uintptr_t counter = (uintptr_t)RTTlsGet(sCounter);
    AssertReturnVoid(counter != 0);
    --counter;
    RTTlsSet(sCounter, (void*)counter);
}

/*static*/
bool MultiResult::isMultiEnabled()
{
    if (sCounter == NIL_RTTLS)
       return false;

    return ((uintptr_t)RTTlsGet(MultiResult::sCounter)) > 0;
}

