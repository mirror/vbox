/** @file
 *
 * ErrorInfo class definition
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if !defined (VBOX_WITH_XPCOM)

#else

#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>

#endif

#include "VBox/com/VirtualBox.h"
#include "VBox/com/ErrorInfo.h"
#include "VBox/com/assert.h"
#include "VBox/com/com.h"

#include <iprt/stream.h>
#include <iprt/string.h>

#include <VBox/err.h>

namespace com
{

// ErrorInfo class
////////////////////////////////////////////////////////////////////////////////

void ErrorInfo::init (bool aKeepObj /* = false */)
{
    HRESULT rc = E_FAIL;

#if !defined (VBOX_WITH_XPCOM)

    ComPtr <IErrorInfo> err;
    rc = ::GetErrorInfo (0, err.asOutParam());
    if (rc == S_OK && err)
    {
        if (aKeepObj)
            mErrorInfo = err;

        ComPtr <IVirtualBoxErrorInfo> info;
        rc = err.queryInterfaceTo (info.asOutParam());
        if (SUCCEEDED (rc) && info)
            init (info);

        if (!mIsFullAvailable)
        {
            bool gotSomething = false;

            rc = err->GetGUID (mInterfaceID.asOutParam());
            gotSomething |= SUCCEEDED (rc);
            if (SUCCEEDED (rc))
                GetInterfaceNameByIID (mInterfaceID, mInterfaceName.asOutParam());

            rc = err->GetSource (mComponent.asOutParam());
            gotSomething |= SUCCEEDED (rc);

            rc = err->GetDescription (mText.asOutParam());
            gotSomething |= SUCCEEDED (rc);

            if (gotSomething)
                mIsBasicAvailable = true;

            AssertMsg (gotSomething, ("Nothing to fetch!\n"));
        }
    }

#else // !defined (VBOX_WITH_XPCOM)

    nsCOMPtr <nsIExceptionService> es;
    es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
    if (NS_SUCCEEDED (rc))
    {
        nsCOMPtr <nsIExceptionManager> em;
        rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
        if (NS_SUCCEEDED (rc))
        {
            ComPtr <nsIException> ex;
            rc = em->GetCurrentException (ex.asOutParam());
            if (NS_SUCCEEDED (rc) && ex)
            {
                if (aKeepObj)
                    mErrorInfo = ex;

                ComPtr <IVirtualBoxErrorInfo> info;
                rc = ex.queryInterfaceTo (info.asOutParam());
                if (NS_SUCCEEDED (rc) && info)
                    init (info);

                if (!mIsFullAvailable)
                {
                    bool gotSomething = false;

                    rc = ex->GetResult (&mResultCode);
                    gotSomething |= NS_SUCCEEDED (rc);

                    Utf8Str message;
                    rc = ex->GetMessage (message.asOutParam());
                    gotSomething |= NS_SUCCEEDED (rc);
                    if (NS_SUCCEEDED (rc))
                        mText = message;

                    if (gotSomething)
                        mIsBasicAvailable = true;

                    AssertMsg (gotSomething, ("Nothing to fetch!\n"));
                }

                // set the exception to NULL (to emulate Win32 behavior)
                em->SetCurrentException (NULL);

                rc = NS_OK;
            }
        }
    }

    AssertComRC (rc);

#endif // !defined (VBOX_WITH_XPCOM)
}

void ErrorInfo::init (IUnknown *aI, const GUID &aIID, bool aKeepObj /* = false */)
{
    Assert (aI);
    if (!aI)
        return;

#if !defined (VBOX_WITH_XPCOM)

    ComPtr <IUnknown> iface = aI;
    ComPtr <ISupportErrorInfo> serr;
    HRESULT rc = iface.queryInterfaceTo (serr.asOutParam());
    if (SUCCEEDED (rc))
    {
        rc = serr->InterfaceSupportsErrorInfo (aIID);
        if (SUCCEEDED (rc))
            init (aKeepObj);
    }

#else

    init (aKeepObj);

#endif

    if (mIsBasicAvailable)
    {
        mCalleeIID = aIID;
        GetInterfaceNameByIID (aIID, mCalleeName.asOutParam());
    }
}

void ErrorInfo::init (IVirtualBoxErrorInfo *info)
{
    AssertReturnVoid (info);

    HRESULT rc = E_FAIL;
    bool gotSomething = false;
    bool gotAll = true;

    rc = info->COMGETTER(ResultCode) (&mResultCode);
    gotSomething |= SUCCEEDED (rc);
    gotAll &= SUCCEEDED (rc);

    rc = info->COMGETTER(InterfaceID) (mInterfaceID.asOutParam());
    gotSomething |= SUCCEEDED (rc);
    gotAll &= SUCCEEDED (rc);
    if (SUCCEEDED (rc))
        GetInterfaceNameByIID (mInterfaceID, mInterfaceName.asOutParam());

    rc = info->COMGETTER(Component) (mComponent.asOutParam());
    gotSomething |= SUCCEEDED (rc);
    gotAll &= SUCCEEDED (rc);

    rc = info->COMGETTER(Text) (mText.asOutParam());
    gotSomething |= SUCCEEDED (rc);
    gotAll &= SUCCEEDED (rc);

    ComPtr <IVirtualBoxErrorInfo> next;
    rc = info->COMGETTER(Next) (next.asOutParam());
    if (SUCCEEDED (rc) && !next.isNull())
    {
        mNext.reset (new ErrorInfo (next));
        Assert (mNext.get());
        if (!mNext.get())
            rc = E_OUTOFMEMORY;
    }
    else
        mNext.reset();
    gotSomething |= SUCCEEDED (rc);
    gotAll &= SUCCEEDED (rc);

    mIsBasicAvailable = gotSomething;
    mIsFullAvailable = gotAll;

    AssertMsg (gotSomething, ("Nothing to fetch!\n"));
}

ErrorInfo::~ErrorInfo()
{
}

void ErrorInfo::print (const char *aPrefix /* = NULL */)
{
    if (aPrefix == NULL)
        aPrefix = "";

    RTPrintf ("%sFull error info present: %RTbool, basic error info present: %RTbool\n", aPrefix,
              mIsFullAvailable, mIsBasicAvailable);
    if (mIsFullAvailable || mIsBasicAvailable)
    {
        RTPrintf ("%sResult Code = %Rwa\n", aPrefix, mResultCode);
        RTPrintf ("%sText        = %ls\n", aPrefix, mText.raw());
        RTPrintf ("%sComponent   = %ls, Interface: %ls, {%s}\n", aPrefix,
                  mComponent.raw(), mInterfaceName.raw(), mInterfaceID.toString().raw());
        RTPrintf ("%sCallee      = %ls, {%s}\n", aPrefix, mCalleeName.raw(), mCalleeIID.toString().raw());
    }
}

// ProgressErrorInfo class
////////////////////////////////////////////////////////////////////////////////

ProgressErrorInfo::ProgressErrorInfo (IProgress *progress) :
    ErrorInfo (false /* aDummy */)
{
    Assert (progress);
    if (!progress)
        return;

    ComPtr <IVirtualBoxErrorInfo> info;
    HRESULT rc = progress->COMGETTER(ErrorInfo) (info.asOutParam());
    if (SUCCEEDED (rc) && info)
        init (info);
}

// ErrorInfoKeeper class
////////////////////////////////////////////////////////////////////////////////

HRESULT ErrorInfoKeeper::restore()
{
    if (mForgot)
        return S_OK;

    HRESULT rc = S_OK;

#if !defined (VBOX_WITH_XPCOM)

    ComPtr <IErrorInfo> err;
    if (!mErrorInfo.isNull())
    {
        rc = mErrorInfo.queryInterfaceTo (err.asOutParam());
        AssertComRC (rc);
    }
    rc = ::SetErrorInfo (0, err);

#else // !defined (VBOX_WITH_XPCOM)

    nsCOMPtr <nsIExceptionService> es;
    es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
    if (NS_SUCCEEDED (rc))
    {
        nsCOMPtr <nsIExceptionManager> em;
        rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
        if (NS_SUCCEEDED (rc))
        {
            ComPtr <nsIException> ex;
            if (!mErrorInfo.isNull())
            {
                rc = mErrorInfo.queryInterfaceTo (ex.asOutParam());
                AssertComRC (rc);
            }
            rc = em->SetCurrentException (ex);
        }
    }

#endif // !defined (VBOX_WITH_XPCOM)

    if (SUCCEEDED (rc))
    {
        mErrorInfo.setNull();
        mForgot = true;
    }

    return rc;
}

} /* namespace com */

