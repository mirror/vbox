/* $Id$ */
/** @file
 * IProgress implementation for Machine::openRemoteSession in VBoxSVC.
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

#ifndef ____H_PROGRESSPROXYIMPL
#define ____H_PROGRESSPROXYIMPL

#include "ProgressImpl.h"
#include "AutoCaller.h"


/**
 * The ProgressProxy class allows proxying the important Progress calls and
 * attributes to a different IProgress object for a period of time.
 */
class ATL_NO_VTABLE ProgressProxy :
    public com::SupportErrorInfoDerived<Progress, ProgressProxy, IProgress>,
    public VirtualBoxSupportTranslation<ProgressProxy>
{
public:
    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(ProgressProxy)
    DECLARE_NOT_AGGREGATABLE(ProgressProxy)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(ProgressProxy)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IProgress)
        COM_INTERFACE_ENTRY2(IDispatch, IProgress)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void    FinalRelease();
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                 VirtualBox *pParent,
#endif
                 IUnknown *pInitiator,
                 CBSTR bstrDescription,
                 BOOL fCancelable);
    HRESULT init(
#if !defined (VBOX_COM_INPROC)
                 VirtualBox *pParent,
#endif
                 IUnknown *pInitiator,
                 CBSTR bstrDescription,
                 BOOL fCancelable,
                 ULONG cOtherProgressObjects,
                 ULONG uTotalOperationsWeight,
                 CBSTR bstrFirstOperationDescription,
                 ULONG uFirstOperationWeight,
                 OUT_GUID pId = NULL);
    void    uninit();

    // IProgress properties
    STDMETHOD(COMGETTER(Percent))(ULONG *aPercent);
    STDMETHOD(COMGETTER(Completed))(BOOL *aCompleted);
    STDMETHOD(COMGETTER(Canceled))(BOOL *aCanceled);
    STDMETHOD(COMGETTER(ResultCode))(LONG *aResultCode);
    STDMETHOD(COMGETTER(ErrorInfo))(IVirtualBoxErrorInfo **aErrorInfo);
    STDMETHOD(COMGETTER(OperationPercent))(ULONG *aOperationPercent);
    STDMETHOD(COMSETTER(Timeout))(ULONG aTimeout);
    STDMETHOD(COMGETTER(Timeout))(ULONG *aTimeout);

    // IProgress methods
    STDMETHOD(WaitForCompletion)(LONG aTimeout);
    STDMETHOD(WaitForOperationCompletion)(ULONG aOperation, LONG aTimeout);
    STDMETHOD(Cancel)();
    STDMETHOD(SetCurrentOperationProgress)(ULONG aPercent);
    STDMETHOD(SetNextOperation)(IN_BSTR bstrNextOperationDescription, ULONG ulNextOperationsWeight);

    // public methods only for internal purposes

    HRESULT setResultCode(HRESULT aResultCode);
    HRESULT notifyComplete(HRESULT aResultCode);
    HRESULT notifyComplete(HRESULT aResultCode,
                           const GUID &aIID,
                           const Bstr &aComponent,
                           const char *aText, ...);
    bool    notifyPointOfNoReturn(void);
    bool    setOtherProgressObject(IProgress *pOtherProgress, ULONG uOperationWeight);
    bool    clearOtherProgressObject(const char *pszLastOperationDescription, ULONG uLastOperationWeight);

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "ProgressProxy"; }

protected:
    void clearOtherProgressObjectInternal(bool fEarly);
    void copyProgressInfo(IProgress *pOtherProgress, bool fEarly);

private:
    /** The other progress object.  This can be NULL. */
    ComPtr<IProgress> mptrOtherProgress;
    /** The number of other progress objects expected. */
    ULONG mcOtherProgressObjects;
    /** The current other progress object. */
    ULONG miCurOtherProgressObject;

};

#endif /* !____H_PROGRESSPROXYIMPL */

