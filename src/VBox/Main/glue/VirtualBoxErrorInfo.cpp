/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * VirtualBoxErrorInfo COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "VBox/com/VirtualBoxErrorInfo.h"

#include "../include/Logging.h"

#include <list>

namespace com
{

////////////////////////////////////////////////////////////////////////////////
// VirtualBoxErrorInfo class
////////////////////////////////////////////////////////////////////////////////

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the error info object with the given error details.
 */
HRESULT VirtualBoxErrorInfo::init (HRESULT aResultCode, const GUID *aIID,
                                   const char *aComponent, const char *aText,
                                   IVirtualBoxErrorInfo *aNext)
{
    mResultCode = aResultCode;

    if (aIID != NULL)
        mIID = *aIID;

    mComponent = aComponent;
    mText = aText;
    mNext = aNext;

    return S_OK;
}

// IVirtualBoxErrorInfo properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(ResultCode) (HRESULT *aResultCode)
{
    if (!aResultCode)
        return E_POINTER;

    *aResultCode = mResultCode;
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(InterfaceID) (OUT_GUID aIID)
{
    if (!aIID)
        return E_POINTER;

    mIID.cloneTo (aIID);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Component) (BSTR *aComponent)
{
    if (!aComponent)
        return E_POINTER;

    mComponent.cloneTo (aComponent);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Text) (BSTR *aText)
{
    if (!aText)
        return E_POINTER;

    mText.cloneTo (aText);
    return S_OK;
}

STDMETHODIMP VirtualBoxErrorInfo::COMGETTER(Next) (IVirtualBoxErrorInfo **aNext)
{
    if (!aNext)
        return E_POINTER;

    /* this will set aNext to NULL if mNext is null */
    return mNext.queryInterfaceTo (aNext);
}

#if !defined (VBOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualBoxErrorInfo::init (IErrorInfo *aInfo)
{
    AssertReturn (aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to IErrorInfo fails below to
     * protect ourselves from bad IErrorInfo implementations (the
     * corresponding fields will simply remain null in this case). */

    mResultCode = S_OK;
    rc = aInfo->GetGUID (mIID.asOutParam());
    AssertComRC (rc);
    rc = aInfo->GetSource (mComponent.asOutParam());
    AssertComRC (rc);
    rc = aInfo->GetDescription (mText.asOutParam());
    AssertComRC (rc);

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

#else // !defined (VBOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualBoxErrorInfo::init (nsIException *aInfo)
{
    AssertReturn (aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to nsIException fails below to
     * protect ourselves from bad nsIException implementations (the
     * corresponding fields will simply remain null in this case). */

    rc = aInfo->GetResult (&mResultCode);
    AssertComRC (rc);
    Utf8Str message;
    rc = aInfo->GetMessage (message.asOutParam());
    AssertComRC (rc);
    mText = message;

    return S_OK;
}

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
    ComPtr <IVirtualBoxErrorInfo> info;
    nsresult rv = COMGETTER(Next) (info.asOutParam());
    CheckComRCReturnRC (rv);
    return info.queryInterfaceTo (aInner);
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

#endif /* !defined (VBOX_WITH_XPCOM) */

////////////////////////////////////////////////////////////////////////////////
// VirtualBoxErrorInfoGlue class
////////////////////////////////////////////////////////////////////////////////

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the glue object with two given error info chains.
 *
 * @param aHead    Chain placed to the beginning.
 * @param aTail    Chain placed to the end.
 */
HRESULT VirtualBoxErrorInfoGlue::init (IVirtualBoxErrorInfo *aHead,
                                       IVirtualBoxErrorInfo *aTail)
{
    AssertReturn (aHead != NULL, E_INVALIDARG);
    AssertReturn (aTail != NULL, E_INVALIDARG);

    HRESULT rc = S_OK;

    typedef std::list <ComPtr <IVirtualBoxErrorInfo> > List;
    List list;

    ComPtr <IVirtualBoxErrorInfo> cur = aHead;

    do
    {
        ComPtr <IVirtualBoxErrorInfo> next;
        rc = cur->COMGETTER(Next) (next.asOutParam());
        CheckComRCReturnRC (rc);

        if (next.isNull())
            break;

        list.push_back (next);
        cur = next;
    }
    while (true);

    if (list.size() == 0)
    {
        /* simplest case */
        mReal = aHead;
        mNext = aTail;
        return S_OK;
    }

    for (List::iterator it = list.end(), prev = it; it != list.begin(); -- it)
    {
        ComObjPtr <VirtualBoxErrorInfoGlue> wrapper;
        rc = wrapper.createObject();
        CheckComRCBreakRC (rc);

        -- prev;

        if (it == list.end())
            rc = wrapper->protectedInit (*prev, aTail);
        else
            rc = wrapper->protectedInit (*prev, *it);

        *prev = wrapper;

        CheckComRCBreakRC (rc);
    }

    mReal = aHead;
    mNext = list.front();

    return S_OK;
}

/**
 * Protected initializer that just sets the data fields as given.
 *
 * @param aReal    Real error info object (not NULL) to forward calls to.
 * @param aNext    Next error info object (may be NULL).
 */
HRESULT VirtualBoxErrorInfoGlue::protectedInit (IVirtualBoxErrorInfo *aReal,
                                                IVirtualBoxErrorInfo *aNext)
{
    AssertReturn (aReal != NULL, E_INVALIDARG);

    mReal = aReal;
    mNext = aNext;

    return S_OK;
}

// IVirtualBoxErrorInfo properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxErrorInfoGlue::COMGETTER(Next) (IVirtualBoxErrorInfo **aNext)
{
    if (!aNext)
        return E_POINTER;

    /* this will set aNext to NULL if mNext is null */
    return mNext.queryInterfaceTo (aNext);
}

#if defined (VBOX_WITH_XPCOM)

NS_IMPL_THREADSAFE_ISUPPORTS2 (VirtualBoxErrorInfoGlue,
                               nsIException, IVirtualBoxErrorInfo)

#endif /* defined (VBOX_WITH_XPCOM) */

} /* namespace com */

