/* $Id$ */

/** @file
 *
 * VirtualBox COM base classes implementation
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

#include <iprt/semaphore.h>
#include <iprt/asm.h>

#if !defined (VBOX_WITH_XPCOM)
#include <windows.h>
#include <dbghelp.h>
#else /* !defined (VBOX_WITH_XPCOM) */
/// @todo remove when VirtualBoxErrorInfo goes away from here
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#endif /* !defined (VBOX_WITH_XPCOM) */

#include "VirtualBoxBase.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "Logging.h"

// VirtualBoxBaseProto methods
////////////////////////////////////////////////////////////////////////////////

VirtualBoxBaseProto::VirtualBoxBaseProto()
{
    mState = NotReady;
    mStateChangeThread = NIL_RTTHREAD;
    mCallers = 0;
    mZeroCallersSem = NIL_RTSEMEVENT;
    mInitUninitSem = NIL_RTSEMEVENTMULTI;
    mInitUninitWaiters = 0;
    mObjectLock = NULL;
}

VirtualBoxBaseProto::~VirtualBoxBaseProto()
{
    if (mObjectLock)
        delete mObjectLock;
    Assert (mInitUninitWaiters == 0);
    Assert (mInitUninitSem == NIL_RTSEMEVENTMULTI);
    if (mZeroCallersSem != NIL_RTSEMEVENT)
        RTSemEventDestroy (mZeroCallersSem);
    mCallers = 0;
    mStateChangeThread = NIL_RTTHREAD;
    mState = NotReady;
}

// util::Lockable interface

RWLockHandle *VirtualBoxBaseProto::lockHandle() const
{
    /* lazy initialization */
    if (RT_UNLIKELY(!mObjectLock))
    {
        AssertCompile (sizeof (RWLockHandle *) == sizeof (void *));
        RWLockHandle *objLock = new RWLockHandle;
        if (!ASMAtomicCmpXchgPtr ((void * volatile *) &mObjectLock, objLock, NULL))
        {
            delete objLock;
            objLock = (RWLockHandle *) ASMAtomicReadPtr ((void * volatile *) &mObjectLock);
        }
        return objLock;
    }
    return mObjectLock;
}

/**
 * Increments the number of calls to this object by one.
 *
 * After this method succeeds, it is guaranted that the object will remain
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
 * This method succeeeds (and increments the number of callers) only if the
 * current object's state is Ready. Otherwise, it will return E_ACCESSDENIED
 * to indicate that the object is not operational. There are two exceptions
 * from this rule:
 * <ol>
 *   <li>If the @a aLimited argument is |true|, then this method will also
 *       succeeed if the object's state is Limited (or Ready, of course).
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
 * Also, addCaller() will block if the object is probing uninitialization on
 * another thread with AutoMayUninitSpan (i.e. the object state is MayUninit).
 * And again, the block will last until the AutoMayUninitSpan destructor signals
 * that it has finished probing and the object is either ready again or will
 * uninitialize shortly (so that addCaller() will fail).
 *
 * When this method returns a failure, the caller must not use the object
 * and should return the failed result code to its own caller.
 *
 * @param aState        Where to store the current object's state (can be
 *                      used in overriden methods to determine the cause of
 *                      the failure).
 * @param aLimited      |true| to add a limited caller.
 *
 * @return              S_OK on success or E_ACCESSDENIED on failure.
 *
 * @note It is preferrable to use the #addLimitedCaller() rather than
 *       calling this method with @a aLimited = |true|, for better
 *       self-descriptiveness.
 *
 * @sa #addLimitedCaller()
 * @sa #releaseCaller()
 */
HRESULT VirtualBoxBaseProto::addCaller (State *aState /* = NULL */,
                                        bool aLimited /* = false */)
{
    AutoWriteLock stateLock (mStateLock);

    HRESULT rc = E_ACCESSDENIED;

    if (mState == Ready || (aLimited && mState == Limited))
    {
        /* if Ready or allows Limited, increase the number of callers */
        ++ mCallers;
        rc = S_OK;
    }
    else
    if (mState == InInit || mState == MayUninit || mState == InUninit)
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /* Called from the same thread that is doing AutoInitSpan or
             * AutoUninitSpan or AutoMayUninitSpan, just succeed */
            rc = S_OK;
        }
        else if (mState == InInit || mState == MayUninit)
        {
            /* One of the two:
             *
             * 1) addCaller() is called by a "child" thread while the "parent"
             *    thread is still doing AutoInitSpan/AutoReinitSpan, so wait for
             *    the state to become either Ready/Limited or InitFailed (in
             *    case of init failure).
             *
             * 2) addCaller() is called while another thread is in
             *    AutoMayUninitSpan, so wait for the state to become either
             *    Ready or WillUninit.
             *
             * Note that in either case we increase the number of callers anyway
             * -- to prevent AutoUninitSpan from early completion if we are
             * still not scheduled to pick up the posted semaphore when uninit()
             * is called.
             */
            ++ mCallers;

            /* lazy semaphore creation */
            if (mInitUninitSem == NIL_RTSEMEVENTMULTI)
            {
                RTSemEventMultiCreate (&mInitUninitSem);
                Assert (mInitUninitWaiters == 0);
            }

            ++ mInitUninitWaiters;

            LogFlowThisFunc ((mState == InInit ?
                              "Waiting for AutoInitSpan/AutoReinitSpan to "
                              "finish...\n" :
                              "Waiting for AutoMayUninitSpan to finish...\n"));

            stateLock.leave();
            RTSemEventMultiWait (mInitUninitSem, RT_INDEFINITE_WAIT);
            stateLock.enter();

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
                Assert (mCallers != 0);
                -- mCallers;
                if (mCallers == 0 && mState == InUninit)
                {
                    /* inform AutoUninitSpan ctor there are no more callers */
                    RTSemEventSignal (mZeroCallersSem);
                }
            }
        }
    }

    if (aState)
        *aState = mState;

    return rc;
}

/**
 * Decreases the number of calls to this object by one.
 *
 * Must be called after every #addCaller() or #addLimitedCaller() when
 * protecting the object from uninitialization is no more necessary.
 */
void VirtualBoxBaseProto::releaseCaller()
{
    AutoWriteLock stateLock (mStateLock);

    if (mState == Ready || mState == Limited)
    {
        /* if Ready or Limited, decrease the number of callers */
        AssertMsgReturn (mCallers != 0, ("mCallers is ZERO!"), (void) 0);
        -- mCallers;

        return;
    }

    if (mState == InInit || mState == MayUninit || mState == InUninit)
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /* Called from the same thread that is doing AutoInitSpan,
             * AutoMayUninitSpan or AutoUninitSpan: just succeed */
            return;
        }

        if (mState == MayUninit || mState == InUninit)
        {
            /* the caller is being released after AutoUninitSpan or
             * AutoMayUninitSpan has begun */
            AssertMsgReturn (mCallers != 0, ("mCallers is ZERO!"), (void) 0);
            -- mCallers;

            if (mCallers == 0)
            {
                /* inform the Auto*UninitSpan ctor there are no more callers */
                RTSemEventSignal (mZeroCallersSem);
            }

            return;
        }
    }

    AssertMsgFailed (("mState = %d!", mState));
}

// VirtualBoxBaseProto::AutoInitSpan methods
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
VirtualBoxBaseProto::AutoInitSpan::
AutoInitSpan (VirtualBoxBaseProto *aObj,  Result aResult /* = Failed */)
    : mObj (aObj), mResult (aResult), mOk (false)
{
    Assert (aObj);

    AutoWriteLock stateLock (mObj->mStateLock);

    mOk = mObj->mState == NotReady;
    AssertReturnVoid (mOk);

    mObj->setState (InInit);
}

/**
 * Places the managed VirtualBoxBase object to  Ready/Limited state if the
 * initialization succeeded or partly succeeded, or places it to InitFailed
 * state and calls the object's uninit() method.
 *
 * Please see the AutoInitSpan class description for more info.
 */
VirtualBoxBaseProto::AutoInitSpan::~AutoInitSpan()
{
    /* if the state was other than NotReady, do nothing */
    if (!mOk)
        return;

    AutoWriteLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InInit);

    if (mObj->mCallers > 0)
    {
        Assert (mObj->mInitUninitWaiters > 0);

        /* We have some pending addCaller() calls on other threads (created
         * during InInit), signal that InInit is finished and they may go on. */
        RTSemEventMultiSignal (mObj->mInitUninitSem);
    }

    if (mResult == Succeeded)
    {
        mObj->setState (Ready);
    }
    else
    if (mResult == Limited)
    {
        mObj->setState (VirtualBoxBaseProto::Limited);
    }
    else
    {
        mObj->setState (InitFailed);
        /* leave the lock to prevent nesting when uninit() is called */
        stateLock.leave();
        /* call uninit() to let the object uninit itself after failed init() */
        mObj->uninit();
        /* Note: the object may no longer exist here (for example, it can call
         * the destructor in uninit()) */
    }
}

// VirtualBoxBaseProto::AutoReinitSpan methods
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
VirtualBoxBaseProto::AutoReinitSpan::
AutoReinitSpan (VirtualBoxBaseProto *aObj)
    : mObj (aObj), mSucceeded (false), mOk (false)
{
    Assert (aObj);

    AutoWriteLock stateLock (mObj->mStateLock);

    mOk = mObj->mState == Limited;
    AssertReturnVoid (mOk);

    mObj->setState (InInit);
}

/**
 * Places the managed VirtualBoxBase object to Ready state if the
 * re-initialization succeeded (i.e. #setSucceeded() has been called) or back to
 * Limited state otherwise.
 *
 * Please see the AutoInitSpan class description for more info.
 */
VirtualBoxBaseProto::AutoReinitSpan::~AutoReinitSpan()
{
    /* if the state was other than Limited, do nothing */
    if (!mOk)
        return;

    AutoWriteLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InInit);

    if (mObj->mCallers > 0 && mObj->mInitUninitWaiters > 0)
    {
        /* We have some pending addCaller() calls on other threads (created
         * during InInit), signal that InInit is finished and they may go on. */
        RTSemEventMultiSignal (mObj->mInitUninitSem);
    }

    if (mSucceeded)
    {
        mObj->setState (Ready);
    }
    else
    {
        mObj->setState (Limited);
    }
}

// VirtualBoxBaseProto::AutoUninitSpan methods
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
VirtualBoxBaseProto::AutoUninitSpan::AutoUninitSpan (VirtualBoxBaseProto *aObj)
    : mObj (aObj), mInitFailed (false), mUninitDone (false)
{
    Assert (aObj);

    AutoWriteLock stateLock (mObj->mStateLock);

    Assert (mObj->mState != InInit);

    /* Set mUninitDone to |true| if this object is already uninitialized
     * (NotReady) or if another AutoUninitSpan is currently active on some
     *  other thread (InUninit). */
    mUninitDone = mObj->mState == NotReady ||
                  mObj->mState == InUninit;

    if (mObj->mState == InitFailed)
    {
        /* we've been called by init() on failure */
        mInitFailed = true;
    }
    else
    {
        if (mUninitDone)
        {
            /* do nothing if already uninitialized */
            if (mObj->mState == NotReady)
                return;

            /* otherwise, wait until another thread finishes uninitialization.
             * This is necessary to make sure that when this method returns, the
             * object is NotReady and therefore can be deleted (for example).
             * In particular, this is used by
             * VirtualBoxBaseWithTypedChildrenNEXT::uninitDependentChildren(). */

            /* lazy semaphore creation */
            if (mObj->mInitUninitSem == NIL_RTSEMEVENTMULTI)
            {
                RTSemEventMultiCreate (&mObj->mInitUninitSem);
                Assert (mObj->mInitUninitWaiters == 0);
            }
            ++ mObj->mInitUninitWaiters;

            LogFlowFunc (("{%p}: Waiting for AutoUninitSpan to finish...\n",
                          mObj));

            stateLock.leave();
            RTSemEventMultiWait (mObj->mInitUninitSem, RT_INDEFINITE_WAIT);
            stateLock.enter();

            if (-- mObj->mInitUninitWaiters == 0)
            {
                /* destroy the semaphore since no more necessary */
                RTSemEventMultiDestroy (mObj->mInitUninitSem);
                mObj->mInitUninitSem = NIL_RTSEMEVENTMULTI;
            }

            return;
        }
    }

    /* go to InUninit to prevent from adding new callers */
    mObj->setState (InUninit);

    /* wait for already existing callers to drop to zero */
    if (mObj->mCallers > 0)
    {
        /* lazy creation */
        Assert (mObj->mZeroCallersSem == NIL_RTSEMEVENT);
        RTSemEventCreate (&mObj->mZeroCallersSem);

        /* wait until remaining callers release the object */
        LogFlowFunc (("{%p}: Waiting for callers (%d) to drop to zero...\n",
                      mObj, mObj->mCallers));

        stateLock.leave();
        RTSemEventWait (mObj->mZeroCallersSem, RT_INDEFINITE_WAIT);
    }
}

/**
 *  Places the managed VirtualBoxBase object to the NotReady state.
 */
VirtualBoxBaseProto::AutoUninitSpan::~AutoUninitSpan()
{
    /* do nothing if already uninitialized */
    if (mUninitDone)
        return;

    AutoWriteLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InUninit);

    mObj->setState (NotReady);
}

// VirtualBoxBaseProto::AutoMayUninitSpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a smart initialization span object that places the object to
 * MayUninit state.
 *
 * Please see the AutoMayUninitSpan class description for more info.
 *
 * @param aObj      |this| pointer of the managed VirtualBoxBase object whose
 *                  uninit() method to be probably called.
 */
VirtualBoxBaseProto::AutoMayUninitSpan::
AutoMayUninitSpan (VirtualBoxBaseProto *aObj)
    : mObj (aObj), mRC (E_FAIL), mAlreadyInProgress (false)
    , mAcceptUninit (false)
{
    Assert (aObj);

    AutoWriteLock stateLock (mObj->mStateLock);

    AssertReturnVoid (mObj->mState != InInit &&
                      mObj->mState != InUninit);

    switch (mObj->mState)
    {
        case Ready:
            break;
        case MayUninit:
            /* Nothing to be done if already in MayUninit. */
            mAlreadyInProgress = true;
            mRC = S_OK;
            return;
        default:
            /* Abuse mObj->addCaller() to get the extended error info possibly
             * set by reimplementations of addCaller() and return it to the
             * caller. Note that this abuse is supposed to be safe because we
             * should've filtered out all states where addCaller() would do
             * something else but set error info. */
            mRC = mObj->addCaller();
            Assert (FAILED (mRC));
            return;
    }

    /* go to MayUninit to cause new callers to wait until we finish */
    mObj->setState (MayUninit);
    mRC = S_OK;

    /* wait for already existing callers to drop to zero */
    if (mObj->mCallers > 0)
    {
        /* lazy creation */
        Assert (mObj->mZeroCallersSem == NIL_RTSEMEVENT);
        RTSemEventCreate (&mObj->mZeroCallersSem);

        /* wait until remaining callers release the object */
        LogFlowFunc (("{%p}: Waiting for callers (%d) to drop to zero...\n",
                      mObj, mObj->mCallers));

        stateLock.leave();
        RTSemEventWait (mObj->mZeroCallersSem, RT_INDEFINITE_WAIT);
    }
}

/**
 * Places the managed VirtualBoxBase object back to Ready state if
 * #acceptUninit() was not called, or places it to WillUninit state and calls
 * the object's uninit() method.
 *
 * Please see the AutoMayUninitSpan class description for more info.
 */
VirtualBoxBaseProto::AutoMayUninitSpan::~AutoMayUninitSpan()
{
    /* if we did nothing in the constructor, do nothing here */
    if (mAlreadyInProgress || FAILED (mRC))
        return;

    AutoWriteLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == MayUninit);

    if (mObj->mCallers > 0)
    {
        Assert (mObj->mInitUninitWaiters > 0);

        /* We have some pending addCaller() calls on other threads made after
         * going to during MayUnit, signal that MayUnit is finished and they may
         * go on. */
        RTSemEventMultiSignal (mObj->mInitUninitSem);
    }

    if (!mAcceptUninit)
    {
        mObj->setState (Ready);
    }
    else
    {
        mObj->setState (WillUninit);
        /* leave the lock to prevent nesting when uninit() is called */
        stateLock.leave();
        /* call uninit() to let the object uninit itself */
        mObj->uninit();
        /* Note: the object may no longer exist here (for example, it can call
         * the destructor in uninit()) */
    }
}

// VirtualBoxBase methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Translates the given text string according to the currently installed
 *  translation table and current context. The current context is determined
 *  by the context parameter. Additionally, a comment to the source text
 *  string text can be given. This comment (which is NULL by default)
 *  is helpful in sutuations where it is necessary to distinguish between
 *  two or more semantically different roles of the same source text in the
 *  same context.
 *
 *  @param context      the context of the translation (can be NULL
 *                      to indicate the global context)
 *  @param sourceText   the string to translate
 *  @param comment      the comment to the string (NULL means no comment)
 *
 *  @return
 *      the translated version of the source string in UTF-8 encoding,
 *      or the source string itself if the translation is not found
 *      in the given context.
 */
// static
const char *VirtualBoxBase::translate (const char *context, const char *sourceText,
                                       const char *comment)
{
#if 0
    Log(("VirtualBoxBase::translate:\n"
         "  context={%s}\n"
         "  sourceT={%s}\n"
         "  comment={%s}\n",
         context, sourceText, comment));
#endif

    /// @todo (dmik) incorporate Qt translation file parsing and lookup

    return sourceText;
}

// VirtualBoxSupportTranslationBase methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Modifies the given argument so that it will contain only a class name
 *  (null-terminated). The argument must point to a <b>non-constant</b>
 *  string containing a valid value, as it is generated by the
 *  __PRETTY_FUNCTION__ built-in macro of the GCC compiler, or by the
 *  __FUNCTION__ macro of any other compiler.
 *
 *  The function assumes that the macro is used within the member of the
 *  class derived from the VirtualBoxSupportTranslation<> template.
 *
 *  @param prettyFunctionName   string to modify
 *  @return
 *      true on success and false otherwise
 */
bool VirtualBoxSupportTranslationBase::cutClassNameFrom__PRETTY_FUNCTION__ (char *fn)
{
    Assert (fn);
    if (!fn)
        return false;

#if defined (__GNUC__)

    // the format is like:
    // VirtualBoxSupportTranslation<C>::VirtualBoxSupportTranslation() [with C = VirtualBox]

    #define START " = "
    #define END   "]"

#elif defined (_MSC_VER)

    // the format is like:
    // VirtualBoxSupportTranslation<class VirtualBox>::__ctor

    #define START "<class "
    #define END   ">::"

#endif

    char *start = strstr (fn, START);
    Assert (start);
    if (start)
    {
        start += sizeof (START) - 1;
        char *end = strstr (start, END);
        Assert (end && (end > start));
        if (end && (end > start))
        {
            size_t len = end - start;
            memmove (fn, start, len);
            fn [len] = 0;
            return true;
        }
    }

    #undef END
    #undef START

    return false;
}

// VirtualBoxSupportErrorInfoImplBase methods
////////////////////////////////////////////////////////////////////////////////

RTTLS VirtualBoxSupportErrorInfoImplBase::MultiResult::sCounter = NIL_RTTLS;

void VirtualBoxSupportErrorInfoImplBase::MultiResult::init()
{
    if (sCounter == NIL_RTTLS)
    {
        sCounter = RTTlsAlloc();
        AssertReturnVoid (sCounter != NIL_RTTLS);
    }

    uintptr_t counter = (uintptr_t) RTTlsGet (sCounter);
    ++ counter;
    RTTlsSet (sCounter, (void *) counter);
}

VirtualBoxSupportErrorInfoImplBase::MultiResult::~MultiResult()
{
    uintptr_t counter = (uintptr_t) RTTlsGet (sCounter);
    AssertReturnVoid (counter != 0);
    -- counter;
    RTTlsSet (sCounter, (void *) counter);
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
HRESULT VirtualBoxSupportErrorInfoImplBase::setErrorInternal (
    HRESULT aResultCode, const GUID &aIID,
    const Bstr &aComponent, const Bstr &aText,
    bool aWarning, bool aLogIt)
{
    /* whether multi-error mode is turned on */
    bool preserve = ((uintptr_t) RTTlsGet (MultiResult::sCounter)) > 0;

    if (aLogIt)
        LogRel (("ERROR [COM]: aRC=%Rhrc (%#08x) aIID={%RTuuid} aComponent={%ls} aText={%ls} "
                 "aWarning=%RTbool, preserve=%RTbool\n",
                 aResultCode, aResultCode, &aIID, aComponent.raw(), aText.raw(), aWarning,
                 preserve));

    /* these are mandatory, others -- not */
    AssertReturn ((!aWarning && FAILED (aResultCode)) ||
                  (aWarning && aResultCode != S_OK),
                  E_FAIL);
    AssertReturn (!aText.isEmpty(), E_FAIL);

    /* reset the error severity bit if it's a warning */
    if (aWarning)
        aResultCode &= ~0x80000000;

    HRESULT rc = S_OK;

    do
    {
        ComObjPtr <VirtualBoxErrorInfo> info;
        rc = info.createObject();
        CheckComRCBreakRC (rc);

#if !defined (VBOX_WITH_XPCOM)

        ComPtr <IVirtualBoxErrorInfo> curInfo;
        if (preserve)
        {
            /* get the current error info if any */
            ComPtr <IErrorInfo> err;
            rc = ::GetErrorInfo (0, err.asOutParam());
            CheckComRCBreakRC (rc);
            rc = err.queryInterfaceTo (curInfo.asOutParam());
            if (FAILED (rc))
            {
                /* create a IVirtualBoxErrorInfo wrapper for the native
                 * IErrorInfo object */
                ComObjPtr <VirtualBoxErrorInfo> wrapper;
                rc = wrapper.createObject();
                if (SUCCEEDED (rc))
                {
                    rc = wrapper->init (err);
                    if (SUCCEEDED (rc))
                        curInfo = wrapper;
                }
            }
        }
        /* On failure, curInfo will stay null */
        Assert (SUCCEEDED (rc) || curInfo.isNull());

        /* set the current error info and preserve the previous one if any */
        rc = info->init (aResultCode, aIID, aComponent, aText, curInfo);
        CheckComRCBreakRC (rc);

        ComPtr <IErrorInfo> err;
        rc = info.queryInterfaceTo (err.asOutParam());
        if (SUCCEEDED (rc))
            rc = ::SetErrorInfo (0, err);

#else // !defined (VBOX_WITH_XPCOM)

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            CheckComRCBreakRC (rc);

            ComPtr <IVirtualBoxErrorInfo> curInfo;
            if (preserve)
            {
                /* get the current error info if any */
                ComPtr <nsIException> ex;
                rc = em->GetCurrentException (ex.asOutParam());
                CheckComRCBreakRC (rc);
                rc = ex.queryInterfaceTo (curInfo.asOutParam());
                if (FAILED (rc))
                {
                    /* create a IVirtualBoxErrorInfo wrapper for the native
                     * nsIException object */
                    ComObjPtr <VirtualBoxErrorInfo> wrapper;
                    rc = wrapper.createObject();
                    if (SUCCEEDED (rc))
                    {
                        rc = wrapper->init (ex);
                        if (SUCCEEDED (rc))
                            curInfo = wrapper;
                    }
                }
            }
            /* On failure, curInfo will stay null */
            Assert (SUCCEEDED (rc) || curInfo.isNull());

            /* set the current error info and preserve the previous one if any */
            rc = info->init (aResultCode, aIID, aComponent, aText, curInfo);
            CheckComRCBreakRC (rc);

            ComPtr <nsIException> ex;
            rc = info.queryInterfaceTo (ex.asOutParam());
            if (SUCCEEDED (rc))
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
            LogWarningFunc (("Will not set an exception because "
                             "nsIExceptionService is not available "
                             "(NS_ERROR_UNEXPECTED). "
                             "XPCOM is being shutdown?\n"));
            rc = NS_OK;
        }

#endif // !defined (VBOX_WITH_XPCOM)
    }
    while (0);

    AssertComRC (rc);

    return SUCCEEDED (rc) ? aResultCode : rc;
}

// VirtualBoxBaseWithChildren methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Uninitializes all dependent children registered with #addDependentChild().
 *
 *  @note
 *      This method will call uninit() methods of children. If these methods
 *      access the parent object, uninitDependentChildren() must be called
 *      either at the beginning of the parent uninitialization sequence (when
 *      it is still operational) or after setReady(false) is called to
 *      indicate the parent is out of action.
 */
void VirtualBoxBaseWithChildren::uninitDependentChildren()
{
    /// @todo (r=dmik) see todo in VirtualBoxBase.h, in
    //  template <class C> void removeDependentChild (C *child)

    LogFlowThisFuncEnter();

    AutoWriteLock alock (this);
    AutoWriteLock mapLock (mMapLock);

    LogFlowThisFunc (("count=%d...\n", mDependentChildren.size()));

    if (mDependentChildren.size())
    {
        /* We keep the lock until we have enumerated all children.
         * Those ones that will try to call #removeDependentChild() from
         * a different thread will have to wait */

        Assert (mUninitDoneSem == NIL_RTSEMEVENT);
        int vrc = RTSemEventCreate (&mUninitDoneSem);
        AssertRC (vrc);

        Assert (mChildrenLeft == 0);
        mChildrenLeft = (unsigned)mDependentChildren.size();

        for (DependentChildren::iterator it = mDependentChildren.begin();
            it != mDependentChildren.end(); ++ it)
        {
            VirtualBoxBase *child = (*it).second;
            Assert (child);
            if (child)
                child->uninit();
        }

        mDependentChildren.clear();
    }

    /* Wait until all children started uninitializing on their own
     * (and therefore are waiting for some parent's method or for
     * #removeDependentChild() to return) are finished uninitialization */

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        /* let stuck children run */
        mapLock.leave();
        alock.leave();

        LogFlowThisFunc (("Waiting for uninitialization of all children...\n"));

        RTSemEventWait (mUninitDoneSem, RT_INDEFINITE_WAIT);

        alock.enter();
        mapLock.enter();

        RTSemEventDestroy (mUninitDoneSem);
        mUninitDoneSem = NIL_RTSEMEVENT;
        Assert (mChildrenLeft == 0);
    }

    LogFlowThisFuncLeave();
}

/**
 *  Returns a pointer to the dependent child corresponding to the given
 *  interface pointer (used as a key in the map) or NULL if the interface
 *  pointer doesn't correspond to any child registered using
 *  #addDependentChild().
 *
 *  @param  unk
 *      Pointer to map to the dependent child object (it is ComPtr <IUnknown>
 *      rather than IUnknown *, to guarantee IUnknown * identity)
 *  @return
 *      Pointer to the dependent child object
 */
VirtualBoxBase *VirtualBoxBaseWithChildren::getDependentChild (
    const ComPtr <IUnknown> &unk)
{
    AssertReturn (!!unk, NULL);

    AutoWriteLock alock (mMapLock);
    if (mUninitDoneSem != NIL_RTSEMEVENT)
        return NULL;

    DependentChildren::const_iterator it = mDependentChildren.find (unk);
    if (it == mDependentChildren.end())
        return NULL;
    return (*it).second;
}

/** Helper for addDependentChild() template method */
void VirtualBoxBaseWithChildren::addDependentChild (
    const ComPtr <IUnknown> &unk, VirtualBoxBase *child)
{
    AssertReturn (!!unk && child, (void) 0);

    AutoWriteLock alock (mMapLock);

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        // for this very unlikely case, we have to increase the number of
        // children left, for symmetry with #removeDependentChild()
        ++ mChildrenLeft;
        return;
    }

    std::pair <DependentChildren::iterator, bool> result =
        mDependentChildren.insert (DependentChildren::value_type (unk, child));
    AssertMsg (result.second, ("Failed to insert a child to the map\n"));
}

/** Helper for removeDependentChild() template method */
void VirtualBoxBaseWithChildren::removeDependentChild (const ComPtr <IUnknown> &unk)
{
    /// @todo (r=dmik) see todo in VirtualBoxBase.h, in
    //  template <class C> void removeDependentChild (C *child)

    AssertReturn (!!unk, (void) 0);

    AutoWriteLock alock (mMapLock);

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        // uninitDependentChildren() is in action, just increase the number
        // of children left and signal a semaphore when it reaches zero
        Assert (mChildrenLeft != 0);
        -- mChildrenLeft;
        if (mChildrenLeft == 0)
        {
            int vrc = RTSemEventSignal (mUninitDoneSem);
            AssertRC (vrc);
        }
        return;
    }

    DependentChildren::size_type result = mDependentChildren.erase (unk);
    AssertMsg (result == 1, ("Failed to remove a child from the map\n"));
    NOREF (result);
}

// VirtualBoxBaseWithChildrenNEXT methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Uninitializes all dependent children registered on this object with
 * #addDependentChild().
 *
 * Must be called from within the VirtualBoxBaseProto::AutoUninitSpan (i.e.
 * typically from this object's uninit() method) to uninitialize children
 * before this object goes out of service and becomes unusable.
 *
 * Note that this method will call uninit() methods of child objects. If
 * these methods need to call the parent object during uninitialization,
 * #uninitDependentChildren() must be called before the relevant part of the
 * parent is uninitialized: usually at the begnning of the parent
 * uninitialization sequence.
 *
 * Keep in mind that the uninitialized child objects may be no longer available
 * (i.e. may be deleted) after this method returns.
 *
 * @note Locks #childrenLock() for writing.
 *
 * @note May lock something else through the called children.
 */
void VirtualBoxBaseWithChildrenNEXT::uninitDependentChildren()
{
    AutoCaller autoCaller (this);

    /* sanity */
    AssertReturnVoid (autoCaller.state() == InUninit ||
                      autoCaller.state() == InInit);

    AutoWriteLock chLock (childrenLock());

    size_t count = mDependentChildren.size();

    while (count != 0)
    {
        /* strongly reference the weak child from the map to make sure it won't
         * be deleted while we've released the lock */
        DependentChildren::iterator it = mDependentChildren.begin();
        ComPtr <IUnknown> unk = it->first;
        Assert (!unk.isNull());

        VirtualBoxBase *child = it->second;

        /* release the lock to let children stuck in removeDependentChild() go
         * on (otherwise we'll deadlock in uninit() */
        chLock.leave();

        /* Note that if child->uninit() happens to be called on another
         * thread right before us and is not yet finished, the second
         * uninit() call will wait until the first one has done so
         * (thanks to AutoUninitSpan). */
        Assert (child);
        if (child)
            child->uninit();

        chLock.enter();

        /* uninit() is guaranteed to be done here so the child must be already
         * deleted from the list by removeDependentChild() called from there.
         * Do some checks to avoid endless loops when the user is forgetful */
        -- count;
        Assert (count == mDependentChildren.size());
        if (count != mDependentChildren.size())
            mDependentChildren.erase (it);

        Assert (count == mDependentChildren.size());
    }
}

/**
 * Returns a pointer to the dependent child (registered using
 * #addDependentChild()) corresponding to the given interface pointer or NULL if
 * the given pointer is unrelated.
 *
 * The relation is checked by using the given interface pointer as a key in the
 * map of dependent children.
 *
 * Note that ComPtr <IUnknown> is used as an argument instead of IUnknown * in
 * order to guarantee IUnknown identity and disambiguation by doing
 * QueryInterface (IUnknown) rather than a regular C cast.
 *
 * @param aUnk  Pointer to map to the dependent child object.
 * @return      Pointer to the dependent VirtualBoxBase child object.
 *
 * @note Locks #childrenLock() for reading.
 */
VirtualBoxBaseNEXT *
VirtualBoxBaseWithChildrenNEXT::getDependentChild (const ComPtr <IUnknown> &aUnk)
{
    AssertReturn (!aUnk.isNull(), NULL);

    AutoCaller autoCaller (this);

    /* return NULL if uninitDependentChildren() is in action */
    if (autoCaller.state() == InUninit)
        return NULL;

    AutoReadLock alock (childrenLock());

    DependentChildren::const_iterator it = mDependentChildren.find (aUnk);
    if (it == mDependentChildren.end())
        return NULL;

    return (*it).second;
}

/** Helper for addDependentChild(). */
void VirtualBoxBaseWithChildrenNEXT::doAddDependentChild (
    IUnknown *aUnk, VirtualBoxBaseNEXT *aChild)
{
    AssertReturnVoid (aUnk != NULL);
    AssertReturnVoid (aChild != NULL);

    AutoCaller autoCaller (this);

    /* sanity */
    AssertReturnVoid (autoCaller.state() == InInit ||
                      autoCaller.state() == Ready ||
                      autoCaller.state() == Limited);

    AutoWriteLock alock (childrenLock());

    std::pair <DependentChildren::iterator, bool> result =
        mDependentChildren.insert (DependentChildren::value_type (aUnk, aChild));
    AssertMsg (result.second, ("Failed to insert child %p to the map\n", aUnk));
}

/** Helper for removeDependentChild(). */
void VirtualBoxBaseWithChildrenNEXT::doRemoveDependentChild (IUnknown *aUnk)
{
    AssertReturnVoid (aUnk);

    AutoCaller autoCaller (this);

    /* sanity */
    AssertReturnVoid (autoCaller.state() == InUninit ||
                      autoCaller.state() == InInit ||
                      autoCaller.state() == Ready ||
                      autoCaller.state() == Limited);

    AutoWriteLock alock (childrenLock());

    DependentChildren::size_type result = mDependentChildren.erase (aUnk);
    AssertMsg (result == 1, ("Failed to remove child %p from the map\n", aUnk));
    NOREF (result);
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
