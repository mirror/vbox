/* $Id$ */

/** @file
 *
 * VirtualBox COM base classes implementation
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

#if !defined (VBOX_WITH_XPCOM)
#if defined (RT_OS_WINDOWS)
#include <windows.h>
#include <dbghelp.h>
#endif
#else // !defined (VBOX_WITH_XPCOM)
#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#endif // !defined (VBOX_WITH_XPCOM)

#include "VirtualBoxBase.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "Logging.h"

#include <iprt/semaphore.h>

// VirtualBoxBaseNEXT_base methods
////////////////////////////////////////////////////////////////////////////////

VirtualBoxBaseNEXT_base::VirtualBoxBaseNEXT_base()
{
    mState = NotReady;
    mStateChangeThread = NIL_RTTHREAD;
    mCallers = 0;
    mZeroCallersSem = NIL_RTSEMEVENT;
    mInitDoneSem = NIL_RTSEMEVENTMULTI;
    mInitDoneSemUsers = 0;
    RTCritSectInit (&mStateLock);
    mObjectLock = NULL;
}

VirtualBoxBaseNEXT_base::~VirtualBoxBaseNEXT_base()
{
    if (mObjectLock)
        delete mObjectLock;
    RTCritSectDelete (&mStateLock);
    Assert (mInitDoneSemUsers == 0);
    Assert (mInitDoneSem == NIL_RTSEMEVENTMULTI);
    if (mZeroCallersSem != NIL_RTSEMEVENT)
        RTSemEventDestroy (mZeroCallersSem);
    mCallers = 0;
    mStateChangeThread = NIL_RTTHREAD;
    mState = NotReady;
}

// AutoLock::Lockable interface
AutoLock::Handle *VirtualBoxBaseNEXT_base::lockHandle() const
{
    /* lasy initialization */
    if (!mObjectLock)
        mObjectLock = new AutoLock::Handle;
    return mObjectLock;
}

/**
 *  Increments the number of calls to this object by one.
 *
 *  After this method succeeds, it is guaranted that the object will remain in
 *  the Ready (or in the Limited) state at least until #releaseCaller() is
 *  called.
 *
 *  This method is intended to mark the beginning of sections of code within
 *  methods of COM objects that depend on the readiness (Ready) state. The
 *  Ready state is a primary "ready to serve" state. Usually all code that
 *  works with component's data depends on it. On practice, this means that
 *  almost every public method, setter or getter of the object should add
 *  itself as an object's caller at the very beginning, to protect from an
 *  unexpected uninitialization that may happen on a different thread.
 *
 *  Besides the Ready state denoting that the object is fully functional,
 *  there is a special Limited state. The Limited state means that the object
 *  is still functional, but its functionality is limited to some degree, so
 *  not all operations are possible. The @a aLimited argument to this method
 *  determines whether the caller represents this limited functionality or not.
 *
 *  This method succeeeds (and increments the number of callers) only if the
 *  current object's state is Ready. Otherwise, it will return E_UNEXPECTED to
 *  indicate that the object is not operational. There are two exceptions from
 *  this rule:
 *  <ol>
 *    <li>If the @a aLimited argument is |true|, then this method will also
 *        succeeed if the object's state is Limited (or Ready, of course).</li>
 *    <li>If this method is called from the same thread that placed the object
 *        to InInit or InUninit state (i.e. either from within the AutoInitSpan
 *        or AutoUninitSpan scope), it will succeed as well (but will not
 *        increase the number of callers).</li>
 *  </ol>
 *
 *  Normally, calling addCaller() never blocks. However, if this method is
 *  called by a thread created from within the AutoInitSpan scope and this
 *  scope is still active (i.e. the object state is InInit), it will block
 *  until the AutoInitSpan destructor signals that it has finished
 *  initialization.
 *
 *  When this method returns a failure, the caller must not use the object
 *  and can return the failed result code to his caller.
 *
 *  @param aState       where to store the current object's state
 *                      (can be used in overriden methods to determine the
 *                      cause of the failure)
 *  @param aLimited     |true| to add a limited caller.
 *  @return             S_OK on success or E_UNEXPECTED on failure
 *
 *  @note It is preferrable to use the #addLimitedCaller() rather than calling
 *        this method with @a aLimited = |true|, for better
 *        self-descriptiveness.
 *
 *  @sa #addLimitedCaller()
 *  @sa #releaseCaller()
 */
HRESULT VirtualBoxBaseNEXT_base::addCaller (State *aState /* = NULL */,
                                            bool aLimited /* = false */)
{
    AutoLock stateLock (mStateLock);

    HRESULT rc = E_UNEXPECTED;

    if (mState == Ready || (aLimited && mState == Limited))
    {
        /* if Ready or allows Limited, increase the number of callers */
        ++ mCallers;
        rc = S_OK;
    }
    else
    if ((mState == InInit || mState == InUninit))
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /*
             *  Called from the same thread that is doing AutoInitSpan or
             *  AutoUninitSpan, just succeed
             */
            rc = S_OK;
        }
        else if (mState == InInit)
        {
            /* addCaller() is called by a "child" thread while the "parent"
             * thread is still doing AutoInitSpan/AutoReadySpan. Wait for the
             * state to become either Ready/Limited or InitFailed/InInit/NotReady
             * (in case of init failure). Note that we increase the number of
             * callers anyway to prevent AutoUninitSpan from early completion.
             */
            ++ mCallers;

            /* lazy creation */
            if (mInitDoneSem == NIL_RTSEMEVENTMULTI)
                RTSemEventMultiCreate (&mInitDoneSem);
            ++ mInitDoneSemUsers;

            LogFlowThisFunc (("Waiting for AutoInitSpan/AutoReadySpan to finish...\n"));

            stateLock.leave();
            RTSemEventMultiWait (mInitDoneSem, RT_INDEFINITE_WAIT);
            stateLock.enter();

            if (-- mInitDoneSemUsers == 0)
            {
                /* destroy the semaphore since no more necessary */
                RTSemEventMultiDestroy (mInitDoneSem);
                mInitDoneSem = NIL_RTSEMEVENTMULTI;
            }

            if (mState == Ready)
                rc = S_OK;
            else
            {
                AssertMsg (mCallers != 0, ("mCallers is ZERO!"));
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
 *  Decrements the number of calls to this object by one.
 *  Must be called after every #addCaller() or #addLimitedCaller() when the
 *  object is no more necessary.
 */
void VirtualBoxBaseNEXT_base::releaseCaller()
{
    AutoLock stateLock (mStateLock);

    if (mState == Ready || mState == Limited)
    {
        /* if Ready or Limited, decrease the number of callers */
        AssertMsgReturn (mCallers != 0, ("mCallers is ZERO!"), (void) 0);
        -- mCallers;

        return;
    }

    if ((mState == InInit || mState == InUninit))
    {
        if (mStateChangeThread == RTThreadSelf())
        {
            /*
             *  Called from the same thread that is doing AutoInitSpan or
             *  AutoUninitSpan, just succeed
             */
            return;
        }

        if (mState == InUninit)
        {
            /* the caller is being released after AutoUninitSpan has begun */
            AssertMsgReturn (mCallers != 0, ("mCallers is ZERO!"), (void) 0);
            -- mCallers;

            if (mCallers == 0)
            {
                /* inform the AutoUninitSpan ctor there are no more callers */
                RTSemEventSignal (mZeroCallersSem);
            }

            return;
        }
    }

    AssertMsgFailed (("mState = %d!", mState));
}

// VirtualBoxBaseNEXT_base::AutoInitSpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Creates a smart initialization span object and places the object to
 *  InInit state.
 *
 *  @param aObj     |this| pointer of the managed VirtualBoxBase object whose
 *                  init() method is being called
 *  @param aStatus  initial initialization status for this span
 */
VirtualBoxBaseNEXT_base::AutoInitSpan::
AutoInitSpan (VirtualBoxBaseNEXT_base *aObj,  Status aStatus /* = Failed */)
    : mObj (aObj), mStatus (aStatus), mOk (false)
{
    Assert (aObj);

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState != InInit && mObj->mState != InUninit &&
            mObj->mState != InitFailed);

    mOk = mObj->mState == NotReady;
    if (!mOk)
        return;

    mObj->setState (InInit);
}

/**
 *  Places the managed VirtualBoxBase object to Ready/Limited state if the
 *  initialization succeeded or partly succeeded, or places it to InitFailed
 *  state and calls the object's uninit() method otherwise.
 */
VirtualBoxBaseNEXT_base::AutoInitSpan::~AutoInitSpan()
{
    /* if the state was other than NotReady, do nothing */
    if (!mOk)
        return;

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InInit);

    if (mObj->mCallers > 0)
    {
        Assert (mObj->mInitDoneSemUsers > 0);

        /* We have some pending addCaller() calls on other threads (created
         * during InInit), signal that InInit is finished. */
        RTSemEventMultiSignal (mObj->mInitDoneSem);
    }

    if (mStatus == Succeeded)
    {
        mObj->setState (Ready);
    }
    else
    if (mStatus == Limited)
    {
        mObj->setState (VirtualBoxBaseNEXT_base::Limited);
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

// VirtualBoxBaseNEXT_base::AutoReadySpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Creates a smart re-initialization span object and places the object to
 *  InInit state.
 *
 *  @param aObj     |this| pointer of the managed VirtualBoxBase object whose
 *                  re-initialization method is being called
 */
VirtualBoxBaseNEXT_base::AutoReadySpan::
AutoReadySpan (VirtualBoxBaseNEXT_base *aObj)
    : mObj (aObj), mSucceeded (false), mOk (false)
{
    Assert (aObj);

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState != InInit && mObj->mState != InUninit &&
            mObj->mState != InitFailed);

    mOk = mObj->mState == Limited;
    if (!mOk)
        return;

    mObj->setState (InInit);
}

/**
 *  Places the managed VirtualBoxBase object to Ready state if the
 *  re-initialization succeeded (i.e. #setSucceeded() has been called) or
 *  back to Limited state otherwise.
 */
VirtualBoxBaseNEXT_base::AutoReadySpan::~AutoReadySpan()
{
    /* if the state was other than Limited, do nothing */
    if (!mOk)
        return;

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InInit);

    if (mObj->mCallers > 0 && mObj->mInitDoneSemUsers > 0)
    {
        /* We have some pending addCaller() calls on other threads,
         * signal that InInit is finished. */
        RTSemEventMultiSignal (mObj->mInitDoneSem);
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

// VirtualBoxBaseNEXT_base::AutoUninitSpan methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Creates a smart uninitialization span object and places this object to
 *  InUninit state.
 *
 *  @note This method blocks the current thread execution until the number of
 *  callers of the managed VirtualBoxBase object drops to zero!
 *
 *  @param aObj |this| pointer of the VirtualBoxBase object whose uninit()
 *              method is being called
 */
VirtualBoxBaseNEXT_base::AutoUninitSpan::AutoUninitSpan (VirtualBoxBaseNEXT_base *aObj)
    : mObj (aObj), mInitFailed (false), mUninitDone (false)
{
    Assert (aObj);

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState != InInit);

    /*
     *  Set mUninitDone to |true| if this object is already uninitialized
     *  (NotReady) or if another AutoUninitSpan is currently active on some
     *  other thread (InUninit).
     */
    mUninitDone = mObj->mState == NotReady ||
                  mObj->mState == InUninit;

    if (mObj->mState == InitFailed)
    {
        /* we've been called by init() on failure */
        mInitFailed = true;
    }
    else
    {
        /* do nothing if already uninitialized */
        if (mUninitDone)
            return;
    }

    /* go to InUninit to prevent from adding new callers */
    mObj->setState (InUninit);

    if (mObj->mCallers > 0)
    {
        /* lazy creation */
        Assert (mObj->mZeroCallersSem == NIL_RTSEMEVENT);
        RTSemEventCreate (&mObj->mZeroCallersSem);

        /* wait until remaining callers release the object */
        LogFlowThisFunc (("Waiting for callers (%d) to drop to zero...\n",
                          mObj->mCallers));

        stateLock.leave();
        RTSemEventWait (mObj->mZeroCallersSem, RT_INDEFINITE_WAIT);
    }
}

/**
 *  Places the managed VirtualBoxBase object to the NotReady state.
 */
VirtualBoxBaseNEXT_base::AutoUninitSpan::~AutoUninitSpan()
{
    /* do nothing if already uninitialized */
    if (mUninitDone)
        return;

    AutoLock stateLock (mObj->mStateLock);

    Assert (mObj->mState == InUninit);

    mObj->setState (NotReady);
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
 *  @param context      the context of the the translation (can be NULL
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
//    Log(("VirtualBoxBase::translate:\n"
//         "  context={%s}\n"
//         "  sourceT={%s}\n"
//         "  comment={%s}\n",
//         context, sourceText, comment));

    /// @todo (dmik) incorporate Qt translation file parsing and lookup

    return sourceText;
}

/// @todo (dmik)
//      Using StackWalk() is not necessary here once we have ASMReturnAddress().
//      Delete later.

#if defined(DEBUG) && 0

//static
void VirtualBoxBase::AutoLock::CritSectEnter (RTCRITSECT *aLock)
{
    AssertReturn (aLock, (void) 0);

#if (defined(RT_OS_LINUX) || defined(RT_OS_OS2)) && defined(__GNUC__)

    RTCritSectEnterDebug (aLock,
                          "AutoLock::lock()/enter() return address >>>", 0,
                          (RTUINTPTR) __builtin_return_address (1));

#elif defined(RT_OS_WINDOWS)

    STACKFRAME sf;
    memset (&sf, 0, sizeof(sf));
    {
        __asm eip:
        __asm mov eax, eip
        __asm lea ebx, sf
        __asm mov [ebx]sf.AddrPC.Offset, eax
        __asm mov [ebx]sf.AddrStack.Offset, esp
        __asm mov [ebx]sf.AddrFrame.Offset, ebp
    }
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Mode      = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // get our stack frame
    BOOL ok = StackWalk (IMAGE_FILE_MACHINE_I386, process, thread,
                         &sf, NULL, NULL,
                         SymFunctionTableAccess,
                         SymGetModuleBase,
                         NULL);
    // sanity check of the returned stack frame
    ok = ok & (sf.AddrFrame.Offset != 0);
    if (ok)
    {
        // get the stack frame of our caller which is either
        // lock() or enter()
        ok = StackWalk (IMAGE_FILE_MACHINE_I386, process, thread,
                        &sf, NULL, NULL,
                        SymFunctionTableAccess,
                        SymGetModuleBase,
                        NULL);
        // sanity check of the returned stack frame
        ok = ok & (sf.AddrFrame.Offset != 0);
    }

    if (ok)
    {
        // the return address here should be the code where lock() or enter()
        // has been called from (to be more precise, where it will return)
        RTCritSectEnterDebug (aLock,
                              "AutoLock::lock()/enter() return address >>>", 0,
                              (RTUINTPTR) sf.AddrReturn.Offset);
    }
    else
    {
        RTCritSectEnter (aLock);
    }

#else

    RTCritSectEnter (aLock);

#endif // defined(RT_OS_LINUX)...
}

#endif // defined(DEBUG)

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

/**
 *  Sets error info for the current thread. This is an internal function that
 *  gets eventually called by all public variants.  If @a aPreserve is
 *  @c true, then the current error info object set on the thread before this
 *  method is called will be preserved in the IVirtualBoxErrorInfo::next
 *  attribute of the new error info object that will be then set as the
 *  current error info object.
 */
// static
HRESULT VirtualBoxSupportErrorInfoImplBase::setErrorInternal (
    HRESULT aResultCode, const GUID &aIID,
    const Bstr &aComponent, const Bstr &aText,
    bool aPreserve)
{
    LogRel (("ERROR [COM]: aRC=%#08x aIID={%Vuuid} aComponent={%ls} aText={%ls} "
             "aPreserve=%RTbool\n",
             aResultCode, &aIID, aComponent.raw(), aText.raw(), aPreserve));

    /* these are mandatory, others -- not */
    AssertReturn (FAILED (aResultCode), E_FAIL);
    AssertReturn (!aText.isEmpty(), E_FAIL);

    HRESULT rc = S_OK;

    do
    {
        ComObjPtr <VirtualBoxErrorInfo> info;
        rc = info.createObject();
        CheckComRCBreakRC (rc);

#if !defined (VBOX_WITH_XPCOM)
#if defined (RT_OS_WINDOWS)

        ComPtr <IVirtualBoxErrorInfo> curInfo;
        if (aPreserve)
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

#endif
#else // !defined (VBOX_WITH_XPCOM)

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            CheckComRCBreakRC (rc);

            ComPtr <IVirtualBoxErrorInfo> curInfo;
            if (aPreserve)
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

    AutoLock alock (this);
    AutoLock mapLock (mMapLock);

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

    AutoLock alock (mMapLock);
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

    AutoLock alock (mMapLock);

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

    AutoLock alock (mMapLock);

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
 * Uninitializes all dependent children registered with #addDependentChild().
 * 
 * Typically called from the uninit() method. Note that this method will call 
 * uninit() methods of child objects. If these methods need to call the parent 
 * object during initialization, uninitDependentChildren() must be called before 
 * the relevant part of the parent is uninitialized, usually at the begnning of 
 * the parent uninitialization sequence. 
 */
void VirtualBoxBaseWithChildrenNEXT::uninitDependentChildren()
{
    LogFlowThisFuncEnter();

    AutoLock mapLock (mMapLock);

    LogFlowThisFunc (("count=%u...\n", mDependentChildren.size()));

    if (mDependentChildren.size())
    {
        /* We keep the lock until we have enumerated all children.
         * Those ones that will try to call removeDependentChild() from a
         * different thread will have to wait */

        Assert (mUninitDoneSem == NIL_RTSEMEVENT);
        int vrc = RTSemEventCreate (&mUninitDoneSem);
        AssertRC (vrc);

        Assert (mChildrenLeft == 0);
        mChildrenLeft = mDependentChildren.size();

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

    /* Wait until all children that called uninit() on their own on other
     * threads but stuck waiting for the map lock in removeDependentChild() have
     * finished uninitialization. */

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        /* let stuck children run */
        mapLock.leave();

        LogFlowThisFunc (("Waiting for uninitialization of all children...\n"));

        RTSemEventWait (mUninitDoneSem, RT_INDEFINITE_WAIT);

        mapLock.enter();

        RTSemEventDestroy (mUninitDoneSem);
        mUninitDoneSem = NIL_RTSEMEVENT;
        Assert (mChildrenLeft == 0);
    }

    LogFlowThisFuncLeave();
}

/**
 * Returns a pointer to the dependent child corresponding to the given
 * interface pointer (used as a key in the map of dependent children) or NULL
 * if the interface pointer doesn't correspond to any child registered using 
 * #addDependentChild(). 
 *  
 * Note that ComPtr <IUnknown> is used as an argument instead of IUnknown * in 
 * order to guarantee IUnknown identity and disambiguation by doing 
 * QueryInterface (IUnknown) rather than a regular C cast. 
 *
 * @param aUnk  Pointer to map to the dependent child object.
 * @return      Pointer to the dependent child object.
 */
VirtualBoxBaseNEXT *
VirtualBoxBaseWithChildrenNEXT::getDependentChild (const ComPtr <IUnknown> &aUnk)
{
    AssertReturn (!!aUnk, NULL);

    AutoLock alock (mMapLock);

    /* return NULL if uninitDependentChildren() is in action */
    if (mUninitDoneSem != NIL_RTSEMEVENT)
        return NULL;

    DependentChildren::const_iterator it = mDependentChildren.find (aUnk);
    if (it == mDependentChildren.end())
        return NULL;
    return (*it).second;
}

void VirtualBoxBaseWithChildrenNEXT::doAddDependentChild (
    IUnknown *aUnk, VirtualBoxBaseNEXT *aChild)
{
    AssertReturnVoid (aUnk && aChild);

    AutoLock alock (mMapLock);

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        /* uninitDependentChildren() is being run. For this very unlikely case,
         * we have to increase the number of children left, for symmetry with
         * a later #removeDependentChild() call. */
        ++ mChildrenLeft;
        return;
    }

    std::pair <DependentChildren::iterator, bool> result =
        mDependentChildren.insert (DependentChildren::value_type (aUnk, aChild));
    AssertMsg (result.second, ("Failed to insert a child to the map\n"));
}

void VirtualBoxBaseWithChildrenNEXT::doRemoveDependentChild (IUnknown *aUnk)
{
    AssertReturnVoid (aUnk);

    AutoLock alock (mMapLock);

    if (mUninitDoneSem != NIL_RTSEMEVENT)
    {
        /* uninitDependentChildren() is being run. Just decrease the number of
         * children left and signal a semaphore if it reaches zero. */
        Assert (mChildrenLeft != 0);
        -- mChildrenLeft;
        if (mChildrenLeft == 0)
        {
            int vrc = RTSemEventSignal (mUninitDoneSem);
            AssertRC (vrc);
        }
        return;
    }

    DependentChildren::size_type result = mDependentChildren.erase (aUnk);
    AssertMsg (result == 1, ("Failed to remove the child %p from the map\n",
                             aUnk));
    NOREF (result);
}

// VirtualBoxBaseWithTypedChildrenNEXT methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Uninitializes all dependent children registered with 
 * #addDependentChild(). 
 *
 * @note This method will call uninit() methods of children. If these 
 *       methods access the parent object, uninitDependentChildren() must be
 *       called either at the beginning of the parent uninitialization
 *       sequence (when it is still operational) or after setReady(false) is
 *       called to indicate the parent is out of action.
 */
template <class C>
void VirtualBoxBaseWithTypedChildrenNEXT <C>::uninitDependentChildren()
{
    AutoLock mapLock (mMapLock);

    if (mDependentChildren.size())
    {
        /* set flag to ignore #removeDependentChild() called from
         * child->uninit() */
        mInUninit = true;

        /* leave the locks to let children waiting for
         * #removeDependentChild() run */
        mapLock.leave();

        for (typename DependentChildren::iterator it = mDependentChildren.begin();
            it != mDependentChildren.end(); ++ it)
        {
            C *child = (*it);
            Assert (child);
            if (child)
                child->uninit();
        }
        mDependentChildren.clear();

        mapLock.enter();

        mInUninit = false;
    }
}

// Settings API additions
////////////////////////////////////////////////////////////////////////////////

#if defined VBOX_MAIN_SETTINGS_ADDONS

namespace settings
{

template<> stdx::char_auto_ptr
ToString <com::Bstr> (const com::Bstr &aValue, unsigned int aExtra)
{
    stdx::char_auto_ptr result;

    if (aValue.raw() == NULL)
        throw ENoValue();

    /* The only way to cause RTUtf16ToUtf8Ex return a number of bytes needed
     * w/o allocating the result buffer itself is to provide that both cch
     * and *ppsz are not NULL. */
    char dummy [1];
    char *dummy2 = dummy;
    size_t strLen = 1;

    int vrc = RTUtf16ToUtf8Ex (aValue.raw(), RTSTR_MAX,
                               &dummy2, strLen, &strLen);
    if (RT_SUCCESS (vrc))
    {
        /* the string only contains '\0' :) */
        result.reset (new char [1]);
        result.get() [0] = '\0';
        return result;
    }

    if (vrc == VERR_BUFFER_OVERFLOW)
    {
        result.reset (new char [strLen + 1]);
        char *buf = result.get();
        vrc = RTUtf16ToUtf8Ex (aValue.raw(), RTSTR_MAX, &buf, strLen + 1, NULL);
    }

    if (RT_FAILURE (vrc))
        throw LogicError (RT_SRC_POS);

    return result;
}

template<> com::Guid FromString <com::Guid> (const char *aValue)
{
    if (aValue == NULL)
        throw ENoValue();

    /* For settings, the format is always {XXX...XXX} */
    char buf [RTUUID_STR_LENGTH];
    if (aValue == NULL || *aValue != '{' ||
        strlen (aValue) != RTUUID_STR_LENGTH + 1 ||
        aValue [RTUUID_STR_LENGTH] != '}')
        throw ENoConversion (FmtStr ("'%s' is not Guid", aValue));

    /* strip { and } */
    memcpy (buf, aValue + 1, RTUUID_STR_LENGTH - 1);
    buf [RTUUID_STR_LENGTH - 1] = '\0';
    /* we don't use Guid (const char *) because we want to throw
     * ENoConversion on format error */
    RTUUID uuid;
    int vrc = RTUuidFromStr (&uuid, buf);
    if (RT_FAILURE (vrc))
        throw ENoConversion (FmtStr ("'%s' is not Guid (%Vrc)", aValue, vrc));
        
    return com::Guid (uuid);
}

template<> stdx::char_auto_ptr
ToString <com::Guid> (const com::Guid &aValue, unsigned int aExtra)
{
    /* For settings, the format is always {XXX...XXX} */
    stdx::char_auto_ptr result (new char [RTUUID_STR_LENGTH + 2]);

    int vrc = RTUuidToStr (aValue.raw(), result.get() + 1, RTUUID_STR_LENGTH);
    if (RT_FAILURE (vrc))
        throw LogicError (RT_SRC_POS);

    result.get() [0] = '{';
    result.get() [RTUUID_STR_LENGTH] = '}';
    result.get() [RTUUID_STR_LENGTH + 1] = '\0';

    return result;
}

} /* namespace settings */

#endif /* VBOX_MAIN_SETTINGS_ADDONS */
