/** @file
 *
 * VirtualBoxErrorInfo COM classe definition
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_VIRTUALBOXERRORINFOIMPL
#define ____H_VIRTUALBOXERRORINFOIMPL

#include "VirtualBoxBase.h"

using namespace com;

class ATL_NO_VTABLE VirtualBoxErrorInfo
#if defined (__WIN__)
    : public CComObjectRootEx <CComMultiThreadModel>
#else
    : public CComObjectRootEx
#endif
    , public IVirtualBoxErrorInfo
{
public:

    DECLARE_NOT_AGGREGATABLE(VirtualBoxErrorInfo)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualBoxErrorInfo)
        COM_INTERFACE_ENTRY(IErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualBoxErrorInfo)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

#if defined (__WIN__)
    STDMETHOD(GetGUID) (GUID *guid);
    STDMETHOD(GetSource) (BSTR *source);
    STDMETHOD(GetDescription) (BSTR *description);
    STDMETHOD(GetHelpFile) (BSTR *pBstrHelpFile);
    STDMETHOD(GetHelpContext) (DWORD *pdwHelpContext);
#else // !defined (__WIN__)
    NS_DECL_NSIEXCEPTION
#endif

    VirtualBoxErrorInfo() : mResultCode (S_OK) {}

    // public initializer/uninitializer for internal purposes only
    void init (HRESULT resultCode, const GUID &iid,
               const BSTR component, const BSTR text);

    // IVirtualBoxErrorInfo properties
    STDMETHOD(COMGETTER(ResultCode)) (HRESULT *resultCode);
    STDMETHOD(COMGETTER(InterfaceID)) (GUIDPARAMOUT iid);
    STDMETHOD(COMGETTER(Component)) (BSTR *component);
    STDMETHOD(COMGETTER(Text)) (BSTR *text);

private:

    HRESULT mResultCode;
    Bstr mText;
    Guid mIID;
    Bstr mComponent;
};

#endif // ____H_VIRTUALBOXERRORINFOIMPL

