/** @file
 * MS COM / XPCOM Abstraction Layer
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

#include <objbase.h>

#else /* !defined (VBOX_WITH_XPCOM) */

#include <stdlib.h>

#include <nsCOMPtr.h>
#include <nsIServiceManagerUtils.h>

#include <nsIInterfaceInfo.h>
#include <nsIInterfaceInfoManager.h>

#endif /* !defined (VBOX_WITH_XPCOM) */

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/string.h>

#include <VBox/err.h>

#include "VBox/com/com.h"
#include "VBox/com/assert.h"


#ifdef RT_OS_DARWIN
#define VBOX_USER_HOME_SUFFIX   "Library/VirtualBox"
#else
#define VBOX_USER_HOME_SUFFIX   ".VirtualBox"
#endif 


namespace com
{

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

#else /* !defined (VBOX_WITH_XPCOM) */

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

#endif /* !defined (VBOX_WITH_XPCOM) */
}

int GetVBoxUserHomeDirectory (char *aDir, size_t aDirLen)
{
    AssertReturn (aDir, VERR_INVALID_POINTER);
    AssertReturn (aDirLen > 0, VERR_BUFFER_OVERFLOW);

    /* start with null */
    *aDir = 0;

    const char *VBoxUserHome = RTEnvGet ("VBOX_USER_HOME");

    char path [RTPATH_MAX];
    int vrc = VINF_SUCCESS;

    if (VBoxUserHome)
    {
        /* get the full path name */
        char *VBoxUserHomeUtf8 = NULL;
        vrc = RTStrCurrentCPToUtf8 (&VBoxUserHomeUtf8, VBoxUserHome);
        if (RT_SUCCESS (vrc))
        {
            vrc = RTPathAbs (VBoxUserHomeUtf8, path, sizeof (path));
            if (RT_SUCCESS (vrc))
            {
                if (aDirLen < strlen (path) + 1)
                    vrc = VERR_BUFFER_OVERFLOW;
                else
                    strcpy (aDir, path);
            }
            RTStrFree (VBoxUserHomeUtf8);
        }
    }
    else
    {
        /* compose the config directory (full path) */
        vrc = RTPathUserHome (path, sizeof (path));
        if (RT_SUCCESS (vrc))
        {
            size_t len = 
                RTStrPrintf (aDir, aDirLen, "%s%c%s",
                             path, RTPATH_DELIMITER, VBOX_USER_HOME_SUFFIX);
            if (len != strlen (path) + 1 + strlen (VBOX_USER_HOME_SUFFIX))
                vrc = VERR_BUFFER_OVERFLOW;
        }
    }

    /* ensure the home directory exists */
    if (RT_SUCCESS (vrc))
        if (!RTDirExists (aDir))
            vrc = RTDirCreateFullPath (aDir, 0777);

    return vrc;
}

} /* namespace com */
