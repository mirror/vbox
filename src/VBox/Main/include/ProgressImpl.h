/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

#include "ProgressWrap.h"
#include "VirtualBoxBase.h"

#include <iprt/semaphore.h>

////////////////////////////////////////////////////////////////////////////////

/**
 * Class for progress objects.
 */
class ATL_NO_VTABLE Progress :
    public ProgressWrap
{
protected:

    DECLARE_EMPTY_CTOR_DTOR (Progress)

    void i_checkForAutomaticTimeout(void);

#if !defined (VBOX_COM_INPROC)
    /** Weak parent. */
    VirtualBox * const      mParent;
#endif

    const ComPtr<IUnknown>  mInitiator;

    const Guid mId;
    const com::Utf8Str mDescription;

    uint64_t m_ullTimestamp;                        // progress object creation timestamp, for ETA computation

    void (*m_pfnCancelCallback)(void *);
    void *m_pvCancelUserArg;

    /* The fields below are to be properly initialized by subclasses */

    BOOL mCompleted;
    BOOL mCancelable;
    BOOL mCanceled;
    HRESULT mResultCode;
    ComPtr<IVirtualBoxErrorInfo> mErrorInfo;

    ULONG m_cOperations;                            // number of operations (so that progress dialog can
                                                    // display something like 1/3)
    ULONG m_ulTotalOperationsWeight;                // sum of weights of all operations, given to constructor

    ULONG m_ulOperationsCompletedWeight;            // summed-up weight of operations that have been completed; initially 0

    ULONG m_ulCurrentOperation;                     // operations counter, incremented with
                                                    // each setNextOperation()
    com::Utf8Str m_operationDescription;            // name of current operation; initially
                                                    // from constructor, changed with setNextOperation()
    ULONG m_ulCurrentOperationWeight;               // weight of current operation, given to setNextOperation()
    ULONG m_ulOperationPercent;                     // percentage of current operation, set with setCurrentOperationProgress()
    ULONG m_cMsTimeout;                             /**< Automatic timeout value. 0 means none. */

public:
    DECLARE_NOT_AGGREGATABLE (Progress)

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
     * @return
     */
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  Utf8Str aDescription,
                  BOOL aCancelable)
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
            aDescription, // aFirstOperationDescription
            1);     // ulFirstOperationWeight
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
     * @return
     */
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  Utf8Str aDescription, BOOL aCancelable,
                  ULONG cOperations,
                  Utf8Str aFirstOperationDescription)
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
            aFirstOperationDescription, // aFirstOperationDescription
            1);     // ulFirstOperationWeight: weigh them all the same
    }

    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                  VirtualBox *aParent,
#endif
                  IUnknown *aInitiator,
                  Utf8Str aDescription,
                  BOOL aCancelable,
                  ULONG cOperations,
                  ULONG ulTotalOperationsWeight,
                  Utf8Str aFirstOperationDescription,
                  ULONG ulFirstOperationWeight);

    HRESULT init(BOOL aCancelable,
                 ULONG aOperationCount,
                 Utf8Str aOperationDescription);

    void uninit();


    // public methods only for internal purposes
    HRESULT i_setResultCode(HRESULT aResultCode);

    HRESULT i_notifyComplete(HRESULT aResultCode);
    HRESULT i_notifyComplete(HRESULT aResultCode,
                             const GUID &aIID,
                             const char *pcszComponent,
                             const char *aText,
                             ...);
    HRESULT i_notifyCompleteV(HRESULT aResultCode,
                              const GUID &aIID,
                              const char *pcszComponent,
                              const char *aText,
                              va_list va);
    bool i_notifyPointOfNoReturn(void);

    // public methods only for internal purposes
    bool i_setCancelCallback(void (*pfnCallback)(void *), void *pvUser);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)
    BOOL i_getCompleted() const { return mCompleted; }
    HRESULT i_getResultCode() const { return mResultCode; }
    double i_calcTotalPercent();

private:

    // Wrapped IProgress Data
    HRESULT getId(com::Guid &aId);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT getInitiator(ComPtr<IUnknown> &aInitiator);
    HRESULT getCancelable(BOOL *aCancelable);
    HRESULT getPercent(ULONG *aPercent);
    HRESULT getTimeRemaining(LONG *aTimeRemaining);
    HRESULT getCompleted(BOOL *aCompleted);
    HRESULT getCanceled(BOOL *aCanceled);
    HRESULT getResultCode(LONG *aResultCode);
    HRESULT getErrorInfo(ComPtr<IVirtualBoxErrorInfo> &aErrorInfo);
    HRESULT getOperationCount(ULONG *aOperationCount);
    HRESULT getOperation(ULONG *aOperation);
    HRESULT getOperationDescription(com::Utf8Str &aOperationDescription);
    HRESULT getOperationPercent(ULONG *aOperationPercent);
    HRESULT getOperationWeight(ULONG *aOperationWeight);
    HRESULT getTimeout(ULONG *aTimeout);
    HRESULT setTimeout(ULONG aTimeout);
    HRESULT setCurrentOperationProgress(ULONG aPercent);
    HRESULT setNextOperation(const com::Utf8Str &aNextOperationDescription,
                             ULONG aNextOperationsWeight);

    // Wrapped Iprogress methods
    HRESULT waitForCompletion(LONG aTimeout);
    HRESULT waitForOperationCompletion(ULONG aOperation,
                                       LONG aTimeout);
    HRESULT waitForAsyncProgressCompletion(const ComPtr<IProgress> &aPProgressAsync);
    HRESULT cancel();


    RTSEMEVENTMULTI mCompletedSem;
    ULONG mWaitersCount;
};

#endif /* ____H_PROGRESSIMPL */

