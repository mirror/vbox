/** @file
 *
 * MS COM / XPCOM Abstraction Layer
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

#if !defined (VBOX_WITH_XPCOM)

#include <objbase.h>

#else

#include <stdlib.h>
#include <VBox/err.h>
#include <iprt/path.h>

#include <nsXPCOMGlue.h>
#include <nsIComponentRegistrar.h>
#include <nsIServiceManager.h>
#include <nsCOMPtr.h>
#include <nsEventQueueUtils.h>

#include <nsIInterfaceInfo.h>
#include <nsIInterfaceInfoManager.h>

#endif

#include <iprt/string.h>
#include <VBox/err.h>

#include "VBox/com/com.h"
#include "VBox/com/assert.h"

namespace com
{

HRESULT Initialize()
{
    HRESULT rc = E_FAIL;

#if !defined (VBOX_WITH_XPCOM)

    rc = CoInitializeEx (NULL, COINIT_MULTITHREADED |
                               COINIT_DISABLE_OLE1DDE |
                               COINIT_SPEED_OVER_MEMORY);

#else

    /* Set VBOX_XPCOM_HOME if not present */
    if (!getenv("VBOX_XPCOM_HOME"))
    {
        /* get the executable path */
        char szPathProgram[1024];
        int rcVBox = RTPathProgram(szPathProgram, sizeof(szPathProgram));
        if (VBOX_SUCCESS(rcVBox))
        {
            setenv("VBOX_XPCOM_HOME", szPathProgram, 1);
        }
    }

    nsCOMPtr <nsIEventQueue> eventQ;
    rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
    if (rc == NS_ERROR_NOT_INITIALIZED)
    {
        XPCOMGlueStartup (nsnull);
        nsCOMPtr <nsIServiceManager> serviceManager;
        rc = NS_InitXPCOM2 (getter_AddRefs (serviceManager), nsnull, nsnull);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIComponentRegistrar> registrar =
                do_QueryInterface (serviceManager, &rc);
            if (NS_SUCCEEDED (rc))
                registrar->AutoRegister (nsnull);
        }
    }

#endif

    AssertComRC (rc);

    return rc;
}

void Shutdown()
{
#if !defined (VBOX_WITH_XPCOM)

    CoUninitialize();

#else

    nsCOMPtr <nsIEventQueue> eventQ;
    nsresult rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
    if (NS_SUCCEEDED (rc))
    {
        BOOL isOnMainThread = FALSE;
        eventQ->IsOnCurrentThread (&isOnMainThread);
        eventQ = nsnull; /* early release */
        if (isOnMainThread)
        {
            /* only the main thread needs to uninitialize XPCOM */
            NS_ShutdownXPCOM (nsnull);
            XPCOMGlueShutdown();
        }
    }

#endif
}

void GetInterfaceNameByIID (const GUID &aIID, BSTR *aName)
{
    Assert (aName);
    if (!aName)
        return;

    *aName = NULL;

#if !defined (VBOX_WITH_XPCOM)

    LONG rc;
    LPOLESTR iidStr = NULL;
    if (StringFromIID (aIID, &iidStr) == S_OK)
    {
        HKEY ifaceKey;
        rc = RegOpenKeyExW (HKEY_CLASSES_ROOT, L"Interface",
                            0, KEY_QUERY_VALUE, &ifaceKey);
        if (rc == ERROR_SUCCESS)
        {
            HKEY iidKey;
            rc = RegOpenKeyExW (ifaceKey, iidStr, 0, KEY_QUERY_VALUE, &iidKey);
            if (rc == ERROR_SUCCESS)
            {
                /* determine the size and type */
                DWORD sz, type;
                rc = RegQueryValueExW (iidKey, NULL, NULL, &type, NULL, &sz);
                if (rc == ERROR_SUCCESS && type == REG_SZ)
                {
                    /* query the value to BSTR */
                    *aName = SysAllocStringLen (NULL, (sz + 1) /
                                                      sizeof (TCHAR) + 1);
                    rc = RegQueryValueExW (iidKey, NULL, NULL, NULL,
                                           (LPBYTE) *aName, &sz);
                    if (rc != ERROR_SUCCESS)
                    {
                        SysFreeString (*aName);
                        aName = NULL;
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
        rv = iim->GetInfoForIID (&aIID, getter_AddRefs (iinfo));
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
                    *aName = SysAllocString ((OLECHAR *) ucs2IName);
                    RTStrUcs2Free (ucs2IName);
                }
                RTStrFree (utf8IName);
            }
        }
    }

#endif
}

}; // namespace com
