/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include "Collection.h"

#include <iprt/semaphore.h>

#include <vector>

class VirtualBox;

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE ProgressBase :
    public VirtualBoxSupportErrorInfoImpl <ProgressBase, IProgress>,
    public VirtualBoxSupportTranslation <ProgressBase>,
    public VirtualBoxBase,
    public IProgress
{
protected:

    BEGIN_COM_MAP(ProgressBase)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IProgress)
    END_COM_MAP()

    HRESULT FinalConstruct();

    // public initializer/uninitializer for internal purposes only
    HRESULT protectedInit (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  const BSTR aDescription, GUIDPARAMOUT aId = NULL);
    HRESULT protectedInit();
    void protectedUninit (AutoWriteLock &alock);

public:

    // IProgress properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT aId);
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

    Guid id() { AutoWriteLock alock (this); return mId; }
    BOOL completed() { AutoWriteLock alock (this); return mCompleted; }
    HRESULT resultCode() { AutoWriteLock alock (this); return mResultCode; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Progress"; }

protected:

#if !defined (VBOX_COM_INPROC)
    /** weak parent */
    ComObjPtr <VirtualBox, ComWeakRef> mParent;
#endif
    ComPtr <IUnknown> mInitiator;

    Guid mId;
    Bstr mDescription;

    // the fields below are to be initalized by subclasses

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

class ATL_NO_VTABLE Progress :
    public VirtualBoxSupportTranslation <Progress>,
    public ProgressBase
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(Progress)

    DECLARE_NOT_AGGREGATABLE(Progress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Progress)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IProgress)
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
                  const BSTR aDescription, BOOL aCancelable,
                  GUIDPARAMOUT aId = NULL)
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
                  const BSTR aDescription, BOOL aCancelable,
                  ULONG aOperationCount, const BSTR aOperationDescription,
                  GUIDPARAMOUT aId = NULL);

    HRESULT init (BOOL aCancelable, ULONG aOperationCount,
                  const BSTR aOperationDescription);

    void uninit();

    // IProgress methods
    STDMETHOD(WaitForCompletion) (LONG aTimeout);
    STDMETHOD(WaitForOperationCompletion) (ULONG aOperation, LONG aTimeout);
    STDMETHOD(Cancel)();

    // public methods only for internal purposes

    HRESULT notifyProgress (LONG aPercent);
    HRESULT advanceOperation (const BSTR aOperationDescription);

    HRESULT notifyComplete (HRESULT aResultCode);
    HRESULT notifyComplete (HRESULT aResultCode, const GUID &aIID,
                            const Bstr &aComponent,
                            const char *aText, ...);
    HRESULT notifyCompleteBstr (HRESULT aResultCode, const GUID &aIID,
                                const Bstr &aComponent, const Bstr &aText);

private:

    RTSEMEVENTMULTI mCompletedSem;
    ULONG mWaitersCount;
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  The CombinedProgress class allows to combine several progress objects
 *  to a single progress component. This single progress component will treat
 *  all operations of individual progress objects as a single sequence of
 *  operations, that follow each other in the same order as progress objects are
 *  passed to the #init() method.
 *
 *  Individual progress objects are sequentially combined so that this progress
 *  object:
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
 *  @note
 *      It's the respoisibility of the combined progress object creator
 *      to complete individual progresses in the right order: if, let's say,
 *      the last progress is completed before all previous ones,
 *      #WaitForCompletion(-1) will most likely give 100% CPU load because it
 *      will be in a loop calling a method that returns immediately.
 */
class ATL_NO_VTABLE CombinedProgress :
    public VirtualBoxSupportTranslation <CombinedProgress>,
    public ProgressBase
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(CombinedProgress)

    DECLARE_NOT_AGGREGATABLE(CombinedProgress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CombinedProgress)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IProgress)
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
                  const BSTR aDescription,
                  IProgress *aProgress1, IProgress *aProgress2,
                  GUIDPARAMOUT aId = NULL)
    {
        AutoWriteLock alock (this);
        ComAssertRet (!isReady(), E_UNEXPECTED);

        mProgresses.resize (2);
        mProgresses [0] = aProgress1;
        mProgresses [1] = aProgress2;

        return protectedInit (
#if !defined (VBOX_COM_INPROC)
                              aParent,
#endif
                              aInitiator, aDescription, aId);
    }

    template <typename InputIterator>
    HRESULT init (
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  const BSTR aDescription,
                  InputIterator aFirstProgress, InputIterator aLastProgress,
                  GUIDPARAMOUT aId = NULL)
    {
        AutoWriteLock alock (this);
        ComAssertRet (!isReady(), E_UNEXPECTED);

        mProgresses = ProgressVector (aFirstProgress, aLastProgress);

        return protectedInit (
#if !defined (VBOX_COM_INPROC)
                              aParent,
#endif
                              aInitiator, aDescription, aId);
    }

protected:

    HRESULT protectedInit (
#if !defined (VBOX_COM_INPROC)
                           VirtualBox *aParent,
#endif
                           IUnknown *aInitiator,
                           const BSTR aDescription, GUIDPARAMOUT aId);

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

private:

    HRESULT checkProgress();

    typedef std::vector <ComPtr <IProgress> > ProgressVector;
    ProgressVector mProgresses;

    size_t mProgress;
    ULONG mCompletedOperations;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_AS (Progress, IProgress)

#endif // ____H_PROGRESSIMPL
