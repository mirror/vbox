/** @file
 *
 * ErrorInfo class definition
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

#if defined (__WIN__)

#else // !defined (__WIN__)

#include <nsIServiceManager.h>
#include <nsIExceptionService.h>
#include <nsCOMPtr.h>

#include <nsIInterfaceInfo.h>
#include <nsIInterfaceInfoManager.h>

#endif // !defined (__WIN__)


#include "VBox/com/VirtualBox.h"
#include "VBox/com/ErrorInfo.h"
#include "VBox/com/assert.h"

#include <iprt/stream.h>
#include <iprt/string.h>
#include <VBox/err.h>

/**
 *  Resolves a given interface ID to a string containint interface name.
 *  If, for some reason, the given IID cannot be resolved to a name,
 *  a NULL string is returned. A non-NULL interface name must be freed
 *  using SysFreeString().
 */
static void GetInterfaceNameByIID (const GUID &id, BSTR *name)
{
    Assert (name);
    if (!name)
        return;

    *name = NULL;

#if defined (__WIN__)

    LONG rc;
    LPOLESTR iidStr = NULL;
    if (StringFromIID (id, &iidStr) == S_OK)
    {
        HKEY ifaceKey;
        rc = RegOpenKeyExW (HKEY_CLASSES_ROOT, L"Interface", 0, KEY_QUERY_VALUE, &ifaceKey);
        if (rc == ERROR_SUCCESS)
        {
            HKEY iidKey;
            rc = RegOpenKeyExW (ifaceKey, iidStr, 0, KEY_QUERY_VALUE, &iidKey);
            if (rc == ERROR_SUCCESS)
            {
                // determine the size and type
                DWORD sz, type;
                rc = RegQueryValueExW (iidKey, NULL, NULL, &type, NULL, &sz);
                if (rc == ERROR_SUCCESS && type == REG_SZ)
                {
                    // query the value to BSTR
                    *name = SysAllocStringLen (NULL, (sz + 1) / sizeof (TCHAR) + 1);
                    rc = RegQueryValueExW (iidKey, NULL, NULL, NULL, (LPBYTE) *name, &sz);
                    if (rc != ERROR_SUCCESS)
                    {
                        SysFreeString (*name);
                        name = NULL;
                    }
                }
                RegCloseKey (iidKey);
            }
            RegCloseKey (ifaceKey);
        }
        CoTaskMemFree (iidStr);
    }

#else

    nsresult rv;
    nsCOMPtr <nsIInterfaceInfoManager> iim =
        do_GetService (NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED (rv))
    {
        nsCOMPtr <nsIInterfaceInfo> iinfo;
        rv = iim->GetInfoForIID (&id, getter_AddRefs (iinfo));
        if (NS_SUCCEEDED (rv))
        {
            const char *iname = NULL;
            iinfo->GetNameShared (&iname);
            char *utf8IName = NULL;
            if (VBOX_SUCCESS (RTStrCurrentCPToUtf8 (&utf8IName, iname)))
            {
                PRTUCS2 ucs2IName = NULL;
                if (VBOX_SUCCESS (RTStrUtf8ToUcs2 (&ucs2IName, utf8IName)))
                {
                    *name = SysAllocString ((OLECHAR *) ucs2IName);
                    RTStrUcs2Free (ucs2IName);
                }
                RTStrFree (utf8IName);
            }
        }
    }

#endif
}


namespace com
{

// IErrorInfo class
////////////////////////////////////////////////////////////////////////////////

void ErrorInfo::init ()
{
    HRESULT rc = E_FAIL;

#if defined (__WIN__)

    ComPtr <IErrorInfo> err;
    rc = ::GetErrorInfo (0, err.asOutParam());
    if (rc == S_OK && err)
    {
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

#else // !defined (__WIN__)

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

#endif // !defined (__WIN__)
}

void ErrorInfo::init (IUnknown *i, const GUID &iid)
{
    Assert (i);
    if (!i)
        return;

#if defined (__WIN__)

    ComPtr <IUnknown> iface = i;
    ComPtr <ISupportErrorInfo> serr;
    HRESULT rc = iface.queryInterfaceTo (serr.asOutParam());
    if (SUCCEEDED (rc))
    {
        rc = serr->InterfaceSupportsErrorInfo (iid);
        if (SUCCEEDED (rc))
            init();
    }

#else // !defined (__WIN__)

    init();

#endif // !defined (__WIN__)

    if (mIsBasicAvailable)
    {
        mCalleeIID = iid;
        GetInterfaceNameByIID (iid, mCalleeName.asOutParam());
    }
}

void ErrorInfo::init (IVirtualBoxErrorInfo *info)
{
    Assert (info);
    if (!info)
        return;

    HRESULT rc = E_FAIL;
    bool gotSomething = false;

    rc = info->COMGETTER(ResultCode) (&mResultCode);
    gotSomething |= SUCCEEDED (rc);

    rc = info->COMGETTER(InterfaceID) (mInterfaceID.asOutParam());
    gotSomething |= SUCCEEDED (rc);
    if (SUCCEEDED (rc))
        GetInterfaceNameByIID (mInterfaceID, mInterfaceName.asOutParam());

    rc = info->COMGETTER(Component) (mComponent.asOutParam());
    gotSomething |= SUCCEEDED (rc);

    rc = info->COMGETTER(Text) (mText.asOutParam());
    gotSomething |= SUCCEEDED (rc);

    if (gotSomething)
        mIsFullAvailable = mIsBasicAvailable = true;

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

// IErrorInfo class
////////////////////////////////////////////////////////////////////////////////

ProgressErrorInfo::ProgressErrorInfo (IProgress *progress) :
    ErrorInfo (true)
{
    Assert (progress);
    if (!progress)
        return;

    ComPtr <IVirtualBoxErrorInfo> info;
    HRESULT rc = progress->COMGETTER(ErrorInfo) (info.asOutParam());
    if (SUCCEEDED (rc) && info)
        init (info);
}

}; // namespace com

