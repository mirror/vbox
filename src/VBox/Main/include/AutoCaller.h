/** @file
 *
 * VirtualBox COM base classes definition
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

#ifndef ____H_AUTOCALLER
#define ____H_AUTOCALLER

////////////////////////////////////////////////////////////////////////////////
//
// AutoCaller* classes
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Smart class that automatically increases the number of callers of the
 * given VirtualBoxBase object when an instance is constructed and decreases
 * it back when the created instance goes out of scope (i.e. gets destroyed).
 *
 * If #rc() returns a failure after the instance creation, it means that
 * the managed VirtualBoxBase object is not Ready, or in any other invalid
 * state, so that the caller must not use the object and can return this
 * failed result code to the upper level.
 *
 * See VirtualBoxBase::addCaller(), VirtualBoxBase::addLimitedCaller() and
 * VirtualBoxBase::releaseCaller() for more details about object callers.
 *
 * @param aLimited  |false| if this template should use
 *                  VirtualBoxBase::addCaller() calls to add callers, or
 *                  |true| if VirtualBoxBase::addLimitedCaller() should be
 *                  used.
 *
 * @note It is preferable to use the AutoCaller and AutoLimitedCaller
 *       classes than specify the @a aLimited argument, for better
 *       self-descriptiveness.
 */
template<bool aLimited>
class AutoCallerBase
{
public:

    /**
     * Increases the number of callers of the given object by calling
     * VirtualBoxBase::addCaller().
     *
     * @param aObj      Object to add a caller to. If NULL, this
     *                  instance is effectively turned to no-op (where
     *                  rc() will return S_OK and state() will be
     *                  NotReady).
     */
    AutoCallerBase(VirtualBoxBase *aObj)
        : mObj(aObj), mRC(S_OK), mState(VirtualBoxBase::NotReady)
    {
        if (mObj)
            mRC = mObj->addCaller(&mState, aLimited);
    }

    /**
     * If the number of callers was successfully increased, decreases it
     * using VirtualBoxBase::releaseCaller(), otherwise does nothing.
     */
    ~AutoCallerBase()
    {
        if (mObj && SUCCEEDED(mRC))
            mObj->releaseCaller();
    }

    /**
     * Stores the result code returned by VirtualBoxBase::addCaller() after
     * instance creation or after the last #add() call. A successful result
     * code means the number of callers was successfully increased.
     */
    HRESULT rc() const { return mRC; }

    /**
     * Returns |true| if |SUCCEEDED(rc())| is |true|, for convenience.
     * |true| means the number of callers was successfully increased.
     */
    bool isOk() const { return SUCCEEDED(mRC); }

    /**
     * Stores the object state returned by VirtualBoxBase::addCaller() after
     * instance creation or after the last #add() call.
     */
    VirtualBoxBase::State state() const { return mState; }

    /**
     * Temporarily decreases the number of callers of the managed object.
     * May only be called if #isOk() returns |true|. Note that #rc() will
     * return E_FAIL after this method succeeds.
     */
    void release()
    {
        Assert(SUCCEEDED(mRC));
        if (SUCCEEDED(mRC))
        {
            if (mObj)
                mObj->releaseCaller();
            mRC = E_FAIL;
        }
    }

    /**
     * Restores the number of callers decreased by #release(). May only be
     * called after #release().
     */
    void add()
    {
        Assert(!SUCCEEDED(mRC));
        if (mObj && !SUCCEEDED(mRC))
            mRC = mObj->addCaller(&mState, aLimited);
    }

    /**
     * Attaches another object to this caller instance.
     * The previous object's caller is released before the new one is added.
     *
     * @param aObj  New object to attach, may be @c NULL.
     */
    void attach(VirtualBoxBase *aObj)
    {
        /* detect simple self-reattachment */
        if (mObj != aObj)
        {
            if (mObj && SUCCEEDED(mRC))
                release();
            else if (!mObj)
            {
                /* Fix up the success state when nothing is attached. Otherwise
                 * there are a couple of assertion which would trigger. */
                mRC = E_FAIL;
            }
            mObj = aObj;
            add();
        }
    }

    /** Verbose equivalent to <tt>attach (NULL)</tt>. */
    void detach() { attach(NULL); }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AutoCallerBase)
    DECLARE_CLS_NEW_DELETE_NOOP(AutoCallerBase)

    VirtualBoxBase *mObj;
    HRESULT mRC;
    VirtualBoxBase::State mState;
};

/**
 * Smart class that automatically increases the number of normal
 * (non-limited) callers of the given VirtualBoxBase object when an instance
 * is constructed and decreases it back when the created instance goes out
 * of scope (i.e. gets destroyed).
 *
 * A typical usage pattern to declare a normal method of some object (i.e. a
 * method that is valid only when the object provides its full
 * functionality) is:
 * <code>
 * STDMETHODIMP Component::Foo()
 * {
 *     AutoCaller autoCaller(this);
 *     HRESULT hrc = autoCaller.rc();
 *     if (SUCCEEDED(hrc))
 *     {
 *         ...
 *     }
 *     return hrc;
 * }
 * </code>
 *
 * Using this class is equivalent to using the AutoCallerBase template with
 * the @a aLimited argument set to |false|, but this class is preferred
 * because provides better self-descriptiveness.
 *
 * See AutoCallerBase for more information about auto caller functionality.
 */
typedef AutoCallerBase<false> AutoCaller;

/**
 * Smart class that automatically increases the number of limited callers of
 * the given VirtualBoxBase object when an instance is constructed and
 * decreases it back when the created instance goes out of scope (i.e. gets
 * destroyed).
 *
 * A typical usage pattern to declare a limited method of some object (i.e.
 * a method that is valid even if the object doesn't provide its full
 * functionality) is:
 * <code>
 * STDMETHODIMP Component::Bar()
 * {
 *     AutoLimitedCaller autoCaller(this);
 *     HRESULT hrc = autoCaller.rc();
 *     if (SUCCEEDED(hrc))
 *     {
 *         ...
 *     }
 *     return hrc;
 * </code>
 *
 * Using this class is equivalent to using the AutoCallerBase template with
 * the @a aLimited argument set to |true|, but this class is preferred
 * because provides better self-descriptiveness.
 *
 * See AutoCallerBase for more information about auto caller functionality.
 */
typedef AutoCallerBase<true> AutoLimitedCaller;

/**
 * Smart class to enclose the state transition NotReady->InInit->Ready.
 *
 * The purpose of this span is to protect object initialization.
 *
 * Instances must be created as a stack-based variable taking |this| pointer
 * as the argument at the beginning of init() methods of VirtualBoxBase
 * subclasses. When this variable is created it automatically places the
 * object to the InInit state.
 *
 * When the created variable goes out of scope (i.e. gets destroyed) then,
 * depending on the result status of this initialization span, it either
 * places the object to Ready or Limited state or calls the object's
 * VirtualBoxBase::uninit() method which is supposed to place the object
 * back to the NotReady state using the AutoUninitSpan class.
 *
 * The initial result status of the initialization span is determined by the
 * @a aResult argument of the AutoInitSpan constructor (Result::Failed by
 * default). Inside the initialization span, the success status can be set
 * to Result::Succeeded using #setSucceeded(), to to Result::Limited using
 * #setLimited() or to Result::Failed using #setFailed(). Please don't
 * forget to set the correct success status before getting the AutoInitSpan
 * variable destroyed (for example, by performing an early return from
 * the init() method)!
 *
 * Note that if an instance of this class gets constructed when the object
 * is in the state other than NotReady, #isOk() returns |false| and methods
 * of this class do nothing: the state transition is not performed.
 *
 * A typical usage pattern is:
 * <code>
 * HRESULT Component::init()
 * {
 *     AutoInitSpan autoInitSpan(this);
 *     AssertReturn(autoInitSpan.isOk(), E_FAIL);
 *     ...
 *     if (FAILED(rc))
 *         return rc;
 *     ...
 *     if (SUCCEEDED(rc))
 *         autoInitSpan.setSucceeded();
 *     return rc;
 * }
 * </code>
 *
 * @note Never create instances of this class outside init() methods of
 *       VirtualBoxBase subclasses and never pass anything other than |this|
 *       as the argument to the constructor!
 */
class AutoInitSpan
{
public:

    enum Result { Failed = 0x0, Succeeded = 0x1, Limited = 0x2 };

    AutoInitSpan(VirtualBoxBase *aObj, Result aResult = Failed);
    ~AutoInitSpan();

    /**
     * Returns |true| if this instance has been created at the right moment
     * (when the object was in the NotReady state) and |false| otherwise.
     */
    bool isOk() const { return mOk; }

    /**
     * Sets the initialization status to Succeeded to indicates successful
     * initialization. The AutoInitSpan destructor will place the managed
     * VirtualBoxBase object to the Ready state.
     */
    void setSucceeded() { mResult = Succeeded; }

    /**
     * Sets the initialization status to Succeeded to indicate limited
     * (partly successful) initialization. The AutoInitSpan destructor will
     * place the managed VirtualBoxBase object to the Limited state.
     */
    void setLimited() { mResult = Limited; }

    /**
     * Sets the initialization status to Failure to indicates failed
     * initialization. The AutoInitSpan destructor will place the managed
     * VirtualBoxBase object to the InitFailed state and will automatically
     * call its uninit() method which is supposed to place the object back
     * to the NotReady state using AutoUninitSpan.
     */
    void setFailed() { mResult = Failed; }

    /** Returns the current initialization result. */
    Result result() { return mResult; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AutoInitSpan)
    DECLARE_CLS_NEW_DELETE_NOOP(AutoInitSpan)

    VirtualBoxBase *mObj;
    Result mResult : 3; // must be at least total number of bits + 1 (sign)
    bool mOk : 1;
};

/**
 * Smart class to enclose the state transition Limited->InInit->Ready.
 *
 * The purpose of this span is to protect object re-initialization.
 *
 * Instances must be created as a stack-based variable taking |this| pointer
 * as the argument at the beginning of methods of VirtualBoxBase
 * subclasses that try to re-initialize the object to bring it to the Ready
 * state (full functionality) after partial initialization (limited
 * functionality). When this variable is created, it automatically places
 * the object to the InInit state.
 *
 * When the created variable goes out of scope (i.e. gets destroyed),
 * depending on the success status of this initialization span, it either
 * places the object to the Ready state or brings it back to the Limited
 * state.
 *
 * The initial success status of the re-initialization span is |false|. In
 * order to make it successful, #setSucceeded() must be called before the
 * instance is destroyed.
 *
 * Note that if an instance of this class gets constructed when the object
 * is in the state other than Limited, #isOk() returns |false| and methods
 * of this class do nothing: the state transition is not performed.
 *
 * A typical usage pattern is:
 * <code>
 * HRESULT Component::reinit()
 * {
 *     AutoReinitSpan autoReinitSpan (this);
 *     AssertReturn (autoReinitSpan.isOk(), E_FAIL);
 *     ...
 *     if (FAILED(rc))
 *         return rc;
 *     ...
 *     if (SUCCEEDED(rc))
 *         autoReinitSpan.setSucceeded();
 *     return rc;
 * }
 * </code>
 *
 * @note Never create instances of this class outside re-initialization
 * methods of VirtualBoxBase subclasses and never pass anything other than
 * |this| as the argument to the constructor!
 */
class AutoReinitSpan
{
public:

    AutoReinitSpan(VirtualBoxBase *aObj);
    ~AutoReinitSpan();

    /**
     * Returns |true| if this instance has been created at the right moment
     * (when the object was in the Limited state) and |false| otherwise.
     */
    bool isOk() const { return mOk; }

    /**
     * Sets the re-initialization status to Succeeded to indicates
     * successful re-initialization. The AutoReinitSpan destructor will place
     * the managed VirtualBoxBase object to the Ready state.
     */
    void setSucceeded() { mSucceeded = true; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AutoReinitSpan)
    DECLARE_CLS_NEW_DELETE_NOOP(AutoReinitSpan)

    VirtualBoxBase *mObj;
    bool mSucceeded : 1;
    bool mOk : 1;
};

/**
 * Smart class to enclose the state transition Ready->InUninit->NotReady,
 * InitFailed->InUninit->NotReady.
 *
 * The purpose of this span is to protect object uninitialization.
 *
 * Instances must be created as a stack-based variable taking |this| pointer
 * as the argument at the beginning of uninit() methods of VirtualBoxBase
 * subclasses. When this variable is created it automatically places the
 * object to the InUninit state, unless it is already in the NotReady state
 * as indicated by #uninitDone() returning |true|. In the latter case, the
 * uninit() method must immediately return because there should be nothing
 * to uninitialize.
 *
 * When this variable goes out of scope (i.e. gets destroyed), it places the
 * object to NotReady state.
 *
 * A typical usage pattern is:
 * <code>
 * void Component::uninit()
 * {
 *     AutoUninitSpan autoUninitSpan (this);
 *     if (autoUninitSpan.uninitDone())
 *         return;
 *     ...
 * }
 * </code>
 *
 * @note The constructor of this class blocks the current thread execution
 *       until the number of callers added to the object using #addCaller()
 *       or AutoCaller drops to zero. For this reason, it is forbidden to
 *       create instances of this class (or call uninit()) within the
 *       AutoCaller or #addCaller() scope because it is a guaranteed
 *       deadlock.
 *
 * @note Never create instances of this class outside uninit() methods and
 *       never pass anything other than |this| as the argument to the
 *       constructor!
 */
class AutoUninitSpan
{
public:

    AutoUninitSpan(VirtualBoxBase *aObj);
    ~AutoUninitSpan();

    /** |true| when uninit() is called as a result of init() failure */
    bool initFailed() { return mInitFailed; }

    /** |true| when uninit() has already been called (so the object is NotReady) */
    bool uninitDone() { return mUninitDone; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AutoUninitSpan)
    DECLARE_CLS_NEW_DELETE_NOOP(AutoUninitSpan)

    VirtualBoxBase *mObj;
    bool mInitFailed : 1;
    bool mUninitDone : 1;
};

#endif // !____H_AUTOCALLER
