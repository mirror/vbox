/** @file
 *
 * VirtualBoxErrorInfo COM classe definition
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

#ifndef ____H_VIRTUALBOXERRORINFOIMPL
#define ____H_VIRTUALBOXERRORINFOIMPL

#include "VirtualBoxBase.h"

using namespace com;

class ATL_NO_VTABLE VirtualBoxErrorInfo
#if defined (RT_OS_WINDOWS)
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

#if defined (RT_OS_WINDOWS)

    HRESULT init (IErrorInfo *aInfo);

    STDMETHOD(GetGUID) (GUID *guid);
    STDMETHOD(GetSource) (BSTR *source);
    STDMETHOD(GetDescription) (BSTR *description);
    STDMETHOD(GetHelpFile) (BSTR *pBstrHelpFile);
    STDMETHOD(GetHelpContext) (DWORD *pdwHelpContext);

#else // !defined (RT_OS_WINDOWS)

    HRESULT init (nsIException *aInfo);

    NS_DECL_NSIEXCEPTION

#endif

    VirtualBoxErrorInfo() : mResultCode (S_OK) {}

    // public initializer/uninitializer for internal purposes only
    HRESULT init (HRESULT aResultCode, const GUID &aIID,
                  const BSTR aComponent, const BSTR aText,
                  IVirtualBoxErrorInfo *aNext = NULL);

    // IVirtualBoxErrorInfo properties
    STDMETHOD(COMGETTER(ResultCode)) (HRESULT *aResultCode);
    STDMETHOD(COMGETTER(InterfaceID)) (GUIDPARAMOUT aIID);
    STDMETHOD(COMGETTER(Component)) (BSTR *aComponent);
    STDMETHOD(COMGETTER(Text)) (BSTR *aText);
    STDMETHOD(COMGETTER(Next)) (IVirtualBoxErrorInfo **aNext);

private:

    HRESULT mResultCode;
    Bstr mText;
    Guid mIID;
    Bstr mComponent;
    ComPtr <IVirtualBoxErrorInfo> mNext;
};

#endif // ____H_VIRTUALBOXERRORINFOIMPL

