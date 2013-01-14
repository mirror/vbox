/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_PROGRESSIMPL
#define ____H_PROGRESSIMPL

#include "VirtualBoxBase.h"

#include <iprt/semaphore.h>

////////////////////////////////////////////////////////////////////////////////

/**
 * Class for progress objects.
 */
class ATL_NO_VTABLE Progress :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IProgress)
{
protected:

    DECLARE_EMPTY_CTOR_DTOR (Progress)


    void checkForAutomaticTimeout(void);

#if !defined (VBOX_COM_INPROC)
    /** Weak parent. */
    VirtualBox * const      mParent;
#endif

    const ComPtr<IUnknown>  mInitiator;

    const Guid mId;
    const Bstr mDescription;

    uint64_t m_ullTimestamp;                        // progress object creation timestamp, for ETA computation

    void (*m_pfnCancelCallback)(void *);
    void *m_pvCancelUserArg;

    /* The fields below are to be properly initialized by subclasses */

    BOOL mCompleted;
    BOOL mCancelable;
    BOOL mCanceled;
    HRESULT mResultCode;
    ComPtr<IVirtualBoxErrorInfo> mErrorInfo;

    ULONG m_cOperations;                            // number of operations (so that progress dialog can display something like 1/3)
    ULONG m_ulTotalOperationsWeight;                // sum of weights of all operations, given to constructor

    ULONG m_ulOperationsCompletedWeight;            // summed-up weight of operations that have been completed; initially 0

    ULONG m_ulCurrentOperation;                     // operations counter, incremented with each setNextOperation()
    Bstr m_bstrOperationDescription;                // name of current operation; initially from constructor, changed with setNextOperation()
    ULONG m_ulCurrentOperationWeight;               // weight of current operation, given to setNextOperation()
    ULONG m_ulOperationPercent;                     // percentage of current operation, set with setCurrentOperationProgress()
    ULONG m_cMsTimeout;                             /**< Automatic timeout value. 0 means none. */

public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Progress, IProgress)

    DECLARE_NOT_AGGREGATABLE (Progress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (Progress)
        VBOX_DEFAULT_INTERFACE_ENTRIES (IProgress)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    /**
     * Simplified constructor for progress objects that have only one
     * operation as a task.
     * @param aParent
     * @param aInitiator
     * @param aDescription
     * @param aCancelable
     * @param aId
     * @return
     */
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription,
                  BOOL aCancelable,
                  OUT_GUID aId = NULL)
    {
        return init(
#if !defined (VBOX_COM_INPROC)
            aParent,
#endif
            aInitiator,
            aDescription,
            aCancelable,
            1,      // cOperations
            1,      // ulTotalOperationsWeight
            aDescription, // bstrFirstOperationDescription
            1,      // ulFirstOperationWeight
            aId);
    }

    /**
     * Not quite so simplified constructor for progress objects that have
     * more than one operation, but all sub-operations are weighed the same.
     * @param aParent
     * @param aInitiator
     * @param aDescription
     * @param aCancelable
     * @param cOperations
     * @param bstrFirstOperationDescription
     * @param aId
     * @return
     */
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription, BOOL aCancelable,
                  ULONG cOperations,
                  CBSTR bstrFirstOperationDescription,
                  OUT_GUID aId = NULL)
    {
        return init(
#if !defined (VBOX_COM_INPROC)
            aParent,
#endif
            aInitiator,
            aDescription,
            aCancelable,
            cOperations,      // cOperations
            cOperations,      // ulTotalOperationsWeight = cOperations
            bstrFirstOperationDescription, // bstrFirstOperationDescription
            1,      // ulFirstOperationWeight: weigh them all the same
            aId);
    }

    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  CBSTR aDescription,
                  BOOL aCancelable,
                  ULONG cOperations,
                  ULONG ulTotalOperationsWeight,
                  CBSTR bstrFirstOperationDescription,
                  ULONG ulFirstOperationWeight,
                  OUT_GUID aId = NULL);

    HRESULT init(BOOL aCancelable,
                 ULONG aOperationCount,
                 CBSTR aOperationDescription);

//   initializer/uninitializer for internal purposes only
    HRESULT init(AutoInitSpan &aAutoInitSpan,
#if !defined (VBOX_COM_INPROC)
               VirtualBox *aParent,
#endif
               IUnknown *aInitiator,
               CBSTR aDescription, OUT_GUID aId = NULL);
    HRESULT init(AutoInitSpan &aAutoInitSpan);
    void init(AutoUninitSpan &aAutoUninitSpan);
    void uninit();
    void uninit(AutoUninitSpan &aAutoUninitSpan);

    // IProgress methods
    STDMETHOD(WaitForCompletion)(LONG aTimeout);
    STDMETHOD(WaitForOperationCompletion)(ULONG aOperation, LONG aTimeout);
    STDMETHOD(WaitForAsyncProgressCompletion)(IProgress *pProgressAsync);
    STDMETHOD(Cancel)();

    STDMETHOD(SetCurrentOperationProgress)(ULONG aPercent);
    STDMETHOD(SetNextOperation)(IN_BSTR bstrNextOperationDescription, ULONG ulNextOperationsWeight);

    // public methods only for internal purposes

    HRESULT setResultCode(HRESULT aResultCode);

    HRESULT notifyComplete(HRESULT aResultCode);
    HRESULT notifyComplete(HRESULT aResultCode,
                           const GUID &aIID,
                           const char *pcszComponent,
                           const char *aText,
                           ...);
    HRESULT notifyCompleteV(HRESULT aResultCode,
                            const GUID &aIID,
                            const char *pcszComponent,
                            const char *aText,
                            va_list va);
    bool notifyPointOfNoReturn(void);

    // IProgress properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMGETTER(Initiator)) (IUnknown **aInitiator);

    // IProgress properties
    STDMETHOD(COMGETTER(Cancelable)) (BOOL *aCancelable);
    STDMETHOD(COMGETTER(Percent)) (ULONG *aPercent);
    STDMETHOD(COMGETTER(TimeRemaining)) (LONG *aTimeRemaining);
    STDMETHOD(COMGETTER(Completed)) (BOOL *aCompleted);
    STDMETHOD(COMGETTER(Canceled)) (BOOL *aCanceled);
    STDMETHOD(COMGETTER(ResultCode)) (LONG *aResultCode);
    STDMETHOD(COMGETTER(ErrorInfo)) (IVirtualBoxErrorInfo **aErrorInfo);
    STDMETHOD(COMGETTER(OperationCount)) (ULONG *aOperationCount);
    STDMETHOD(COMGETTER(Operation)) (ULONG *aOperation);
    STDMETHOD(COMGETTER(OperationDescription)) (BSTR *aOperationDescription);
    STDMETHOD(COMGETTER(OperationPercent)) (ULONG *aOperationPercent);
    STDMETHOD(COMGETTER(OperationWeight)) (ULONG *aOperationWeight);
    STDMETHOD(COMSETTER(Timeout)) (ULONG aTimeout);
    STDMETHOD(COMGETTER(Timeout)) (ULONG *aTimeout);

    // public methods only for internal purposes

    bool setCancelCallback(void (*pfnCallback)(void *), void *pvUser);


    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)
    BOOL getCompleted() const { return mCompleted; }
    HRESULT getResultCode() const { return mResultCode; }
    double calcTotalPercent();

private:

    RTSEMEVENTMULTI mCompletedSem;
    ULONG mWaitersCount;
};

#endif /* ____H_PROGRESSIMPL */

