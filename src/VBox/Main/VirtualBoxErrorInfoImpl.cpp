/** @file
 *
 * VirtualBoxErrorInfo COM classe implementation
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

#include "VirtualBoxErrorInfoImpl.h"
#include "Logging.h"

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

void VirtualBoxErrorInfo::init (HRESULT resultCode, const GUID &iid,
                                const BSTR component, const BSTR text)
{
    mResultCode = resultCode;
    mIID = iid;
    mComponent = component;
    mText = text;
}

// IVirtualBoxErrorInfo properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(ResultCode) (HRESULT *resultCode)
{
    if (!resultCode)
        return E_POINTER;

    *resultCode = mResultCode;
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(InterfaceID) (GUIDPARAMOUT iid)
{
    if (!iid)
        return E_POINTER;

    mIID.cloneTo (iid);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Component) (BSTR *component)
{
    if (!component)
        return E_POINTER;

    mComponent.cloneTo (component);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Text) (BSTR *text)
{
    if (!text)
        return E_POINTER;

    mText.cloneTo (text);
    return S_OK;
}

#if defined (__WIN__)

// IErrorInfo methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfo::GetDescription (BSTR *description)
{
    return COMGETTER(Text) (description);
}

STDMETHODIMP VirtualBoxErrorInfo::GetGUID (GUID *guid)
{
    return COMGETTER(InterfaceID) (guid);
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

#else // !defined (__WIN__)

// nsIException methods
////////////////////////////////////////////////////////////////////////////////

/* readonly attribute string message; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetMessage (char **aMessage)
{
    if (!aMessage)
        return NS_ERROR_INVALID_POINTER;

    Utf8Str (mText).cloneTo (aMessage);
    return S_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetResult (nsresult *aResult)
{
    return COMGETTER(ResultCode) (aResult);
}

/* readonly attribute string name; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetName (char **aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string filename; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetFilename (char **aFilename)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 lineNumber; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetLineNumber (PRUint32 *aLineNumber)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 columnNumber; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetColumnNumber (PRUint32 *aColumnNumber)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetLocation (nsIStackFrame **aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetInner (nsIException **aInner)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP VirtualBoxErrorInfo::GetData (nsISupports **aData)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string toString (); */
NS_IMETHODIMP VirtualBoxErrorInfo::ToString (char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS2 (VirtualBoxErrorInfo,
                               nsIException, IVirtualBoxErrorInfo)

#endif

