/** @file
 *
 * VirtualBoxErrorInfo COM classe implementation
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

#include "VirtualBoxErrorInfoImpl.h"
#include "Logging.h"

#include <VBox/com/ErrorInfo.h>

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

HRESULT VirtualBoxErrorInfo::init(HRESULT aResultCode,
                                  const GUID &aIID,
                                  const char *pcszComponent,
                                  const Utf8Str &strText,
                                  IVirtualBoxErrorInfo *aNext)
{
    m_resultCode = aResultCode;
    m_IID = aIID;
    m_strComponent = pcszComponent;
    m_strText = strText;
    mNext = aNext;

    return S_OK;
}

HRESULT VirtualBoxErrorInfo::init(const com::ErrorInfo &info,
                                  IVirtualBoxErrorInfo *aNext)
{
    m_resultCode = info.getResultCode();
    m_IID = info.getInterfaceID();
    m_strComponent = info.getComponent();
    m_strText = info.getText();

    /* Recursively create VirtualBoxErrorInfo instances for the next objects. */
    const com::ErrorInfo *pInfo = info.getNext();
    if (pInfo)
    {
        ComObjPtr<VirtualBoxErrorInfo> nextEI;
        HRESULT rc = nextEI.createObject();
        if (FAILED(rc)) return rc;
        rc = nextEI->init(*pInfo, aNext);
        if (FAILED(rc)) return rc;
        mNext = nextEI;
    }else
        mNext = aNext;

    return S_OK;
}

// IVirtualBoxErrorInfo properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(ResultCode) (LONG *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    *aResultCode = m_resultCode;
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(InterfaceID) (BSTR *aIID)
{
    CheckComArgOutPointerValid(aIID);

    m_IID.toUtf16().cloneTo(aIID);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Component) (BSTR *aComponent)
{
    CheckComArgOutPointerValid(aComponent);

    m_strComponent.cloneTo(aComponent);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Text) (BSTR *aText)
{
    CheckComArgOutPointerValid(aText);

    m_strText.cloneTo(aText);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Next) (IVirtualBoxErrorInfo **aNext)
{
    CheckComArgOutPointerValid(aNext);

    /* this will set aNext to NULL if mNext is null */
    return mNext.queryInterfaceTo(aNext);
}

#if !defined(VBOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualBoxErrorInfo::init (IErrorInfo *aInfo)
{
    AssertReturn(aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to IErrorInfo fails below to
     * protect ourselves from bad IErrorInfo implementations (the
     * corresponding fields will simply remain null in this case). */

    m_resultCode = S_OK;
    rc = aInfo->GetGUID(m_IID.asOutParam());
    AssertComRC (rc);
    Bstr bstrComponent;
    rc = aInfo->GetSource(bstrComponent.asOutParam());
    AssertComRC (rc);
    m_strComponent = bstrComponent;
    Bstr bstrText;
    rc = aInfo->GetDescription(bstrText.asOutParam());
    AssertComRC (rc);
    m_strText = bstrText;

    return S_OK;
}

// IErrorInfo methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfo::GetDescription (BSTR *description)
{
    return COMGETTER(Text) (description);
}

STDMETHODIMP VirtualBoxErrorInfo::GetGUID (GUID *guid)
{
    Bstr iid;
    HRESULT rc = COMGETTER(InterfaceID) (iid.asOutParam());
    if (SUCCEEDED(rc))
        *guid = Guid(iid).ref();
    return rc;
}

STDMETHODIMP VirtualBoxErrorInfo::GetHelpContext (DWORD *pdwHelpContext)
{
    return E_NOTIMPL;
}

STDMETHODIMP VirtualBoxErrorInfo::GetHelpFile (BSTR *pbstrHelpFile)
{
    return E_NOTIMPL;
}

STDMETHODIMP VirtualBoxErrorInfo::GetSource (BSTR *source)
{
    return COMGETTER(Component) (source);
}

#else // defined(VBOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualBoxErrorInfo::init(nsIException *aInfo)
{
    AssertReturn(aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to nsIException fails below to
     * protect ourselves from bad nsIException implementations (the
     * corresponding fields will simply remain null in this case). */

    rc = aInfo->GetResult(&m_resultCode);
    AssertComRC(rc);

    char *pszMsg;             /* No Utf8Str.asOutParam, different allocator! */
    rc = aInfo->GetMessage(&pszMsg);
    AssertComRC(rc);
    if (NS_SUCCEEDED(rc))
    {
        m_strText = pszMsg;
        nsMemory::Free(pszMsg);
    }
    else
        m_strText.setNull();

    return S_OK;
}

// nsIException methods
////////////////////////////////////////////////////////////////////////////////

/* readonly attribute string message; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetMessage (char **aMessage)
{
    CheckComArgOutPointerValid(aMessage);

    m_strText.cloneTo(aMessage);
    return S_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetResult (nsresult *aResult)
{
    if (!aResult)
      return NS_ERROR_INVALID_POINTER;

    PRInt32 lrc;
    nsresult rc = COMGETTER(ResultCode) (&lrc);
    if (SUCCEEDED(rc))
      *aResult = lrc;
    return rc;
}

/* readonly attribute string name; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetName (char ** /* aName */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string filename; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetFilename (char ** /* aFilename */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 lineNumber; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetLineNumber (PRUint32 * /* aLineNumber */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 columnNumber; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetColumnNumber (PRUint32 * /*aColumnNumber */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetLocation (nsIStackFrame ** /* aLocation */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetInner (nsIException **aInner)
{
    ComPtr<IVirtualBoxErrorInfo> info;
    nsresult rv = COMGETTER(Next) (info.asOutParam());
    if (FAILED(rv)) return rv;
    return info.queryInterfaceTo(aInner);
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetData (nsISupports ** /* aData */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string toString (); */
NS_IMETHODIMP VirtualBoxErrorInfo::ToString (char ** /* retval */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS2 (VirtualBoxErrorInfo,
                               nsIException, IVirtualBoxErrorInfo)

#endif // defined(VBOX_WITH_XPCOM)
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
