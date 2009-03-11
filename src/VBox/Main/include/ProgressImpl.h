/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_PROGRESSIMPL
#define ____H_PROGRESSIMPL

#include "VirtualBoxBase.h"

#include <VBox/com/SupportErrorInfo.h>

#include <iprt/semaphore.h>

#include <vector>

class VirtualBox;

////////////////////////////////////////////////////////////////////////////////

/**
 * Base component class for progress objects.
 */
class ATL_NO_VTABLE ProgressBase :
    public VirtualBoxBaseNEXT,
    public com::SupportErrorInfoBase,
    public VirtualBoxSupportTranslation <ProgressBase>,
    public IProgress
{
protected:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (ProgressBase)

    DECLARE_EMPTY_CTOR_DTOR (ProgressBase)

    HRESULT FinalConstruct();

    // protected initializer/uninitializer for internal purposes only
    HRESULT protectedInit (AutoInitSpan &aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription, OUT_GUID aId = NULL);
    HRESULT protectedInit (AutoInitSpan &aAutoInitSpan);
    void protectedUninit (AutoUninitSpan &aAutoUninitSpan);

public:

    // IProgress properties
    STDMETHOD(COMGETTER(Id)) (OUT_GUID aId);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMGETTER(Initiator)) (IUnknown **aInitiator);

    // IProgress properties
    STDMETHOD(COMGETTER(Cancelable)) (BOOL *aCancelable);
    STDMETHOD(COMGETTER(Percent)) (LONG *aPercent);
    STDMETHOD(COMGETTER(Completed)) (BOOL *aCompleted);
    STDMETHOD(COMGETTER(Canceled)) (BOOL *aCanceled);
    STDMETHOD(COMGETTER(ResultCode)) (HRESULT *aResultCode);
    STDMETHOD(COMGETTER(ErrorInfo)) (IVirtualBoxErrorInfo **aErrorInfo);
    STDMETHOD(COMGETTER(OperationCount)) (ULONG *aOperationCount);
    STDMETHOD(COMGETTER(Operation)) (ULONG *aCount);
    STDMETHOD(COMGETTER(OperationDescription)) (BSTR *aOperationDescription);
    STDMETHOD(COMGETTER(OperationPercent)) (LONG *aOperationPercent);

    // public methods only for internal purposes

    static HRESULT setErrorInfoOnThread (IProgress *aProgress);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    BOOL completed() const { return mCompleted; }
    HRESULT resultCode() const { return mResultCode; }

protected:

#if !defined (VBOX_COM_INPROC)
    /** Weak parent. */
    const ComObjPtr <VirtualBox, ComWeakRef> mParent;
#endif

    const ComPtr <IUnknown> mInitiator;

    const Guid mId;
    const Bstr mDescription;

    /* The fields below are to be properly initalized by subclasses */

    BOOL mCompleted;
    BOOL mCancelable;
    BOOL mCanceled;
    HRESULT mResultCode;
    ComPtr <IVirtualBoxErrorInfo> mErrorInfo;

    ULONG mOperationCount;
    ULONG mOperation;
    Bstr mOperationDescription;
    LONG mOperationPercent;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Normal progress object.
 */
class ATL_NO_VTABLE Progress :
    public com::SupportErrorInfoDerived <ProgressBase, Progress, IProgress>,
    public VirtualBoxSupportTranslation <Progress>
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE (Progress)

    DECLARE_NOT_AGGREGATABLE (Progress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (Progress)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IProgress)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription, BOOL aCancelable,
                  OUT_GUID aId = NULL)
    {
        return init (
#if !defined (VBOX_COM_INPROC)
            aParent,
#endif
            aInitiator, aDescription, aCancelable, 1, aDescription, aId);
    }

    HRESULT init (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription, BOOL aCancelable,
                  ULONG aOperationCount, CBSTR aOperationDescription,
                  OUT_GUID aId = NULL);

    HRESULT init (BOOL aCancelable, ULONG aOperationCount,
                  CBSTR aOperationDescription);

    void uninit();

    // IProgress methods
    STDMETHOD(WaitForCompletion) (LONG aTimeout);
    STDMETHOD(WaitForOperationCompletion) (ULONG aOperation, LONG aTimeout);
    STDMETHOD(Cancel)();

    // public methods only for internal purposes

    HRESULT notifyProgress (LONG aPercent);
    HRESULT advanceOperation (CBSTR aOperationDescription);

    HRESULT notifyComplete (HRESULT aResultCode);
    HRESULT notifyComplete (HRESULT aResultCode, const GUID &aIID,
                            const Bstr &aComponent,
                            const char *aText, ...);
    HRESULT notifyCompleteBstr (HRESULT aResultCode, const GUID &aIID,
                                const Bstr &aComponent, const Bstr &aText);

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "Progress"; }

private:

    RTSEMEVENTMULTI mCompletedSem;
    ULONG mWaitersCount;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * The CombinedProgress class allows to combine several progress objects to a
 * single progress component. This single progress component will treat all
 * operations of individual progress objects as a single sequence of operations
 * that follow each other in the same order as progress objects are passed to
 * the #init() method.
 *
 * Individual progress objects are sequentially combined so that this progress
 * object:
 *
 *  -   is cancelable only if all progresses are cancelable.
 *  -   is canceled once a progress that follows next to successfully completed
 *      ones reports it was canceled.
 *  -   is completed successfully only after all progresses are completed
 *      successfully.
 *  -   is completed unsuccessfully once a progress that follows next to
 *      successfully completed ones reports it was completed unsuccessfully;
 *      the result code and error info of the unsuccessful progress
 *      will be reported as the result code and error info of this progress.
 *  -   returns N as the operation number, where N equals to the number of
 *      operations in all successfully completed progresses starting from the
 *      first one plus the operation number of the next (not yet complete)
 *      progress; the operation description of the latter one is reported as
 *      the operation description of this progress object.
 *  -   returns P as the percent value, where P equals to the sum of percents
 *      of all successfully completed progresses starting from the
 *      first one plus the percent value of the next (not yet complete)
 *      progress, normalized to 100%.
 *
 * @note It's the respoisibility of the combined progress object creator to
 *       complete individual progresses in the right order: if, let's say, the
 *       last progress is completed before all previous ones,
 *       #WaitForCompletion(-1) will most likely give 100% CPU load because it
 *       will be in a loop calling a method that returns immediately.
 */
class ATL_NO_VTABLE CombinedProgress :
    public com::SupportErrorInfoDerived <ProgressBase, CombinedProgress, IProgress>,
    public VirtualBoxSupportTranslation <CombinedProgress>
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE (CombinedProgress)

    DECLARE_NOT_AGGREGATABLE (CombinedProgress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (CombinedProgress)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IProgress)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription,
                  IProgress *aProgress1, IProgress *aProgress2,
                  OUT_GUID aId = NULL);

    /**
     * Initializes the combined progress object given the first and the last
     * normal progress object from the list.
     *
     * @param aParent       See ProgressBase::init().
     * @param aInitiator    See ProgressBase::init().
     * @param aDescription  See ProgressBase::init().
     * @param aFirstProgress Iterator of the first normal progress object.
     * @param aSecondProgress Iterator of the last normal progress object.
     * @param aId           See ProgressBase::init().
     */
    template <typename InputIterator>
    HRESULT init (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription,
                  InputIterator aFirstProgress, InputIterator aLastProgress,
                  OUT_GUID aId = NULL)
    {
        /* Enclose the state transition NotReady->InInit->Ready */
        AutoInitSpan autoInitSpan (this);
        AssertReturn (autoInitSpan.isOk(), E_FAIL);

        mProgresses = ProgressVector (aFirstProgress, aLastProgress);

        HRESULT rc = protectedInit (autoInitSpan,
#if !defined (VBOX_COM_INPROC)
                                    aParent,
#endif
                                    aInitiator, aDescription, aId);

        /* Confirm a successful initialization when it's the case */
        if (SUCCEEDED (rc))
            autoInitSpan.setSucceeded();

        return rc;
    }

protected:

    HRESULT protectedInit (AutoInitSpan &aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
                           VirtualBox *aParent,
#endif
                           IUnknown *aInitiator,
                           CBSTR aDescription, OUT_GUID aId);

public:

    void uninit();

    // IProgress properties
    STDMETHOD(COMGETTER(Percent)) (LONG *aPercent);
    STDMETHOD(COMGETTER(Completed)) (BOOL *aCompleted);
    STDMETHOD(COMGETTER(Canceled)) (BOOL *aCanceled);
    STDMETHOD(COMGETTER(ResultCode)) (HRESULT *aResultCode);
    STDMETHOD(COMGETTER(ErrorInfo)) (IVirtualBoxErrorInfo **aErrorInfo);
    STDMETHOD(COMGETTER(Operation)) (ULONG *aCount);
    STDMETHOD(COMGETTER(OperationDescription)) (BSTR *aOperationDescription);
    STDMETHOD(COMGETTER(OperationPercent)) (LONG *aOperationPercent);

    // IProgress methods
    STDMETHOD(WaitForCompletion) (LONG aTimeout);
    STDMETHOD(WaitForOperationCompletion) (ULONG aOperation, LONG aTimeout);
    STDMETHOD(Cancel)();

    // public methods only for internal purposes

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "CombinedProgress"; }

private:

    HRESULT checkProgress();

    typedef std::vector <ComPtr <IProgress> > ProgressVector;
    ProgressVector mProgresses;

    size_t mProgress;
    ULONG mCompletedOperations;
};

#endif /* ____H_PROGRESSIMPL */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
